// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#include "common.h"

#include "arx_pistoris/arx_math.h"
#include "arx_pistoris/level.hpp"
#include "arx_pistoris/level/types.h"
#include "arx_pistoris/pistoris_types.h"

#include "../palette.h"
#include "external/glb/container.h"
#include "modules/geometry.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <limits>
#include <span>
#include <string>
#include <utility>
#include <vector>

namespace pistoris::glb_level_debug {

GlbVec3 toVec3(const pistoris::ArxVector3& value) { return {value.x, value.y, value.z}; }

GlbVec4 debugColor(bool active, const GlbVec4& color) { return active ? color : GlbVec4{0.0f, 0.0f, 0.0f, 1.0f}; }

int addMarkerMesh(Builder& builder, const std::string& name, int material, float size) {
  std::vector<GlbVec3> positions = {
      {0.0f, -size, 0.0f},
      {size, 0.0f, 0.0f},
      {0.0f, 0.0f, size},
      {-size, 0.0f, 0.0f},
      {0.0f, 0.0f, -size},
      {0.0f, size, 0.0f},
  };
  std::vector<std::uint32_t> indices = {
      0, 1, 2, 0, 2, 3, 0, 3, 4, 0, 4, 1, 5, 2, 1, 5, 3, 2, 5, 4, 3, 5, 1, 4,
  };
  Primitive primitive;
  primitive.indices =
      builder.addAccessor(std::span<const std::uint32_t>(indices), cgltf_component_type_r_32u, cgltf_type_scalar);
  primitive.material = material;
  primitive.attributes.emplace_back("POSITION", builder.addVec3Accessor(positions));
  return builder.addMesh(name, {std::move(primitive)});
}

void addMarkerNode(Builder& builder, int parent, int mesh, const std::string& name, const ArxVector3& position) {
  int node = builder.addNode(name, mesh);
  if (position.x != 0.0f || position.y != 0.0f || position.z != 0.0f)
    builder.setNodeTranslation(node, toVec3(position));
  builder.addChild(parent, node);
}

int addDebugMeshChild(Builder& builder, int parent, const std::string& name, std::span<const GlbVec3> positions,
                      std::span<const std::uint32_t> indices, int material) {
  if (positions.empty() || indices.empty()) return -1;
  Primitive primitive;
  primitive.indices = builder.addAccessor(indices, cgltf_component_type_r_32u, cgltf_type_scalar);
  primitive.material = material;
  primitive.attributes.emplace_back("POSITION", builder.addVec3Accessor(positions));
  int mesh = builder.addMesh(name, {std::move(primitive)});
  int node = builder.addNode(name, mesh);
  builder.addChild(parent, node);
  return node;
}

ArxVector3 debugGroupPosition(const ArxAabb& bounds) {
  constexpr float kDebugGroupYOffset = 100.0f;
  return {
      (bounds.min.x + bounds.max.x) * 0.5f,
      bounds.max.y + kDebugGroupYOffset,
      (bounds.min.z + bounds.max.z) * 0.5f,
  };
}

GlbVec3 operator-(const GlbVec3& a, const ArxVector3& b) { return {a.x - b.x, a.y - b.y, a.z - b.z}; }

ArxVector3 positionsCenter(std::span<const GlbVec3> positions) {
  if (positions.empty()) return {};
  ArxVector3 min{positions[0].x, positions[0].y, positions[0].z};
  ArxVector3 max = min;
  for (const GlbVec3& position : positions) {
    min.x = std::min(min.x, position.x);
    min.y = std::min(min.y, position.y);
    min.z = std::min(min.z, position.z);
    max.x = std::max(max.x, position.x);
    max.y = std::max(max.y, position.y);
    max.z = std::max(max.z, position.z);
  }
  return {(min.x + max.x) * 0.5f, (min.y + max.y) * 0.5f, (min.z + max.z) * 0.5f};
}

int addDebugMeshChildAtCenter(Builder& builder, int parent, const std::string& name, std::span<const GlbVec3> positions,
                              std::span<const std::uint32_t> indices, int material, const ArxVector3& parent_origin) {
  if (positions.empty() || indices.empty()) return -1;
  ArxVector3 center = positionsCenter(positions);
  std::vector<GlbVec3> local_positions;
  local_positions.reserve(positions.size());
  for (const GlbVec3& position : positions) local_positions.push_back(position - center);

  Primitive primitive;
  primitive.indices = builder.addAccessor(indices, cgltf_component_type_r_32u, cgltf_type_scalar);
  primitive.material = material;
  primitive.attributes.emplace_back("POSITION", builder.addVec3Accessor(local_positions));
  int mesh = builder.addMesh(name, {std::move(primitive)});
  int node = builder.addNode(name, mesh);
  builder.setNodeTranslation(
      node, toVec3({center.x - parent_origin.x, center.y - parent_origin.y, center.z - parent_origin.z}));
  builder.addChild(parent, node);
  return node;
}

int addDebugGroupUnderMap(Builder& builder, int parent, const std::string& name, const ArxAabb& bounds,
                          ArxVector3& out_origin) {
  out_origin = debugGroupPosition(bounds);
  int group = builder.addNode(name);
  builder.setNodeTranslation(group, toVec3(out_origin));
  builder.addChild(parent, group);
  return group;
}

void appendSurfaceSupportMeshData(std::span<const geometry::SurfaceSupportTriangle> faces,
                                  std::vector<GlbVec3>& positions, std::vector<std::uint32_t>& indices) {
  for (const geometry::SurfaceSupportTriangle& face : faces) {
    std::uint32_t base = static_cast<std::uint32_t>(positions.size());
    positions.push_back(toVec3(face.vertices[0]));
    positions.push_back(toVec3(face.vertices[1]));
    positions.push_back(toVec3(face.vertices[2]));
    indices.push_back(base);
    indices.push_back(base + 1);
    indices.push_back(base + 2);
  }
}

void appendMarkerMeshData(const ArxVector3& position, float size, std::vector<GlbVec3>& positions,
                          std::vector<std::uint32_t>& indices) {
  std::uint32_t base = static_cast<std::uint32_t>(positions.size());
  positions.push_back({position.x, position.y - size, position.z});
  positions.push_back({position.x + size, position.y, position.z});
  positions.push_back({position.x, position.y, position.z + size});
  positions.push_back({position.x - size, position.y, position.z});
  positions.push_back({position.x, position.y, position.z - size});
  positions.push_back({position.x, position.y + size, position.z});

  const std::array<std::uint32_t, 24> marker_indices = {
      0, 1, 2, 0, 2, 3, 0, 3, 4, 0, 4, 1, 5, 2, 1, 5, 3, 2, 5, 4, 3, 5, 1, 4,
  };
  for (std::uint32_t index : marker_indices) indices.push_back(base + index);
}

void addSurfaceSupportMeshChild(Builder& builder, int parent, const std::string& name,
                                std::span<const geometry::SurfaceSupportTriangle> faces, int material) {
  std::vector<GlbVec3> positions;
  std::vector<std::uint32_t> indices;
  positions.reserve(faces.size() * 3);
  indices.reserve(faces.size() * 3);
  appendSurfaceSupportMeshData(faces, positions, indices);
  addDebugMeshChild(builder, parent, name, positions, indices, material);
}

void addGeometryContextMesh(Builder& builder, int parent, const pistoris::Level& level, glb_level::Palette& palette) {
  std::vector<ArxLevelFace> faces(level.faceCount());
  std::vector<ArxLevelVertex> vertices(level.vertexCount());
  if (level.copyFaces(0, faces.size(), faces.data()) != ARX_OK ||
      level.copyVertices(0, vertices.size(), vertices.data()) != ARX_OK)
    return;
  std::vector<GlbVec3> positions;
  std::vector<std::uint32_t> indices;
  positions.reserve(faces.size() * 3);
  indices.reserve(faces.size() * 3);
  for (const ArxLevelFace& face : faces) {
    std::uint32_t base = static_cast<std::uint32_t>(positions.size());
    for (const ArxLevelCorner& corner : face.corners) {
      positions.push_back(toVec3(vertices[corner.vertex].position));
    }
    indices.push_back(base);
    indices.push_back(base + 1);
    indices.push_back(base + 2);
  }
  int material = palette.material(glb_level::PaletteItem::kGeometryContext);
  addDebugMeshChild(builder, parent, "navigation_debug_geometry_context", positions, indices, material);
}

void appendSegmentQuad(const ArxVector3& a, const ArxVector3& b, float half_width, std::vector<GlbVec3>& positions,
                       std::vector<std::uint32_t>& indices) {
  float dx = b.x - a.x;
  float dy = b.y - a.y;
  float dz = b.z - a.z;
  float len = std::sqrt(dx * dx + dz * dz);
  float ox = half_width;
  float oz = 0.0f;
  if (len > std::numeric_limits<float>::epsilon()) {
    ox = -dz / len * half_width;
    oz = dx / len * half_width;
  } else if (std::abs(dy) <= std::numeric_limits<float>::epsilon()) {
    return;
  }
  std::uint32_t base = static_cast<std::uint32_t>(positions.size());
  positions.push_back({a.x + ox, a.y, a.z + oz});
  positions.push_back({a.x - ox, a.y, a.z - oz});
  positions.push_back({b.x - ox, b.y, b.z - oz});
  positions.push_back({b.x + ox, b.y, b.z + oz});
  indices.push_back(base);
  indices.push_back(base + 1);
  indices.push_back(base + 2);
  indices.push_back(base);
  indices.push_back(base + 2);
  indices.push_back(base + 3);
}

}  // namespace pistoris::glb_level_debug
