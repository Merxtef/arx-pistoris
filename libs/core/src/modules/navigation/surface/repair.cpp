// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#include "arx_pistoris/base/math.h"
#include "arx_pistoris/base/math.hpp"
#include "arx_pistoris/runtime/types.h"

#include "modules/geometry.h"
#include "modules/navigation.h"
#include "modules/navigation/surface/internal.h"
#include "modules/navigation/traversal.h"
#include "utils/log.h"
#include "utils/math/geometry_algorithms.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <format>
#include <functional>
#include <limits>
#include <numeric>
#include <optional>
#include <span>
#include <unordered_set>
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

  bool operator==(const TriangleKey& other) const = default;
};

struct TriangleKeyHash {
  std::size_t operator()(const TriangleKey& key) const noexcept {
    std::size_t seed = 0;
    for (std::uint32_t vertex : key.vertices)
      seed ^= std::hash<std::uint32_t>{}(vertex) + 0x9e3779b9U + (seed << 6U) + (seed >> 2U);
    return seed;
  }
};

struct BoundaryNeighbor {
  std::uint32_t vertex = 0;
  std::size_t edge = 0;
};

struct BoundaryTopology {
  std::vector<EdgeKey> all_edges;
  std::vector<EdgeKey> edges;
  std::vector<std::size_t> offsets;
  std::vector<std::size_t> write_offsets;
  std::vector<BoundaryNeighbor> neighbors;
  std::vector<std::uint8_t> visited;
  std::unordered_set<TriangleKey, TriangleKeyHash> triangle_keys;
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

std::span<const BoundaryNeighbor> boundaryNeighbors(const BoundaryTopology& topology, std::uint32_t vertex) {
  if (vertex + 1U >= topology.offsets.size()) return {};
  const std::size_t first = topology.offsets[vertex];
  return std::span<const BoundaryNeighbor>(topology.neighbors).subspan(first, topology.offsets[vertex + 1U] - first);
}

void buildBoundaryTopology(const NavSurface& surface, BoundaryTopology& topology) {
  topology.all_edges.clear();
  topology.edges.clear();
  topology.offsets.clear();
  topology.write_offsets.clear();
  topology.neighbors.clear();
  topology.visited.clear();
  topology.triangle_keys.clear();
  topology.all_edges.reserve(surface.triangles.size() * 3U);
  topology.edges.reserve(surface.triangles.size() * 3U);
  topology.triangle_keys.reserve(surface.triangles.size());
  for (const NavSurfaceTriangle& triangle : surface.triangles) {
    std::uint32_t a = triangle.vertices[0];
    std::uint32_t b = triangle.vertices[1];
    std::uint32_t c = triangle.vertices[2];
    topology.all_edges.push_back(edgeKey(a, b));
    topology.all_edges.push_back(edgeKey(b, c));
    topology.all_edges.push_back(edgeKey(c, a));
    topology.triangle_keys.emplace(triangleKey(a, b, c));
  }

  std::sort(topology.all_edges.begin(), topology.all_edges.end());
  for (std::size_t first = 0; first < topology.all_edges.size();) {
    std::size_t last = first + 1U;
    while (last < topology.all_edges.size() && !(topology.all_edges[first] < topology.all_edges[last]) &&
           !(topology.all_edges[last] < topology.all_edges[first]))
      ++last;
    if (last == first + 1U && topology.all_edges[first].first < surface.vertices.size() &&
        topology.all_edges[first].second < surface.vertices.size())
      topology.edges.push_back(topology.all_edges[first]);
    first = last;
  }

  topology.offsets.assign(surface.vertices.size() + 1U, 0);
  for (const EdgeKey& edge : topology.edges) {
    ++topology.offsets[edge.first + 1U];
    ++topology.offsets[edge.second + 1U];
  }
  std::partial_sum(topology.offsets.begin(), topology.offsets.end(), topology.offsets.begin());
  topology.neighbors.resize(topology.offsets.back());
  topology.write_offsets.assign(topology.offsets.begin(), topology.offsets.end() - 1);
  for (std::size_t edge_index = 0; edge_index < topology.edges.size(); ++edge_index) {
    const EdgeKey& edge = topology.edges[edge_index];
    topology.neighbors[topology.write_offsets[edge.first]++] = {edge.second, edge_index};
    topology.neighbors[topology.write_offsets[edge.second]++] = {edge.first, edge_index};
  }
  topology.visited.assign(topology.edges.size(), 0);
}

bool traceBoundaryLoop(BoundaryTopology& topology, std::size_t start_edge_index, std::vector<std::uint32_t>& loop) {
  loop.clear();
  if (start_edge_index >= topology.edges.size() || topology.visited[start_edge_index] != 0) return false;
  const EdgeKey& start_edge = topology.edges[start_edge_index];
  const std::uint32_t start = start_edge.first;
  std::uint32_t current = start_edge.second;
  std::size_t current_edge = start_edge_index;
  loop.push_back(start);
  topology.visited[current_edge] = 1;
  for (;;) {
    loop.push_back(current);
    const std::span<const BoundaryNeighbor> neighbors = boundaryNeighbors(topology, current);
    if (neighbors.size() != 2U) return false;
    const BoundaryNeighbor* next = nullptr;
    if (neighbors[0].edge == current_edge) {
      next = &neighbors[1];
    } else if (neighbors[1].edge == current_edge) {
      next = &neighbors[0];
    } else {
      return false;
    }
    if (next->vertex == start) {
      topology.visited[next->edge] = 1;
      return loop.size() >= 3U;
    }
    if (topology.visited[next->edge] != 0) return false;
    topology.visited[next->edge] = 1;
    current = next->vertex;
    current_edge = next->edge;
    if (loop.size() > topology.edges.size() + 1U) return false;
  }
}

bool validRepairShortcut(const geometry::SurfaceSupportIndex& index,
                         const geometry::SurfaceSupportIndex& geometry_support, const GeometryData& geometry,
                         const SurfaceSupportFilter& final_support_filter, StaticAnchorTraversal& traversal,
                         const NavSurfaceGenerationOptions& options, const ArxVector3& a, const ArxVector3& b,
                         const ArxVector3& c, std::vector<geometry::SurfaceSupportHit>& scratch) {
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
    if (!validateSupportPoint(
            index, geometry_support, geometry, final_support_filter, traversal, options, x, z, y, scratch))
      return false;
  }
  return true;
}

