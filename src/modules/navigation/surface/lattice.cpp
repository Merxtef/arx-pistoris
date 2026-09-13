// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#include "arx_pistoris/base/indices.h"
#include "arx_pistoris/base/math.h"
#include "arx_pistoris/base/math.hpp"
#include "arx_pistoris/runtime/types.h"

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
#include <optional>
#include <span>
#include <vector>

namespace pistoris::navigation::surface {
namespace {

constexpr float kNavMergeDistance = 1.0f;
constexpr float kNavSqrt3Over2 = 0.8660254037844386f;

struct LatticeKey {
  int row = 0;
  int col = 0;
};

struct NavSample {
  std::uint32_t vertex = 0;
  ArxVector3 support{};
};

struct LatticeCell {
  std::size_t first_sample = 0;
  std::size_t sample_count = 0;
};

struct LatticeRow {
  std::size_t first_cell = 0;
  std::size_t cell_count = 0;
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

bool tryAddNavigationSample(StaticAnchorTraversal& traversal, const geometry::SurfaceSupportIndex& geometry_support,
                            const GeometryData& geometry, const SurfaceSupportFilter& final_support_filter,
                            const NavSurfaceGenerationOptions& options, const geometry::SurfaceSupportHit& hit,
                            NavSurface& surface, geometry::PositionIndex& vertex_index, std::vector<NavSample>& samples,
                            std::size_t first_sample) {
  if (!usableSurfaceSupport(geometry_support, geometry, final_support_filter, traversal, hit.position, options))
    return false;

  ArxVector3 vertex_position = navigationVertexPosition(hit.position, options.clearance);
  std::optional<std::uint32_t> existing = vertex_index.find(vertex_position);
  std::uint32_t vertex = existing.value_or(kInvalidNavSurfaceVertexIndex);
  if (!existing.has_value()) {
    if (surface.vertices.size() >= static_cast<std::size_t>(kInvalidNavSurfaceVertexIndex)) return false;
    vertex = static_cast<std::uint32_t>(surface.vertices.size());
    surface.vertices.push_back({vertex_position});
    if (!vertex_index.tryAdd(vertex, vertex_position)) {
      surface.vertices.pop_back();
      return false;
    }
  }

  for (std::size_t sample = first_sample; sample < samples.size(); ++sample)
    if (samples[sample].vertex == vertex) return false;

  samples.push_back({vertex, hit.position});
  return true;
}

LatticeCell addNavigationSamplesAt(const geometry::SurfaceSupportIndex& index, StaticAnchorTraversal& traversal,
                                   const geometry::SurfaceSupportIndex& geometry_support, const GeometryData& geometry,
                                   const SurfaceSupportFilter& final_support_filter,
                                   const NavSurfaceGenerationOptions& options, float x, float z, NavSurface& surface,
                                   geometry::PositionIndex& vertex_index, std::vector<NavSample>& samples,
                                   std::vector<geometry::SurfaceSupportHit>& hits) {
  const std::size_t first_sample = samples.size();
  for (const ArxVector3& offset : navigationProbeOffsets(options.radius)) {
    index.findHitsAt(hits, x + offset.x, z + offset.z);
    geometry::mergeSortedSurfaceSupportHits(hits);
    for (const geometry::SurfaceSupportHit& hit : hits) {
      tryAddNavigationSample(traversal,
                             geometry_support,
                             geometry,
                             final_support_filter,
                             options,
                             hit,
                             surface,
                             vertex_index,
                             samples,
                             first_sample);
    }
    if (samples.size() != first_sample) break;
  }
  return {first_sample, samples.size() - first_sample};
}

bool navigationHeightDeltaWithinCylinder(const NavSurfaceGenerationOptions& options, const NavSample& a,
                                         const NavSample& b) {
  return std::abs(a.support.y - b.support.y) <= std::abs(options.height);
}

const LatticeCell* findLatticeCell(std::span<const LatticeRow> rows, std::span<const LatticeCell> cells,
                                   const LatticeKey& key) {
  if (key.row < 0 || key.col < 0 || static_cast<std::size_t>(key.row) >= rows.size()) return nullptr;
  const LatticeRow& row = rows[static_cast<std::size_t>(key.row)];
  if (static_cast<std::size_t>(key.col) >= row.cell_count) return nullptr;
  return &cells[row.first_cell + static_cast<std::size_t>(key.col)];
}

std::span<const NavSample> cellSamples(std::span<const NavSample> samples, const LatticeCell& cell) {
  return samples.subspan(cell.first_sample, cell.sample_count);
}

void addLatticeTrianglesForKeys(const NavSurfaceGenerationOptions& options, std::span<const NavSample> samples,
                                std::span<const LatticeRow> rows, std::span<const LatticeCell> cells,
                                const std::array<LatticeKey, 3>& keys, NavSurface& surface,
                                NavLatticeDiagnostics& diagnostics) {
  ++diagnostics.key_triangles;
  const LatticeCell* cell_a = findLatticeCell(rows, cells, keys[0]);
  const LatticeCell* cell_b = findLatticeCell(rows, cells, keys[1]);
  const LatticeCell* cell_c = findLatticeCell(rows, cells, keys[2]);
  if (cell_a == nullptr || cell_b == nullptr || cell_c == nullptr) {
    ++diagnostics.missing_key;
    return;
  }

  for (const NavSample& a : cellSamples(samples, *cell_a)) {
    for (const NavSample& b : cellSamples(samples, *cell_b)) {
      for (const NavSample& c : cellSamples(samples, *cell_c)) {
        ++diagnostics.candidate_triangles;
        std::uint32_t va = a.vertex;
        std::uint32_t vb = b.vertex;
        std::uint32_t vc = c.vertex;
        if (va == vb || va == vc || vb == vc) {
          ++diagnostics.duplicate_vertex;
          continue;
        }

        if (!navigationHeightDeltaWithinCylinder(options, a, b) ||
            !navigationHeightDeltaWithinCylinder(options, b, c) ||
            !navigationHeightDeltaWithinCylinder(options, a, c)) {
          ++diagnostics.edge_height;
          continue;
        }
        if (!tryAppendNavigationTriangle(surface, va, vb, vc)) {
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
                         const SurfaceSupportFilter& final_support_filter, StaticAnchorTraversal& traversal,
                         const NavSurfaceGenerationOptions& options, NavSurface& surface) {
  std::vector<NavSample> samples;
  std::vector<LatticeCell> cells;
  std::vector<LatticeRow> rows;
  std::vector<geometry::SurfaceSupportHit> support_hits;
  geometry::PositionIndex vertex_index(kNavMergeDistance, geometry::PositionWeldMetric::kEuclidean);
  vertex_index.reservePositionCapacity(support_index.size());

  const float sample_spacing = options.radius;
  const float row_spacing = sample_spacing * kNavSqrt3Over2;
  int row = 0;
  // Float accumulation order affects generated topology
  // NOLINTBEGIN(bugprone-float-loop-counter)
  for (float z = spatial::firstCellCenter(support_bounds.min.z, row_spacing); z <= support_bounds.max.z;
       z += row_spacing, ++row) {
    const std::size_t first_cell = cells.size();
    float x_offset = (row % 2 == 0) ? 0.0f : sample_spacing * 0.5f;
    for (float x = spatial::firstCellCenter(support_bounds.min.x, sample_spacing) + x_offset; x <= support_bounds.max.x;
         x += sample_spacing) {
      cells.push_back(addNavigationSamplesAt(support_index,
                                             traversal,
                                             geometry_support,
                                             geometry,
                                             final_support_filter,
                                             options,
                                             x,
                                             z,
                                             surface,
                                             vertex_index,
                                             samples,
                                             support_hits));
    }
    rows.push_back({first_cell, cells.size() - first_cell});
  }
  // NOLINTEND(bugprone-float-loop-counter)

  NavLatticeDiagnostics lattice_diagnostics;
  for (std::size_t row_index = 0; row_index < rows.size(); ++row_index) {
    const LatticeRow& lattice_row = rows[row_index];
    for (std::size_t col_index = 0; col_index < lattice_row.cell_count; ++col_index) {
      const LatticeKey key{static_cast<int>(row_index), static_cast<int>(col_index)};
      if (key.row % 2 == 0) {
        addLatticeTrianglesForKeys(options,
                                   samples,
                                   rows,
                                   cells,
                                   {key, {key.row, key.col + 1}, {key.row + 1, key.col}},
                                   surface,
                                   lattice_diagnostics);
        addLatticeTrianglesForKeys(options,
                                   samples,
                                   rows,
                                   cells,
                                   {key, {key.row + 1, key.col}, {key.row + 1, key.col - 1}},
                                   surface,
                                   lattice_diagnostics);
      } else {
        addLatticeTrianglesForKeys(options,
                                   samples,
                                   rows,
                                   cells,
                                   {key, {key.row, key.col + 1}, {key.row + 1, key.col + 1}},
                                   surface,
                                   lattice_diagnostics);
        addLatticeTrianglesForKeys(options,
                                   samples,
                                   rows,
                                   cells,
                                   {key, {key.row + 1, key.col}, {key.row + 1, key.col + 1}},
                                   surface,
                                   lattice_diagnostics);
      }
    }
  }
  log(ARX_LOG_DEBUG,
      "Navigation surface lattice triangles: key_triangles={}, missing_key={}, candidates={}, "
      "added={}, duplicate_vertex={}, edge_height={}, degenerate={}",
      lattice_diagnostics.key_triangles,
      lattice_diagnostics.missing_key,
      lattice_diagnostics.candidate_triangles,
      lattice_diagnostics.added,
      lattice_diagnostics.duplicate_vertex,
      lattice_diagnostics.edge_height,
      lattice_diagnostics.degenerate);
}

}  // namespace pistoris::navigation::surface
