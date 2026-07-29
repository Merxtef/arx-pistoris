// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#include "arx_pistoris/arx_math.h"
#include "arx_pistoris/pistoris_types.h"

#include "external/glb/accessor.h"
#include "external/glb/container.h"
#include "external/glb/level/coordinates.h"
#include "internal.h"
#include "utils/log.h"
#include "utils/math/mat4.h"

#include <cmath>
#include <cstddef>
#include <cstdint>
#include <format>
#include <limits>
#include <map>
#include <optional>
#include <string_view>
#include <utility>
#include <vector>

namespace pistoris::glb_level::zone_internal {
namespace {

bool samePosition(const ArxVector3& a, const ArxVector3& b) {
  return std::abs(a.x - b.x) <= kVertexWeldEpsilon && std::abs(a.y - b.y) <= kVertexWeldEpsilon &&
         std::abs(a.z - b.z) <= kVertexWeldEpsilon;
}

std::uint32_t weldVertex(const ArxVector3& position, Mesh& mesh) {
  for (std::uint32_t i = 0; i < mesh.positions.size(); ++i) {
    if (samePosition(mesh.positions[i], position)) return i;
  }
  if (mesh.positions.size() >= std::numeric_limits<std::uint32_t>::max())
    return std::numeric_limits<std::uint32_t>::max();
  const std::uint32_t index = static_cast<std::uint32_t>(mesh.positions.size());
  mesh.positions.push_back(position);
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

  std::map<std::pair<const cgltf_accessor*, std::uint32_t>, std::uint32_t> vertex_by_source;
  std::size_t source_vertex_count = 0;
  std::size_t degenerate_triangles = 0;
  for (std::size_t primitive_index = 0; primitive_index < node.mesh->primitives_count; ++primitive_index) {
    const cgltf_primitive& primitive = node.mesh->primitives[primitive_index];
    if (primitive.type != cgltf_primitive_type_triangles || primitive.indices == nullptr ||
        primitive.extensions_count != 0 || primitive.targets_count != 0 || primitive.has_draco_mesh_compression) {
      logFailure(
          node_index,
          name,
          std::format("primitive {} must be indexed triangle geometry without extensions/targets", primitive_index));
      return ARX_GLB_BAD_LEVEL_ZONE;
    }
    const cgltf_accessor* position_source = cgltf_find_accessor(&primitive, cgltf_attribute_type_position, 0);
    if (position_source == nullptr) {
      logFailure(node_index, name, std::format("primitive {} lacks POSITION", primitive_index));
      return ARX_GLB_BAD_LEVEL_ZONE;
    }
    glb::AccessorView positions;
    ArxReturnCode rc = glb::getAccessor(asset, position_source, positions);
    if (rc != ARX_OK) return rc;
    rc = glb::validatePositionAccessor(positions);
    if (rc != ARX_OK) return rc;
    glb::AccessorView indices;
    rc = glb::getAccessor(asset, primitive.indices, indices);
    if (rc != ARX_OK) return rc;
    rc = glb::validateIndexAccessor(indices);
    if (rc != ARX_OK) return rc;
    if (indices.indices.empty() || indices.indices.size() % 3 != 0) {
      logFailure(node_index,
                 name,
                 std::format("primitive {} has {} index/indexes, expected non-empty multiple of 3",
                             primitive_index,
                             indices.indices.size()));
      return ARX_GLB_BAD_LEVEL_ZONE;
    }

    std::vector<std::uint32_t> mapped;
    mapped.reserve(indices.indices.size());
    for (std::uint32_t source_index : indices.indices) {
      if (source_index >= positions.count) {
        logFailure(node_index,
                   name,
                   std::format("primitive {} index {} is outside {} POSITION value(s)",
                               primitive_index,
                               source_index,
                               positions.count));
        return ARX_GLB_BAD_LEVEL_ZONE;
      }
      const auto key = std::pair{position_source, source_index};
      auto [entry, inserted] = vertex_by_source.emplace(key, 0);
      if (inserted) {
        const glb::Vec3 source = glb::readVec3(positions, source_index);
        ArxVector3 position = math::xformPoint(world, {source.x, source.y, source.z});
        const std::optional<ArxVector3> converted = toArxPoint(position, units);
        if (!converted) return ARX_GLB_BAD_FORMAT;
        position = *converted;
        entry->second = weldVertex(position, out);
        if (entry->second == std::numeric_limits<std::uint32_t>::max()) return ARX_GLB_BAD_LEVEL_ZONE;
        ++source_vertex_count;
      }
      mapped.push_back(entry->second);
    }
    for (std::size_t i = 0; i < mapped.size(); i += 3) {
      if (mapped[i] == mapped[i + 1] || mapped[i] == mapped[i + 2] || mapped[i + 1] == mapped[i + 2]) {
        ++degenerate_triangles;
        continue;
      }
      out.triangles.push_back({mapped[i], mapped[i + 1], mapped[i + 2]});
    }
  }
  if (source_vertex_count != out.positions.size() || degenerate_triangles != 0) {
    log(ARX_LOG_DEBUG,
        std::format("GLB -> Level: zone node {} '{}' welded {} source vertex/vertices to {}; discarded {} "
                    "degenerate triangle(s)",
                    node_index,
                    name,
                    source_vertex_count,
                    out.positions.size(),
                    degenerate_triangles));
  }
  return ARX_OK;
}

}  // namespace pistoris::glb_level::zone_internal
