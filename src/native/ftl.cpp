// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#include "native/ftl.h"

#include "arx_pistoris/base/flags.h"
#include "arx_pistoris/base/status.h"
#include "arx_pistoris/native/ftl.hpp"
#include "arx_pistoris/runtime/types.h"

#include "native/fixed_string.h"
#include "native/resource_lookup.h"
#include "native/resource_path.h"
#include "utils/container_allocation.h"
#include "utils/cursor.h"
#include "utils/log.h"
#include "utils/native_text.h"
#include "utils/return_code.h"

#include <cstdint>
#include <cstring>
#include <format>
#include <limits>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace pistoris {
namespace {

char lowerAscii(char value) noexcept {
  return value >= 'A' && value <= 'Z' ? static_cast<char>(value - 'A' + 'a') : value;
}

template <std::size_t N>
bool canonicalizeFixedPath(char (&value)[N], bool stem) {
  if (!isNullTerminated(value)) return false;
  std::string normalized;
  const bool valid =
      stem ? normalizeNativeResourceStem(value, normalized) : normalizeNativeResourcePath(value, normalized);
  if (!valid || normalized.size() >= N) return false;
  std::memset(value, 0, N);
  std::memcpy(value, normalized.data(), normalized.size());
  return true;
}

template <std::size_t N>
void lowercaseFixed(char (&value)[N]) noexcept {
  for (std::size_t index = 0; index < N && value[index] != '\0'; ++index) value[index] = lowerAscii(value[index]);
}

template <std::size_t N>
bool isCanonicalFixedPath(const char (&value)[N], bool stem) {
  if (!isNullTerminated(value)) return false;
  std::string normalized;
  if (!normalizeNativeResourcePath(value, normalized) || normalized != value) return false;
  if (!stem) return true;
  std::string encoded;
  return encodeNativeResourceStem(normalized, N, encoded);
}

template <std::size_t N>
bool isLowercaseFixed(const char (&value)[N]) noexcept {
  if (!isNullTerminated(value)) return false;
  for (std::size_t index = 0; index < N && value[index] != '\0'; ++index)
    if (lowerAscii(value[index]) != value[index]) return false;
  return true;
}

template <std::size_t N>
bool writeStem(char (&out)[N], const char (&source)[N]) {
  std::string encoded;
  if (!encodeNativeResourceStem(source, N, encoded)) return false;
  std::memset(out, 0, N);
  std::memcpy(out, encoded.data(), encoded.size());
  return true;
}

}  // namespace

static ArxReturnCode readVertices(ftl::Data* d, ReadCursor& c, std::int32_t n) {
  if (!tryResize(d->vertices, n)) return ARX_BAD_ALLOC;

  for (auto& vertex : d->vertices) {
    c.skip(32);  // legacy unused prefix
    c.read(vertex);
  }

  if (!c) return ARX_UNEXPECTED_EOF;
  return ARX_OK;
}

static ArxReturnCode readFaces(ftl::Data* d, ReadCursor& c, std::int32_t n) {
  if (!tryResize(d->faces, n)) return ARX_BAD_ALLOC;

  for (auto& face : d->faces) {
    c.read(face.type);
    c.skip(4 * 3);  // rgb
    c.read(face.vertex_idx);
    c.read(face.texture_id);
    c.read(face.u);
    c.read(face.v);
    c.skip(2 * 6);  // ou, ov
    c.read(face.transval);
    c.read(face.norm);
    c.skip(4 * 9 + 4);  // per-vertex normals x3, temp scratch float
  }

  if (!c) return ARX_UNEXPECTED_EOF;
  return ARX_OK;
}

static ArxReturnCode readGroups(ftl::Data* d, ReadCursor& c, std::int32_t n) {
  if (!tryResize(d->groups, n)) return ARX_BAD_ALLOC;
  std::vector<std::size_t> index_counts;
  if (!tryResize(index_counts, static_cast<std::size_t>(n))) return ARX_BAD_ALLOC;

  std::size_t total_indices = 0;
  for (std::size_t index = 0; index < d->groups.size(); ++index) {
    ftl::Group& group = d->groups[index];
    c.read(group.name);
    canonicalizeFixedString(group.name, "FTL: group.name", static_cast<int>(index));
    c.read(group.origin);
    std::int32_t num_indices = 0;
    c.read(num_indices);
    if (!c) return ARX_UNEXPECTED_EOF;
    if (num_indices < 0 || static_cast<std::size_t>(num_indices) > d->vertices.size()) return ARX_FTL_BAD_GROUP_IDX_N;
    const std::size_t count = static_cast<std::size_t>(num_indices);
    if (count > std::numeric_limits<std::size_t>::max() - total_indices) return ARX_FTL_BAD_GROUP_IDX_N;
    total_indices += count;
    index_counts[index] = count;
    c.skip(4);  // reserved
    c.read(group.blob_shadow_size);
  }

  if (total_indices > c.remaining() / sizeof(std::int32_t)) return ARX_UNEXPECTED_EOF;
  for (std::size_t index = 0; index < d->groups.size(); ++index) {
    ftl::Group& group = d->groups[index];
    if (!tryResize(group.indices, index_counts[index])) return ARX_BAD_ALLOC;
    c.readArray(group.indices);
  }

  if (!c) return ARX_UNEXPECTED_EOF;
  return ARX_OK;
}

