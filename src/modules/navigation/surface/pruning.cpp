// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#include "arx_pistoris/arx_math.h"
#include "arx_pistoris/arx_math.hpp"
#include "arx_pistoris/indices.h"
#include "arx_pistoris/pistoris_types.h"

#include "modules/geometry.h"
#include "modules/navigation.h"
#include "utils/log.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <format>
#include <limits>
#include <unordered_map>
#include <utility>
#include <vector>

namespace pistoris::navigation {
namespace {

struct DisjointSet {
  std::vector<std::uint32_t> parent;
  std::vector<std::uint32_t> rank;

  explicit DisjointSet(std::size_t count) : parent(count), rank(count, 0) {
    for (std::uint32_t i = 0; i < parent.size(); ++i) parent[i] = i;
  }

  std::uint32_t find(std::uint32_t value) {
    if (parent[value] != value) parent[value] = find(parent[value]);
    return parent[value];
  }

  void unite(std::uint32_t a, std::uint32_t b) {
    a = find(a);
    b = find(b);
    if (a == b) return;
    if (rank[a] < rank[b]) std::swap(a, b);
    parent[b] = a;
    if (rank[a] == rank[b]) ++rank[a];
  }
};

struct ComponentAnalysis {
  explicit ComponentAnalysis(std::size_t triangle_count)
      : components(triangle_count), area_by_component(triangle_count, 0.0), triangles_by_component(triangle_count, 0) {}

