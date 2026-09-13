// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#pragma once

#include "arx_pistoris/base/indices.h"
#include "arx_pistoris/model.h"
#include "arx_pistoris/model.hpp"
#include "arx_pistoris/model/obj.hpp"
#include "arx_pistoris/model/types.h"

#include "api/c/internal.h"
#include "api/c/texture/internal.h"
#include "modules/skeleton.h"

#include <string>
#include <vector>

struct arx_pistoris_model {
  pistoris::Model value;
};

struct arx_pistoris_obj_material_library_paths {
  std::vector<std::string> value;
};

struct arx_pistoris_obj_texture_files {
  std::vector<pistoris::ObjTextureFile> value;
};

namespace pistoris::c_api {

inline bool valid(const ArxModelBone& value) noexcept { return valid(value.name); }

inline bool valid(const ArxModelActionPoint& value) noexcept { return valid(value.name); }

inline bool valid(const ArxModelSelection& value) noexcept { return valid(value.name); }

inline bool valid(const ArxModelSelectionMembersInput& value) noexcept {
  return valid(value.vertices, value.vertex_count) && valid(value.bones, value.bone_count) &&
         valid(value.action_points, value.action_point_count);
}

inline bool validModelMeshCounts(const ArxModelMeshInput& value) noexcept {
  return value.vertex_count <= static_cast<std::size_t>(kInvalidVertexIndex) &&
         value.face_count <= static_cast<std::size_t>(kInvalidFaceIndex) &&
         value.texture_count <= static_cast<std::size_t>(kNoTexture);
}

inline bool validModelSkeletonCount(const ArxModelSkeletonInput& value) noexcept {
  return value.bone_count <= skeleton::kMaxBones;
}

inline bool validModelActionPointCount(const ArxModelActionPointsInput& value) noexcept {
  return value.action_point_count <= static_cast<std::size_t>(kInvalidActionPointIndex);
}

inline bool valid(const ArxModelMeshInput& value) noexcept {
  if (!valid(value.vertices, value.vertex_count) || !valid(value.faces, value.face_count) ||
      !valid(value.textures, value.texture_count))
    return false;
  for (std::size_t index = 0; index < value.texture_count; ++index) {
    if (!valid(value.textures[index])) return false;
  }
  return true;
}

inline bool valid(const ArxModelSkeletonInput& value) noexcept {
  if (!valid(value.bones, value.bone_count)) return false;
  for (std::size_t index = 0; index < value.bone_count; ++index) {
    if (!valid(value.bones[index])) return false;
  }
  return true;
}

inline bool valid(const ArxModelActionPointsInput& value) noexcept {
  if (!valid(value.action_points, value.action_point_count)) return false;
  for (std::size_t index = 0; index < value.action_point_count; ++index) {
    if (!valid(value.action_points[index])) return false;
  }
  return true;
}

}  // namespace pistoris::c_api