static ArxReturnCode readSelections(ftl::Data* d, ReadCursor& c, std::int32_t n) {
  if (!tryResize(d->selections, n)) return ARX_BAD_ALLOC;
  std::vector<std::size_t> selected_counts;
  if (!tryResize(selected_counts, static_cast<std::size_t>(n))) return ARX_BAD_ALLOC;

  std::size_t total_selected = 0;
  for (std::size_t index = 0; index < d->selections.size(); ++index) {
    ftl::Selection& selection = d->selections[index];
    c.read(selection.name);
    canonicalizeFixedString(selection.name, "FTL: selection.name", static_cast<int>(index));
    std::int32_t num_selected = 0;
    c.read(num_selected);
    if (!c) return ARX_UNEXPECTED_EOF;
    if (num_selected <= 0 || static_cast<std::size_t>(num_selected) > d->vertices.size()) return ARX_FTL_BAD_SEL_IDX_N;
    const std::size_t count = static_cast<std::size_t>(num_selected);
    if (count > std::numeric_limits<std::size_t>::max() - total_selected) return ARX_FTL_BAD_SEL_IDX_N;
    total_selected += count;
    selected_counts[index] = count;
    c.skip(4);  // reserved
  }

  if (total_selected > c.remaining() / sizeof(std::int32_t)) return ARX_UNEXPECTED_EOF;
  for (std::size_t index = 0; index < d->selections.size(); ++index) {
    ftl::Selection& selection = d->selections[index];
    if (!tryResize(selection.selected, selected_counts[index])) return ARX_BAD_ALLOC;
    c.readArray(selection.selected);
  }
  if (!c) return ARX_UNEXPECTED_EOF;
  return ARX_OK;
}

ArxReturnCode loadFtl(ftl::Data* d, ReadCursor& c) {
  if (!d) return ARX_INVALID_DATA_POINTER;

  ftl::Data result;
  char identifier[4] = {};
  c.read(identifier);
  if (!c) return ARX_UNEXPECTED_EOF;
  if (std::memcmp(identifier, kFtlMagic, 4) != 0) return ARX_INVALID_IDENTIFIER;

  uint32_t version = 0;
  c.read(version);
  if (!c) return ARX_UNEXPECTED_EOF;
  if (version != kFtlVersion) return ARX_FTL_BAD_VERSION;

  c.skip(512);  // checksum, never validated
  std::int32_t offset_3d_data = 0;
  c.read(offset_3d_data);
  c.skip(20);  // 5 secondary header offsets, always -1
  if (!c) return ARX_UNEXPECTED_EOF;
  if (offset_3d_data < 544) return ARX_FTL_BAD_OFFSET;  // 8 + 512 + 24
  auto gap = static_cast<std::size_t>(offset_3d_data) - 544;
  if (gap > c.remaining()) return ARX_FTL_BAD_OFFSET;
  c.skip(gap);

  std::int32_t num_vertices = 0;
  std::int32_t num_faces = 0;
  std::int32_t num_textures = 0;
  std::int32_t num_groups = 0;
  std::int32_t num_actions = 0;
  std::int32_t num_selections = 0;
  c.read(num_vertices);
  c.read(num_faces);
  c.read(num_textures);
  c.read(num_groups);
  c.read(num_actions);
  c.read(num_selections);
  if (!c) return ARX_UNEXPECTED_EOF;
  if (num_vertices <= 0 || static_cast<std::size_t>(num_vertices) > kFtlMaxVertices) return ARX_FTL_BAD_VERT_N;
  if (num_faces < 0 || static_cast<std::size_t>(num_faces) > kFtlMaxFaces) return ARX_FTL_BAD_FACE_N;
  if (num_textures < 0 || static_cast<std::size_t>(num_textures) > kFtlMaxTextures) return ARX_FTL_BAD_TEX_N;
  if (num_groups < 0 || static_cast<std::size_t>(num_groups) > kFtlMaxGroups) return ARX_FTL_BAD_GROUP_N;
  if (num_actions < 0 || static_cast<std::size_t>(num_actions) > kFtlMaxActions) return ARX_FTL_BAD_ACTION_N;
  if (num_selections < 0 || static_cast<std::size_t>(num_selections) > kFtlMaxSelections) return ARX_FTL_BAD_SEL_N;

  c.read(result.header);
  if (!c) return ARX_UNEXPECTED_EOF;
  canonicalizeFixedString(result.header.name, "FTL: header.name");

  if (!tryResize(result.texture_containers, num_textures)) return ARX_BAD_ALLOC;
  if (!tryResize(result.actions, num_actions)) return ARX_BAD_ALLOC;

  ARX_RETURN_IF_ERR(readVertices(&result, c, num_vertices), c);
  ARX_RETURN_IF_ERR(readFaces(&result, c, num_faces), c);
  ARX_RETURN_IF_ERR(c.readArray(result.texture_containers));
  for (std::size_t i = 0; i < result.texture_containers.size(); ++i)
    canonicalizeFixedString(result.texture_containers[i].filename, "FTL: texture.filename", static_cast<int>(i));
  ARX_RETURN_IF_ERR(readGroups(&result, c, num_groups), c);
  ARX_RETURN_IF_ERR(c.readArray(result.actions));
  for (std::size_t i = 0; i < result.actions.size(); ++i)
    canonicalizeFixedString(result.actions[i].name, "FTL: action.name", static_cast<int>(i));
  ARX_RETURN_IF_ERR(readSelections(&result, c, num_selections), c);

  ARX_RETURN_IF_ERR(canonicalizeFtl(&result));
  ARX_RETURN_IF_ERR(validateFtl(&result));

  *d = std::move(result);

  log(ARX_LOG_INFO,
      "FTL loaded: {} vertices, {} faces, {} textures, {} groups, {} actions, {} selections",
      d->vertices.size(),
      d->faces.size(),
      d->texture_containers.size(),
      d->groups.size(),
      d->actions.size(),
      d->selections.size());

  return ARX_OK;
}

