// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#include "arx/conversion/level/fts/quad_packing.h"

#include "arx_pistoris/indices.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <map>
#include <optional>
#include <span>
#include <utility>
#include <vector>

namespace pistoris::arx_level_conversion::fts_bake {
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
            std::vector<bool>& consumed, std::vector<std::optional<Polygon>>& packed) {
  std::optional<Polygon> candidate = tryPack(triangles[first], triangles[second], require_shared_source_edge);
  if (!candidate.has_value()) return false;
  consumed[first] = true;
  consumed[second] = true;
  packed[first] = candidate;
  return true;
}

void pairSameSource(std::span<const Triangle> triangles, std::vector<bool>& consumed,
                    std::vector<std::optional<Polygon>>& packed) {
  std::map<std::size_t, std::vector<std::size_t>> by_source;
  for (std::size_t index = 0; index < triangles.size(); ++index)
    by_source[triangles[index].source_face].push_back(index);

  for (const auto& entry : by_source) {
    const std::vector<std::size_t>& indices = entry.second;
    for (std::size_t first_offset = 0; first_offset < indices.size(); ++first_offset) {
      const std::size_t first = indices[first_offset];
      if (consumed[first]) continue;
      for (std::size_t second_offset = first_offset + 1U; second_offset < indices.size(); ++second_offset) {
        const std::size_t second = indices[second_offset];
        if (!consumed[second] && pairAt(triangles, first, second, false, consumed, packed)) break;
      }
    }
  }
}

void pairAcrossSources(std::span<const Triangle> triangles, std::vector<bool>& consumed,
                       std::vector<std::optional<Polygon>>& packed) {
  using Edge = std::pair<VertexIndex, VertexIndex>;
  std::map<Edge, std::vector<std::size_t>> by_edge;
  for (std::size_t triangle = 0; triangle < triangles.size(); ++triangle) {
    if (consumed[triangle]) continue;
    for (std::size_t edge = 0; edge < triangles[triangle].corners.size(); ++edge) {
      const std::optional<Edge> source_edge = sourceEdge(triangles[triangle], edge);
      if (source_edge.has_value()) by_edge[*source_edge].push_back(triangle);
    }
  }

  for (std::size_t first = 0; first < triangles.size(); ++first) {
    if (consumed[first]) continue;
    for (std::size_t edge = 0; edge < triangles[first].corners.size() && !consumed[first]; ++edge) {
      const std::optional<Edge> source_edge = sourceEdge(triangles[first], edge);
      if (!source_edge.has_value()) continue;
      for (const std::size_t second : by_edge[*source_edge]) {
        if (second <= first || consumed[second] || triangles[first].source_face == triangles[second].source_face)
          continue;
        if (pairAt(triangles, first, second, true, consumed, packed)) break;
      }
    }
  }
}

}  // namespace

std::vector<Polygon> packCellPolygons(std::span<const Triangle> triangles, bool reconstruct_quads) {
  std::vector<Polygon> result;
  result.reserve(triangles.size());
  if (!reconstruct_quads || triangles.size() < 2U) {
    for (const Triangle& triangle : triangles) result.push_back(asPolygon(triangle));
    return result;
  }

  std::vector<bool> consumed(triangles.size(), false);
  std::vector<std::optional<Polygon>> packed(triangles.size());
  pairSameSource(triangles, consumed, packed);
  pairAcrossSources(triangles, consumed, packed);

  for (std::size_t index = 0; index < triangles.size(); ++index) {
    const std::optional<Polygon>& polygon = packed[index];
    if (polygon.has_value()) {
      result.push_back(*polygon);
    } else if (!consumed[index]) {
      result.push_back(asPolygon(triangles[index]));
    }
  }
  return result;
}

}  // namespace pistoris::arx_level_conversion::fts_bake
