// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#include "modules/geometry.h"

#include "arx_pistoris/base/flags.h"
#include "arx_pistoris/base/indices.h"
#include "arx_pistoris/base/math.h"
#include "arx_pistoris/base/status.h"
#include "arx_pistoris/runtime/types.h"

#include "external/glb/accessor.h"
#include "external/glb/container.h"
#include "external/glb/geometry_material.h"
#include "external/glb/level/coordinates.h"
#include "external/glb/level/import/internal.h"
#include "external/glb/level/palette.h"
#include "external/glb/primitive_indices.h"
#include "external/glb/utils/texture.h"
#include "level/data.h"
#include "modules/lights.h"
#include "utils/log.h"
#include "utils/math/mat4.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <limits>
#include <optional>
#include <string_view>
#include <vector>

namespace pistoris::glb_level_import {

using namespace detail;

namespace {

ArxReturnCode importMaterial(const cgltf_material* source, glb::TextureImporter& texture_importer,
                             ImportWarnings& diagnostics, ImportedMaterial& out) {
  glb::GeometryMaterial decoded;
  glb::GeometryMaterialInfo info;
  switch (glb::decodeGeometryMaterial(source, decoded, &info)) {
    case glb::GeometryMaterialError::kNone:
      break;
    case glb::GeometryMaterialError::kBadMaterial:
    case glb::GeometryMaterialError::kBadAlpha:
      return ARX_GLB_BAD_LEVEL_MATERIAL;
    case glb::GeometryMaterialError::kUnsupportedFeature:
      return ARX_GLB_UNSUPPORTED_FEATURE;
  }

  ImportedMaterial result{.flags = decoded.flags, .transval = decoded.transval, .uv_set = decoded.uv_set};
  const std::string_view material_name = source != nullptr && source->name != nullptr ? source->name : "";
  if (info.duplicate_flags != 0)
    log(ARX_LOG_WARN,
        "GLB -> Level: material '{}' repeats {} face flag token(s); duplicates ignored",
        material_name,
        info.duplicate_flags);
  if (info.stripped_quad) ++diagnostics.skipped_quad_flags;
  if (info.normalized_mask) {
    const float alpha = source->has_pbr_metallic_roughness ? source->pbr_metallic_roughness.base_color_factor[3] : 1.0f;
    log(ARX_LOG_WARN,
        "GLB -> Level: material '{}' MASK base alpha {} and cutoff {} normalized to texture alpha with cutoff 0.5",
        material_name,
        alpha,
        source->alpha_cutoff);
  }
  if (info.no_tex_with_image)
    log(ARX_LOG_WARN, "GLB -> Level: no_tex material '{}' uses its referenced texture", material_name);

  if (decoded.fallback_stem == "arx_portal") {
    result.portal_fallback = true;
  } else {
    if (glb_level::isReservedPaletteStem(decoded.fallback_stem)) return ARX_GLB_BAD_LEVEL_MATERIAL_RESERVED_STEM;
    if (decoded.texture.has_value()) {
      switch (texture_importer.import(*decoded.texture, result.texture)) {
        case glb::TextureImportError::kNone:
          break;
        case glb::TextureImportError::kOutOfMemory:
          return ARX_BAD_ALLOC;
        case glb::TextureImportError::kBadImage:
          return ARX_GLB_BAD_FORMAT;
        case glb::TextureImportError::kBadPath:
          return ARX_GLB_BAD_LEVEL_MATERIAL;
        case glb::TextureImportError::kTooManyTextures:
          return ARX_LEVEL_TOO_MANY_TEXTURES;
      }
    }
  }
  if (info.normalized_blend) {
    if (result.texture != kNoTexture)
      log(ARX_LOG_WARN,
          "GLB -> Level: material '{}' BLEND with base alpha 1 imported without TRANS; texture alpha, if present, "
          "remains native cutout",
          material_name);
    else
      log(ARX_LOG_WARN,
          "GLB -> Level: material '{}' BLEND with base alpha 1 and no texture imported as opaque",
          material_name);
  }
  out = result;
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

}  // namespace

ArxReturnCode readGeometryPrimitive(LevelMeshImportContext& context, const cgltf_node& node,
                                    const cgltf_primitive& primitive, const math::Mat4& world, const ImportUnits& units,
                                    std::uint32_t room, LevelModules& level, glb::TextureImporter& texture_importer,
                                    ImportWarnings& diagnostics) {
  if (primitive.type != cgltf_primitive_type_triangles) return ARX_GLB_UNSUPPORTED_FEATURE;
  if (node.extensions_count != 0 || node.mesh->extensions_count != 0 || primitive.extensions_count != 0)
    return ARX_GLB_UNSUPPORTED_FEATURE;
  if (node.has_mesh_gpu_instancing || primitive.targets_count != 0 || primitive.has_draco_mesh_compression)
    return ARX_GLB_UNSUPPORTED_FEATURE;
  if (hasSkinning(node, primitive)) diagnostics.ignored_skinning = true;

  const cgltf_accessor* position_source = cgltf_find_accessor(&primitive, cgltf_attribute_type_position, 0);
  if (position_source == nullptr) return ARX_GLB_BAD_LEVEL_POSITION_ATTRIBUTE;
  const AccessorView* positions = nullptr;
  ArxReturnCode rc = context.accessors.get(position_source, positions);
  if (rc != ARX_OK) return rc;
  rc = validatePositionAccessor(*positions);
  if (rc != ARX_OK) return ARX_GLB_BAD_LEVEL_POSITION_ATTRIBUTE;

  auto material_found = context.materials.find(primitive.material);
  if (material_found == context.materials.end()) {
    ImportedMaterial parsed;
    rc = importMaterial(primitive.material, texture_importer, diagnostics, parsed);
    if (rc != ARX_OK) return rc;
    material_found = context.materials.emplace(primitive.material, parsed).first;
  }
  const ImportedMaterial& material = material_found->second;
  if (material.portal_fallback) ++diagnostics.portal_materials;
  countDiscardedAttributes(primitive, material.uv_set, diagnostics);

  const cgltf_accessor* normal_source = cgltf_find_accessor(&primitive, cgltf_attribute_type_normal, 0);
  const cgltf_accessor* uv_source = cgltf_find_accessor(&primitive, cgltf_attribute_type_texcoord, material.uv_set);
  const cgltf_accessor* color_source = cgltf_find_accessor(&primitive, cgltf_attribute_type_color, 0);
  const AccessorView* normals = nullptr;
  const AccessorView* uv = nullptr;
  const AccessorView* colors = nullptr;
  bool has_normals = normal_source != nullptr;
  bool has_uv = uv_source != nullptr;
  bool has_colors = color_source != nullptr;
  if (has_normals) {
    rc = context.accessors.get(normal_source, normals);
    if (rc != ARX_OK) return rc;
    rc = validateNormalAccessor(*normals);
    if (rc != ARX_OK) return ARX_GLB_BAD_LEVEL_NORMAL_ATTRIBUTE;
    if (normals->count != positions->count) return ARX_GLB_BAD_LEVEL_NORMAL_ATTRIBUTE;
  }
  const std::optional<math::Mat4> normal_inverse = has_normals ? math::inverseAffine(world) : std::nullopt;
  if (has_uv) {
    rc = context.accessors.get(uv_source, uv);
    if (rc != ARX_OK) return rc;
    rc = validateTexcoordAccessor(*uv);
    if (rc != ARX_OK) return ARX_GLB_BAD_LEVEL_TEXCOORD_ATTRIBUTE;
    if (uv->count != positions->count) return ARX_GLB_BAD_LEVEL_TEXCOORD_ATTRIBUTE;
  } else if (material.texture != kNoTexture) {
    return ARX_GLB_BAD_LEVEL_TEXCOORD_ATTRIBUTE;
  }
  if (has_colors) {
    rc = context.accessors.get(color_source, colors);
    if (rc != ARX_OK) return rc;
    rc = validateColorAccessor(*colors);
    if (rc != ARX_OK) return ARX_GLB_BAD_LEVEL_COLOR_ATTRIBUTE;
    if (colors->count != positions->count) return ARX_GLB_BAD_LEVEL_COLOR_ATTRIBUTE;
  }

  PrimitiveIndices order;
  rc = glb::readPrimitiveIndices(
      context.accessors, primitive, positions->count, false, math::linearDeterminant(world) < 0.0, order);
  if (rc != ARX_OK) return rc == ARX_GLB_BAD_FORMAT ? ARX_GLB_BAD_LEVEL_INDEX_ACCESSOR : rc;
  if (order.size() % 3 != 0) return ARX_GLB_BAD_LEVEL_GEOMETRY;

  for (std::size_t triangle = 0; triangle < order.size(); triangle += 3) {
    std::array<ArxVector3, 3> triangle_positions{};
    std::array<ArxVector3, 3> corner_normals{};
    std::array<Corner, 3> corners{};
    std::array<ArxColor3, 3> corner_colors = {
        lights::kDefaultCornerColor, lights::kDefaultCornerColor, lights::kDefaultCornerColor};
    for (std::size_t corner_index = 0; corner_index < corners.size(); ++corner_index) {
      std::uint32_t source_index = order[triangle + corner_index];
      if (source_index >= positions->count) return ARX_GLB_BAD_LEVEL_INDEX_ACCESSOR;
      GlbVec3 source_position = readVec3(*positions, source_index);
      ArxVector3 position = math::xformPoint(world, {source_position.x, source_position.y, source_position.z});
      const std::optional<ArxVector3> converted = glb_level::toArxPoint(position, units);
      if (!converted) return ARX_LEVEL_BAD_VERTEX_POSITION;
      position = *converted;

      const glb::AccessorElementKey source_key{position_source, source_index};
      auto [entry, inserted] = context.vertices_by_source.emplace(source_key, 0);
      if (inserted) {
        if (level.geometry.vertices.size() >= static_cast<std::size_t>(kInvalidVertexIndex))
          return ARX_LEVEL_TOO_MANY_VERTICES;
        entry->second = static_cast<std::uint32_t>(level.geometry.vertices.size());
        level.geometry.vertices.push_back({position});
      }

      Corner corner{};
      corner.vertex = entry->second;
      if (has_normals) {
        GlbVec3 source_normal = readVec3(*normals, source_index);
        std::optional<ArxVector3> transformed =
            normal_inverse ? transformNormal(*normal_inverse, {source_normal.x, source_normal.y, source_normal.z})
                           : std::nullopt;
        if (transformed.has_value()) {
          const std::optional<ArxVector3> converted_normal = glb_level::toArxVector(*transformed, units);
          if (!converted_normal) return ARX_LEVEL_BAD_CORNER_NORMAL;
          corner_normals[corner_index] = toArx(normalize(toVec3(*converted_normal)));
          float source_length = length(source_normal);
          if (std::abs(source_length - 1.0f) > kLevelGlbEpsilon) ++diagnostics.normalized_normals;
        }
      }
      if (has_uv) {
        GlbVec2 source_uv = readVec2(*uv, source_index);
        corner.u = source_uv.x;
        corner.v = source_uv.y;
      }
      if (has_colors) {
        GlbVec3 source_color = readColor3(*colors, source_index);
        if (!unitColor(source_color)) return ARX_GLB_BAD_LEVEL_COLOR_ATTRIBUTE;
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
    for (std::size_t corner_index = 0; corner_index < corners.size(); ++corner_index) {
      ArxVector3& normal = corner_normals[corner_index];
      if (length(toVec3(normal)) <= std::numeric_limits<float>::epsilon()) {
        normal = fallback;
        ++diagnostics.regenerated_normals;
      }
      corners[corner_index].normal = normal;
    }
    if (!has_colors) diagnostics.defaulted_colors += corners.size();

    Face face{};
    face.corners = corners;
    face.texture = material.texture;
    face.flags = static_cast<FaceType>(material.flags & ~kFaceBitQuad);
    face.transval = material.transval;
    face.normal = fallback;
    if (has_colors) ensureCornerColors(level);
    if (level.geometry.faces.size() >= static_cast<std::size_t>(kInvalidFaceIndex)) return ARX_LEVEL_TOO_MANY_FACES;
    level.geometry.faces.push_back(face);
    level.rooms.face_rooms.push_back(room);
    if (has_colors || !level.lighting.corner_colors.empty()) appendCornerColors(level, corner_colors);
  }
  return ARX_OK;
}

}  // namespace pistoris::glb_level_import