bool validRepairTriangle(const geometry::SurfaceSupportIndex& index,
                         const geometry::SurfaceSupportIndex& geometry_support, const GeometryData& geometry,
                         const SurfaceSupportFilter& final_support_filter, StaticAnchorTraversal& traversal,
                         const NavSurfaceGenerationOptions& options, const NavSurface& surface,
                         const BoundaryTopology& topology, std::uint32_t a, std::uint32_t b, std::uint32_t c,
                         std::vector<geometry::SurfaceSupportHit>& scratch) {
  if (a == b || a == c || b == c) return false;
  if (topology.triangle_keys.contains(triangleKey(a, b, c))) return false;
  const ArxVector3& pa = surface.vertices[a].position;
  const ArxVector3& pb = surface.vertices[b].position;
  const ArxVector3& pc = surface.vertices[c].position;
  if (std::abs(math::projectedAreaXz2(pa, pb, pc)) <= kSurfaceTriangleAreaEpsilon) return false;
  return validRepairShortcut(
      index, geometry_support, geometry, final_support_filter, traversal, options, pa, pb, pc, scratch);
}

}  // namespace

std::size_t repairNavSurfaceEdges(const geometry::SurfaceSupportIndex& index,
                                  const geometry::SurfaceSupportIndex& geometry_support, const GeometryData& geometry,
                                  const SurfaceSupportFilter& final_support_filter, StaticAnchorTraversal& traversal,
                                  const NavSurfaceGenerationOptions& options, NavSurface& surface) {
  std::size_t total_added = 0;
  std::size_t total_considered = 0;
  std::vector<geometry::SurfaceSupportHit> support_hits;
  std::vector<std::uint32_t> loop;
  BoundaryTopology topology;
  std::vector<NavSurfaceTriangle> additions;
  for (int pass = 0; pass < kNavRepairMaxPasses; ++pass) {
    buildBoundaryTopology(surface, topology);
    additions.clear();
    for (std::size_t edge = 0; edge < topology.edges.size(); ++edge) {
      if (!traceBoundaryLoop(topology, edge, loop)) continue;
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
                                 c,
                                 support_hits))
          continue;

        std::optional<NavSurfaceTriangle> triangle = makeNavigationTriangle(surface, a, b, c);
        if (!triangle.has_value()) continue;
        additions.push_back(*triangle);
        topology.triangle_keys.emplace(triangleKey(a, b, c));
      }
    }

    if (additions.empty()) break;
    total_added += additions.size();
    surface.triangles.insert(surface.triangles.end(), additions.begin(), additions.end());
  }

  log(ARX_LOG_DEBUG,
      "Navigation surface boundary repair: considered {} dent triangle(s), added {} triangle(s)",
      total_considered,
      total_added);
  return total_added;
}

}  // namespace pistoris::navigation::surface
