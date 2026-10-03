// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#pragma once

#include "arx_pistoris/base/flags.h"
#include "arx_pistoris/base/indices.h"
#include "arx_pistoris/base/math.h"
#include "arx_pistoris/base/status.h"
#include "arx_pistoris/level.hpp"
#include "arx_pistoris/texture.hpp"

#include "external/glb/accessor.h"
#include "external/glb/container.h"
#include "external/glb/geometry_material.h"
#include "external/glb/level/api.h"
#include "external/glb/level/coordinates.h"
#include "external/glb/level/discovery.h"
#include "external/glb/level/entities.h"
#include "external/glb/level/fogs.h"
#include "external/glb/level/lighting.h"
#include "external/glb/level/material.h"
#include "external/glb/level/names.h"
#include "external/glb/level/objects.h"
#include "external/glb/level/paths.h"
#include "external/glb/level/player_spawn.h"
#include "external/glb/level/topology.h"
#include "external/glb/level/zones.h"
#include "external/glb/node_graph.h"
#include "external/glb/primitive_indices.h"
#include "external/glb/utils/image.h"
#include "external/glb/utils/texture.h"
#include "level/anchor_bounds.h"
#include "level/data.h"
#include "level/validation.h"
#include "modules/geometry.h"
#include "modules/lights.h"
#include "modules/navigation.h"
#include "modules/rooms.h"
#include "modules/scene.h"
#include "utils/identifier.h"
#include "utils/log.h"
#include "utils/math/bounds.h"
#include "utils/math/mat4.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <map>
#include <optional>
#include <set>
#include <span>
#include <string>
#include <string_view>
#include <tuple>
#include <unordered_map>
#include <utility>
#include <vector>

