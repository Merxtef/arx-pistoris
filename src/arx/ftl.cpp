// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#include "arx/ftl.h"

#include "arx_pistoris/common_data.hpp"
#include "arx_pistoris/ftl_data.hpp"
#include "arx_pistoris/pistoris_types.h"

#include "utils/cursor.h"
#include "utils/log.h"
#include "utils/mem_utils.h"
#include "utils/parse_utils.h"

#include <cstdint>
#include <cstring>
#include <format>

namespace pistoris {

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

  for (auto& group : d->groups) {
    c.read(group.name);
    clampStr(group.name, "FTL: group.name", static_cast<int>(&group - d->groups.data()));
    c.read(group.origin);
    std::int32_t num_indices = 0;
    c.read(num_indices);
    if (!c) return ARX_UNEXPECTED_EOF;
    if (num_indices < 0 || static_cast<std::size_t>(num_indices) > d->vertices.size()) return ARX_FTL_BAD_GROUP_IDX_N;
    if (!tryResize(group.indices, num_indices)) return ARX_BAD_ALLOC;
    c.skip(4);  // reserved
    c.read(group.blob_shadow_size);
  }

  for (auto& group : d->groups) {
    c.readArray(group.indices);
  }

  if (!c) return ARX_UNEXPECTED_EOF;
  return ARX_OK;
}

static ArxReturnCode readSelections(ftl::Data* d, ReadCursor& c, std::int32_t n) {
  if (!tryResize(d->selections, n)) return ARX_BAD_ALLOC;

  for (auto& selection : d->selections) {
    c.read(selection.name);
    clampStr(selection.name, "FTL: selection.name", static_cast<int>(&selection - d->selections.data()));
    std::int32_t num_selected = 0;
    c.read(num_selected);
    if (!c) return ARX_UNEXPECTED_EOF;
    if (num_selected <= 0 || static_cast<std::size_t>(num_selected) > d->vertices.size()) return ARX_FTL_BAD_SEL_IDX_N;
    if (!tryResize(selection.selected, num_selected)) return ARX_BAD_ALLOC;
    c.skip(4);  // reserved
  }

  for (auto& selection : d->selections) {
    c.readArray(selection.selected);
  }
  if (!c) return ARX_UNEXPECTED_EOF;
  return ARX_OK;
}

ArxReturnCode loadFtl(ftl::Data* d, ReadCursor& c) {
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

  std::int32_t num_vertices   = 0;
  std::int32_t num_faces      = 0;
  std::int32_t num_textures   = 0;
  std::int32_t num_groups     = 0;
  std::int32_t num_actions    = 0;
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

  c.read(d->header);
  if (!c) return ARX_UNEXPECTED_EOF;
  clampStr(d->header.name, "FTL: header.name");

  if (!tryResize(d->texture_containers, num_textures)) return ARX_BAD_ALLOC;
  if (!tryResize(d->actions, num_actions)) return ARX_BAD_ALLOC;

  ARX_RETURN_IF_ERR(readVertices(d, c, num_vertices), c);
  ARX_RETURN_IF_ERR(readFaces(d, c, num_faces), c);
  ARX_RETURN_IF_ERR(c.readArray(d->texture_containers));
  for (std::size_t i = 0; i < d->texture_containers.size(); ++i)
    clampStr(d->texture_containers[i].filename, "FTL: texture.filename", static_cast<int>(i));
  ARX_RETURN_IF_ERR(readGroups(d, c, num_groups), c);
  ARX_RETURN_IF_ERR(c.readArray(d->actions));
  for (std::size_t i = 0; i < d->actions.size(); ++i)
    clampStr(d->actions[i].name, "FTL: action.name", static_cast<int>(i));
  ARX_RETURN_IF_ERR(readSelections(d, c, num_selections), c);

  ARX_RETURN_IF_ERR(validateFtl(d));

  log(ARX_LOG_INFO, std::format("FTL loaded: {} vertices, {} faces, {} textures, {} groups, {} actions, {} selections",
                                d->vertices.size(), d->faces.size(), d->texture_containers.size(), d->groups.size(),
                                d->actions.size(), d->selections.size()));

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

  log(ARX_LOG_INFO, std::format("FTL saving: {} vertices, {} faces, {} textures, {} groups, {} actions, {} selections",
                                d->vertices.size(), d->faces.size(), d->texture_containers.size(), d->groups.size(),
                                d->actions.size(), d->selections.size()));

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
  c.writeArray(d->texture_containers);
  writeGroups(d, c);
  c.writeArray(d->actions);
  writeSelections(d, c);

  return c ? ARX_OK : ARX_BAD_ALLOC;
}

