// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#include "arx_pistoris/arx_math.h"
#include "arx_pistoris/flags.h"
#include "arx_pistoris/indices.h"
#include "arx_pistoris/level.hpp"
#include "arx_pistoris/pistoris_types.h"

#include "../accessor.h"
#include "../container.h"
#include "api.h"
#include "coordinates.h"
#include "discovery.h"
#include "entities.h"
#include "external/glb/node_graph.h"
#include "fogs.h"
#include "level/anchor_bounds.h"
#include "level/data.h"
#include "level/level.h"
#include "lighting.h"
#include "material.h"
#include "modules/geometry.h"
#include "modules/lights.h"
#include "modules/navigation.h"
#include "modules/rooms.h"
#include "modules/scene.h"
#include "names.h"
#include "objects.h"
#include "paths.h"
#include "player_spawn.h"
#include "topology.h"
#include "utils/log.h"
#include "utils/math/bounds.h"
#include "utils/math/mat4.h"
#include "utils/unique_name.h"
#include "zones.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <format>
#include <limits>
#include <map>
#include <optional>
#include <set>
#include <span>
#include <string>
#include <string_view>
#include <tuple>
#include <unordered_set>
#include <utility>
#include <vector>

namespace pistoris {
namespace {

using glb::AccessorView;
using glb::Asset;
using glb::getAccessor;
using glb::parse;
using glb::readColor3;
using glb::readVec2;
using glb::readVec3;
using glb::validateColorAccessor;
using glb::validateIndexAccessor;
using glb::validateNormalAccessor;
using glb::validatePositionAccessor;
using glb::validateTexcoordAccessor;
using glb_level::compactLevelVertices;
using glb_level::ImportImageCache;
using glb_level::ImportMaterial;
using glb_level::ImportUnits;
using glb_level::isReservedLightName;
using glb_level::kLevelGlbEpsilon;
using glb_level::parseAnchorNodeName;
using glb_level::ParsedLightName;
using glb_level::ParsedPortalName;
using glb_level::parseImportMaterial;
using glb_level::parseLightEffects;
using glb_level::parseLightFlags;
using glb_level::parseLightName;
using glb_level::parseLightSettings;
using glb_level::parsePortalNodeName;
using glb_level::roomNameFromNode;
using GlbVec2 = glb::Vec2;
using GlbVec3 = glb::Vec3;

char lowerAscii(char value) {
  if (value >= 'A' && value <= 'Z') return static_cast<char>(value - 'A' + 'a');
  return value;
}

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

std::string nodeName(const cgltf_node& node) { return node.name != nullptr ? node.name : ""; }

void logLevelObjectFailure(std::string_view stage, std::size_t node_index, std::string_view node_name,
                           ArxReturnCode rc) {
  if (rc < ARX_GLB_BAD_LEVEL_HIERARCHY || rc > ARX_GLB_BAD_LEVEL_LIGHT) return;
  log(ARX_LOG_DEBUG, std::format("GLB -> Level object failure: {} at node {} '{}'", stage, node_index, node_name));
}

void logLevelObjectFailure(std::string_view stage, ArxReturnCode rc) {
  if (rc < ARX_GLB_BAD_LEVEL_HIERARCHY || rc > ARX_GLB_BAD_LEVEL_LIGHT) return;
  log(ARX_LOG_DEBUG, std::format("GLB -> Level object failure: {}", stage));
}

bool finite(const ArxVector3& value) {
  return std::isfinite(value.x) && std::isfinite(value.y) && std::isfinite(value.z);
}

bool unitColor(GlbVec3 value) {
  return value.x >= 0.0f && value.x <= 1.0f && value.y >= 0.0f && value.y <= 1.0f && value.z >= 0.0f && value.z <= 1.0f;
}

void ensureCornerColors(LevelModules& level) {
  if (!level.lighting.corner_colors.empty()) return;
  level.lighting.corner_colors.resize(level.geometry.faces.size() * 3U, lights::kDefaultCornerColor);
}

void appendCornerColors(LevelModules& level, const std::array<ArxColor3, 3>& colors) {
  level.lighting.corner_colors.insert(level.lighting.corner_colors.end(), colors.begin(), colors.end());
}

float length(GlbVec3 value) { return std::sqrt(value.x * value.x + value.y * value.y + value.z * value.z); }

GlbVec3 sub(GlbVec3 a, GlbVec3 b) { return {a.x - b.x, a.y - b.y, a.z - b.z}; }

GlbVec3 cross(GlbVec3 a, GlbVec3 b) { return {a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x}; }

GlbVec3 normalize(GlbVec3 value) {
  float value_length = length(value);
  if (value_length <= std::numeric_limits<float>::epsilon()) return {};
  return {value.x / value_length, value.y / value_length, value.z / value_length};
}

ArxVector3 toArx(GlbVec3 value) { return {value.x, value.y, value.z}; }
GlbVec3 toVec3(const ArxVector3& value) { return {value.x, value.y, value.z}; }

double determinant(const math::Mat4& matrix) {
  return static_cast<double>(matrix(0, 0)) *
             (static_cast<double>(matrix(1, 1)) * matrix(2, 2) - static_cast<double>(matrix(1, 2)) * matrix(2, 1)) -
         static_cast<double>(matrix(0, 1)) *
             (static_cast<double>(matrix(1, 0)) * matrix(2, 2) - static_cast<double>(matrix(1, 2)) * matrix(2, 0)) +
         static_cast<double>(matrix(0, 2)) *
             (static_cast<double>(matrix(1, 0)) * matrix(2, 1) - static_cast<double>(matrix(1, 1)) * matrix(2, 0));
}

std::optional<ArxVector3> transformNormal(const math::Mat4& inverse, const ArxVector3& normal) {
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

ArxReturnCode primitiveOrder(const Asset& asset, const cgltf_primitive& primitive, const AccessorView& positions,
                             bool require_indices, std::vector<std::uint32_t>& out) {
  if (primitive.indices == nullptr) {
    if (require_indices) return ARX_GLB_BAD_LEVEL_GEOMETRY;
    if (positions.count % 3 != 0) return ARX_GLB_BAD_LEVEL_GEOMETRY;
    out.resize(positions.count);
    for (std::size_t i = 0; i < positions.count; ++i) out[i] = static_cast<std::uint32_t>(i);
    return ARX_OK;
  }

  AccessorView indices;
  ArxReturnCode rc = getAccessor(asset, primitive.indices, indices);
  if (rc != ARX_OK) return rc;
  rc = validateIndexAccessor(indices);
  if (rc != ARX_OK) return rc;
  out = std::move(indices.indices);
  return ARX_OK;
}

bool hasSkinning(const cgltf_node& node, const cgltf_primitive& primitive) {
  return node.skin != nullptr || cgltf_find_accessor(&primitive, cgltf_attribute_type_joints, 0) != nullptr ||
         cgltf_find_accessor(&primitive, cgltf_attribute_type_weights, 0) != nullptr;
}

struct TextureRegistry {
  std::map<const cgltf_image*, TextureIndex> images;
  std::map<std::string, TextureIndex> external_images;
  std::map<std::string, TextureIndex> material_stems;
};

std::string texturePathKey(std::string_view path, bool remove_extension) {
  std::string key;
  key.reserve(path.size());
  for (char value : path) {
    if (value == '\\') value = '/';
    key.push_back(lowerAscii(value));
  }
  if (remove_extension) {
    const std::size_t separator = key.find_last_of('/');
    const std::size_t dot = key.find_last_of('.');
    if (dot != std::string::npos && (separator == std::string::npos || dot > separator)) key.resize(dot);
  }
  return key;
}

std::string texturePathWithKey(std::string_view path, std::string_view natural_key, std::string_view unique_key) {
  const std::size_t path_separator = path.find_last_of("/\\");
  const std::size_t dot = path.find_last_of('.');
  const bool has_extension =
      dot != std::string_view::npos && (path_separator == std::string_view::npos || dot > path_separator);
  const std::size_t stem_end = has_extension ? dot : path.size();

  std::string result(path.substr(0, stem_end));
  result.append(unique_key.substr(natural_key.size()));
  if (has_extension) result.append(path.substr(dot));
  return result;
}

void makeTexturePathsUnique(std::span<Texture> textures) {
  std::unordered_set<std::string> unavailable;
  unavailable.reserve(textures.size() * 2U);
  for (const Texture& texture : textures) unavailable.insert(texturePathKey(texture.path, true));

  std::unordered_set<std::string> assigned;
  assigned.reserve(textures.size());
  for (Texture& texture : textures) {
    std::string key = texturePathKey(texture.path, true);
    if (assigned.insert(key).second) continue;
    std::string unique = makeUniqueName(key, unavailable);
    unavailable.insert(unique);
    assigned.insert(unique);
    texture.path = texturePathWithKey(texture.path, key, unique);
  }
}

ArxReturnCode addTexture(const ImportMaterial& material, LevelModules& level, TextureRegistry& registry,
                         TextureIndex& out) {
  if (material.stem == "no_tex" || material.portal_fallback) {
    out = kNoTexture;
    return ARX_OK;
  }

  std::map<const cgltf_image*, TextureIndex>* image_map = nullptr;
  std::map<std::string, TextureIndex>* name_map = nullptr;
  std::string key;
  if (material.external_image) {
    name_map = &registry.external_images;
    key = texturePathKey(material.path, false);
  } else if (material.image_source != nullptr) {
    image_map = &registry.images;
  } else {
    name_map = &registry.material_stems;
    key = texturePathKey(material.stem, true);
  }

  if (image_map != nullptr) {
    auto existing = image_map->find(material.image_source);
    if (existing != image_map->end()) {
      out = existing->second;
      return ARX_OK;
    }
  } else {
    auto existing = name_map->find(key);
    if (existing != name_map->end()) {
      out = existing->second;
      return ARX_OK;
    }
  }

  if (material.path.empty()) return ARX_GLB_BAD_LEVEL_MATERIAL;
  if (level.geometry.textures.size() >= static_cast<std::size_t>(kNoTexture)) return ARX_GLB_BAD_LEVEL_MATERIAL;
  out = static_cast<TextureIndex>(level.geometry.textures.size());
  level.geometry.textures.push_back(material.path);
  if (material.encoded_image != nullptr) level.geometry.textures.back().encoded_image = *material.encoded_image;
  if (image_map != nullptr) {
    image_map->emplace(material.image_source, out);
  } else {
    name_map->emplace(std::move(key), out);
  }
  return ARX_OK;
}

void countDiscardedAttributes(const cgltf_primitive& primitive, std::int32_t selected_uv, ImportWarnings& diagnostics) {
  for (std::size_t i = 0; i < primitive.attributes_count; ++i) {
    const cgltf_attribute& attribute = primitive.attributes[i];
    switch (attribute.type) {
      case cgltf_attribute_type_position:
      case cgltf_attribute_type_normal:
        break;
      case cgltf_attribute_type_texcoord:
        if (attribute.index != selected_uv) ++diagnostics.discarded_uv_sets;
        break;
      case cgltf_attribute_type_color:
        if (attribute.index != 0) ++diagnostics.discarded_colors;
        break;
      case cgltf_attribute_type_tangent:
        ++diagnostics.discarded_tangents;
        break;
      case cgltf_attribute_type_joints:
      case cgltf_attribute_type_weights:
        break;
      case cgltf_attribute_type_invalid:
      case cgltf_attribute_type_custom:
      case cgltf_attribute_type_max_enum:
        ++diagnostics.discarded_custom;
        break;
    }
  }
}

ArxReturnCode readGeometryPrimitive(
    const Asset& asset, const cgltf_data& data, const cgltf_node& node, std::size_t node_index,
    const cgltf_primitive& primitive, const math::Mat4& world, const ImportUnits& units, std::uint32_t room,
    LevelModules& level, TextureRegistry& texture_registry, ImportImageCache& image_cache,
    std::map<std::tuple<std::size_t, std::size_t, std::uint32_t>, std::uint32_t>& vertices_by_source,
    ImportWarnings& diagnostics) {
  if (primitive.type != cgltf_primitive_type_triangles) return ARX_GLB_UNSUPPORTED_FEATURE;
  if (node.extensions_count != 0 || node.mesh->extensions_count != 0 || primitive.extensions_count != 0)
    return ARX_GLB_UNSUPPORTED_FEATURE;
  if (node.has_mesh_gpu_instancing || primitive.targets_count != 0 || primitive.has_draco_mesh_compression)
    return ARX_GLB_UNSUPPORTED_FEATURE;
  if (hasSkinning(node, primitive)) diagnostics.ignored_skinning = true;

  const cgltf_accessor* position_source = cgltf_find_accessor(&primitive, cgltf_attribute_type_position, 0);
  if (position_source == nullptr) return ARX_GLB_BAD_LEVEL_GEOMETRY;
  AccessorView positions;
  ArxReturnCode rc = getAccessor(asset, position_source, positions);
  if (rc != ARX_OK) return rc;
  rc = validatePositionAccessor(positions);
  if (rc != ARX_OK) return rc;

  ImportMaterial material;
  rc = parseImportMaterial(primitive.material, image_cache, material, diagnostics.skipped_quad_flags);
  if (rc != ARX_OK) return rc;
  if (material.portal_fallback) ++diagnostics.portal_materials;
  countDiscardedAttributes(primitive, material.uv_set, diagnostics);

  const cgltf_accessor* normal_source = cgltf_find_accessor(&primitive, cgltf_attribute_type_normal, 0);
  const cgltf_accessor* uv_source = cgltf_find_accessor(&primitive, cgltf_attribute_type_texcoord, material.uv_set);
  const cgltf_accessor* color_source = cgltf_find_accessor(&primitive, cgltf_attribute_type_color, 0);
  AccessorView normals;
  AccessorView uv;
  AccessorView colors;
  bool has_normals = normal_source != nullptr;
  bool has_uv = uv_source != nullptr;
  bool has_colors = color_source != nullptr;
  if (has_normals) {
    rc = getAccessor(asset, normal_source, normals);
    if (rc != ARX_OK) return rc;
    rc = validateNormalAccessor(normals);
    if (rc != ARX_OK) return rc;
    if (normals.count != positions.count) return ARX_GLB_BAD_LEVEL_GEOMETRY;
  }
  const std::optional<math::Mat4> normal_inverse = has_normals ? math::inverseAffine(world) : std::nullopt;
  if (has_uv) {
    rc = getAccessor(asset, uv_source, uv);
    if (rc != ARX_OK) return rc;
    rc = validateTexcoordAccessor(uv);
    if (rc != ARX_OK) return rc;
    if (uv.count != positions.count) return ARX_GLB_BAD_LEVEL_GEOMETRY;
  } else if (!material.path.empty()) {
    return ARX_GLB_BAD_FORMAT;
  }
  if (has_colors) {
    rc = getAccessor(asset, color_source, colors);
    if (rc != ARX_OK) return rc;
    rc = validateColorAccessor(colors);
    if (rc != ARX_OK) return rc;
    if (colors.count != positions.count) return ARX_GLB_BAD_LEVEL_GEOMETRY;
  }

  std::vector<std::uint32_t> order;
  rc = primitiveOrder(asset, primitive, positions, false, order);
  if (rc != ARX_OK) return rc;
  if (order.size() % 3 != 0) return ARX_GLB_BAD_LEVEL_GEOMETRY;
  if (determinant(world) < 0.0) {
    for (std::size_t triangle = 0; triangle < order.size(); triangle += 3)
      std::swap(order[triangle + 1], order[triangle + 2]);
  }

  TextureIndex texture = kNoTexture;
  rc = addTexture(material, level, texture_registry, texture);
  if (rc != ARX_OK) return rc;
  std::size_t position_accessor_index = cgltf_accessor_index(&data, position_source);

  for (std::size_t triangle = 0; triangle < order.size(); triangle += 3) {
    std::array<ArxVector3, 3> triangle_positions{};
    std::array<Corner, 3> corners{};
    std::array<ArxColor3, 3> corner_colors = {
        lights::kDefaultCornerColor, lights::kDefaultCornerColor, lights::kDefaultCornerColor};
    for (std::size_t corner_index = 0; corner_index < corners.size(); ++corner_index) {
      std::uint32_t source_index = order[triangle + corner_index];
      if (source_index >= positions.count) return ARX_GLB_BAD_FORMAT;
      GlbVec3 source_position = readVec3(positions, source_index);
      ArxVector3 position = math::xformPoint(world, {source_position.x, source_position.y, source_position.z});
      const std::optional<ArxVector3> converted = glb_level::toArxPoint(position, units);
      if (!converted) return ARX_GLB_BAD_FORMAT;
      position = *converted;

      auto source_key = std::tuple{node_index, position_accessor_index, source_index};
      auto [entry, inserted] = vertices_by_source.emplace(source_key, 0);
      if (inserted) {
        if (level.geometry.vertices.size() >= static_cast<std::size_t>(kInvalidVertexIndex))
          return ARX_GLB_BAD_LEVEL_GEOMETRY;
        entry->second = static_cast<std::uint32_t>(level.geometry.vertices.size());
        level.geometry.vertices.push_back({position});
      }

      Corner corner{};
      corner.vertex = entry->second;
      if (has_normals) {
        GlbVec3 source_normal = readVec3(normals, source_index);
        std::optional<ArxVector3> transformed =
            normal_inverse ? transformNormal(*normal_inverse, {source_normal.x, source_normal.y, source_normal.z})
                           : std::nullopt;
        if (transformed.has_value()) {
          const std::optional<ArxVector3> converted_normal = glb_level::toArxVector(*transformed, units);
          if (!converted_normal) return ARX_GLB_BAD_FORMAT;
          corner.normal = toArx(normalize(toVec3(*converted_normal)));
          float source_length = length(source_normal);
          if (std::abs(source_length - 1.0f) > kLevelGlbEpsilon) ++diagnostics.normalized_normals;
        }
      }
      if (has_uv) {
        GlbVec2 source_uv = readVec2(uv, source_index);
        corner.u = source_uv.x;
        corner.v = source_uv.y;
      }
      if (has_colors) {
        GlbVec3 source_color = readColor3(colors, source_index);
        if (!unitColor(source_color)) return ARX_GLB_BAD_LEVEL_GEOMETRY;
        corner_colors[corner_index] = {source_color.x, source_color.y, source_color.z};
      }
      triangle_positions[corner_index] = position;
      corners[corner_index] = corner;
    }

    if (geometry::degenerateTriangle(triangle_positions[0], triangle_positions[1], triangle_positions[2])) {
      ++diagnostics.discarded_faces;
      continue;
    }
    GlbVec3 face_normal = cross(sub(toVec3(triangle_positions[1]), toVec3(triangle_positions[0])),
                                sub(toVec3(triangle_positions[2]), toVec3(triangle_positions[0])));
    ArxVector3 fallback = toArx(normalize(face_normal));
    for (Corner& corner : corners) {
      if (length(toVec3(corner.normal)) <= std::numeric_limits<float>::epsilon()) {
        corner.normal = fallback;
        ++diagnostics.regenerated_normals;
      }
    }
    if (!has_colors) diagnostics.defaulted_colors += corners.size();

    Face face{};
    face.corners = corners;
    face.texture = texture;
    face.flags = static_cast<FaceType>(material.flags & ~kFaceBitQuad);
    face.transval = material.transval;
    if (has_colors) ensureCornerColors(level);
    if (level.geometry.faces.size() >= static_cast<std::size_t>(kInvalidFaceIndex)) return ARX_GLB_BAD_LEVEL_GEOMETRY;
    level.geometry.faces.push_back(face);
    level.rooms.face_rooms.push_back(room);
    if (has_colors || !level.lighting.corner_colors.empty()) appendCornerColors(level, corner_colors);
  }
  return ARX_OK;
}

ArxReturnCode importNavSurface(const Asset& asset, const cgltf_node& node, std::size_t node_index,
                               const math::Mat4& world, const ImportUnits& units, NavSurface& out) {
  std::string name = nodeName(node);
  if (node.mesh == nullptr || node.mesh->primitives_count == 0 || node.camera != nullptr || node.light != nullptr ||
      node.extensions_count != 0 || node.mesh->extensions_count != 0 || node.has_mesh_gpu_instancing ||
      node.skin != nullptr) {
    log(ARX_LOG_DEBUG,
        std::format("GLB -> Level object failure: nav surface node {} '{}' has invalid payload", node_index, name));
    return ARX_GLB_BAD_LEVEL_NAV_SURFACE;
  }

  NavSurface surface;
  geometry::PositionIndex vertices_by_position(kLevelGlbEpsilon);
  auto add_vertex = [&](const ArxVector3& position) {
    if (std::optional<std::uint32_t> candidate = vertices_by_position.find(position)) return *candidate;
    if (surface.vertices.size() >= static_cast<std::size_t>(kInvalidNavSurfaceVertexIndex))
      return kInvalidNavSurfaceVertexIndex;
    std::uint32_t index = static_cast<std::uint32_t>(surface.vertices.size());
    surface.vertices.push_back({position});
    vertices_by_position.add(index, position);
    return index;
  };

  for (std::size_t primitive_index = 0; primitive_index < node.mesh->primitives_count; ++primitive_index) {
    const cgltf_primitive& primitive = node.mesh->primitives[primitive_index];
    if (primitive.type != cgltf_primitive_type_triangles || primitive.extensions_count != 0 ||
        primitive.targets_count != 0 || primitive.has_draco_mesh_compression) {
      log(ARX_LOG_DEBUG,
          std::format("GLB -> Level object failure: nav surface node {} '{}' primitive {} is unsupported",
                      node_index,
                      name,
                      primitive_index));
      return ARX_GLB_BAD_LEVEL_NAV_SURFACE;
    }
    const cgltf_accessor* position_source = cgltf_find_accessor(&primitive, cgltf_attribute_type_position, 0);
    if (position_source == nullptr) {
      log(ARX_LOG_DEBUG,
          std::format("GLB -> Level object failure: nav surface node {} '{}' lacks POSITION", node_index, name));
      return ARX_GLB_BAD_LEVEL_NAV_SURFACE;
    }
    AccessorView positions;
    ArxReturnCode rc = getAccessor(asset, position_source, positions);
    if (rc != ARX_OK) return rc;
    rc = validatePositionAccessor(positions);
    if (rc != ARX_OK) return rc;

    std::vector<std::uint32_t> order;
    rc = primitiveOrder(asset, primitive, positions, false, order);
    if (rc == ARX_GLB_BAD_LEVEL_GEOMETRY) return ARX_GLB_BAD_LEVEL_NAV_SURFACE;
    if (rc != ARX_OK) return rc;
    if (order.empty() || order.size() % 3 != 0) {
      log(ARX_LOG_DEBUG,
          std::format(
              "GLB -> Level object failure: nav surface node {} '{}' has invalid index count", node_index, name));
      return ARX_GLB_BAD_LEVEL_NAV_SURFACE;
    }
    if (determinant(world) < 0.0) {
      for (std::size_t triangle = 0; triangle < order.size(); triangle += 3)
        std::swap(order[triangle + 1], order[triangle + 2]);
    }

    for (std::size_t triangle = 0; triangle < order.size(); triangle += 3) {
      std::array<std::uint32_t, 3> mapped{};
      std::array<ArxVector3, 3> triangle_positions{};
      for (std::size_t corner = 0; corner < mapped.size(); ++corner) {
        std::uint32_t source_index = order[triangle + corner];
        if (source_index >= positions.count) return ARX_GLB_BAD_FORMAT;
        GlbVec3 source_position = readVec3(positions, source_index);
        ArxVector3 position = math::xformPoint(world, {source_position.x, source_position.y, source_position.z});
        const std::optional<ArxVector3> converted = glb_level::toArxPoint(position, units);
        if (!converted) return ARX_GLB_BAD_FORMAT;
        position = *converted;
        mapped[corner] = add_vertex(position);
        if (mapped[corner] == kInvalidNavSurfaceVertexIndex) return ARX_GLB_BAD_LEVEL_NAV_SURFACE;
        triangle_positions[corner] = position;
      }
      if (mapped[0] == mapped[1] || mapped[0] == mapped[2] || mapped[1] == mapped[2] ||
          geometry::degenerateTriangle(triangle_positions[0], triangle_positions[1], triangle_positions[2])) {
        log(ARX_LOG_DEBUG,
            std::format(
                "GLB -> Level object failure: nav surface node {} '{}' has degenerate triangle", node_index, name));
        return ARX_GLB_BAD_LEVEL_NAV_SURFACE;
      }
      surface.triangles.push_back({mapped});
    }
  }
  if (surface.vertices.empty() || surface.triangles.empty()) return ARX_GLB_BAD_LEVEL_NAV_SURFACE;
  out = std::move(surface);
  return ARX_OK;
}

std::uint64_t navSurfaceComponents(const NavSurface& surface) {
  return static_cast<std::uint64_t>(navigation::surfaceComponentCount(surface));
}

ArxReturnCode convertLightSpatialFields(Light& light, const ImportUnits& units) {
  const std::optional<ArxVector3> position = glb_level::toArxPoint(light.position, units);
  const std::optional<float> fallstart = glb_level::toArxLength(light.fallstart, units);
  const std::optional<float> fallend = glb_level::toArxLength(light.fallend, units);
  const std::optional<float> effect_radius = glb_level::toArxLength(light.effect_radius, units);
  if (!position || !fallstart || !fallend || !effect_radius) return ARX_GLB_BAD_FORMAT;
  light.position = *position;
  light.fallstart = *fallstart;
  light.fallend = *fallend;
  light.effect_radius = *effect_radius;
  return ARX_OK;
}

ArxReturnCode importPointLight(const cgltf_node& node, std::size_t node_index, const math::Mat4& world,
                               const ImportUnits& units, std::size_t ordinal, ImportWarnings& diagnostics, Light& out) {
  const cgltf_light& source = *node.light;
  if (!std::isfinite(source.range) || !std::isfinite(source.intensity) || source.intensity < 0.0f) {
    log(ARX_LOG_DEBUG,
        std::format(
            "GLB -> Level light failure: node {} '{}' has invalid range/intensity", node_index, nodeName(node)));
    return ARX_GLB_BAD_LEVEL_LIGHT;
  }
  GlbVec3 color{source.color[0], source.color[1], source.color[2]};
  if (!std::isfinite(color.x) || !std::isfinite(color.y) || !std::isfinite(color.z) || !unitColor(color)) {
    log(ARX_LOG_DEBUG,
        std::format("GLB -> Level light failure: node {} '{}' has invalid color", node_index, nodeName(node)));
    return ARX_GLB_BAD_LEVEL_LIGHT;
  }

  ParsedLightName parsed_name;
  float fallback_fallend = source.range > 0.0f ? source.range : 0.0f;
  ArxReturnCode rc =
      parseLightName(nodeName(node), source.name != nullptr ? source.name : "", ordinal, fallback_fallend, parsed_name);
  if (rc != ARX_OK) return rc;
  if (!parsed_name.has_fallend) {
    log(ARX_LOG_DEBUG,
        std::format(
            "GLB -> Level light failure: node {} '{}' has no FALLEND or positive range", node_index, nodeName(node)));
    return ARX_GLB_BAD_LEVEL_LIGHT;
  }

  Light light;
  light.name = std::move(parsed_name.name);
  light.position = math::translation(world);
  light.fallstart = parsed_name.fallstart;
  light.fallend = parsed_name.fallend;
  if (!finite(light.position)) return ARX_GLB_BAD_FORMAT;
  rc = parseLightSettings(node, true, true, {color.x, color.y, color.z}, source.intensity, light);
  if (rc != ARX_OK) return rc;
  rc = parseLightFlags(node, light.flags);
  if (rc != ARX_OK) return rc;
  rc = parseLightEffects(node, light);
  if (rc != ARX_OK) return rc;
  rc = convertLightSpatialFields(light, units);
  if (rc != ARX_OK) return rc;

  if (parsed_name.generated) ++diagnostics.generated_light_names;
  if (parsed_name.repaired)
    log(ARX_LOG_WARN,
        std::format("GLB -> Level: light '{}' has invalid FALLSTART; using {}", light.name, light.fallstart));
  log(ARX_LOG_DEBUG,
      std::format("GLB -> Level: imported point light node {} '{}' as '{}', fallstart {}, fallend {}, intensity {}",
                  node_index,
                  nodeName(node),
                  light.name,
                  light.fallstart,
                  light.fallend,
                  light.intensity));
  out = std::move(light);
  return ARX_OK;
}

ArxReturnCode importReservedLight(const cgltf_node& node, std::size_t node_index, const math::Mat4& world,
                                  const ImportUnits& units, std::size_t ordinal, ImportWarnings& diagnostics,
                                  Light& out) {
  if (!isReservedLightName(nodeName(node))) return ARX_GLB_BAD_LEVEL_LIGHT;
  if (node.camera != nullptr) {
    log(ARX_LOG_DEBUG,
        std::format(
            "GLB -> Level light failure: reserved light node {} '{}' has camera payload", node_index, nodeName(node)));
    return ARX_GLB_BAD_LEVEL_LIGHT;
  }
  if (node.light != nullptr) {
    if (node.light->type != cgltf_light_type_point) {
      log(ARX_LOG_DEBUG,
          std::format("GLB -> Level light failure: reserved light node {} '{}' is not a point light",
                      node_index,
                      nodeName(node)));
      return ARX_GLB_BAD_LEVEL_LIGHT;
    }
    return importPointLight(node, node_index, world, units, ordinal, diagnostics, out);
  }
  ParsedLightName parsed_name;
  ArxReturnCode rc = parseLightName(nodeName(node), "", ordinal, 0.0f, parsed_name);
  if (rc != ARX_OK) return rc;
  Light light;
  light.name = std::move(parsed_name.name);
  light.position = math::translation(world);
  light.fallstart = parsed_name.fallstart;
  light.fallend = parsed_name.fallend;
  if (!finite(light.position)) return ARX_GLB_BAD_FORMAT;
  rc = parseLightSettings(node, parsed_name.has_fallend, false, {}, 0.0f, light);
  if (rc != ARX_OK) return rc;
  rc = parseLightFlags(node, light.flags);
  if (rc != ARX_OK) return rc;
  rc = parseLightEffects(node, light);
  if (rc != ARX_OK) return rc;
  rc = convertLightSpatialFields(light, units);
  if (rc != ARX_OK) return rc;
  if (parsed_name.generated) ++diagnostics.generated_light_names;
  if (parsed_name.repaired)
    log(ARX_LOG_WARN,
        std::format("GLB -> Level: light '{}' has invalid FALLSTART; using {}", light.name, light.fallstart));
  log(ARX_LOG_DEBUG,
      std::format("GLB -> Level: imported reserved light node {} '{}' as '{}', fallstart {}, fallend {}, intensity {}",
                  node_index,
                  nodeName(node),
                  light.name,
                  light.fallstart,
                  light.fallend,
                  light.intensity));
  out = std::move(light);
  return ARX_OK;
}

bool cyclicTriangle(const std::array<std::uint32_t, 3>& triangle, const std::array<std::uint32_t, 3>& expected) {
  for (std::size_t offset = 0; offset < 3; ++offset) {
    bool matches = true;
    for (std::size_t i = 0; i < 3; ++i)
      if (triangle[(i + offset) % 3] != expected[i]) matches = false;
    if (matches) return true;
  }
  return false;
}

bool canonicalPortalQuad(const std::vector<std::uint32_t>& order, std::array<std::uint32_t, 4>& perimeter) {
  if (order.size() != 6) return false;
  std::array<std::uint32_t, 3> first = {order[0], order[1], order[2]};
  std::array<std::uint32_t, 3> second = {order[3], order[4], order[5]};
  std::set<std::uint32_t> unique(order.begin(), order.end());
  if (unique.size() != 4) return false;

  std::array<std::uint32_t, 4> candidate{};
  std::copy(unique.begin(), unique.end(), candidate.begin());
  do {
    const std::array<std::uint32_t, 3> arx_a = {candidate[0], candidate[1], candidate[3]};
    const std::array<std::uint32_t, 3> arx_b = {candidate[2], candidate[3], candidate[1]};
    if ((cyclicTriangle(first, arx_a) && cyclicTriangle(second, arx_b)) ||
        (cyclicTriangle(first, arx_b) && cyclicTriangle(second, arx_a))) {
      perimeter = candidate;
      return true;
    }

    const std::array<std::uint32_t, 3> diagonal_a = {candidate[0], candidate[1], candidate[2]};
    const std::array<std::uint32_t, 3> diagonal_b = {candidate[0], candidate[2], candidate[3]};
    if ((cyclicTriangle(first, diagonal_a) && cyclicTriangle(second, diagonal_b)) ||
        (cyclicTriangle(first, diagonal_b) && cyclicTriangle(second, diagonal_a))) {
      perimeter = candidate;
      return true;
    }
  } while (std::next_permutation(candidate.begin(), candidate.end()));
  return false;
}

ArxReturnCode importPortal(const Asset& asset, const cgltf_node& node, const math::Mat4& world,
                           const std::map<std::string, std::uint32_t>& rooms_by_name, ImportWarnings& diagnostics,
                           const ImportUnits& units, Portal& out) {
  std::string name = nodeName(node);
  std::optional<ParsedPortalName> parsed = parsePortalNodeName(name);
  if (!parsed.has_value()) {
    log(ARX_LOG_DEBUG, std::format("GLB -> Level object failure: portal '{}' has invalid name", name));
    return ARX_GLB_BAD_LEVEL_PORTAL;
  }
  auto room_1 = rooms_by_name.find(parsed->room_1);
  auto room_2 = rooms_by_name.find(parsed->room_2);
  if (room_1 == rooms_by_name.end() || room_2 == rooms_by_name.end() || room_1->second == room_2->second) {
    log(ARX_LOG_DEBUG,
        std::format("GLB -> Level object failure: portal '{}' references invalid rooms '{}' and '{}'",
                    name,
                    parsed->room_1,
                    parsed->room_2));
    return ARX_GLB_BAD_LEVEL_PORTAL;
  }
  if (node.mesh == nullptr || node.mesh->primitives_count != 1) {
    log(ARX_LOG_DEBUG,
        std::format("GLB -> Level object failure: portal '{}' must have exactly one mesh primitive", name));
    return ARX_GLB_BAD_LEVEL_PORTAL;
  }
  const cgltf_primitive& primitive = node.mesh->primitives[0];
  if (primitive.type != cgltf_primitive_type_triangles || primitive.indices == nullptr) {
    log(ARX_LOG_DEBUG, std::format("GLB -> Level object failure: portal '{}' must be indexed triangle geometry", name));
    return ARX_GLB_BAD_LEVEL_PORTAL;
  }
  if (node.extensions_count != 0 || node.mesh->extensions_count != 0 || primitive.extensions_count != 0)
    return ARX_GLB_UNSUPPORTED_FEATURE;
  if (node.has_mesh_gpu_instancing || primitive.targets_count != 0 || primitive.has_draco_mesh_compression)
    return ARX_GLB_UNSUPPORTED_FEATURE;
  if (hasSkinning(node, primitive)) diagnostics.ignored_skinning = true;

  const cgltf_accessor* position_source = cgltf_find_accessor(&primitive, cgltf_attribute_type_position, 0);
  if (position_source == nullptr) {
    log(ARX_LOG_DEBUG, std::format("GLB -> Level object failure: portal '{}' lacks POSITION", name));
    return ARX_GLB_BAD_LEVEL_PORTAL;
  }
  AccessorView positions;
  ArxReturnCode rc = getAccessor(asset, position_source, positions);
  if (rc != ARX_OK) return rc;
  rc = validatePositionAccessor(positions);
  if (rc != ARX_OK) return rc;

  std::vector<std::uint32_t> order;
  rc = primitiveOrder(asset, primitive, positions, true, order);
  if (rc != ARX_OK) return rc;
  if (order.size() != 3 && order.size() != 6) {
    log(ARX_LOG_DEBUG,
        std::format("GLB -> Level object failure: portal '{}' has {} indices, expected 3 or 6", name, order.size()));
    return ARX_GLB_BAD_LEVEL_PORTAL;
  }
  if (determinant(world) < 0.0) {
    for (std::size_t triangle = 0; triangle < order.size(); triangle += 3)
      std::swap(order[triangle + 1], order[triangle + 2]);
  }
  for (std::uint32_t index : order) {
    if (index >= positions.count) {
      log(ARX_LOG_DEBUG, std::format("GLB -> Level object failure: portal '{}' index {} is out of range", name, index));
      return ARX_GLB_BAD_LEVEL_PORTAL;
    }
  }

  geometry::PositionIndex position_index(kLevelGlbEpsilon, geometry::PositionWeldMetric::kEuclidean);
  std::map<std::uint32_t, std::uint32_t> welded_by_source;
  std::vector<ArxVector3> welded_positions;
  std::vector<std::uint32_t> welded_order;
  welded_positions.reserve(order.size());
  welded_order.reserve(order.size());
  for (std::uint32_t source_index : order) {
    auto source = welded_by_source.find(source_index);
    if (source == welded_by_source.end()) {
      GlbVec3 encoded = readVec3(positions, source_index);
      const std::optional<ArxVector3> converted =
          glb_level::toArxPoint(math::xformPoint(world, {encoded.x, encoded.y, encoded.z}), units);
      if (!converted) return ARX_GLB_BAD_FORMAT;
      const ArxVector3 position = *converted;
      std::optional<std::uint32_t> existing = position_index.find(position);
      std::uint32_t welded_index = 0;
      if (existing.has_value()) {
        welded_index = *existing;
      } else {
        welded_index = static_cast<std::uint32_t>(welded_positions.size());
        welded_positions.push_back(position);
        position_index.add(welded_index, position);
      }
      source = welded_by_source.emplace(source_index, welded_index).first;
    }
    welded_order.push_back(source->second);
  }
  if (welded_by_source.size() != welded_positions.size()) {
    log(ARX_LOG_DEBUG,
        std::format("GLB -> Level: portal '{}' welded {} referenced source vertices to {} positions",
                    name,
                    welded_by_source.size(),
                    welded_positions.size()));
  }

  std::array<std::uint32_t, 4> perimeter{};
  if (welded_positions.size() == 3) {
    if (order.size() != 3) {
      log(ARX_LOG_DEBUG,
          std::format(
              "GLB -> Level object failure: portal '{}' welded to 3 positions but has {} indices", name, order.size()));
      return ARX_GLB_BAD_LEVEL_PORTAL;
    }
    std::set<std::uint32_t> unique(welded_order.begin(), welded_order.end());
    if (unique.size() != 3) {
      log(ARX_LOG_DEBUG,
          std::format("GLB -> Level object failure: triangle portal '{}' has duplicate welded indices", name));
      return ARX_GLB_BAD_LEVEL_PORTAL;
    }
    perimeter = {welded_order[0], welded_order[1], welded_order[2], 0};
    out.shape = PortalShape::kTriangle;
  } else if (welded_positions.size() == 4) {
    if (!canonicalPortalQuad(welded_order, perimeter)) {
      log(ARX_LOG_DEBUG,
          std::format("GLB -> Level object failure: quad portal '{}' does not form two consistently oriented "
                      "triangles after position welding",
                      name));
      return ARX_GLB_BAD_LEVEL_PORTAL;
    }
    out.shape = PortalShape::kQuad;
  } else {
    log(ARX_LOG_DEBUG,
        std::format("GLB -> Level object failure: portal '{}' has {} referenced source vertices and {} welded "
                    "positions, expected 3 or 4 welded positions",
                    name,
                    welded_by_source.size(),
                    welded_positions.size()));
    return ARX_GLB_BAD_LEVEL_PORTAL;
  }

  out.name = std::move(parsed->name);
  out.room_1 = room_1->second;
  out.room_2 = room_2->second;
  std::size_t count = out.shape == PortalShape::kQuad ? 4 : 3;
  for (std::size_t i = 0; i < count; ++i) {
    out.vertices[i] = welded_positions[perimeter[i]];
  }
  rooms::PortalValidation portal_validation = rooms::validatePortalGeometry(out);
  if (portal_validation != rooms::PortalValidation::kValid) {
    log(ARX_LOG_DEBUG,
        std::format("GLB -> Level object failure: portal '{}' failed geometry validation ({})",
                    name,
                    static_cast<int>(portal_validation)));
    return ARX_GLB_BAD_LEVEL_PORTAL;
  }
  return ARX_OK;
}

ArxAabb referencedGeometryBounds(const LevelModules& level) {
  ArxAabb bounds{};
  bool initialized = false;
  for (const Face& face : level.geometry.faces) {
    for (const Corner& corner : face.corners) {
      const ArxVector3& position = level.geometry.vertices[corner.vertex].position;
      if (!initialized) {
        bounds.min = position;
        bounds.max = position;
        initialized = true;
      } else {
        math::expand(bounds, position);
      }
    }
  }
  return bounds;
}

void logWarnings(const ImportWarnings& diagnostics) {
  if (diagnostics.regenerated_normals != 0 || diagnostics.normalized_normals != 0 || diagnostics.discarded_faces != 0 ||
      diagnostics.discarded_vertices != 0 || diagnostics.skipped_quad_flags != 0 || diagnostics.portal_materials != 0 ||
      diagnostics.discarded_uv_sets != 0 || diagnostics.defaulted_colors != 0 || diagnostics.discarded_colors != 0 ||
      diagnostics.discarded_tangents != 0 || diagnostics.discarded_custom != 0 ||
      diagnostics.generated_light_names != 0 || diagnostics.normalized_legacy_teo != 0) {
    std::string warning = "GLB -> Level repairs:";
    if (diagnostics.regenerated_normals != 0)
      warning += std::format(" {} corner normal(s) regenerated;", diagnostics.regenerated_normals);
    if (diagnostics.normalized_normals != 0)
      warning += std::format(" {} corner normal(s) normalized;", diagnostics.normalized_normals);
    if (diagnostics.discarded_faces != 0)
      warning += std::format(" {} degenerate face(s) discarded;", diagnostics.discarded_faces);
    if (diagnostics.discarded_vertices != 0)
      warning += std::format(" {} unreferenced vertex/vertices discarded;", diagnostics.discarded_vertices);
    if (diagnostics.skipped_quad_flags != 0)
      warning += std::format(" {} QUAD material flag(s) intentionally skipped;", diagnostics.skipped_quad_flags);
    if (diagnostics.portal_materials != 0)
      warning += std::format(" {} arx_portal material primitive(s) treated as no_tex;", diagnostics.portal_materials);
    if (diagnostics.discarded_uv_sets != 0)
      warning += std::format(" {} unselected UV set binding(s) discarded;", diagnostics.discarded_uv_sets);
    if (diagnostics.defaulted_colors != 0)
      warning += std::format(" {} baked color(s) defaulted;", diagnostics.defaulted_colors);
    if (diagnostics.discarded_colors != 0)
      warning += std::format(" {} color attribute binding(s) discarded;", diagnostics.discarded_colors);
    if (diagnostics.discarded_tangents != 0)
      warning += std::format(" {} tangent attribute binding(s) discarded;", diagnostics.discarded_tangents);
    if (diagnostics.discarded_custom != 0)
      warning += std::format(" {} custom attribute binding(s) discarded;", diagnostics.discarded_custom);
    if (diagnostics.generated_light_names != 0)
      warning += std::format(" {} light name(s) generated;", diagnostics.generated_light_names);
    if (diagnostics.normalized_legacy_teo != 0)
      warning += std::format(" normalized {} legacy .teo entity class path(s);", diagnostics.normalized_legacy_teo);
    warning.pop_back();
    log(ARX_LOG_WARN, warning);
  }
  if (diagnostics.ignored_skinning) {
    log(ARX_LOG_WARN, "GLB -> Level: skinning data was ignored");
  }
  if (diagnostics.nav_surface_components > 1) {
    log(ARX_LOG_WARN,
        std::format("GLB -> Level: navigation surface has {} disconnected component(s)",
                    diagnostics.nav_surface_components));
  }
}

}  // namespace

ArxReturnCode importLevelFromGlb(std::span<const std::uint8_t> bytes, LevelModules& out,
                                 const Level::GlbImportOptions& options, LevelValidationState* out_validation,
                                 Level::GlbImportInfo* info) {
  ArxReturnCode rc = glb_level::validateGlbImportOptions(options);
  if (rc != ARX_OK) return rc;
  const ImportUnits units{options.arx_units_per_glb_unit};
  Asset asset;
  rc = parse(bytes, asset);
  if (rc != ARX_OK) return rc;
  cgltf_data& data = *asset.data();
  for (std::size_t i = 0; i < data.extensions_required_count; ++i) {
    if (data.extensions_required[i] == nullptr) return ARX_GLB_BAD_FORMAT;
    if (std::string_view(data.extensions_required[i]) != "KHR_lights_punctual") return ARX_GLB_UNSUPPORTED_FEATURE;
  }

  glb::NodeGraph graph;
  rc = glb::buildNodeGraph(data, graph);
  if (rc != ARX_OK) return rc;
  glb_level::LevelDiscovery discovery;
  rc = glb_level::discoverLevelNodes(data, graph, discovery);
  if (rc != ARX_OK) return rc;
  const std::vector<math::Mat4>& world = graph.world;

  LevelModules tmp;
  ImportWarnings diagnostics;
  TextureRegistry texture_registry;
  ImportImageCache image_cache;
  std::map<std::tuple<std::size_t, std::size_t, std::uint32_t>, std::uint32_t> vertices_by_source;
  std::map<std::string, std::uint32_t> rooms_by_name;
  std::size_t point_light_ordinal = 0;
  rc = glb_level::importPlayerSpawn(data, world, discovery.player_spawns, units, tmp);
  if (rc != ARX_OK) {
    logLevelObjectFailure("player spawn import", rc);
    return rc;
  }
  rc = glb_level::importEntities(data, world, discovery.entities, units, tmp, diagnostics.normalized_legacy_teo);
  if (rc != ARX_OK) {
    logLevelObjectFailure("entity import", rc);
    return rc;
  }
  std::vector<glb_level::PendingZone> pending_zones;
  std::vector<std::string> zone_warnings;
  rc = glb_level::importZones(asset, world, discovery.zones, units, pending_zones, zone_warnings);
  if (rc != ARX_OK) {
    logLevelObjectFailure("zone import", rc);
    return rc;
  }
  std::vector<std::string> path_warnings;
  rc = glb_level::importPaths(data, world, discovery.paths, units, tmp, path_warnings);
  if (rc != ARX_OK) {
    logLevelObjectFailure("path import", rc);
    return rc;
  }
  std::vector<std::string> fog_warnings;
  rc = glb_level::importFogs(data, world, discovery.fogs, units, tmp, fog_warnings);
  if (rc != ARX_OK) {
    logLevelObjectFailure("fog import", rc);
    return rc;
  }
  const bool has_explicit_rooms = !discovery.rooms.empty();
  const bool has_reserved_portals = !discovery.portals.empty();

  auto add_room = [&](std::string name, std::uint32_t& out_room) -> ArxReturnCode {
    if (rooms::validateRoom({name}) != rooms::Error::kNone) return ARX_GLB_BAD_LEVEL_ROOM;
    if (rooms_by_name.find(name) != rooms_by_name.end()) return ARX_GLB_BAD_LEVEL_ROOM;
    if (tmp.rooms.definitions.size() >= static_cast<std::size_t>(kInvalidRoomIndex)) return ARX_GLB_BAD_LEVEL_ROOM;
    out_room = static_cast<std::uint32_t>(tmp.rooms.definitions.size());
    tmp.rooms.definitions.push_back({std::move(name)});
    rooms_by_name.emplace(tmp.rooms.definitions.back().name, out_room);
    return ARX_OK;
  };

  std::vector<std::uint32_t> rooms_by_node(data.nodes_count, kInvalidRoomIndex);
  for (std::size_t node_index : discovery.rooms) {
    const cgltf_node& node = data.nodes[node_index];
    const std::string name = nodeName(node);
    log(ARX_LOG_DEBUG, std::format("GLB -> Level: importing room node {} '{}'", node_index, name));
    if (node.camera != nullptr || node.light != nullptr) {
      log(ARX_LOG_DEBUG,
          std::format("GLB -> Level object failure: room node {} '{}' has non-geometry payload", node_index, name));
      return ARX_GLB_BAD_LEVEL_ROOM;
    }
    std::uint32_t room = 0;
    rc = add_room(roomNameFromNode(name), room);
    if (rc != ARX_OK) {
      log(ARX_LOG_DEBUG,
          std::format("GLB -> Level object failure: duplicate or invalid room node {} '{}'", node_index, name));
      return rc;
    }
    rooms_by_node[node_index] = room;
  }

  auto import_geometry_node =
      [&](const cgltf_node& node, std::size_t node_index, std::string_view name, std::uint32_t room) -> ArxReturnCode {
    if (node.mesh == nullptr) return ARX_GLB_BAD_LEVEL_GEOMETRY;
    for (std::size_t primitive_index = 0; primitive_index < node.mesh->primitives_count; ++primitive_index) {
      ArxReturnCode node_rc = readGeometryPrimitive(asset,
                                                    data,
                                                    node,
                                                    node_index,
                                                    node.mesh->primitives[primitive_index],
                                                    world[node_index],
                                                    units,
                                                    room,
                                                    tmp,
                                                    texture_registry,
                                                    image_cache,
                                                    vertices_by_source,
                                                    diagnostics);
      if (node_rc != ARX_OK) {
        if (node_rc == ARX_GLB_BAD_LEVEL_GEOMETRY || node_rc == ARX_GLB_BAD_LEVEL_MATERIAL) {
          log(ARX_LOG_DEBUG,
              std::format("GLB -> Level geometry failure: node {} '{}' primitive {} returned code {}",
                          node_index,
                          name,
                          primitive_index,
                          static_cast<int>(node_rc)));
        }
        return node_rc;
      }
    }
    return ARX_OK;
  };

  if (has_explicit_rooms) {
    for (const glb_level::DiscoveredGeometryNode& geometry : discovery.geometry) {
      if (geometry.room == glb::kInvalidNodeIndex || geometry.room >= rooms_by_node.size() ||
          rooms_by_node[geometry.room] == kInvalidRoomIndex) {
        log(ARX_LOG_DEBUG,
            std::format("GLB -> Level object failure: non-room mesh node {} '{}' found beside explicit rooms",
                        geometry.node,
                        nodeName(data.nodes[geometry.node])));
        return ARX_GLB_BAD_LEVEL_GEOMETRY;
      }
      rc = import_geometry_node(
          data.nodes[geometry.node], geometry.node, nodeName(data.nodes[geometry.node]), rooms_by_node[geometry.room]);
      if (rc != ARX_OK) return rc;
    }
  } else {
    if (has_reserved_portals) {
      log(ARX_LOG_DEBUG, "GLB -> Level object failure: room-linked portals require explicit room nodes");
      return ARX_GLB_BAD_LEVEL_PORTAL;
    }
    if (!discovery.geometry.empty()) {
      std::uint32_t room = 0;
      rc = add_room("room", room);
      if (rc != ARX_OK) return rc;
      for (const glb_level::DiscoveredGeometryNode& geometry : discovery.geometry) {
        rc = import_geometry_node(data.nodes[geometry.node], geometry.node, nodeName(data.nodes[geometry.node]), room);
        if (rc != ARX_OK) return rc;
      }
    }
  }

  if (discovery.navigation_surfaces.size() > 1) {
    log(ARX_LOG_DEBUG, "GLB -> Level object failure: duplicate navigation surface nodes");
    return ARX_GLB_BAD_LEVEL_NAV_SURFACE;
  }
  if (!discovery.navigation_surfaces.empty()) {
    const std::size_t node_index = discovery.navigation_surfaces.front();
    const cgltf_node& node = data.nodes[node_index];
    const std::string name = nodeName(node);
    log(ARX_LOG_DEBUG, std::format("GLB -> Level: importing nav surface node {} '{}'", node_index, name));
    if (!glb_level::isNavSurfaceRootName(name)) {
      logLevelObjectFailure("navigation surface import", node_index, name, ARX_GLB_BAD_LEVEL_NAV_SURFACE);
      return ARX_GLB_BAD_LEVEL_NAV_SURFACE;
    }
    NavSurface surface;
    rc = importNavSurface(asset, node, node_index, world[node_index], units, surface);
    if (rc != ARX_OK) {
      logLevelObjectFailure("navigation surface import", node_index, name, rc);
      return rc;
    }
    diagnostics.nav_surface_components = navSurfaceComponents(surface);
    tmp.navigation.surface = std::move(surface);
  }

  for (std::size_t node_index : discovery.lights) {
    const cgltf_node& node = data.nodes[node_index];
    const std::string name = nodeName(node);
    if (isReservedLightName(name)) {
      if (node.mesh != nullptr)
        discovery.warnings.push_back(
            std::format("GLB -> Level: reserved light node {} '{}' also has a mesh; mesh discarded", node_index, name));
      Light light;
      rc = importReservedLight(node, node_index, world[node_index], units, point_light_ordinal++, diagnostics, light);
      if (rc != ARX_OK) {
        logLevelObjectFailure("reserved light import", node_index, name, rc);
        return rc;
      }
      if (glb::hasNonIdentityLocalScale(node))
        log(ARX_LOG_WARN,
            std::format(
                "GLB -> Level: light node {} '{}' has nonidentity local scale; scale ignored", node_index, name));
      tmp.lighting.lights.push_back(std::move(light));
    } else {
      if (node.mesh != nullptr)
        discovery.warnings.push_back(std::format(
            "GLB -> Level: generic point light node {} '{}' also has a mesh; mesh discarded", node_index, name));
      Light light;
      rc = importPointLight(node, node_index, world[node_index], units, point_light_ordinal++, diagnostics, light);
      if (rc != ARX_OK) {
        logLevelObjectFailure("point light import", node_index, name, rc);
        return rc;
      }
      if (glb::hasNonIdentityLocalScale(node))
        log(ARX_LOG_WARN,
            std::format(
                "GLB -> Level: light node {} '{}' has nonidentity local scale; scale ignored", node_index, name));
      tmp.lighting.lights.push_back(std::move(light));
    }
  }

  for (std::size_t node_index : discovery.anchors) {
    const cgltf_node& node = data.nodes[node_index];
    const std::string name = nodeName(node);
    log(ARX_LOG_DEBUG, std::format("GLB -> Level: importing anchor node {} '{}'", node_index, name));
    if (node.mesh != nullptr || node.camera != nullptr || node.light != nullptr) {
      log(ARX_LOG_DEBUG,
          std::format("GLB -> Level object failure: anchor node {} '{}' must be an empty node", node_index, name));
      return ARX_GLB_BAD_LEVEL_ANCHOR;
    }
    std::optional<glb_level::ParsedAnchorName> parsed = parseAnchorNodeName(name);
    if (!parsed.has_value()) {
      log(ARX_LOG_DEBUG,
          std::format("GLB -> Level object failure: anchor node {} '{}' has invalid name", node_index, name));
      return ARX_GLB_BAD_LEVEL_ANCHOR;
    }
    glb_level::ParsedAnchorName& parsed_name = *parsed;
    Anchor anchor{};
    const std::optional<ArxVector3> position = glb_level::toArxPoint(math::translation(world[node_index]), units);
    if (!position) return ARX_GLB_BAD_FORMAT;
    anchor.position = *position;
    const auto& parsed_radius = parsed_name.radius;
    if (parsed_radius) {
      const std::optional<float> radius = glb_level::toArxLength(*parsed_radius, units);
      if (!radius) return ARX_GLB_BAD_FORMAT;
      anchor.radius = *radius;
    } else {
      anchor.radius = kDefaultAnchorRadius;
    }
    const auto& parsed_height = parsed_name.height;
    if (parsed_height) {
      const std::optional<float> height = glb_level::toArxLength(*parsed_height, units);
      if (!height) return ARX_GLB_BAD_FORMAT;
      anchor.height = -*height;
    } else {
      anchor.height = kDefaultAnchorHeight;
    }
    anchor.flags = parsed_name.flags;
    anchor.name = std::move(parsed_name.name);
    if (glb::hasNonIdentityLocalScale(node))
      log(ARX_LOG_WARN,
          std::format(
              "GLB -> Level: anchor node {} '{}' has nonidentity local scale; scale ignored", node_index, name));
    tmp.navigation.anchors.push_back(anchor);
  }

  for (std::size_t node_index : discovery.portals) {
    const cgltf_node& node = data.nodes[node_index];
    const std::string name = nodeName(node);
    log(ARX_LOG_DEBUG, std::format("GLB -> Level: importing portal node {} '{}'", node_index, name));
    Portal portal;
    rc = importPortal(asset, node, world[node_index], rooms_by_name, diagnostics, units, portal);
    if (rc != ARX_OK) {
      logLevelObjectFailure("portal import", node_index, name, rc);
      return rc;
    }
    tmp.rooms.portals.push_back(std::move(portal));
  }

  if (tmp.geometry.faces.empty()) return ARX_GLB_NO_LEVEL_GEOMETRY;
  makeTexturePathsUnique(tmp.geometry.textures);
  diagnostics.discarded_vertices = compactLevelVertices(tmp);
  if (!pending_zones.empty()) {
    const ArxAabb zone_bounds = referencedGeometryBounds(tmp);
    tmp.scene.zones = glb_level::finalizeImportedZones(std::move(pending_zones), zone_bounds);
  }
  const std::size_t renamed_portals = rooms::makePortalNamesUnique(tmp.rooms.portals);
  const std::size_t renamed_anchors = navigation::makeAnchorNamesUnique(tmp.navigation.anchors);
  const std::size_t renamed_lights = lights::makeLightNamesUnique(tmp.lighting.lights);
  const std::size_t renamed_fogs = scene::makeFogNamesUnique(tmp.scene.fogs);
  const std::size_t renamed_zones = scene::makeZoneNamesUnique(tmp.scene.zones);
  if (renamed_portals != 0)
    discovery.warnings.push_back(
        std::format("GLB -> Level repairs: {} duplicate portal name(s) renamed", renamed_portals));
  if (renamed_anchors != 0)
    discovery.warnings.push_back(
        std::format("GLB -> Level repairs: {} duplicate anchor name(s) renamed", renamed_anchors));
  if (renamed_lights != 0)
    discovery.warnings.push_back(
        std::format("GLB -> Level repairs: {} duplicate light name(s) renamed", renamed_lights));
  if (renamed_fogs != 0)
    discovery.warnings.push_back(std::format("GLB -> Level repairs: {} duplicate fog name(s) renamed", renamed_fogs));
  if (renamed_zones != 0)
    discovery.warnings.push_back(std::format("GLB -> Level repairs: {} duplicate zone name(s) renamed", renamed_zones));
  Level::GlbImportInfo import_info;
  rc = glb_level::applyGlbImportPlacement(tmp, options, import_info);
  if (rc != ARX_OK) return rc;
  ArxAabb bounds = referencedGeometryBounds(tmp);
  std::size_t outside_geometry_anchors = 0;
  for (std::size_t anchor_index = 0; anchor_index < tmp.navigation.anchors.size(); ++anchor_index) {
    const Anchor& anchor = tmp.navigation.anchors[anchor_index];
    if (!level_anchor_bounds::insideNativeMap(anchor.position)) {
      log(ARX_LOG_DEBUG,
          std::format("GLB -> Level object failure: anchor {} position ({}, {}, {}) is outside native X/Z bounds",
                      anchor_index,
                      anchor.position.x,
                      anchor.position.y,
                      anchor.position.z));
      return ARX_GLB_BAD_LEVEL_ANCHOR;
    }
    if (level_anchor_bounds::materiallyOutsideGeometry(bounds, anchor.position)) ++outside_geometry_anchors;
  }
  if (outside_geometry_anchors != 0)
    discovery.warnings.push_back(std::format("GLB -> Level: {} anchor(s) outside referenced geometry bounds retained",
                                             outside_geometry_anchors));
  LevelValidationState validation;
  rc = validateLevelModules(tmp, validation);
  if (rc != ARX_OK) return rc;
  for (const std::string& warning : zone_warnings) log(ARX_LOG_WARN, warning);
  for (const std::string& warning : path_warnings) log(ARX_LOG_WARN, warning);
  for (const std::string& warning : fog_warnings) log(ARX_LOG_WARN, warning);
  for (const std::string& warning : discovery.warnings) log(ARX_LOG_WARN, warning);
  logWarnings(diagnostics);
  out = std::move(tmp);
  if (out_validation) *out_validation = validation;
  if (info) *info = import_info;
  return ARX_OK;
}

}  // namespace pistoris