  DisjointSet components;
  std::vector<double> area_by_component;
  std::vector<std::uint32_t> triangles_by_component;
  double largest_area = 0.0;
  std::size_t component_count = 0;
};

std::uint64_t edgeKey(std::uint32_t first, std::uint32_t second) noexcept {
  if (first > second) std::swap(first, second);
  return (static_cast<std::uint64_t>(first) << 32U) | second;
}

double triangleArea(const NavSurface& surface, const NavSurfaceTriangle& triangle) {
  const ArxVector3& a = surface.vertices[triangle.vertices[0]].position;
  const ArxVector3& b = surface.vertices[triangle.vertices[1]].position;
  const ArxVector3& c = surface.vertices[triangle.vertices[2]].position;
  const Vec3<double> ab{static_cast<double>(b.x) - a.x, static_cast<double>(b.y) - a.y, static_cast<double>(b.z) - a.z};
  const Vec3<double> ac{static_cast<double>(c.x) - a.x, static_cast<double>(c.y) - a.y, static_cast<double>(c.z) - a.z};
  const Vec3<double> cross_product{ab.y * ac.z - ab.z * ac.y, ab.z * ac.x - ab.x * ac.z, ab.x * ac.y - ab.y * ac.x};
  return math::length(cross_product) * 0.5;
}

SurfaceDebugTriangle debugTriangle(const NavSurface& surface, const NavSurfaceTriangle& triangle) {
  return {{surface.vertices[triangle.vertices[0]].position,
           surface.vertices[triangle.vertices[1]].position,
           surface.vertices[triangle.vertices[2]].position}};
}

ComponentAnalysis analyzeComponents(const NavSurface& surface) {
  ComponentAnalysis analysis(surface.triangles.size());
  std::unordered_map<std::uint64_t, std::uint32_t> owner_by_edge;
  owner_by_edge.reserve(surface.triangles.size() * 3U);
  for (std::uint32_t triangle_index = 0; triangle_index < surface.triangles.size(); ++triangle_index) {
    const auto& vertices = surface.triangles[triangle_index].vertices;
    for (std::size_t edge = 0; edge < vertices.size(); ++edge) {
      std::uint64_t key = edgeKey(vertices[edge], vertices[(edge + 1U) % vertices.size()]);
      auto [owner, inserted] = owner_by_edge.emplace(key, triangle_index);
      if (!inserted) analysis.components.unite(triangle_index, owner->second);
    }
  }

  for (std::uint32_t triangle_index = 0; triangle_index < surface.triangles.size(); ++triangle_index) {
    std::uint32_t component = analysis.components.find(triangle_index);
    analysis.area_by_component[component] += triangleArea(surface, surface.triangles[triangle_index]);
    ++analysis.triangles_by_component[component];
  }
  for (std::size_t component = 0; component < analysis.triangles_by_component.size(); ++component) {
    if (analysis.triangles_by_component[component] == 0) continue;
    ++analysis.component_count;
    analysis.largest_area = std::max(analysis.largest_area, analysis.area_by_component[component]);
  }
  return analysis;
}

bool validOptions(const NavSurfacePruneOptions& options) noexcept {
  return std::isfinite(options.min_component_area_ratio) && options.min_component_area_ratio >= 0.0f &&
         options.min_component_area_ratio <= 1.0f && std::isfinite(options.min_component_area) &&
         options.min_component_area >= 0.0;
}

}  // namespace

std::size_t surfaceComponentCount(const NavSurface& surface) {
  if (validateSurface(surface) != Error::kNone) return 0;
  return analyzeComponents(surface).component_count;
}

Error pruneSurfaceComponents(NavSurface& surface, const NavSurfacePruneOptions& options,
                             NavSurfacePruneDiagnostics* diagnostics) {
  if (!validOptions(options)) return Error::kInvalidOptions;
  Error error = validateSurface(surface);
  if (error != Error::kNone) return error;
  if (diagnostics) *diagnostics = {};

  ComponentAnalysis analysis = analyzeComponents(surface);
  const double relative_threshold = analysis.largest_area * options.min_component_area_ratio;
  std::vector<std::uint8_t> keep_component(surface.triangles.size(), 0);
  std::size_t removed_components = 0;
  double removed_area = 0.0;
  for (std::size_t component = 0; component < analysis.triangles_by_component.size(); ++component) {
    if (analysis.triangles_by_component[component] == 0) continue;
    const double area = analysis.area_by_component[component];
    if (area >= relative_threshold && area >= options.min_component_area) {
      keep_component[component] = 1;
    } else {
      ++removed_components;
      removed_area += area;
    }
  }
  if (removed_components == 0) return Error::kNone;

  std::vector<NavSurfaceTriangle> kept_triangles;
  kept_triangles.reserve(surface.triangles.size());
  for (std::uint32_t triangle_index = 0; triangle_index < surface.triangles.size(); ++triangle_index) {
    if (keep_component[analysis.components.find(triangle_index)] != 0) {
      kept_triangles.push_back(surface.triangles[triangle_index]);
    } else if (diagnostics) {
      diagnostics->pruned.push_back(debugTriangle(surface, surface.triangles[triangle_index]));
    }
  }
  if (kept_triangles.empty()) return Error::kEmptyResult;

  std::vector<std::uint8_t> used_vertices(surface.vertices.size(), 0);
  for (const NavSurfaceTriangle& triangle : kept_triangles)
    for (NavSurfaceVertexIndex vertex : triangle.vertices) used_vertices[vertex] = 1;

  std::vector<NavSurfaceVertexIndex> remap(surface.vertices.size(), kInvalidNavSurfaceVertexIndex);
  std::vector<Vertex> kept_vertices;
  kept_vertices.reserve(surface.vertices.size());
  for (NavSurfaceVertexIndex vertex = 0; vertex < surface.vertices.size(); ++vertex) {
    if (used_vertices[vertex] == 0) continue;
    if (kept_vertices.size() >= static_cast<std::size_t>(std::numeric_limits<NavSurfaceVertexIndex>::max()))
      return Error::kTooManySurfaceVertices;
    remap[vertex] = static_cast<NavSurfaceVertexIndex>(kept_vertices.size());
    kept_vertices.push_back(surface.vertices[vertex]);
  }
  for (NavSurfaceTriangle& triangle : kept_triangles)
    for (NavSurfaceVertexIndex& vertex : triangle.vertices) vertex = remap[vertex];

  const std::size_t removed_triangles = surface.triangles.size() - kept_triangles.size();
  NavSurface kept_surface{std::move(kept_vertices), std::move(kept_triangles)};
  error = validateSurface(kept_surface);
  if (error != Error::kNone) return error;
  surface = std::move(kept_surface);
  log(ARX_LOG_WARN,
      std::format("Level navigation surface pruning removed {} component(s), {} triangle(s), area {:.3f}",
                  removed_components,
                  removed_triangles,
                  removed_area));
  return Error::kNone;
}

}  // namespace pistoris::navigation
