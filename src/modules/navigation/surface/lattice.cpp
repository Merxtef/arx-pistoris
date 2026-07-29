// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#include "arx_pistoris/arx_math.h"
#include "arx_pistoris/arx_math.hpp"
#include "arx_pistoris/indices.h"
#include "arx_pistoris/pistoris_types.h"

#include "modules/geometry.h"
#include "modules/navigation.h"
#include "modules/navigation/internal.h"
#include "modules/navigation/surface/internal.h"
#include "modules/navigation/traversal.h"
#include "utils/log.h"
#include "utils/spatial/grid.h"

#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <format>
#include <map>
#include <optional>
#include <vector>

namespace pistoris::navigation::surface {
namespace {

constexpr float kNavMergeDistance = 1.0f;
constexpr float kNavSqrt3Over2 = 0.8660254037844386f;

struct LatticeKey {
  int row = 0;
  int col = 0;

  friend bool operator<(const LatticeKey& a, const LatticeKey& b) {
    if (a.row != b.row) return a.row < b.row;
    return a.col < b.col;
  }
};

struct NavSample {
  LatticeKey key{};
  std::uint32_t vertex = 0;
  ArxVector3 support{};
};

struct NavLatticeDiagnostics {
  std::size_t key_triangles = 0;
  std::size_t missing_key = 0;
  std::size_t candidate_triangles = 0;
  std::size_t duplicate_vertex = 0;
  std::size_t edge_height = 0;
  std::size_t degenerate = 0;
  std::size_t added = 0;
};

std::optional<NavSample> addNavigationSample(
    const StaticAnchorTraversal& traversal, const geometry::SurfaceSupportIndex& geometry_support,
    const GeometryData& geometry, const SurfaceSupportFilter& final_support_filter, const NavSurfaceGenOptions& options,
    const geometry::SurfaceSupportHit& hit, const LatticeKey& key, NavSurface& surface, std::vector<NavSample>& samples,
    std::map<LatticeKey, std::vector<std::uint32_t>>& by_key) {
  if (!usableSurfaceSupport(geometry_support, geometry, final_support_filter, traversal, hit.position, options))
    return std::nullopt;

  ArxVector3 vertex_position = navigationVertexPosition(hit.position, options.clearance);
  std::uint32_t vertex = kInvalidNavSurfaceVertexIndex;
  for (std::uint32_t i = 0; i < surface.vertices.size(); ++i) {
    if (math::lengthSquared(surface.vertices[i].position - vertex_position) <= kNavMergeDistance * kNavMergeDistance) {
      vertex = i;
      break;
    }
  }
  if (vertex == kInvalidNavSurfaceVertexIndex) {
    vertex = static_cast<std::uint32_t>(surface.vertices.size());
    surface.vertices.push_back({vertex_position});
  }

  for (std::uint32_t sample_index : by_key[key]) {
    if (samples[sample_index].vertex == vertex) return std::nullopt;
  }

  std::uint32_t sample_index = static_cast<std::uint32_t>(samples.size());
  samples.push_back({key, vertex, hit.position});
  by_key[key].push_back(sample_index);
  return samples.back();
}

void addNavigationSamplesAt(const geometry::SurfaceSupportIndex& index, const StaticAnchorTraversal& traversal,
                            const geometry::SurfaceSupportIndex& geometry_support, const GeometryData& geometry,
                            const SurfaceSupportFilter& final_support_filter, const NavSurfaceGenOptions& options,
                            const LatticeKey& key, float x, float z, NavSurface& surface,
                            std::vector<NavSample>& samples, std::map<LatticeKey, std::vector<std::uint32_t>>& by_key) {
  std::size_t before = by_key[key].size();
  for (const ArxVector3& offset : navigationProbeOffsets(options.radius)) {
    std::vector<geometry::SurfaceSupportHit> hits = index.hitsAt(x + offset.x, z + offset.z);
    geometry::mergeSurfaceSupportHits(hits);
    for (const geometry::SurfaceSupportHit& hit : hits) {
      addNavigationSample(
          traversal, geometry_support, geometry, final_support_filter, options, hit, key, surface, samples, by_key);
    }
    if (by_key[key].size() != before) return;
  }
}

bool navigationHeightDeltaWithinCylinder(const NavSurfaceGenOptions& options, const NavSample& a, const NavSample& b) {
  return std::abs(a.support.y - b.support.y) <= std::abs(options.height);
}

void addLatticeTrianglesForKeys(const NavSurfaceGenOptions& options, const std::vector<NavSample>& samples,
                                const std::map<LatticeKey, std::vector<std::uint32_t>>& by_key,
                                const std::array<LatticeKey, 3>& keys, NavSurface& surface,
                                NavLatticeDiagnostics& diagnostics) {
  ++diagnostics.key_triangles;
  auto ia = by_key.find(keys[0]);
  auto ib = by_key.find(keys[1]);
  auto ic = by_key.find(keys[2]);
  if (ia == by_key.end() || ib == by_key.end() || ic == by_key.end()) {
    ++diagnostics.missing_key;
    return;
  }

  for (std::uint32_t a : ia->second) {
    for (std::uint32_t b : ib->second) {
      for (std::uint32_t c : ic->second) {
        ++diagnostics.candidate_triangles;
        std::uint32_t va = samples[a].vertex;
        std::uint32_t vb = samples[b].vertex;
        std::uint32_t vc = samples[c].vertex;
        if (va == vb || va == vc || vb == vc) {
          ++diagnostics.duplicate_vertex;
          continue;
        }

        if (!navigationHeightDeltaWithinCylinder(options, samples[a], samples[b]) ||
            !navigationHeightDeltaWithinCylinder(options, samples[b], samples[c]) ||
            !navigationHeightDeltaWithinCylinder(options, samples[a], samples[c])) {
          ++diagnostics.edge_height;
          continue;
        }
        if (!appendNavigationTriangle(surface, va, vb, vc)) {
          ++diagnostics.degenerate;
          continue;
        }
        ++diagnostics.added;
      }
    }
  }
}

}  // namespace

void buildSurfaceLattice(const ArxAabb& support_bounds, const geometry::SurfaceSupportIndex& support_index,
                         const geometry::SurfaceSupportIndex& geometry_support, const GeometryData& geometry,
                         const SurfaceSupportFilter& final_support_filter, const StaticAnchorTraversal& traversal,
                         const NavSurfaceGenOptions& options, NavSurface& surface) {
  std::vector<NavSample> samples;
  std::map<LatticeKey, std::vector<std::uint32_t>> by_key;

  const float sample_spacing = options.radius;
  const float row_spacing = sample_spacing * kNavSqrt3Over2;
  int row = 0;
  // The lattice must preserve its established floating-point sampling sequence
  // NOLINTBEGIN(bugprone-float-loop-counter)
  for (float z = spatial::firstCellCenter(support_bounds.min.z, row_spacing); z <= support_bounds.max.z;
       z += row_spacing, ++row) {
    float x_offset = (row % 2 == 0) ? 0.0f : sample_spacing * 0.5f;
    int col = 0;
    for (float x = spatial::firstCellCenter(support_bounds.min.x, sample_spacing) + x_offset; x <= support_bounds.max.x;
         x += sample_spacing, ++col) {
      addNavigationSamplesAt(support_index,
                             traversal,
                             geometry_support,
                             geometry,
                             final_support_filter,
                             options,
                             {row, col},
                             x,
                             z,
                             surface,
                             samples,
                             by_key);
    }
  }
  // NOLINTEND(bugprone-float-loop-counter)

  NavLatticeDiagnostics lattice_diagnostics;
  for (const auto& [key, sample_indices] : by_key) {
    (void)sample_indices;
    if (key.row % 2 == 0) {
      addLatticeTrianglesForKeys(options,
                                 samples,
                                 by_key,
                                 {key, {key.row, key.col + 1}, {key.row + 1, key.col}},
                                 surface,
                                 lattice_diagnostics);
      addLatticeTrianglesForKeys(options,
                                 samples,
                                 by_key,
                                 {key, {key.row + 1, key.col}, {key.row + 1, key.col - 1}},
                                 surface,
                                 lattice_diagnostics);
    } else {
      addLatticeTrianglesForKeys(options,
                                 samples,
                                 by_key,
                                 {key, {key.row, key.col + 1}, {key.row + 1, key.col + 1}},
                                 surface,
                                 lattice_diagnostics);
      addLatticeTrianglesForKeys(options,
                                 samples,
                                 by_key,
                                 {key, {key.row + 1, key.col}, {key.row + 1, key.col + 1}},
                                 surface,
                                 lattice_diagnostics);
    }
  }
  log(ARX_LOG_DEBUG,
      std::format("Level navigation surface lattice triangles: key_triangles={}, missing_key={}, candidates={}, "
                  "added={}, duplicate_vertex={}, edge_height={}, degenerate={}",
                  lattice_diagnostics.key_triangles,
                  lattice_diagnostics.missing_key,
                  lattice_diagnostics.candidate_triangles,
                  lattice_diagnostics.added,
                  lattice_diagnostics.duplicate_vertex,
                  lattice_diagnostics.edge_height,
                  lattice_diagnostics.degenerate));
}

}  // namespace pistoris::navigation::surface
