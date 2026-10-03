// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#include "arx_pistoris/base/indices.h"
#include "arx_pistoris/base/math.hpp"

#include "modules/geometry.h"
#include "modules/rooms.h"
#include "modules/rooms/internal.h"
#include "utils/spatial/xz_point_index.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <vector>

namespace pistoris::rooms {

bool segmentIntersectsRoomDistanceTriangle(const ArxVector3& start, const ArxVector3& end, const ArxVector3& a,
                                           const ArxVector3& b, const ArxVector3& c) {
  double t = 0.0;
  return geometry::segmentTriangleIntersectionT(start, end, a, b, c, t) && t > kVisibilityEndpointEpsilon &&
         t < 1.0f - kVisibilityEndpointEpsilon;
}

bool blockedByRoomGeometry(const RoomGeometryIndex& room_geometry, RoomIndex room, const ArxVector3& start,
                           const ArxVector3& end, std::vector<std::uint32_t>& candidate_scratch) {
  return room_geometry.segmentBlocked(room, start, end, kVisibilityEndpointEpsilon, candidate_scratch);
}

void addVisibilityEdges(const RoomsData& rooms, const RoomGeometryIndex& room_geometry,
                        const RoomDistanceOptions& options, RoomDistanceGenerationGraph& graph,
                        RoomDistanceGenerationDiagnostics* diagnostics) {
  if (diagnostics) diagnostics->in_room_visibility_edges.clear();
  if (options.sample_spacing <= 0.0f || !std::isfinite(options.sample_spacing) || options.max_link_distance <= 0.0f ||
      !std::isfinite(options.max_link_distance))
    return;
  const float max_distance = options.max_link_distance;
  const double max_distance_squared = static_cast<double>(max_distance) * max_distance;
  spatial::XzPointIndex node_index;
  std::vector<std::uint32_t> node_candidates;
  std::vector<std::uint32_t> triangle_candidates;
  const std::size_t count = std::min(rooms.definitions.size(), graph.rooms.size());
  for (std::size_t room_index = 0; room_index < count; ++room_index) {
    RoomIndex room = static_cast<RoomIndex>(room_index);
    RoomGraph& room_graph = graph.rooms[room_index];
    const std::vector<RoomDistanceNode>& nodes = room_graph.nodes;
    node_index.rebuild(nodes.size(), max_distance, [&](std::size_t index) { return nodes[index].position; });

    for (std::uint32_t i = 0; i < nodes.size(); ++i) {
      node_index.findNeighborCellCandidates(node_candidates, nodes[i].position.x, nodes[i].position.z);
      for (std::uint32_t j : node_candidates) {
        if (j <= i) continue;
        const ArxVector3 delta = nodes[j].position - nodes[i].position;
        const double xz_distance_squared =
            static_cast<double>(delta.x) * delta.x + static_cast<double>(delta.z) * delta.z;
        if (xz_distance_squared > max_distance_squared) continue;
        if (blockedByRoomGeometry(room_geometry, room, nodes[i].position, nodes[j].position, triangle_candidates))
          continue;
        const float cost = std::sqrt(static_cast<float>(math::lengthSquared(delta)));
        addGraphEdge(room_graph, i, j, cost);
        if (diagnostics)
          diagnostics->in_room_visibility_edges.push_back({nodes[i].position, nodes[j].position, room, room});
      }
    }
  }
}

}  // namespace pistoris::rooms
