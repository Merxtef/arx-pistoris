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

}  // namespace pistoris::c_api
