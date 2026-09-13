// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#include "modules/navigation.h"

#include "arx_pistoris/base/indices.h"
#include "arx_pistoris/base/math.h"
#include "arx_pistoris/base/status.h"
#include "arx_pistoris/runtime/types.h"

#include "external/glb/accessor.h"
#include "external/glb/container.h"
#include "external/glb/level/coordinates.h"
#include "external/glb/level/import/internal.h"
#include "external/glb/primitive_indices.h"
#include "modules/geometry.h"
#include "utils/log.h"
#include "utils/math/mat4.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string_view>
#include <utility>
#include <vector>

namespace pistoris::glb_level_import {

using namespace detail;

ArxReturnCode importNavSurface(const Asset& asset, const cgltf_node& node, std::size_t node_index,
                               const math::Mat4& world, const ImportUnits& units, NavSurface& out) {
  const std::string_view name = nodeName(node);
  if (node.mesh == nullptr || node.mesh->primitives_count == 0 || node.camera != nullptr || node.light != nullptr ||
      node.extensions_count != 0 || node.mesh->extensions_count != 0 || node.has_mesh_gpu_instancing ||
      node.skin != nullptr) {
    log(ARX_LOG_DEBUG, "GLB -> Level object failure: nav surface node {} '{}' has invalid payload", node_index, name);
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
    if (!vertices_by_position.tryAdd(index, position)) return kInvalidNavSurfaceVertexIndex;
    return index;
  };
  glb::AccessorCache accessors(asset, node.mesh->primitives_count * 2U);

  for (std::size_t primitive_index = 0; primitive_index < node.mesh->primitives_count; ++primitive_index) {
    const cgltf_primitive& primitive = node.mesh->primitives[primitive_index];
    if (primitive.type != cgltf_primitive_type_triangles || primitive.extensions_count != 0 ||
        primitive.targets_count != 0 || primitive.has_draco_mesh_compression) {
      log(ARX_LOG_DEBUG,
          "GLB -> Level object failure: nav surface node {} '{}' primitive {} is unsupported",
          node_index,
          name,
          primitive_index);
      return ARX_GLB_BAD_LEVEL_NAV_SURFACE;
    }
    const cgltf_accessor* position_source = cgltf_find_accessor(&primitive, cgltf_attribute_type_position, 0);
    if (position_source == nullptr) {
      log(ARX_LOG_DEBUG, "GLB -> Level object failure: nav surface node {} '{}' lacks POSITION", node_index, name);
      return ARX_GLB_BAD_LEVEL_NAV_SURFACE;
    }
    const AccessorView* positions = nullptr;
    ArxReturnCode rc = accessors.get(position_source, positions);
    if (rc != ARX_OK) return rc;
    rc = validatePositionAccessor(*positions);
    if (rc != ARX_OK) return rc;

    PrimitiveIndices order;
    rc = glb::readPrimitiveIndices(
        accessors, primitive, positions->count, false, math::linearDeterminant(world) < 0.0, order);
    if (rc == ARX_GLB_BAD_FORMAT) return ARX_GLB_BAD_LEVEL_NAV_SURFACE;
    if (rc != ARX_OK) return rc;
    if (order.empty() || order.size() % 3 != 0) {
      log(ARX_LOG_DEBUG,
          "GLB -> Level object failure: nav surface node {} '{}' has invalid index count",
          node_index,
          name);
      return ARX_GLB_BAD_LEVEL_NAV_SURFACE;
    }
    for (std::size_t triangle = 0; triangle < order.size(); triangle += 3) {
      std::array<std::uint32_t, 3> mapped{};
      std::array<ArxVector3, 3> triangle_positions{};
      for (std::size_t corner = 0; corner < mapped.size(); ++corner) {
        std::uint32_t source_index = order[triangle + corner];
        if (source_index >= positions->count) return ARX_GLB_BAD_FORMAT;
        GlbVec3 source_position = readVec3(*positions, source_index);
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
            "GLB -> Level object failure: nav surface node {} '{}' has degenerate triangle",
            node_index,
            name);
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

}  // namespace pistoris::glb_level_import