namespace pistoris::glb_level_import {

using glb::AccessorView;
using glb::Asset;
using glb::getAccessor;
using glb::parse;
using glb::PrimitiveIndices;
using glb::readColor3;
using glb::readVec2;
using glb::readVec3;
using glb::validateColorAccessor;
using glb::validateIndexAccessor;
using glb::validateNormalAccessor;
using glb::validatePositionAccessor;
using glb::validateTexcoordAccessor;
using glb_level::compactLevelVertices;
using glb_level::ImportUnits;
using glb_level::isReservedLightName;
using glb_level::kLevelGlbEpsilon;
using glb_level::parseAnchorNodeName;
using glb_level::ParsedLightName;
using glb_level::ParsedPortalName;
using glb_level::parseLightEffects;
using glb_level::parseLightFlags;
using glb_level::parseLightName;
using glb_level::parseLightSettings;
using glb_level::parsePortalNodeName;
using glb_level::roomNameFromNode;
using GlbVec2 = glb::Vec2;
using GlbVec3 = glb::Vec3;

struct ImportWarnings {
  std::uint64_t regenerated_normals = 0;
  std::uint64_t normalized_normals = 0;
  std::uint64_t discarded_faces = 0;
  std::uint64_t discarded_vertices = 0;
  std::uint64_t skipped_quad_flags = 0;
  std::uint64_t portal_materials = 0;
  std::uint64_t discarded_uv_sets = 0;
  std::uint64_t defaulted_colors = 0;
  std::uint64_t discarded_colors = 0;
  std::uint64_t discarded_tangents = 0;
  std::uint64_t discarded_custom = 0;
  std::uint64_t generated_light_names = 0;
  std::uint64_t normalized_legacy_teo = 0;
  std::uint64_t nav_surface_components = 0;
  bool ignored_skinning = false;
};

struct ImportedMaterial {
  TextureIndex texture = kNoTexture;
  FaceType flags = 0;
  float transval = 0.0f;
  std::int32_t uv_set = 0;
  bool portal_fallback = false;
};

namespace detail {

inline std::string_view nodeName(const cgltf_node& node) noexcept { return node.name != nullptr ? node.name : ""; }

inline void logLevelObjectFailure(std::string_view stage, std::size_t node_index, std::string_view node_name,
                                  ArxReturnCode rc) {
  if (rc < ARX_GLB_BAD_LEVEL_HIERARCHY || rc > ARX_GLB_BAD_LEVEL_LIGHT) return;
  log(ARX_LOG_DEBUG,
      "GLB -> Level object failure: {} at node {} '{}' returned code {}",
      stage,
      node_index,
      node_name,
      rc);
}

inline void logLevelObjectFailure(std::string_view stage, ArxReturnCode rc) {
  if (rc < ARX_GLB_BAD_LEVEL_HIERARCHY || rc > ARX_GLB_BAD_LEVEL_LIGHT) return;
  log(ARX_LOG_DEBUG, "GLB -> Level object failure: {} returned code {}", stage, rc);
}

inline bool finite(const ArxVector3& value) {
  return std::isfinite(value.x) && std::isfinite(value.y) && std::isfinite(value.z);
}

inline bool unitColor(GlbVec3 value) {
  return value.x >= 0.0f && value.x <= 1.0f && value.y >= 0.0f && value.y <= 1.0f && value.z >= 0.0f && value.z <= 1.0f;
}

inline void ensureCornerColors(LevelModules& level) {
  if (!level.lighting.corner_colors.empty()) return;
  level.lighting.corner_colors.resize(level.geometry.faces.size() * 3U, lights::kDefaultCornerColor);
}

inline void appendCornerColors(LevelModules& level, const std::array<ArxColor3, 3>& colors) {
  level.lighting.corner_colors.insert(level.lighting.corner_colors.end(), colors.begin(), colors.end());
}

inline float length(GlbVec3 value) { return std::sqrt(value.x * value.x + value.y * value.y + value.z * value.z); }

inline GlbVec3 sub(GlbVec3 a, GlbVec3 b) { return {a.x - b.x, a.y - b.y, a.z - b.z}; }

inline GlbVec3 cross(GlbVec3 a, GlbVec3 b) {
  return {a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x};
}

inline GlbVec3 normalize(GlbVec3 value) {
  float value_length = length(value);
  if (value_length <= std::numeric_limits<float>::epsilon()) return {};
  return {value.x / value_length, value.y / value_length, value.z / value_length};
}

inline ArxVector3 toArx(GlbVec3 value) { return {value.x, value.y, value.z}; }
inline GlbVec3 toVec3(const ArxVector3& value) { return {value.x, value.y, value.z}; }

inline std::optional<ArxVector3> transformNormal(const math::Mat4& inverse, const ArxVector3& normal) {
  ArxVector3 transformed{
      inverse(0, 0) * normal.x + inverse(1, 0) * normal.y + inverse(2, 0) * normal.z,
      inverse(0, 1) * normal.x + inverse(1, 1) * normal.y + inverse(2, 1) * normal.z,
      inverse(0, 2) * normal.x + inverse(1, 2) * normal.y + inverse(2, 2) * normal.z,
  };
  float transformed_length = length(toVec3(transformed));
  if (!std::isfinite(transformed_length) || transformed_length <= std::numeric_limits<float>::epsilon())
    return std::nullopt;
  return toArx(normalize(toVec3(transformed)));
}

inline bool hasSkinning(const cgltf_node& node, const cgltf_primitive& primitive) {
  return node.skin != nullptr || cgltf_find_accessor(&primitive, cgltf_attribute_type_joints, 0) != nullptr ||
         cgltf_find_accessor(&primitive, cgltf_attribute_type_weights, 0) != nullptr;
}

}  // namespace detail

struct LevelMeshImportContext {
  LevelMeshImportContext(const Asset& asset, std::size_t expected_accessors,
                         std::unordered_map<const cgltf_material*, ImportedMaterial>& materials_value)
      : accessors(asset, expected_accessors), materials(materials_value) {}

  glb::AccessorCache accessors;
  std::unordered_map<glb::AccessorElementKey, std::uint32_t, glb::AccessorElementKeyHash> vertices_by_source;
  std::unordered_map<const cgltf_material*, ImportedMaterial>& materials;
};

ArxReturnCode readGeometryPrimitive(LevelMeshImportContext& context, const cgltf_node& node,
                                    const cgltf_primitive& primitive, const math::Mat4& world, const ImportUnits& units,
                                    std::uint32_t room, LevelModules& level, glb::TextureImporter& texture_importer,
                                    ImportWarnings& diagnostics);

ArxReturnCode importNavSurface(const Asset& asset, const cgltf_node& node, std::size_t node_index,
                               const math::Mat4& world, const ImportUnits& units, NavSurface& out);

std::uint64_t navSurfaceComponents(const NavSurface& surface);

ArxReturnCode importPointLight(const cgltf_node& node, std::size_t node_index, const math::Mat4& world,
                               const ImportUnits& units, std::size_t ordinal, ImportWarnings& diagnostics, Light& out);

ArxReturnCode importReservedLight(const cgltf_node& node, std::size_t node_index, const math::Mat4& world,
                                  const ImportUnits& units, std::size_t ordinal, ImportWarnings& diagnostics,
                                  Light& out);

ArxReturnCode importPortal(const Asset& asset, const cgltf_node& node, const math::Mat4& world,
                           const std::map<std::string, std::uint32_t>& rooms_by_name, ImportWarnings& diagnostics,
                           const ImportUnits& units, Portal& out);

}  // namespace pistoris::glb_level_import