static WriteCursor& writeVertices(const ftl::Data* d, WriteCursor& c) {
  for (const auto& vertex : d->vertices) {
    c.pad(32);
    c.write(vertex);
  }
  return c;
}

static WriteCursor& writeFaces(const ftl::Data* d, WriteCursor& c) {
  for (const auto& face : d->faces) {
    c.write(face.type);
    c.pad(4 * 3);  // rgb
    c.write(face.vertex_idx);
    c.write(face.texture_id);
    c.write(face.u);
    c.write(face.v);
    c.pad(2 * 6);  // ou, ov
    c.write(face.transval);
    c.write(face.norm);
    c.pad(4 * 9 + 4);  // per-vertex normals x3, temp scratch float
  }
  return c;
}

static WriteCursor& writeGroups(const ftl::Data* d, WriteCursor& c) {
  for (const auto& group : d->groups) {
    c.write(group.name);
    c.write(group.origin);
    c.write(static_cast<std::int32_t>(group.indices.size()));
    c.pad(4);  // reserved
    c.write(group.blob_shadow_size);
  }
  for (const auto& group : d->groups) {
    c.writeArray(group.indices);
  }
  return c;
}

static WriteCursor& writeSelections(const ftl::Data* d, WriteCursor& c) {
  for (const auto& selection : d->selections) {
    c.write(selection.name);
    c.write(static_cast<std::int32_t>(selection.selected.size()));
    c.pad(4);  // reserved
  }
  for (const auto& selection : d->selections) {
    c.writeArray(selection.selected);
  }
  return c;
}

ArxReturnCode saveFtl(const ftl::Data* d, WriteCursor& c) {
  ARX_RETURN_IF_ERR(validateFtl(d));

  for (std::size_t index = 0; index < d->texture_containers.size(); ++index) {
    const std::string_view path = d->texture_containers[index].filename;
    if (!path.empty() && !resolvesThroughDefaultLooseRoot({}, path)) {
      log(ARX_LOG_WARN,
          "FTL saving: texture[{}] path '{}' resolves outside Libertatis default loose roots; it may not be discovered",
          index,
          native_text::diagnostic(path));
    }
  }

  log(ARX_LOG_INFO,
      "FTL saving: {} vertices, {} faces, {} textures, {} groups, {} actions, {} selections",
      d->vertices.size(),
      d->faces.size(),
      d->texture_containers.size(),
      d->groups.size(),
      d->actions.size(),
      d->selections.size());

  c.write(kFtlMagic);
  c.write(kFtlVersion);
  c.pad(512);                               // checksum
  c.write(static_cast<std::int32_t>(544));  // offset_3d_data
  for (int i = 0; i < 5; ++i) c.write(static_cast<std::int32_t>(-1));

  c.write(static_cast<std::int32_t>(d->vertices.size()));
  c.write(static_cast<std::int32_t>(d->faces.size()));
  c.write(static_cast<std::int32_t>(d->texture_containers.size()));
  c.write(static_cast<std::int32_t>(d->groups.size()));
  c.write(static_cast<std::int32_t>(d->actions.size()));
  c.write(static_cast<std::int32_t>(d->selections.size()));
  c.write(d->header);

  writeVertices(d, c);
  writeFaces(d, c);
  for (const ftl::TextureContainer& texture : d->texture_containers) {
    ftl::TextureContainer encoded = texture;
    if (!writeStem(encoded.filename, texture.filename)) return ARX_FTL_BAD_TEXTURE_PATH;
    c.write(encoded);
  }
  writeGroups(d, c);
  c.writeArray(d->actions);
  writeSelections(d, c);

  return c ? ARX_OK : ARX_BAD_ALLOC;
}

