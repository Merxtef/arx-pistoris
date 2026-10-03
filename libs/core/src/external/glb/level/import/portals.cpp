// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#include "arx_pistoris/base/math.h"
#include "arx_pistoris/base/status.h"
#include "arx_pistoris/runtime/types.h"

#include "external/glb/accessor.h"
#include "external/glb/container.h"
#include "external/glb/level/coordinates.h"
#include "external/glb/level/import/internal.h"
#include "external/glb/primitive_indices.h"
#include "modules/geometry.h"
#include "modules/rooms.h"
#include "utils/identifier.h"
#include "utils/log.h"
#include "utils/math/mat4.h"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <map>
#include <optional>
#include <set>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace pistoris::glb_level_import {

using namespace detail;

namespace {

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

}  // namespace

ArxReturnCode importPortal(const Asset& asset, const cgltf_node& node, const math::Mat4& world,
                           const std::map<std::string, std::uint32_t>& rooms_by_name, ImportWarnings& diagnostics,
                           const ImportUnits& units, Portal& out) {
  const std::string_view name = nodeName(node);
  std::optional<ParsedPortalName> parsed = parsePortalNodeName(name);
  if (!parsed.has_value()) {
    log(ARX_LOG_DEBUG, "GLB -> Level object failure: portal '{}' has invalid name", name);
    return ARX_GLB_BAD_LEVEL_PORTAL;
  }
  const IdentifierNormalization room_1_name = normalizeIdentifier(parsed->room_1);
  const IdentifierNormalization room_2_name = normalizeIdentifier(parsed->room_2);

  auto room_1 = rooms_by_name.find(room_1_name.value);
  auto room_2 = rooms_by_name.find(room_2_name.value);
  if (room_1 == rooms_by_name.end() || room_2 == rooms_by_name.end() || room_1->second == room_2->second) {
    log(ARX_LOG_DEBUG,
        "GLB -> Level object failure: portal '{}' references invalid rooms '{}' and '{}'",
        name,
        parsed->room_1,
        parsed->room_2);
    return ARX_GLB_BAD_LEVEL_PORTAL;
  }
  if (node.mesh == nullptr || node.mesh->primitives_count != 1) {
    log(ARX_LOG_DEBUG, "GLB -> Level object failure: portal '{}' must have exactly one mesh primitive", name);
    return ARX_GLB_BAD_LEVEL_PORTAL;
  }
  const cgltf_primitive& primitive = node.mesh->primitives[0];
  if (primitive.type != cgltf_primitive_type_triangles || primitive.indices == nullptr) {
    log(ARX_LOG_DEBUG, "GLB -> Level object failure: portal '{}' must be indexed triangle geometry", name);
    return ARX_GLB_BAD_LEVEL_PORTAL;
  }
  if (node.extensions_count != 0 || node.mesh->extensions_count != 0 || primitive.extensions_count != 0)
    return ARX_GLB_UNSUPPORTED_FEATURE;
  if (node.has_mesh_gpu_instancing || primitive.targets_count != 0 || primitive.has_draco_mesh_compression)
    return ARX_GLB_UNSUPPORTED_FEATURE;
  if (hasSkinning(node, primitive)) diagnostics.ignored_skinning = true;

  const cgltf_accessor* position_source = cgltf_find_accessor(&primitive, cgltf_attribute_type_position, 0);
  if (position_source == nullptr) {
    log(ARX_LOG_DEBUG, "GLB -> Level object failure: portal '{}' lacks POSITION", name);
    return ARX_GLB_BAD_LEVEL_PORTAL;
  }
  glb::AccessorCache accessors(asset, 2U);
  const AccessorView* positions = nullptr;
  ArxReturnCode rc = accessors.get(position_source, positions);
  if (rc != ARX_OK) return rc;
  rc = validatePositionAccessor(*positions);
  if (rc != ARX_OK) return rc;

  PrimitiveIndices order;
  rc = glb::readPrimitiveIndices(
      accessors, primitive, positions->count, true, math::linearDeterminant(world) < 0.0, order);
  if (rc != ARX_OK) return rc == ARX_GLB_BAD_FORMAT ? ARX_GLB_BAD_LEVEL_INDEX_ACCESSOR : rc;
  if (order.size() != 3 && order.size() != 6) {
    log(ARX_LOG_DEBUG, "GLB -> Level object failure: portal '{}' has {} indices, expected 3 or 6", name, order.size());
    return ARX_GLB_BAD_LEVEL_PORTAL;
  }
  for (std::size_t order_index = 0; order_index < order.size(); ++order_index) {
    const std::uint32_t index = order[order_index];
    if (index >= positions->count) {
      log(ARX_LOG_DEBUG, "GLB -> Level object failure: portal '{}' index {} is out of range", name, index);
      return ARX_GLB_BAD_LEVEL_PORTAL;
    }
  }

  geometry::PositionIndex position_index(kLevelGlbEpsilon, geometry::PositionWeldMetric::kEuclidean);
  std::map<std::uint32_t, std::uint32_t> welded_by_source;
  std::vector<ArxVector3> welded_positions;
  std::vector<std::uint32_t> welded_order;
  welded_positions.reserve(order.size());
  welded_order.reserve(order.size());
  for (std::size_t order_index = 0; order_index < order.size(); ++order_index) {
    const std::uint32_t source_index = order[order_index];
    auto source = welded_by_source.find(source_index);
    if (source == welded_by_source.end()) {
      GlbVec3 encoded = readVec3(*positions, source_index);
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
        if (!position_index.tryAdd(welded_index, position)) return ARX_GLB_BAD_LEVEL_PORTAL;
      }
      source = welded_by_source.emplace(source_index, welded_index).first;
    }
    welded_order.push_back(source->second);
  }
  if (welded_by_source.size() != welded_positions.size()) {
    log(ARX_LOG_DEBUG,
        "GLB -> Level: portal '{}' welded {} referenced source vertices to {} positions",
        name,
        welded_by_source.size(),
        welded_positions.size());
  }

  std::array<std::uint32_t, 4> perimeter{};
  if (welded_positions.size() == 3) {
    if (order.size() != 3) {
      log(ARX_LOG_DEBUG,
          "GLB -> Level object failure: portal '{}' welded to 3 positions but has {} indices",
          name,
          order.size());
      return ARX_GLB_BAD_LEVEL_PORTAL;
    }
    std::set<std::uint32_t> unique(welded_order.begin(), welded_order.end());
    if (unique.size() != 3) {
      log(ARX_LOG_DEBUG, "GLB -> Level object failure: triangle portal '{}' has duplicate welded indices", name);
      return ARX_GLB_BAD_LEVEL_PORTAL;
    }
    perimeter = {welded_order[0], welded_order[1], welded_order[2], 0};
    out.shape = PortalShape::kTriangle;
  } else if (welded_positions.size() == 4) {
    if (!canonicalPortalQuad(welded_order, perimeter)) {
      log(ARX_LOG_DEBUG,
          "GLB -> Level object failure: quad portal '{}' does not form two consistently oriented "
          "triangles after position welding",
          name);
      return ARX_GLB_BAD_LEVEL_PORTAL;
    }
    out.shape = PortalShape::kQuad;
  } else {
    log(ARX_LOG_DEBUG,
        "GLB -> Level object failure: portal '{}' has {} referenced source vertices and {} welded "
        "positions, expected 3 or 4 welded positions",
        name,
        welded_by_source.size(),
        welded_positions.size());
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
        "GLB -> Level object failure: portal '{}' failed geometry validation ({})",
        name,
        static_cast<int>(portal_validation));
    return ARX_GLB_BAD_LEVEL_PORTAL;
  }
  return ARX_OK;
}

}  // namespace pistoris::glb_level_import