static ArxReturnCode validateFtlData(const ftl::Data* d) {
  auto nv = d->vertices.size();
  auto nt = d->texture_containers.size();

  if (nv == 0 || nv > kFtlMaxVertices) return ARX_FTL_BAD_VERT_N;
  if (nt > kFtlMaxTextures) return ARX_FTL_BAD_TEX_N;
  if (d->faces.size() > kFtlMaxFaces) return ARX_FTL_BAD_FACE_N;
  if (d->groups.size() > kFtlMaxGroups) return ARX_FTL_BAD_GROUP_N;
  if (d->actions.size() > kFtlMaxActions) return ARX_FTL_BAD_ACTION_N;
  if (d->selections.size() > kFtlMaxSelections) return ARX_FTL_BAD_SEL_N;

  if (static_cast<std::size_t>(d->header.origin) >= nv) return ARX_FTL_BAD_ORIGIN;

  for (const auto& face : d->faces) {
    if ((face.type & kFaceBitsAll) != face.type) return ARX_FTL_BAD_FACE_TYPE;
    if (face.vertex_idx.x >= nv || face.vertex_idx.y >= nv || face.vertex_idx.z >= nv) return ARX_FTL_BAD_FACE_VERT_IDX;
    if (face.texture_id < kFtlTextureNone) return ARX_FTL_BAD_FACE_TEX;
    if (face.texture_id != kFtlTextureNone && static_cast<std::size_t>(face.texture_id) >= nt)
      return ARX_FTL_BAD_FACE_TEX;
  }

  for (const auto& group : d->groups) {
    if (group.indices.size() > nv) return ARX_FTL_BAD_GROUP_IDX_N;
    if (static_cast<std::size_t>(group.origin) >= nv) return ARX_FTL_BAD_GROUP_ORIGIN;
    for (auto idx : group.indices) {
      if (idx < 0 || static_cast<std::size_t>(idx) >= nv) return ARX_FTL_BAD_GROUP_IDX;
    }
  }

  for (const auto& action : d->actions) {
    if (action.vertex_idx < 0 || static_cast<std::size_t>(action.vertex_idx) >= nv) return ARX_FTL_BAD_ACTION_VERT_IDX;
  }

  for (const auto& selection : d->selections) {
    if (selection.selected.empty() || selection.selected.size() > nv) return ARX_FTL_BAD_SEL_IDX_N;
    for (auto idx : selection.selected) {
      if (idx < 0 || static_cast<std::size_t>(idx) >= nv) return ARX_FTL_BAD_SEL_IDX;
    }
  }

  return ARX_OK;
}

static void buildFtlExtras(const ftl::Data* d) {
  auto& e = d->extras;
  auto ng = static_cast<std::int32_t>(d->groups.size());
  auto nv = d->vertices.size();

  e.vertex_to_bone.assign(nv, -1);
  for (std::int32_t gi = 0; gi < ng; ++gi)
    for (auto vi : d->groups[gi].indices) e.vertex_to_bone[vi] = gi;

  e.bone_world_pos.assign(d->groups.size(), ArxVector3{0.0f, 0.0f, 0.0f});
  for (std::size_t gi = 0; gi < d->groups.size(); ++gi)
    e.bone_world_pos[gi] = d->vertices[d->groups[gi].origin].position;

  e.parent_bone.assign(d->groups.size(), -1);
  auto find_parent = [&](std::uint32_t origin, std::int32_t gi) -> std::int32_t {
    for (std::int32_t pgi = gi - 1; pgi >= 0; --pgi)
      for (auto vi : d->groups[pgi].indices)
        if (static_cast<std::uint32_t>(vi) == origin) return pgi;
    return -1;
  };
  for (std::int32_t gi = 1; gi < ng; ++gi) {
    std::uint32_t origin = d->groups[gi].origin;
    std::int32_t owner   = e.vertex_to_bone[origin];
    if (owner >= 0 && owner < gi)
      e.parent_bone[gi] = owner;
    else
      e.parent_bone[gi] = find_parent(origin, gi);
  }
}

static ArxReturnCode validateFtlExtras(const ftl::Data* d) {
  const auto& parent = d->extras.parent_bone;
  for (std::size_t gi = 1; gi < parent.size(); ++gi)
    if (parent[gi] < 0) return ARX_FTL_ORPHAN_BONE;
  return ARX_OK;
}

ArxReturnCode validateFtl(const ftl::Data* d) {
  ARX_RETURN_IF_ERR(validateFtlData(d));
  buildFtlExtras(d);
  ARX_RETURN_IF_ERR(validateFtlExtras(d));
  return ARX_OK;
}

}  // namespace pistoris