ArxReturnCode canonicalizeFtl(ftl::Data* d) {
  if (!d) return ARX_INVALID_DATA_POINTER;
  if (!canonicalizeFixedPath(d->header.name, false)) return ARX_FTL_BAD_SOURCE_PATH;
  for (ftl::TextureContainer& texture : d->texture_containers)
    if (!canonicalizeFixedPath(texture.filename, true)) return ARX_FTL_BAD_TEXTURE_PATH;
  for (ftl::Group& group : d->groups) lowercaseFixed(group.name);
  for (ftl::Action& action : d->actions) lowercaseFixed(action.name);
  for (ftl::Selection& selection : d->selections) lowercaseFixed(selection.name);
  return ARX_OK;
}

static ArxReturnCode validateFtlData(const ftl::Data* d) {
  if (!d) return ARX_INVALID_DATA_POINTER;

  auto nv = d->vertices.size();
  auto nt = d->texture_containers.size();

  if (nv == 0 || nv > kFtlMaxVertices) return ARX_FTL_BAD_VERT_N;
  if (nt > kFtlMaxTextures) return ARX_FTL_BAD_TEX_N;
  if (d->faces.size() > kFtlMaxFaces) return ARX_FTL_BAD_FACE_N;
  if (d->groups.size() > kFtlMaxGroups) return ARX_FTL_BAD_GROUP_N;
  if (d->actions.size() > kFtlMaxActions) return ARX_FTL_BAD_ACTION_N;
  if (d->selections.size() > kFtlMaxSelections) return ARX_FTL_BAD_SEL_N;
  if (!isCanonicalFixedPath(d->header.name, false)) return ARX_FTL_BAD_SOURCE_PATH;
  for (const ftl::TextureContainer& texture : d->texture_containers)
    if (!isCanonicalFixedPath(texture.filename, true)) return ARX_FTL_BAD_TEXTURE_PATH;

  if (static_cast<std::size_t>(d->header.origin) >= nv) return ARX_FTL_BAD_ORIGIN;

  for (const auto& face : d->faces) {
    if ((face.type & kFaceBitsAll) != face.type) return ARX_FTL_BAD_FACE_TYPE;
    if (face.vertex_idx.x >= nv || face.vertex_idx.y >= nv || face.vertex_idx.z >= nv) return ARX_FTL_BAD_FACE_VERT_IDX;
    if (face.texture_id < kFtlTextureNone) return ARX_FTL_BAD_FACE_TEX;
    if (face.texture_id != kFtlTextureNone && static_cast<std::size_t>(face.texture_id) >= nt)
      return ARX_FTL_BAD_FACE_TEX;
  }

  for (const auto& group : d->groups) {
    if (!isLowercaseFixed(group.name)) return ARX_FTL_BAD_GROUP_NAME;
    if (group.indices.size() > nv) return ARX_FTL_BAD_GROUP_IDX_N;
    if (static_cast<std::size_t>(group.origin) >= nv) return ARX_FTL_BAD_GROUP_ORIGIN;
    for (auto idx : group.indices) {
      if (idx < 0 || static_cast<std::size_t>(idx) >= nv) return ARX_FTL_BAD_GROUP_IDX;
    }
  }

  for (const auto& action : d->actions) {
    if (!isLowercaseFixed(action.name)) return ARX_FTL_BAD_ACTION_NAME;
    if (action.vertex_idx < 0 || static_cast<std::size_t>(action.vertex_idx) >= nv) return ARX_FTL_BAD_ACTION_VERT_IDX;
  }

  for (const auto& selection : d->selections) {
    if (!isLowercaseFixed(selection.name)) return ARX_FTL_BAD_SELECTION_NAME;
    if (selection.selected.empty() || selection.selected.size() > nv) return ARX_FTL_BAD_SEL_IDX_N;
    for (auto idx : selection.selected) {
      if (idx < 0 || static_cast<std::size_t>(idx) >= nv) return ARX_FTL_BAD_SEL_IDX;
    }
  }

  return ARX_OK;
}

ArxReturnCode validateFtl(const ftl::Data* d) { return validateFtlData(d); }

}  // namespace pistoris
