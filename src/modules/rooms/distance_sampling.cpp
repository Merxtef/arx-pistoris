// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#include "arx_pistoris/base/indices.h"
#include "arx_pistoris/base/math.hpp"

#include "modules/geometry.h"
#include "modules/rooms.h"
#include "modules/rooms/internal.h"
#include "utils/spatial/hash_grid.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <vector>

namespace pistoris::rooms {

ArxVector3 offsetRoomDistanceSample(const geometry::SurfaceSupportHit& hit, const RoomDistanceOptions& options) {
  ArxVector3 position = hit.position;
  if (hit.normal.y > 0.0f)
    position.y += options.sample_height_offset;
  else
    position.y -= options.sample_height_offset;
  return position;
}

bool sampleOffsetClear(const RoomGeometryIndex& room_geometry, RoomIndex room, const ArxVector3& support_position,
                       const ArxVector3& sample_position, std::vector<std::uint32_t>& candidate_scratch) {
  return !room_geometry.segmentBlocked(
      room, support_position, sample_position, kSampleOffsetEndpointEpsilon, candidate_scratch);
}

void addSampleNodes(const RoomsData& rooms, const RoomGeometryIndex& room_geometry, const RoomDistanceOptions& options,
                    RoomDistanceGenerationGraph& graph, RoomDistanceGenerationDiagnostics* diagnostics) {
  if (diagnostics) diagnostics->sampled_points_by_room.assign(rooms.definitions.size(), {});
  if (options.sample_spacing <= 0.0f || !std::isfinite(options.sample_spacing)) return;
  std::vector<geometry::SurfaceSupportHit> hits;
  std::vector<std::uint32_t> candidate_scratch;
  const std::size_t count = std::min(rooms.definitions.size(), graph.rooms.size());
  for (std::size_t room_index = 0; room_index < count; ++room_index) {
    RoomIndex room = static_cast<RoomIndex>(room_index);
    if (graph.rooms[room_index].portal_nodes.size() < 2U) continue;
    if (!room_geometry.hasRoomBounds(room)) continue;
    const ArxAabb& bounds = room_geometry.roomBounds(room);
    const int min_x = spatial::cellCoord(bounds.min.x, options.sample_spacing);
    const int max_x = spatial::cellCoord(bounds.max.x, options.sample_spacing);
    const int min_z = spatial::cellCoord(bounds.min.z, options.sample_spacing);
    const int max_z = spatial::cellCoord(bounds.max.z, options.sample_spacing);
    for (int z = min_z; z <= max_z; ++z) {
      const float sample_z = (static_cast<float>(z) + 0.5f) * options.sample_spacing;
      for (int x = min_x; x <= max_x; ++x) {
        const float sample_x = (static_cast<float>(x) + 0.5f) * options.sample_spacing;
        room_geometry.findSupportHits(hits, room, sample_x, sample_z);
        geometry::mergeSurfaceSupportHits(hits);
        for (const geometry::SurfaceSupportHit& hit : hits) {
          ArxVector3 position = offsetRoomDistanceSample(hit, options);
          if (!sampleOffsetClear(room_geometry, room, hit.position, position, candidate_scratch)) continue;
          if (diagnostics) diagnostics->sampled_points_by_room[room].push_back({position, room});
          addRoomNode(graph.rooms[room], {position, room});
        }
      }
    }
  }
}

}  // namespace pistoris::rooms
