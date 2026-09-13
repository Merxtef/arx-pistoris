// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#include "level/native/fts/quad_packing.h"

#include "arx_pistoris/base/indices.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <optional>
#include <span>
#include <tuple>
#include <utility>
#include <vector>

namespace pistoris::level_native::fts_bake {
namespace {

bool nearEqual(float first, float second) {
  constexpr float kToleranceFactor = 4.0f;
  const float scale = std::max({1.0f, std::abs(first), std::abs(second)});
  return std::abs(first - second) <= kToleranceFactor * std::numeric_limits<float>::epsilon() * scale;
}

bool samePosition(const Corner& first, const Corner& second) {
  return nearEqual(first.position.x, second.position.x) && nearEqual(first.position.y, second.position.y) &&
         nearEqual(first.position.z, second.position.z);
}

bool compatibleCorner(const Corner& first, const Corner& second) {
  return samePosition(first, second) && nearEqual(first.normal.x, second.normal.x) &&
         nearEqual(first.normal.y, second.normal.y) && nearEqual(first.normal.z, second.normal.z) &&
         nearEqual(first.u, second.u) && nearEqual(first.v, second.v) && nearEqual(first.color.r, second.color.r) &&
         nearEqual(first.color.g, second.color.g) && nearEqual(first.color.b, second.color.b);
}

bool compatibleProperties(const Triangle& first, const Triangle& second) {
  return first.texture == second.texture && first.flags == second.flags && first.transval == second.transval &&
         first.room == second.room;
}

std::optional<std::pair<VertexIndex, VertexIndex>> sourceEdge(const Triangle& triangle, std::size_t edge) {
  const std::size_t next = (edge + 1U) % triangle.corners.size();
  const std::uint8_t mask = triangle.corners[edge].source_edge_mask & triangle.corners[next].source_edge_mask;
  std::size_t source_edge = 0;
  switch (mask) {
    case 1U:
      source_edge = 0;
      break;
    case 2U:
      source_edge = 1;
      break;
    case 4U:
      source_edge = 2;
      break;
    default:
      return std::nullopt;
  }
  VertexIndex first = triangle.source_vertices[source_edge];
  VertexIndex second = triangle.source_vertices[(source_edge + 1U) % triangle.source_vertices.size()];
  if (second < first) std::swap(first, second);
  return std::pair{first, second};
}

std::optional<Polygon> tryPack(const Triangle& first, const Triangle& second, bool require_shared_source_edge) {
  if (!compatibleProperties(first, second)) return std::nullopt;

  for (std::size_t first_edge = 0; first_edge < first.corners.size(); ++first_edge) {
    const std::size_t first_next = (first_edge + 1U) % first.corners.size();
    for (std::size_t second_edge = 0; second_edge < second.corners.size(); ++second_edge) {
      const std::size_t second_next = (second_edge + 1U) % second.corners.size();
      if (!compatibleCorner(first.corners[first_edge], second.corners[second_next]) ||
          !compatibleCorner(first.corners[first_next], second.corners[second_edge]))
        continue;
      if (require_shared_source_edge) {
        const std::optional<std::pair<VertexIndex, VertexIndex>> first_source = sourceEdge(first, first_edge);
        const std::optional<std::pair<VertexIndex, VertexIndex>> second_source = sourceEdge(second, second_edge);
        if (!first_source.has_value() || first_source != second_source) continue;
      }

      const Corner& first_unique = first.corners[(first_edge + 2U) % first.corners.size()];
      const Corner& second_unique = second.corners[(second_edge + 2U) % second.corners.size()];
      if (samePosition(first_unique, second_unique) || samePosition(first_unique, first.corners[first_edge]) ||
          samePosition(first_unique, first.corners[first_next]) ||
          samePosition(second_unique, first.corners[first_edge]) ||
          samePosition(second_unique, first.corners[first_next]))
        continue;

      Polygon result;
      result.corners = {first_unique, first.corners[first_edge], first.corners[first_next], second_unique};
      result.norm = first.face_normal;
      result.norm2 = second.face_normal;
      result.texture = first.texture;
      result.flags = first.flags;
      result.transval = first.transval;
      result.room = first.room;
      result.kind = require_shared_source_edge ? PolygonKind::kCrossFaceQuad : PolygonKind::kClippedFragmentQuad;
      return result;
    }
  }
  return std::nullopt;
}

Polygon asPolygon(const Triangle& triangle) {
  Polygon result;
  std::copy(triangle.corners.begin(), triangle.corners.end(), result.corners.begin());
  result.norm = triangle.face_normal;
  result.norm2 = triangle.face_normal;
  result.texture = triangle.texture;
  result.flags = triangle.flags;
  result.transval = triangle.transval;
  result.room = triangle.room;
  return result;
}

bool pairAt(std::span<const Triangle> triangles, std::size_t first, std::size_t second, bool require_shared_source_edge,
            CellPackingScratch& scratch) {
  std::optional<Polygon> candidate = tryPack(triangles[first], triangles[second], require_shared_source_edge);
  if (!candidate.has_value()) return false;
  scratch.consumed[first] = 1;
  scratch.consumed[second] = 1;
  scratch.packed[first] = candidate;
  return true;
}

void pairSameSource(std::span<const Triangle> triangles, CellPackingScratch& scratch) {
  scratch.sources.clear();
  for (std::size_t index = 0; index < triangles.size(); ++index)
    scratch.sources.push_back({triangles[index].source_face, index});
  std::ranges::sort(
      scratch.sources, {}, [](const SourceEntry& entry) { return std::pair{entry.source_face, entry.triangle}; });

  std::size_t begin = 0;
  while (begin < scratch.sources.size()) {
    std::size_t end = begin + 1U;
    while (end < scratch.sources.size() && scratch.sources[end].source_face == scratch.sources[begin].source_face)
      ++end;
    for (std::size_t first_offset = begin; first_offset < end; ++first_offset) {
      const std::size_t first = scratch.sources[first_offset].triangle;
      if (scratch.consumed[first] != 0) continue;
      for (std::size_t second_offset = first_offset + 1U; second_offset < end; ++second_offset) {
        const std::size_t second = scratch.sources[second_offset].triangle;
        if (scratch.consumed[second] == 0 && pairAt(triangles, first, second, false, scratch)) break;
      }
    }
    begin = end;
  }
}

void pairAcrossSources(std::span<const Triangle> triangles, CellPackingScratch& scratch) {
  using Edge = std::pair<VertexIndex, VertexIndex>;
  scratch.edges.clear();
  for (std::size_t triangle = 0; triangle < triangles.size(); ++triangle) {
    if (scratch.consumed[triangle] != 0) continue;
    for (std::size_t edge = 0; edge < triangles[triangle].corners.size(); ++edge) {
      const std::optional<Edge> source_edge = sourceEdge(triangles[triangle], edge);
      if (source_edge.has_value()) scratch.edges.push_back({source_edge->first, source_edge->second, triangle});
    }
  }
  std::ranges::sort(
      scratch.edges, {}, [](const EdgeEntry& entry) { return std::tuple{entry.first, entry.second, entry.triangle}; });

  for (std::size_t first = 0; first < triangles.size(); ++first) {
    if (scratch.consumed[first] != 0) continue;
    for (std::size_t edge = 0; edge < triangles[first].corners.size() && scratch.consumed[first] == 0; ++edge) {
      const std::optional<Edge> source_edge = sourceEdge(triangles[first], edge);
      if (!source_edge.has_value()) continue;
      const Edge key = *source_edge;
      const auto range = std::ranges::equal_range(
          scratch.edges, key, {}, [](const EdgeEntry& entry) { return std::pair{entry.first, entry.second}; });
      for (const EdgeEntry& candidate : range) {
        const std::size_t second = candidate.triangle;
        if (second <= first || scratch.consumed[second] != 0 ||
            triangles[first].source_face == triangles[second].source_face)
          continue;
        if (pairAt(triangles, first, second, true, scratch)) break;
      }
    }
  }
}

}  // namespace

void reserveCellPackingScratch(CellPackingScratch& scratch, std::size_t triangle_count) {
  scratch.consumed.reserve(triangle_count);
  scratch.packed.reserve(triangle_count);
  scratch.sources.reserve(triangle_count);
  scratch.edges.reserve(triangle_count * 3U);
  scratch.polygons.reserve(triangle_count);
}

std::span<const Polygon> packCellPolygons(std::span<const Triangle> triangles, bool reconstruct_quads,
                                          CellPackingScratch& scratch) {
  reserveCellPackingScratch(scratch, triangles.size());
  scratch.polygons.clear();
  if (!reconstruct_quads || triangles.size() < 2U) {
    for (const Triangle& triangle : triangles) scratch.polygons.push_back(asPolygon(triangle));
    return scratch.polygons;
  }

  scratch.consumed.assign(triangles.size(), 0);
  scratch.packed.clear();
  scratch.packed.resize(triangles.size());
  pairSameSource(triangles, scratch);
  pairAcrossSources(triangles, scratch);

  for (std::size_t index = 0; index < triangles.size(); ++index) {
    const std::optional<Polygon>& polygon = scratch.packed[index];
    if (polygon.has_value()) {
      scratch.polygons.push_back(*polygon);
    } else if (scratch.consumed[index] == 0) {
      scratch.polygons.push_back(asPolygon(triangles[index]));
    }
  }
  return scratch.polygons;
}

}  // namespace pistoris::level_native::fts_bake
