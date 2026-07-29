// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#include "arx_pistoris/arx_math.h"
#include "arx_pistoris/pistoris_types.h"

#include "internal.h"
#include "modules/scene.h"

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
#include <string_view>
#include <utility>
#include <vector>

namespace pistoris::glb_level::zone_internal {
namespace {

struct Column {  // NOLINT(bugprone-exception-escape): MSVC debug std::set move allocates its sentinel
  std::uint32_t top = 0;
  std::uint32_t bottom = 0;
  std::set<std::uint32_t> neighbours;
};

bool xzEqual(const ArxVector3& a, const ArxVector3& b) {
  return std::abs(a.x - b.x) <= kXzPairEpsilon && std::abs(a.z - b.z) <= kXzPairEpsilon;
}

std::pair<std::uint32_t, std::uint32_t> edge(std::uint32_t a, std::uint32_t b) { return std::minmax(a, b); }

float median(std::vector<float> values) {
  std::sort(values.begin(), values.end());
  const std::size_t middle = values.size() / 2;
  if ((values.size() & 1U) != 0) return values[middle];
  return (values[middle - 1] + values[middle]) * 0.5f;
}

}  // namespace

ArxReturnCode reconstruct(const Mesh& mesh, Zone& zone, float& top_y, float& bottom_y, float& top_movement,
                          float& bottom_movement, std::size_t node_index, std::string_view name) {
  if (mesh.positions.size() < 6 || mesh.triangles.empty()) {
    logFailure(node_index,
               name,
               std::format("has {} vertex/vertices and {} triangle(s), expected closed extruded zone mesh",
                           mesh.positions.size(),
                           mesh.triangles.size()));
    return ARX_GLB_BAD_LEVEL_ZONE;
  }

  std::set<std::pair<std::uint32_t, std::uint32_t>> mesh_edges;
  for (const auto& triangle : mesh.triangles) {
    mesh_edges.insert(edge(triangle[0], triangle[1]));
    mesh_edges.insert(edge(triangle[1], triangle[2]));
    mesh_edges.insert(edge(triangle[2], triangle[0]));
  }

  std::vector<std::optional<std::uint32_t>> pair_vertex(mesh.positions.size());
  for (const auto& [a, b] : mesh_edges) {
    const ArxVector3& first = mesh.positions[a];
    const ArxVector3& second = mesh.positions[b];
    if (!xzEqual(first, second) || std::abs(first.y - second.y) <= kXzPairEpsilon) continue;
    if (pair_vertex[a].has_value() || pair_vertex[b].has_value()) {
      logFailure(node_index, name, "has ambiguous top/bottom vertex pairing");
      return ARX_GLB_BAD_LEVEL_ZONE;
    }
    pair_vertex[a] = b;
    pair_vertex[b] = a;
  }
  const auto unpaired = std::ranges::find_if(pair_vertex, [](const auto& value) { return !value.has_value(); });
  if (unpaired != pair_vertex.end()) {
    logFailure(node_index,
               name,
               std::format("vertex {} has no connected top/bottom pair",
                           static_cast<std::size_t>(unpaired - pair_vertex.begin())));
    return ARX_GLB_BAD_LEVEL_ZONE;
  }

  std::vector<std::uint32_t> column_by_vertex(mesh.positions.size());
  std::vector<Column> columns;
  for (std::uint32_t vertex = 0; vertex < mesh.positions.size(); ++vertex) {
    const auto& paired = pair_vertex[vertex];
    if (!paired.has_value()) return ARX_GLB_BAD_LEVEL_ZONE;
    const std::uint32_t other = *paired;
    if (vertex > other) continue;
    Column column;
    if (mesh.positions[vertex].y < mesh.positions[other].y) {
      column.top = vertex;
      column.bottom = other;
    } else {
      column.top = other;
      column.bottom = vertex;
    }
    const std::uint32_t index = static_cast<std::uint32_t>(columns.size());
    column_by_vertex[vertex] = index;
    column_by_vertex[other] = index;
    columns.push_back(std::move(column));
  }
  if (columns.size() < 3) {
    logFailure(node_index, name, std::format("has {} column(s), expected at least 3", columns.size()));
    return ARX_GLB_BAD_LEVEL_ZONE;
  }

  std::map<std::pair<std::uint32_t, std::uint32_t>, std::uint32_t> top_cap_edges;
  std::map<std::pair<std::uint32_t, std::uint32_t>, std::uint32_t> bottom_cap_edges;
  std::map<std::pair<std::uint32_t, std::uint32_t>, std::uint32_t> side_triangles;
  std::size_t top_triangles = 0;
  std::size_t bottom_triangles = 0;

  for (const auto& triangle : mesh.triangles) {
    const std::array<std::uint32_t, 3> column = {
        column_by_vertex[triangle[0]], column_by_vertex[triangle[1]], column_by_vertex[triangle[2]]};
    bool all_top = true;
    bool all_bottom = true;
    for (std::size_t i = 0; i < 3; ++i) {
      all_top &= triangle[i] == columns[column[i]].top;
      all_bottom &= triangle[i] == columns[column[i]].bottom;
    }
    if (all_top || all_bottom) {
      if (column[0] == column[1] || column[0] == column[2] || column[1] == column[2]) {
        logFailure(node_index, name, "has cap triangle with duplicate zone columns");
        return ARX_GLB_BAD_LEVEL_ZONE;
      }
      auto& counts = all_top ? top_cap_edges : bottom_cap_edges;
      ++counts[edge(column[0], column[1])];
      ++counts[edge(column[1], column[2])];
      ++counts[edge(column[2], column[0])];
      if (all_top)
        ++top_triangles;
      else
        ++bottom_triangles;
      continue;
    }

    const std::set<std::uint32_t> unique(column.begin(), column.end());
    if (unique.size() != 2) {
      logFailure(node_index, name, "has side triangle spanning more than two zone columns");
      return ARX_GLB_BAD_LEVEL_ZONE;
    }
    auto iterator = unique.begin();
    const std::uint32_t a = *iterator++;
    const std::uint32_t b = *iterator;
    const std::set<std::uint32_t> vertices(triangle.begin(), triangle.end());
    const bool vertical_a = vertices.contains(columns[a].top) && vertices.contains(columns[a].bottom);
    const bool vertical_b = vertices.contains(columns[b].top) && vertices.contains(columns[b].bottom);
    if (vertical_a == vertical_b) {
      logFailure(node_index, name, "has invalid side triangle topology");
      return ARX_GLB_BAD_LEVEL_ZONE;
    }
    columns[a].neighbours.insert(b);
    columns[b].neighbours.insert(a);
    ++side_triangles[edge(a, b)];
  }

  if (top_triangles == 0 || bottom_triangles == 0) {
    logFailure(
        node_index,
        name,
        std::format("has {} top cap triangle(s) and {} bottom cap triangle(s)", top_triangles, bottom_triangles));
    return ARX_GLB_BAD_LEVEL_ZONE;
  }
  for (std::size_t i = 0; i < columns.size(); ++i) {
    if (columns[i].neighbours.size() != 2) {
      logFailure(node_index,
                 name,
                 std::format("column {} has {} side neighbour(s), expected 2", i, columns[i].neighbours.size()));
      return ARX_GLB_BAD_LEVEL_ZONE;
    }
  }

  std::vector<std::uint32_t> cycle;
  cycle.reserve(columns.size());
  std::uint32_t current = 0;
  std::uint32_t previous = std::numeric_limits<std::uint32_t>::max();
  do {
    cycle.push_back(current);
    const auto& neighbours = columns[current].neighbours;
    auto iterator = neighbours.begin();
    const std::uint32_t first = *iterator++;
    const std::uint32_t second = *iterator;
    const std::uint32_t next = previous == std::numeric_limits<std::uint32_t>::max()
                                   ? std::min(first, second)
                                   : (first == previous ? second : first);
    previous = current;
    current = next;
    if (cycle.size() > columns.size()) {
      logFailure(node_index, name, "has non-terminating perimeter cycle");
      return ARX_GLB_BAD_LEVEL_ZONE;
    }
  } while (current != cycle.front());
  if (cycle.size() != columns.size()) {
    logFailure(
        node_index, name, std::format("perimeter cycle covers {} of {} column(s)", cycle.size(), columns.size()));
    return ARX_GLB_BAD_LEVEL_ZONE;
  }

  for (std::size_t i = 0; i < cycle.size(); ++i) {
    const auto boundary = edge(cycle[i], cycle[(i + 1) % cycle.size()]);
    if (side_triangles[boundary] != 2 || top_cap_edges[boundary] != 1 || bottom_cap_edges[boundary] != 1) {
      logFailure(node_index,
                 name,
                 std::format("boundary {} has {} side, {} top, {} bottom triangle contribution(s)",
                             i,
                             side_triangles[boundary],
                             top_cap_edges[boundary],
                             bottom_cap_edges[boundary]));
      return ARX_GLB_BAD_LEVEL_ZONE;
    }
  }
  if (top_triangles != columns.size() - 2 || bottom_triangles != columns.size() - 2) {
    logFailure(node_index,
               name,
               std::format("has {} top and {} bottom cap triangle(s), expected {} each",
                           top_triangles,
                           bottom_triangles,
                           columns.size() - 2));
    return ARX_GLB_BAD_LEVEL_ZONE;
  }
  const auto valid_cap_edges = [&](const auto& counts) {
    for (const auto& [candidate, count] : counts) {
      const bool boundary = columns[candidate.first].neighbours.contains(candidate.second);
      if (count != (boundary ? 1U : 2U)) return false;
    }
    return true;
  };
  if (!valid_cap_edges(top_cap_edges) || !valid_cap_edges(bottom_cap_edges)) {
    logFailure(node_index, name, "has invalid cap triangulation");
    return ARX_GLB_BAD_LEVEL_ZONE;
  }

  std::vector<float> top_values;
  std::vector<float> bottom_values;
  top_values.reserve(columns.size());
  bottom_values.reserve(columns.size());
  for (const Column& column : columns) {
    top_values.push_back(mesh.positions[column.top].y);
    bottom_values.push_back(mesh.positions[column.bottom].y);
  }
  top_y = median(top_values);
  bottom_y = median(bottom_values);
  if (!std::isfinite(top_y) || !std::isfinite(bottom_y) || bottom_y - top_y <= kPlaneEpsilon) {
    logFailure(node_index, name, std::format("has invalid vertical span: top {}, bottom {}", top_y, bottom_y));
    return ARX_GLB_BAD_LEVEL_ZONE;
  }
  top_movement = 0.0f;
  bottom_movement = 0.0f;
  for (const Column& column : columns) {
    top_movement = std::max(top_movement, std::abs(mesh.positions[column.top].y - top_y));
    bottom_movement = std::max(bottom_movement, std::abs(mesh.positions[column.bottom].y - bottom_y));
  }

  zone.perimeter_xz.reserve(cycle.size());
  for (std::uint32_t index : cycle) {
    const ArxVector3& position = mesh.positions[columns[index].bottom];
    zone.perimeter_xz.push_back({position.x, position.z});
  }
  zone.reference_y = bottom_y;
  zone.height = bottom_y - top_y;
  zone.height_mode = ZoneHeightMode::kFinite;
  return ARX_OK;
}

}  // namespace pistoris::glb_level::zone_internal
