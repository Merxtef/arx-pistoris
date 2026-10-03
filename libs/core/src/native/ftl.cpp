// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#include "native/ftl.h"

#include "arx_pistoris/base/flags.h"
#include "arx_pistoris/base/location.hpp"
#include "arx_pistoris/base/status.h"
#include "arx_pistoris/native/ftl.hpp"
#include "arx_pistoris/native/location.hpp"
#include "arx_pistoris/runtime/types.h"

#include "api/result_failure.h"
#include "native/binary_location.h"
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

  for (std::size_t index = 0; index < d->vertices.size(); ++index) {
    auto& vertex = d->vertices[index];
    c.locate(FtlElement::kVertex, "vertex", index);
    c.skip(32);  // legacy unused prefix
    c.read(vertex);
  }

  if (!c) return ARX_UNEXPECTED_EOF;
  return ARX_OK;
}

static ArxReturnCode readFaces(ftl::Data* d, ReadCursor& c, std::int32_t n) {
  if (!tryResize(d->faces, n)) return ARX_BAD_ALLOC;

  for (std::size_t index = 0; index < d->faces.size(); ++index) {
    auto& face = d->faces[index];
    c.locate(FtlElement::kFace, "face", index);
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
    c.locate(FtlElement::kBone, "group", index);
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

  if (total_indices > c.remaining() / sizeof(std::int32_t)) {
    c.locate(FtlElement::kBone, "group_indices").skip(total_indices * sizeof(std::int32_t));
    return ARX_UNEXPECTED_EOF;
  }
  for (std::size_t index = 0; index < d->groups.size(); ++index) {
    ftl::Group& group = d->groups[index];
    if (!tryResize(group.indices, index_counts[index])) return ARX_BAD_ALLOC;
    c.locate(FtlElement::kBone, "vertex_indices", index);
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
    c.locate(FtlElement::kSelection, "selection", index);
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

  if (total_selected > c.remaining() / sizeof(std::int32_t)) {
    c.locate(FtlElement::kSelection, "selected_vertices").skip(total_selected * sizeof(std::int32_t));
    return ARX_UNEXPECTED_EOF;
  }
  for (std::size_t index = 0; index < d->selections.size(); ++index) {
    ftl::Selection& selection = d->selections[index];
    if (!tryResize(selection.selected, selected_counts[index])) return ARX_BAD_ALLOC;
    c.locate(FtlElement::kSelection, "selected_vertices", index);
    c.readArray(selection.selected);
  }
  if (!c) return ARX_UNEXPECTED_EOF;
  return ARX_OK;
}

FtlBinaryResult<ftl::Data> loadFtl(ReadCursor& c, NativeBinaryRegion region) {
  auto fail = [&](ArxReturnCode code, const CursorLocation& location) {
    return api_detail::ftlBinaryFailure<ftl::Data>(code, native_binary::location<FtlElement>(location, region));
  };
  auto fail_cursor = [&](ArxReturnCode code) {
    return api_detail::ftlBinaryFailure<ftl::Data>(code, native_binary::location<FtlElement>(c, region));
  };

  ftl::Data result;
  char identifier[4] = {};
  const CursorLocation identifier_location = c.mark(FtlElement::kHeader, "identifier");
  c.read(identifier);
  if (!c) return fail_cursor(ARX_UNEXPECTED_EOF);
  if (std::memcmp(identifier, kFtlMagic, 4) != 0) return fail(ARX_INVALID_IDENTIFIER, identifier_location);

  uint32_t version = 0;
  const CursorLocation version_location = c.mark(FtlElement::kHeader, "version");
  c.read(version);
  if (!c) return fail_cursor(ARX_UNEXPECTED_EOF);
  if (version != kFtlVersion) return fail(ARX_FTL_BAD_VERSION, version_location);

  c.locate(FtlElement::kHeader, "checksum").skip(512);  // never validated
  std::int32_t offset_3d_data = 0;
  const CursorLocation data_offset_location = c.mark(FtlElement::kHeader, "offset_3d_data");
  c.read(offset_3d_data);
  c.locate(FtlElement::kHeader, "secondary_offsets");
  c.skip(20);  // 5 secondary header offsets, always -1
  if (!c) return fail_cursor(ARX_UNEXPECTED_EOF);
  if (offset_3d_data < 544) return fail(ARX_FTL_BAD_OFFSET, data_offset_location);  // 8 + 512 + 24
  auto gap = static_cast<std::size_t>(offset_3d_data) - 544;
  if (gap > c.remaining()) return fail(ARX_FTL_BAD_OFFSET, data_offset_location);
  c.locate(FtlElement::kHeader, "data_gap").skip(gap);

  std::int32_t num_vertices = 0;
  std::int32_t num_faces = 0;
  std::int32_t num_textures = 0;
  std::int32_t num_groups = 0;
  std::int32_t num_actions = 0;
  std::int32_t num_selections = 0;
  const CursorLocation vertex_count_location = c.mark(FtlElement::kHeader, "vertex_count");
  c.read(num_vertices);
  const CursorLocation face_count_location = c.mark(FtlElement::kHeader, "face_count");
  c.read(num_faces);
  const CursorLocation texture_count_location = c.mark(FtlElement::kHeader, "texture_count");
  c.read(num_textures);
  const CursorLocation group_count_location = c.mark(FtlElement::kHeader, "group_count");
  c.read(num_groups);
  const CursorLocation action_count_location = c.mark(FtlElement::kHeader, "action_point_count");
  c.read(num_actions);
  const CursorLocation selection_count_location = c.mark(FtlElement::kHeader, "selection_count");
  c.read(num_selections);
  if (!c) return fail_cursor(ARX_UNEXPECTED_EOF);
  if (num_vertices <= 0 || static_cast<std::size_t>(num_vertices) > kFtlMaxVertices)
    return fail(ARX_FTL_BAD_VERT_N, vertex_count_location);
  if (num_faces < 0 || static_cast<std::size_t>(num_faces) > kFtlMaxFaces)
    return fail(ARX_FTL_BAD_FACE_N, face_count_location);
  if (num_textures < 0 || static_cast<std::size_t>(num_textures) > kFtlMaxTextures)
    return fail(ARX_FTL_BAD_TEX_N, texture_count_location);
  if (num_groups < 0 || static_cast<std::size_t>(num_groups) > kFtlMaxGroups)
    return fail(ARX_FTL_BAD_GROUP_N, group_count_location);
  if (num_actions < 0 || static_cast<std::size_t>(num_actions) > kFtlMaxActions)
    return fail(ARX_FTL_BAD_ACTION_N, action_count_location);
  if (num_selections < 0 || static_cast<std::size_t>(num_selections) > kFtlMaxSelections)
    return fail(ARX_FTL_BAD_SEL_N, selection_count_location);

  c.locate(FtlElement::kHeader, "model_header").read(result.header);
  if (!c) return fail_cursor(ARX_UNEXPECTED_EOF);
  canonicalizeFixedString(result.header.name, "FTL: header.name");

  if (!tryResize(result.texture_containers, num_textures)) return fail(ARX_BAD_ALLOC, texture_count_location);
  if (!tryResize(result.actions, num_actions)) return fail(ARX_BAD_ALLOC, action_count_location);

  if (const ArxReturnCode code = readVertices(&result, c, num_vertices); code != ARX_OK) return fail_cursor(code);
  if (const ArxReturnCode code = readFaces(&result, c, num_faces); code != ARX_OK) return fail_cursor(code);
  c.locate(FtlElement::kTexture, "textures");
  c.readArray(result.texture_containers);
  if (!c) return fail_cursor(ARX_UNEXPECTED_EOF);
  for (std::size_t i = 0; i < result.texture_containers.size(); ++i)
    canonicalizeFixedString(result.texture_containers[i].filename, "FTL: texture.filename", static_cast<int>(i));
  if (const ArxReturnCode code = readGroups(&result, c, num_groups); code != ARX_OK) return fail_cursor(code);
  c.locate(FtlElement::kActionPoint, "action_points");
  c.readArray(result.actions);
  if (!c) return fail_cursor(ARX_UNEXPECTED_EOF);
  for (std::size_t i = 0; i < result.actions.size(); ++i)
    canonicalizeFixedString(result.actions[i].name, "FTL: action.name", static_cast<int>(i));
  if (const ArxReturnCode code = readSelections(&result, c, num_selections); code != ARX_OK) return fail_cursor(code);

  FtlLocation semantic_location;
  if (const ArxReturnCode code = canonicalizeFtl(&result, &semantic_location); code != ARX_OK)
    return api_detail::ftlBinaryFailure<ftl::Data>(code, native_binary::semanticLocation(semantic_location, region));
  FtlLocation validation_location;
  if (const ArxReturnCode code = validateFtl(&result, &validation_location); code != ARX_OK) {
    return api_detail::ftlBinaryFailure<ftl::Data>(code, native_binary::semanticLocation(validation_location, region));
  }

  log(ARX_LOG_INFO,
      "FTL loaded: {} vertices, {} faces, {} textures, {} groups, {} actions, {} selections",
      result.vertices.size(),
      result.faces.size(),
      result.texture_containers.size(),
      result.groups.size(),
      result.actions.size(),
      result.selections.size());

  return FtlBinaryResult<ftl::Data>::success(std::move(result));
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

ArxReturnCode canonicalizeFtl(ftl::Data* d, FtlLocation* failure_location) {
  auto fail = [failure_location](ArxReturnCode code,
                                 FtlElement element = FtlElement::kHeader,
                                 std::size_t index = kNoElementIndex,
                                 std::string field = {}) {
    if (failure_location) *failure_location = {.element = element, .index = index, .field = std::move(field)};
    return code;
  };
  if (!d) return fail(ARX_INVALID_DATA_POINTER);
  if (!canonicalizeFixedPath(d->header.name, false))
    return fail(ARX_FTL_BAD_SOURCE_PATH, FtlElement::kHeader, kNoElementIndex, "name");
  for (std::size_t index = 0; index < d->texture_containers.size(); ++index) {
    if (!canonicalizeFixedPath(d->texture_containers[index].filename, true))
      return fail(ARX_FTL_BAD_TEXTURE_PATH, FtlElement::kTexture, index, "filename");
  }
  for (ftl::Group& group : d->groups) lowercaseFixed(group.name);
  for (ftl::Action& action : d->actions) lowercaseFixed(action.name);
  for (ftl::Selection& selection : d->selections) lowercaseFixed(selection.name);
  return ARX_OK;
}

static ArxReturnCode validateFtlData(const ftl::Data* d, FtlLocation* failure_location) {
  auto fail = [failure_location](ArxReturnCode code,
                                 FtlElement element = FtlElement::kHeader,
                                 std::size_t index = kNoElementIndex,
                                 std::size_t subindex = kNoElementIndex,
                                 std::string field = {}) {
    if (failure_location)
      *failure_location = {.element = element, .index = index, .subindex = subindex, .field = std::move(field)};
    return code;
  };
  if (!d) return fail(ARX_INVALID_DATA_POINTER);

  auto nv = d->vertices.size();
  auto nt = d->texture_containers.size();

  if (nv == 0 || nv > kFtlMaxVertices)
    return fail(ARX_FTL_BAD_VERT_N, FtlElement::kHeader, kNoElementIndex, kNoElementIndex, "vertices");
  if (nt > kFtlMaxTextures)
    return fail(ARX_FTL_BAD_TEX_N, FtlElement::kHeader, kNoElementIndex, kNoElementIndex, "texture_containers");
  if (d->faces.size() > kFtlMaxFaces)
    return fail(ARX_FTL_BAD_FACE_N, FtlElement::kHeader, kNoElementIndex, kNoElementIndex, "faces");
  if (d->groups.size() > kFtlMaxGroups)
    return fail(ARX_FTL_BAD_GROUP_N, FtlElement::kHeader, kNoElementIndex, kNoElementIndex, "groups");
  if (d->actions.size() > kFtlMaxActions)
    return fail(ARX_FTL_BAD_ACTION_N, FtlElement::kHeader, kNoElementIndex, kNoElementIndex, "actions");
  if (d->selections.size() > kFtlMaxSelections)
    return fail(ARX_FTL_BAD_SEL_N, FtlElement::kHeader, kNoElementIndex, kNoElementIndex, "selections");
  if (!isCanonicalFixedPath(d->header.name, false))
    return fail(ARX_FTL_BAD_SOURCE_PATH, FtlElement::kHeader, kNoElementIndex, kNoElementIndex, "name");
  for (std::size_t index = 0; index < d->texture_containers.size(); ++index)
    if (!isCanonicalFixedPath(d->texture_containers[index].filename, true))
      return fail(ARX_FTL_BAD_TEXTURE_PATH, FtlElement::kTexture, index, kNoElementIndex, "filename");

  if (static_cast<std::size_t>(d->header.origin) >= nv)
    return fail(ARX_FTL_BAD_ORIGIN, FtlElement::kHeader, kNoElementIndex, kNoElementIndex, "origin");

  for (std::size_t index = 0; index < d->faces.size(); ++index) {
    const ftl::Face& face = d->faces[index];
    if ((face.type & kFaceBitsAll) != face.type)
      return fail(ARX_FTL_BAD_FACE_TYPE, FtlElement::kFace, index, kNoElementIndex, "type");
    if (face.vertex_idx.x >= nv) return fail(ARX_FTL_BAD_FACE_VERT_IDX, FtlElement::kFace, index, 0, "vertex_idx");
    if (face.vertex_idx.y >= nv) return fail(ARX_FTL_BAD_FACE_VERT_IDX, FtlElement::kFace, index, 1, "vertex_idx");
    if (face.vertex_idx.z >= nv) return fail(ARX_FTL_BAD_FACE_VERT_IDX, FtlElement::kFace, index, 2, "vertex_idx");
    if (face.texture_id < kFtlTextureNone)
      return fail(ARX_FTL_BAD_FACE_TEX, FtlElement::kFace, index, kNoElementIndex, "texture_id");
    if (face.texture_id != kFtlTextureNone && static_cast<std::size_t>(face.texture_id) >= nt)
      return fail(ARX_FTL_BAD_FACE_TEX, FtlElement::kFace, index, kNoElementIndex, "texture_id");
  }

  for (std::size_t index = 0; index < d->groups.size(); ++index) {
    const ftl::Group& group = d->groups[index];
    if (!isLowercaseFixed(group.name))
      return fail(ARX_FTL_BAD_GROUP_NAME, FtlElement::kBone, index, kNoElementIndex, "name");
    if (group.indices.size() > nv)
      return fail(ARX_FTL_BAD_GROUP_IDX_N, FtlElement::kBone, index, kNoElementIndex, "indices");
    if (static_cast<std::size_t>(group.origin) >= nv)
      return fail(ARX_FTL_BAD_GROUP_ORIGIN, FtlElement::kBone, index, kNoElementIndex, "origin");
    for (std::size_t member = 0; member < group.indices.size(); ++member) {
      const std::int32_t vertex = group.indices[member];
      if (vertex < 0 || static_cast<std::size_t>(vertex) >= nv)
        return fail(ARX_FTL_BAD_GROUP_IDX, FtlElement::kBone, index, member, "indices");
    }
  }

  for (std::size_t index = 0; index < d->actions.size(); ++index) {
    const ftl::Action& action = d->actions[index];
    if (!isLowercaseFixed(action.name))
      return fail(ARX_FTL_BAD_ACTION_NAME, FtlElement::kActionPoint, index, kNoElementIndex, "name");
    if (action.vertex_idx < 0 || static_cast<std::size_t>(action.vertex_idx) >= nv)
      return fail(ARX_FTL_BAD_ACTION_VERT_IDX, FtlElement::kActionPoint, index, kNoElementIndex, "vertex_idx");
  }

  for (std::size_t index = 0; index < d->selections.size(); ++index) {
    const ftl::Selection& selection = d->selections[index];
    if (!isLowercaseFixed(selection.name))
      return fail(ARX_FTL_BAD_SELECTION_NAME, FtlElement::kSelection, index, kNoElementIndex, "name");
    if (selection.selected.empty() || selection.selected.size() > nv)
      return fail(ARX_FTL_BAD_SEL_IDX_N, FtlElement::kSelection, index, kNoElementIndex, "selected");
    for (std::size_t member = 0; member < selection.selected.size(); ++member) {
      const std::int32_t vertex = selection.selected[member];
      if (vertex < 0 || static_cast<std::size_t>(vertex) >= nv)
        return fail(ARX_FTL_BAD_SEL_IDX, FtlElement::kSelection, index, member, "selected");
    }
  }

  return ARX_OK;
}

ArxReturnCode validateFtl(const ftl::Data* d, FtlLocation* failure_location) {
  return validateFtlData(d, failure_location);
}

}  // namespace pistoris
