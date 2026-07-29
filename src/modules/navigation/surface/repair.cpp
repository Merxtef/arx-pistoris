// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#include "arx_pistoris/arx_math.h"
#include "arx_pistoris/arx_math.hpp"
#include "arx_pistoris/pistoris_types.h"

#include "modules/geometry.h"
#include "modules/navigation.h"
#include "modules/navigation/surface/internal.h"
#include "modules/navigation/traversal.h"
#include "utils/log.h"
#include "utils/math/geometry.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <format>
#include <limits>
#include <map>
#include <optional>
#include <utility>
#include <vector>

namespace pistoris::navigation::surface {
namespace {

constexpr int kNavRepairMaxPasses = 8;

struct EdgeKey {
  std::uint32_t first = 0;
  std::uint32_t second = 0;

  friend bool operator<(const EdgeKey& a, const EdgeKey& b) {
    if (a.first != b.first) return a.first < b.first;
    return a.second < b.second;
  }
};

struct TriangleKey {
  std::array<std::uint32_t, 3> vertices{};

  friend bool operator<(const TriangleKey& a, const TriangleKey& b) { return a.vertices < b.vertices; }
};

struct BoundaryTopology {  // NOLINT(bugprone-exception-escape): MSVC debug std::map move allocates its sentinel
  std::vector<std::vector<std::uint32_t>> loops;
  std::map<TriangleKey, bool> triangle_keys;
};

EdgeKey edgeKey(std::uint32_t a, std::uint32_t b) {
  if (a > b) std::swap(a, b);
  return {a, b};
}

TriangleKey triangleKey(std::uint32_t a, std::uint32_t b, std::uint32_t c) {
  std::array<std::uint32_t, 3> vertices = {a, b, c};
  std::sort(vertices.begin(), vertices.end());
  return {vertices};
}

double signedLoopAreaXz(const NavSurface& surface, const std::vector<std::uint32_t>& loop) {
  double area = 0.0;
  for (std::size_t i = 0; i < loop.size(); ++i) {
    const ArxVector3& a = surface.vertices[loop[i]].position;
    const ArxVector3& b = surface.vertices[loop[(i + 1) % loop.size()]].position;
    area += static_cast<double>(a.x) * b.z - static_cast<double>(a.z) * b.x;
  }
  return area * 0.5;
}

bool boundaryEdgeVisited(const std::map<EdgeKey, bool>& visited, std::uint32_t a, std::uint32_t b) {
  auto it = visited.find(edgeKey(a, b));
  return it == visited.end() || it->second;
}

BoundaryTopology buildBoundaryTopology(const NavSurface& surface) {
  BoundaryTopology topology;
  std::map<EdgeKey, std::uint32_t> edge_counts;
  for (const NavSurfaceTriangle& triangle : surface.triangles) {
    std::uint32_t a = triangle.vertices[0];
    std::uint32_t b = triangle.vertices[1];
    std::uint32_t c = triangle.vertices[2];
    ++edge_counts[edgeKey(a, b)];
    ++edge_counts[edgeKey(b, c)];
    ++edge_counts[edgeKey(c, a)];
    topology.triangle_keys.emplace(triangleKey(a, b, c), true);
  }

  std::map<std::uint32_t, std::vector<std::uint32_t>> boundary_neighbors;
  std::map<EdgeKey, bool> visited_edges;
  for (const auto& [edge, count] : edge_counts) {
    if (count != 1) continue;
    boundary_neighbors[edge.first].push_back(edge.second);
    boundary_neighbors[edge.second].push_back(edge.first);
    visited_edges.emplace(edge, false);
  }

  for (auto& [vertex, neighbors] : boundary_neighbors) {
    (void)vertex;
    std::sort(neighbors.begin(), neighbors.end());
    neighbors.erase(std::unique(neighbors.begin(), neighbors.end()), neighbors.end());
  }

  for (auto& [start_edge, visited] : visited_edges) {
    if (visited) continue;
    std::uint32_t start = start_edge.first;
    std::uint32_t prev = start_edge.first;
    std::uint32_t cur = start_edge.second;
    std::vector<std::uint32_t> loop{start};
    visited = true;
    bool valid = true;
    for (;;) {
      loop.push_back(cur);
      auto neighbors_it = boundary_neighbors.find(cur);
      if (neighbors_it == boundary_neighbors.end() || neighbors_it->second.size() != 2) {
        valid = false;
        break;
      }

      std::uint32_t next = neighbors_it->second[0] == prev ? neighbors_it->second[1] : neighbors_it->second[0];
      if (next == start) {
        visited_edges[edgeKey(cur, next)] = true;
        break;
      }
      if (boundaryEdgeVisited(visited_edges, cur, next)) {
        valid = false;
        break;
      }

      visited_edges[edgeKey(cur, next)] = true;
      prev = cur;
      cur = next;
      if (loop.size() > visited_edges.size() + 1) {
        valid = false;
        break;
      }
    }

    if (valid && loop.size() >= 3) topology.loops.push_back(std::move(loop));
  }

  return topology;
}

bool validRepairShortcut(const geometry::SurfaceSupportIndex& index,
                         const geometry::SurfaceSupportIndex& geometry_support, const GeometryData& geometry,
                         const SurfaceSupportFilter& final_support_filter, const StaticAnchorTraversal& traversal,
                         const NavSurfaceGenOptions& options, const ArxVector3& a, const ArxVector3& b,
                         const ArxVector3& c) {
  ArxVector3 support_a = navigationSupportPosition(a, options.clearance);
  ArxVector3 support_b = navigationSupportPosition(b, options.clearance);
  ArxVector3 support_c = navigationSupportPosition(c, options.clearance);
  ArxVector3 delta = support_c - support_a;
  double xz_distance_squared = static_cast<double>(delta.x) * delta.x + static_cast<double>(delta.z) * delta.z;
  double xz_distance = std::sqrt(xz_distance_squared);
  if (xz_distance <= std::numeric_limits<float>::epsilon()) return false;
  if (xz_distance > static_cast<double>(options.radius) * 3.0) return false;
  if (std::abs(support_a.y - support_c.y) > std::abs(options.height) * 0.5f) return false;
  double dent_depth = math::distancePointSegment(math::xz(support_b), math::xz(support_a), math::xz(support_c));
  if (dent_depth <= std::numeric_limits<float>::epsilon()) return false;
  float offset = static_cast<float>(std::min(static_cast<double>(options.radius) * 0.25, dent_depth));

  std::size_t sample_count =
      std::clamp<std::size_t>(static_cast<std::size_t>(std::ceil(xz_distance / options.radius)), 1, 3);

  for (std::size_t i = 0; i < sample_count; ++i) {
    float t = static_cast<float>(i + 1) / static_cast<float>(sample_count + 1);
    float x = support_a.x + (support_c.x - support_a.x) * t;
    float y = support_a.y + (support_c.y - support_a.y) * t;
    float z = support_a.z + (support_c.z - support_a.z) * t;
    ArxVector3 inside = {support_b.x - x, 0.0f, support_b.z - z};
    float inside_len = static_cast<float>(math::length(inside));
    if (inside_len <= std::numeric_limits<float>::epsilon()) return false;
    x += inside.x / inside_len * offset;
    z += inside.z / inside_len * offset;
    if (!validateSupportPoint(index, geometry_support, geometry, final_support_filter, traversal, options, x, z, y))
      return false;
  }
  return true;
}

bool validRepairTriangle(const geometry::SurfaceSupportIndex& index,
                         const geometry::SurfaceSupportIndex& geometry_support, const GeometryData& geometry,
                         const SurfaceSupportFilter& final_support_filter, const StaticAnchorTraversal& traversal,
                         const NavSurfaceGenOptions& options, const NavSurface& surface,
                         const BoundaryTopology& topology, std::uint32_t a, std::uint32_t b, std::uint32_t c) {
  if (a == b || a == c || b == c) return false;
  if (topology.triangle_keys.find(triangleKey(a, b, c)) != topology.triangle_keys.end()) return false;
  const ArxVector3& pa = surface.vertices[a].position;
  const ArxVector3& pb = surface.vertices[b].position;
  const ArxVector3& pc = surface.vertices[c].position;
  if (std::abs(math::projectedAreaXz2(pa, pb, pc)) <= kSurfaceTriangleAreaEpsilon) return false;
  return validRepairShortcut(index, geometry_support, geometry, final_support_filter, traversal, options, pa, pb, pc);
}

}  // namespace

std::size_t repairNavSurfaceEdges(const geometry::SurfaceSupportIndex& index,
                                  const geometry::SurfaceSupportIndex& geometry_support, const GeometryData& geometry,
                                  const SurfaceSupportFilter& final_support_filter,
                                  const StaticAnchorTraversal& traversal, const NavSurfaceGenOptions& options,
                                  NavSurface& surface) {
  std::size_t total_added = 0;
  std::size_t total_considered = 0;
  for (int pass = 0; pass < kNavRepairMaxPasses; ++pass) {
    BoundaryTopology topology = buildBoundaryTopology(surface);
    std::vector<NavSurfaceTriangle> additions;
    for (const std::vector<std::uint32_t>& loop : topology.loops) {
      if (loop.size() < 4) continue;
      double loop_area = signedLoopAreaXz(surface, loop);
      if (std::abs(loop_area) <= kSurfaceTriangleAreaEpsilon) continue;

      for (std::size_t i = 0; i < loop.size(); ++i) {
        std::uint32_t a = loop[i];
        std::uint32_t b = loop[(i + 1) % loop.size()];
        std::uint32_t c = loop[(i + 2) % loop.size()];
        double turn = math::projectedAreaXz2(
            surface.vertices[a].position, surface.vertices[b].position, surface.vertices[c].position);
        if (turn * loop_area >= -kSurfaceTriangleAreaEpsilon) continue;
        ++total_considered;
        if (!validRepairTriangle(index,
                                 geometry_support,
                                 geometry,
                                 final_support_filter,
                                 traversal,
                                 options,
                                 surface,
                                 topology,
                                 a,
                                 b,
                                 c))
          continue;

        std::optional<NavSurfaceTriangle> triangle = makeNavigationTriangle(surface, a, b, c);
        if (!triangle.has_value()) continue;
        additions.push_back(*triangle);
        topology.triangle_keys.emplace(triangleKey(a, b, c), true);
      }
    }

    if (additions.empty()) break;
    total_added += additions.size();
    surface.triangles.insert(surface.triangles.end(), additions.begin(), additions.end());
  }

  log(ARX_LOG_DEBUG,
      std::format("Level navigation surface boundary repair: considered {} dent triangle(s), added {} triangle(s)",
                  total_considered,
                  total_added));
  return total_added;
}

}  // namespace pistoris::navigation::surface
