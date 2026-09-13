// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#include "minimap.h"

#include "arx_pistoris/base/flags.h"
#include "arx_pistoris/base/math.h"
#include "arx_pistoris/base/status.h"
#include "arx_pistoris/runtime/types.h"

#include "cgltf/cgltf.h"
#include "external/glb/accessor.h"
#include "external/glb/level/coordinates.h"
#include "external/glb/primitive_indices.h"
#include "external/glb/utils/image.h"
#include "external/glb/writer.h"
#include "modules/minimap.h"
#include "utils/encoded_image.h"
#include "utils/log.h"
#include "utils/math/mat4.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <optional>
#include <span>
#include <string>
#include <utility>
#include <vector>

namespace pistoris::glb_level {
namespace {

constexpr float kPlaneOffset = 10.0f;
constexpr float kCoordinateEpsilon = 1.0e-3f;
constexpr float kUvEpsilon = 1.0e-4f;

bool near(float first, float second, float epsilon) noexcept { return std::abs(first - second) <= epsilon; }

ArxReturnCode preparedImage(const MinimapData& minimap, std::vector<std::uint8_t>& storage,
                            std::span<const std::uint8_t>& encoded, image::Format& format) {
  format = image::detectFormat(minimap.encoded_image);
  encoded = minimap.encoded_image;
  if (format == image::Format::kPng || format == image::Format::kJpeg) return ARX_OK;
  const image::Error transcode_error = image::transcodeToPng(minimap.encoded_image, storage);
  if (transcode_error == image::Error::kOutOfMemory) return ARX_BAD_ALLOC;
  if (transcode_error != image::Error::kNone) return ARX_LEVEL_BAD_MINIMAP_IMAGE;
  format = image::Format::kPng;
  encoded = storage;
  return ARX_OK;
}

const cgltf_accessor* attribute(const cgltf_primitive& primitive, cgltf_attribute_type type, int index) noexcept {
  return cgltf_find_accessor(&primitive, type, index);
}

std::optional<std::size_t> positionCorner(const ArxVector3& position, float min_x, float max_x, float min_z,
                                          float max_z) noexcept {
  const bool left = near(position.x, min_x, kCoordinateEpsilon);
  const bool right = near(position.x, max_x, kCoordinateEpsilon);
  const bool top = near(position.z, max_z, kCoordinateEpsilon);
  const bool bottom = near(position.z, min_z, kCoordinateEpsilon);
  if (left && top) return 0;
  if (right && top) return 1;
  if (right && bottom) return 2;
  if (left && bottom) return 3;
  return std::nullopt;
}

std::optional<std::size_t> textureCorner(const glb::Vec2& uv) noexcept {
  constexpr std::array<glb::Vec2, 4> kCorners = {{{0.0f, 0.0f}, {1.0f, 0.0f}, {1.0f, 1.0f}, {0.0f, 1.0f}}};
  for (std::size_t corner = 0; corner < kCorners.size(); ++corner)
    if (near(uv.x, kCorners[corner].x, kUvEpsilon) && near(uv.y, kCorners[corner].y, kUvEpsilon)) return corner;
  return std::nullopt;
}

bool canonicalTopology(const std::array<std::array<std::size_t, 3>, 2>& triangles) noexcept {
  constexpr std::array<std::size_t, 3> kTopRight = {0, 1, 2};
  constexpr std::array<std::size_t, 3> kBottomLeft = {0, 2, 3};
  constexpr std::array<std::size_t, 3> kTopLeft = {0, 1, 3};
  constexpr std::array<std::size_t, 3> kBottomRight = {1, 2, 3};
  return (triangles[0] == kTopRight && triangles[1] == kBottomLeft) ||
         (triangles[1] == kTopRight && triangles[0] == kBottomLeft) ||
         (triangles[0] == kTopLeft && triangles[1] == kBottomRight) ||
         (triangles[1] == kTopLeft && triangles[0] == kBottomRight);
}

std::optional<image::QuarterTurn> imageRotation(const std::array<int, 4>& texture_corners) noexcept {
  constexpr std::array<int, 4> kNone = {0, 1, 2, 3};
  constexpr std::array<int, 4> kClockwise90 = {3, 0, 1, 2};
  constexpr std::array<int, 4> kClockwise180 = {2, 3, 0, 1};
  constexpr std::array<int, 4> kClockwise270 = {1, 2, 3, 0};
  if (texture_corners == kNone) return image::QuarterTurn::kNone;
  if (texture_corners == kClockwise90) return image::QuarterTurn::kClockwise90;
  if (texture_corners == kClockwise180) return image::QuarterTurn::kClockwise180;
  if (texture_corners == kClockwise270) return image::QuarterTurn::kClockwise270;
  return std::nullopt;
}

MinimapImportError importError(ArxReturnCode error) noexcept {
  return error == ARX_BAD_ALLOC ? MinimapImportError::kOutOfMemory : MinimapImportError::kBadData;
}

MinimapImportError embeddedBaseColorImage(const cgltf_material* material, const cgltf_image*& out) noexcept {
  if (material == nullptr || !material->has_pbr_metallic_roughness || material->extensions_count != 0)
    return MinimapImportError::kBadData;
  const cgltf_texture_view& view = material->pbr_metallic_roughness.base_color_texture;
  if (view.texture == nullptr || view.texture->image == nullptr || view.has_transform || view.texcoord != 0 ||
      view.texture->extensions_count != 0 || view.texture->image->extensions_count != 0) {
    return MinimapImportError::kBadData;
  }
  out = view.texture->image;
  return MinimapImportError::kNone;
}

}  // namespace

ArxReturnCode exportMinimap(const MinimapData& minimap, const ArxAabb& referenced_bounds, glb::Builder& builder) {
  if (minimap.encoded_image.empty()) return ARX_OK;

  std::vector<std::uint8_t> prepared;
  std::span<const std::uint8_t> encoded;
  image::Format format = image::Format::kUnknown;
  ArxReturnCode rc = preparedImage(minimap, prepared, encoded, format);
  if (rc != ARX_OK) return rc;

  const ArxRect& bounds = minimap.world_xz_bounds;
  const float y = referenced_bounds.max.y + kPlaneOffset;
  const std::array<glb::Vec3, 4> positions = {{
      {bounds.min.x, y, bounds.max.y},
      {bounds.max.x, y, bounds.max.y},
      {bounds.max.x, y, bounds.min.y},
      {bounds.min.x, y, bounds.min.y},
  }};
  const std::array<glb::Vec2, 4> texcoords = {{{0.0f, 0.0f}, {1.0f, 0.0f}, {1.0f, 1.0f}, {0.0f, 1.0f}}};
  const std::array<std::uint32_t, 6> indices = {0, 1, 2, 0, 2, 3};

  const int texture = builder.addEmbeddedTexture("minimap", std::string(glb::imageMimeType(format)), encoded);
  const int material = builder.addMaterial("minimap", texture, kFaceBitDoublesided, 1.0f, true);
  glb::Primitive primitive;
  primitive.indices =
      builder.addAccessor(std::span<const std::uint32_t>(indices), cgltf_component_type_r_32u, cgltf_type_scalar);
  primitive.material = material;
  primitive.attributes = {
      {"POSITION", builder.addVec3Accessor(positions)},
      {"TEXCOORD_0",
       builder.addAccessor(std::span<const glb::Vec2>(texcoords), cgltf_component_type_r_32f, cgltf_type_vec2)},
  };
  const int mesh = builder.addMesh("minimap", {std::move(primitive)});
  const int node = builder.addNode("arx_minimap__map", mesh);
  builder.addRoot(node);
  return ARX_OK;
}

MinimapImportError importMinimap(const glb::Asset& asset, const cgltf_node& node, const math::Mat4& world,
                                 const ImportUnits& units, MinimapData& out) {
  if (node.mesh == nullptr || node.camera != nullptr || node.light != nullptr || node.skin != nullptr ||
      node.mesh->primitives_count != 1) {
    return MinimapImportError::kBadData;
  }
  const cgltf_primitive& primitive = node.mesh->primitives[0];
  if (primitive.type != cgltf_primitive_type_triangles || primitive.targets_count != 0)
    return MinimapImportError::kBadData;

  const cgltf_accessor* position_source = attribute(primitive, cgltf_attribute_type_position, 0);
  const cgltf_accessor* texcoord_source = attribute(primitive, cgltf_attribute_type_texcoord, 0);
  if (position_source == nullptr) return MinimapImportError::kBadData;
  bool seen_position = false;
  bool seen_texcoord = false;
  for (std::size_t index = 0; index < primitive.attributes_count; ++index) {
    const cgltf_attribute& current = primitive.attributes[index];
    if (current.type == cgltf_attribute_type_position && current.index == 0 && !seen_position) {
      seen_position = true;
    } else if (current.type == cgltf_attribute_type_texcoord && current.index == 0 && !seen_texcoord) {
      seen_texcoord = true;
    } else if (!((current.type == cgltf_attribute_type_normal || current.type == cgltf_attribute_type_tangent) &&
                 current.index == 0)) {
      return MinimapImportError::kBadData;
    }
  }
  if (!seen_position) return MinimapImportError::kBadData;

  glb::AccessorCache accessors(asset, (primitive.indices != nullptr ? 2 : 1) + (texcoord_source != nullptr ? 1 : 0));
  const glb::AccessorView* positions = nullptr;
  const glb::AccessorView* texcoords = nullptr;
  ArxReturnCode rc = accessors.get(position_source, positions);
  if (rc != ARX_OK) return importError(rc);
  if (glb::validatePositionAccessor(*positions) != ARX_OK || positions->count < 3) {
    return MinimapImportError::kBadData;
  }
  const bool missing_texcoords = texcoord_source == nullptr;
  bool unusable_texcoords = false;
  bool usable_texcoords = !missing_texcoords;
  if (usable_texcoords) {
    rc = accessors.get(texcoord_source, texcoords);
    if (rc != ARX_OK) return importError(rc);
    if (glb::validateTexcoordAccessor(*texcoords) != ARX_OK || positions->count != texcoords->count) {
      usable_texcoords = false;
      unusable_texcoords = true;
    }
  }

  glb::PrimitiveIndices indices;
  rc = glb::readPrimitiveIndices(accessors, primitive, positions->count, false, false, indices);
  if (rc != ARX_OK) return importError(rc);
  if (indices.size() < 3 || indices.size() % 3 != 0) return MinimapImportError::kBadData;

  float min_x = std::numeric_limits<float>::infinity();
  float max_x = -std::numeric_limits<float>::infinity();
  float min_z = std::numeric_limits<float>::infinity();
  float max_z = -std::numeric_limits<float>::infinity();
  float min_y = std::numeric_limits<float>::infinity();
  float max_y = -std::numeric_limits<float>::infinity();
  for (std::size_t element = 0; element < indices.size(); ++element) {
    const std::uint32_t index = indices[element];
    if (index >= positions->count) return MinimapImportError::kBadData;
    const glb::Vec3 source = glb::readVec3(*positions, index);
    const std::optional<ArxVector3> value = toArxPoint(math::xformPoint(world, {source.x, source.y, source.z}), units);
    if (!value) return MinimapImportError::kBadData;
    min_y = std::min(min_y, value->y);
    max_y = std::max(max_y, value->y);
    min_x = std::min(min_x, value->x);
    max_x = std::max(max_x, value->x);
    min_z = std::min(min_z, value->z);
    max_z = std::max(max_z, value->z);
  }
  if (!(min_x < max_x && min_z < max_z)) return MinimapImportError::kBadData;
  const bool varying_height = !near(min_y, max_y, kCoordinateEpsilon);

  std::array<bool, 4> corners{};
  std::array<int, 4> texture_corners = {-1, -1, -1, -1};
  std::array<std::array<std::size_t, 3>, 2> triangles{};
  bool rectangle_geometry = indices.size() == 6;
  bool canonical_uv = usable_texcoords;
  for (std::size_t element = 0; element < indices.size(); ++element) {
    const std::uint32_t index = indices[element];
    const glb::Vec3 source = glb::readVec3(*positions, index);
    const std::optional<ArxVector3> position =
        toArxPoint(math::xformPoint(world, {source.x, source.y, source.z}), units);
    if (!position) return MinimapImportError::kBadData;
    const std::optional<std::size_t> corner = positionCorner(*position, min_x, max_x, min_z, max_z);
    if (!corner) {
      rectangle_geometry = false;
      canonical_uv = false;
      continue;
    }
    corners[*corner] = true;
    if (element < 6) triangles[element / 3][element % 3] = *corner;
    if (!usable_texcoords) continue;

    const std::optional<std::size_t> texture_corner = textureCorner(glb::readVec2(*texcoords, index));
    if (!texture_corner ||
        (texture_corners[*corner] != -1 && texture_corners[*corner] != static_cast<int>(*texture_corner))) {
      canonical_uv = false;
      continue;
    }
    texture_corners[*corner] = static_cast<int>(*texture_corner);
  }
  if (!std::ranges::all_of(corners, [](bool present) { return present; })) rectangle_geometry = false;
  if (rectangle_geometry) {
    for (std::array<std::size_t, 3>& triangle : triangles) {
      std::ranges::sort(triangle);
      if (triangle[0] == triangle[1] || triangle[1] == triangle[2]) rectangle_geometry = false;
    }
    if (rectangle_geometry && !canonicalTopology(triangles)) rectangle_geometry = false;
  }
  std::optional<image::QuarterTurn> rotation;
  if (canonical_uv) rotation = imageRotation(texture_corners);
  const bool noncanonical_uv = usable_texcoords && !rotation;

  const cgltf_image* source_image = nullptr;
  const MinimapImportError import_error = embeddedBaseColorImage(primitive.material, source_image);
  if (import_error != MinimapImportError::kNone) return import_error;
  image::Format format = image::Format::kUnknown;
  std::vector<std::uint8_t> encoded;
  rc = glb::readEmbeddedImage(*source_image, encoded, format);
  if (rc != ARX_OK) return importError(rc);
  if (rotation && *rotation != image::QuarterTurn::kNone) {
    std::vector<std::uint8_t> rotated;
    const image::Error rotate_error = image::rotateQuarterTurnToPng(encoded, *rotation, rotated);
    if (rotate_error == image::Error::kOutOfMemory) return MinimapImportError::kOutOfMemory;
    if (rotate_error != image::Error::kNone) return MinimapImportError::kBadData;
    encoded = std::move(rotated);
  }

  MinimapData result{
      .encoded_image = std::move(encoded),
      .world_xz_bounds = {.min = {min_x, min_z}, .max = {max_x, max_z}},
  };
  out = std::move(result);
  if (unusable_texcoords)
    log(ARX_LOG_WARN,
        "GLB -> Level: minimap has an unusable TEXCOORD_0 attribute; image orientation assumed canonical");
  else if (missing_texcoords)
    log(ARX_LOG_WARN, "GLB -> Level: minimap has no TEXCOORD_0 attribute; image orientation assumed canonical");
  if (varying_height) log(ARX_LOG_WARN, "GLB -> Level: minimap mesh has varying height; height discarded");
  if (!rectangle_geometry)
    log(ARX_LOG_WARN, "GLB -> Level: minimap mesh does not use rectangular topology; X/Z bounds used");
  if (noncanonical_uv)
    log(ARX_LOG_WARN, "GLB -> Level: minimap UV mapping is noncanonical; image orientation assumed canonical");
  return MinimapImportError::kNone;
}

}  // namespace pistoris::glb_level
