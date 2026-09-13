// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#include "arx_pistoris/base/math.h"
#include "arx_pistoris/base/status.h"
#include "arx_pistoris/runtime/types.h"

#include "external/glb/accessor.h"
#include "external/glb/container.h"
#include "external/glb/level/coordinates.h"
#include "external/glb/primitive_indices.h"
#include "internal.h"
#include "modules/geometry.h"
#include "utils/log.h"
#include "utils/math/mat4.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <optional>
#include <string_view>
#include <unordered_map>

namespace pistoris::glb_level::zone_internal {
namespace {

std::uint32_t weldVertex(const ArxVector3& position, Mesh& mesh, geometry::PositionIndex& position_index) {
  if (const std::optional<std::uint32_t> existing = position_index.find(position)) return *existing;
  if (mesh.positions.size() >= std::numeric_limits<std::uint32_t>::max())
    return std::numeric_limits<std::uint32_t>::max();
  const std::uint32_t index = static_cast<std::uint32_t>(mesh.positions.size());
  mesh.positions.push_back(position);
  if (!position_index.tryAdd(index, position)) return std::numeric_limits<std::uint32_t>::max();
  return index;
}

}  // namespace

ArxReturnCode readMesh(const glb::Asset& asset, const cgltf_node& node, const math::Mat4& world,
                       const ImportUnits& units, std::size_t node_index, std::string_view name, Mesh& out) {
  if (node.mesh == nullptr || node.mesh->primitives_count == 0) {
    logFailure(node_index, name, "must have a mesh with at least one primitive");
    return ARX_GLB_BAD_LEVEL_ZONE;
  }
  if (node.camera != nullptr || node.light != nullptr || node.extensions_count != 0 ||
      node.mesh->extensions_count != 0 || node.has_mesh_gpu_instancing || node.skin != nullptr) {
    logFailure(node_index, name, "has unsupported node/mesh payload");
    return ARX_GLB_BAD_LEVEL_ZONE;
  }

  glb::AccessorCache accessors(asset, node.mesh->primitives_count * 2U);
  geometry::PositionIndex position_index(kVertexWeldEpsilon);
  position_index.reservePositionCapacity(node.mesh->primitives_count * 3U);
  std::unordered_map<glb::AccessorElementKey, std::uint32_t, glb::AccessorElementKeyHash> vertex_by_source;
  std::size_t source_vertex_count = 0;
  std::size_t degenerate_triangles = 0;
  for (std::size_t primitive_index = 0; primitive_index < node.mesh->primitives_count; ++primitive_index) {
    const cgltf_primitive& primitive = node.mesh->primitives[primitive_index];
    if (primitive.type != cgltf_primitive_type_triangles || primitive.indices == nullptr ||
        primitive.extensions_count != 0 || primitive.targets_count != 0 || primitive.has_draco_mesh_compression) {
      logFailure(node_index,
                 name,
                 "primitive {} must be indexed triangle geometry without extensions/targets",
                 primitive_index);
      return ARX_GLB_BAD_LEVEL_ZONE;
    }
    const cgltf_accessor* position_source = cgltf_find_accessor(&primitive, cgltf_attribute_type_position, 0);
    if (position_source == nullptr) {
      logFailure(node_index, name, "primitive {} lacks POSITION", primitive_index);
      return ARX_GLB_BAD_LEVEL_ZONE;
    }
    const glb::AccessorView* positions = nullptr;
    ArxReturnCode rc = accessors.get(position_source, positions);
    if (rc != ARX_OK) return rc;
    rc = glb::validatePositionAccessor(*positions);
    if (rc != ARX_OK) return rc;
    glb::PrimitiveIndices indices;
    rc = glb::readPrimitiveIndices(accessors, primitive, positions->count, true, false, indices);
    if (rc != ARX_OK) return rc;
    if (indices.size() == 0 || indices.size() % 3 != 0) {
      logFailure(node_index,
                 name,
                 "primitive {} has {} index/indexes, expected non-empty multiple of 3",
                 primitive_index,
                 indices.size());
      return ARX_GLB_BAD_LEVEL_ZONE;
    }

    for (std::size_t i = 0; i < indices.size(); i += 3) {
      std::array<std::uint32_t, 3> mapped{};
      for (std::size_t corner = 0; corner < mapped.size(); ++corner) {
        const std::uint32_t source_index = indices[i + corner];
        if (source_index >= positions->count) {
          logFailure(node_index,
                     name,
                     "primitive {} index {} is outside {} POSITION value(s)",
                     primitive_index,
                     source_index,
                     positions->count);
          return ARX_GLB_BAD_LEVEL_ZONE;
        }
        const glb::AccessorElementKey key{position_source, source_index};
        auto [entry, inserted] = vertex_by_source.emplace(key, 0);
        if (inserted) {
          const glb::Vec3 source = glb::readVec3(*positions, source_index);
          ArxVector3 position = math::xformPoint(world, {source.x, source.y, source.z});
          const std::optional<ArxVector3> converted = toArxPoint(position, units);
          if (!converted) return ARX_GLB_BAD_FORMAT;
          entry->second = weldVertex(*converted, out, position_index);
          if (entry->second == std::numeric_limits<std::uint32_t>::max()) return ARX_GLB_BAD_LEVEL_ZONE;
          ++source_vertex_count;
        }
        mapped[corner] = entry->second;
      }
      if (mapped[0] == mapped[1] || mapped[0] == mapped[2] || mapped[1] == mapped[2]) {
        ++degenerate_triangles;
        continue;
      }
      out.triangles.push_back(mapped);
    }
  }
  if (source_vertex_count != out.positions.size() || degenerate_triangles != 0) {
    log(ARX_LOG_DEBUG,
        "GLB -> Level: zone node {} '{}' welded {} source vertex/vertices to {}; discarded {} "
        "degenerate triangle(s)",
        node_index,
        name,
        source_vertex_count,
        out.positions.size(),
        degenerate_triangles);
  }
  return ARX_OK;
}

}  // namespace pistoris::glb_level::zone_internal
