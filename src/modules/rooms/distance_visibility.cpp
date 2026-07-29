// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#include "arx_pistoris/arx_math.hpp"
#include "arx_pistoris/indices.h"

#include "modules/geometry.h"
#include "modules/rooms.h"
#include "modules/rooms/internal.h"
#include "utils/spatial/hash_grid.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <map>
#include <vector>

namespace pistoris::rooms {

bool segmentIntersectsRoomDistanceTriangle(const ArxVector3& start, const ArxVector3& end, const ArxVector3& a,
                                           const ArxVector3& b, const ArxVector3& c) {
  double t = 0.0;
  return geometry::segmentTriangleIntersectionT(start, end, a, b, c, t) && t > kVisibilityEndpointEpsilon &&
         t < 1.0f - kVisibilityEndpointEpsilon;
}

bool blockedByRoomGeometry(const RoomsData& rooms, const GeometryData& geometry, RoomIndex room,
                           const ArxVector3& start, const ArxVector3& end) {
  for (FaceIndex face_index : faceIndicesForRoom(rooms, geometry, room, kRoomDistanceIgnoreFlags)) {
    std::array<ArxVector3, 3> vertices = geometry::facePositions(geometry, geometry.faces[face_index]);
    if (segmentIntersectsRoomDistanceTriangle(start, end, vertices[0], vertices[1], vertices[2])) return true;
  }
  return false;
}

void addVisibilityEdges(const RoomsData& rooms, const GeometryData& geometry, const RoomDistanceOptions& options,
                        RoomDistanceGenerationGraph& graph, RoomDistanceGenDiagnostics* diagnostics) {
  if (diagnostics) diagnostics->in_room_visibility_edges.clear();
  if (options.sample_spacing <= 0.0f || !std::isfinite(options.sample_spacing) || options.max_link_distance <= 0.0f ||
      !std::isfinite(options.max_link_distance))
    return;
  const float max_distance = options.max_link_distance;
  const double max_distance_squared = static_cast<double>(max_distance) * max_distance;
  const std::size_t count = std::min(rooms.definitions.size(), graph.rooms.size());
  for (std::size_t room_index = 0; room_index < count; ++room_index) {
    RoomIndex room = static_cast<RoomIndex>(room_index);
    RoomGraph& room_graph = graph.rooms[room_index];
    const std::vector<RoomDistanceNode>& nodes = room_graph.nodes;
    std::map<std::uint64_t, std::vector<std::uint32_t>> buckets;
    for (std::uint32_t i = 0; i < nodes.size(); ++i)
      buckets[spatial::cellKey(nodes[i].position.x, nodes[i].position.z, options.sample_spacing)].push_back(i);

    for (std::uint32_t i = 0; i < nodes.size(); ++i) {
      const int cell_x = spatial::cellCoord(nodes[i].position.x, options.sample_spacing);
      const int cell_z = spatial::cellCoord(nodes[i].position.z, options.sample_spacing);
      for (int dz = -1; dz <= 1; ++dz) {
        for (int dx = -1; dx <= 1; ++dx) {
          auto it = buckets.find(spatial::cellKey(cell_x + dx, cell_z + dz));
          if (it == buckets.end()) continue;
          for (std::uint32_t j : it->second) {
            if (j <= i) continue;
            const ArxVector3 delta = nodes[j].position - nodes[i].position;
            const double xz_distance_squared =
                static_cast<double>(delta.x) * delta.x + static_cast<double>(delta.z) * delta.z;
            if (xz_distance_squared > max_distance_squared) continue;
            if (blockedByRoomGeometry(rooms, geometry, room, nodes[i].position, nodes[j].position)) continue;
            const float cost = std::sqrt(static_cast<float>(math::lengthSquared(delta)));
            addGraphEdge(room_graph, i, j, cost);
            if (diagnostics)
              diagnostics->in_room_visibility_edges.push_back({nodes[i].position, nodes[j].position, room, room});
          }
        }
      }
    }
  }
}

}  // namespace pistoris::rooms
