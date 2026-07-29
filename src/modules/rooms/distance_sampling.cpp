// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#include "arx_pistoris/arx_math.hpp"
#include "arx_pistoris/flags.h"
#include "arx_pistoris/indices.h"

#include "modules/geometry.h"
#include "modules/rooms.h"
#include "modules/rooms/internal.h"
#include "utils/spatial/hash_grid.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <vector>

namespace pistoris::rooms {

namespace {

struct RoomSupportPredicateContext {
  const RoomsData* rooms = nullptr;
  const GeometryData* geometry = nullptr;
  RoomIndex room = kInvalidRoomIndex;
  FaceType ignore_flags = 0;
};

bool roomSupportFace(FaceIndex face, const void* user_data) noexcept {
  if (user_data == nullptr) return false;
  const auto& context = *static_cast<const RoomSupportPredicateContext*>(user_data);
  if (context.rooms == nullptr || context.geometry == nullptr || face >= context.rooms->face_rooms.size() ||
      face >= context.geometry->faces.size())
    return false;
  return context.rooms->face_rooms[face] == context.room &&
         (context.geometry->faces[face].flags & context.ignore_flags) == 0;
}

}  // namespace

std::vector<FaceIndex> faceIndicesForRoom(const RoomsData& rooms, const GeometryData& geometry, RoomIndex room,
                                          FaceType ignore_flags) {
  std::vector<FaceIndex> face_indices;
  const std::size_t count = std::min(geometry.faces.size(), rooms.face_rooms.size());
  for (std::size_t face_index = 0; face_index < count; ++face_index) {
    const Face& face = geometry.faces[face_index];
    if (rooms.face_rooms[face_index] != room || (face.flags & ignore_flags) != 0) continue;
    face_indices.push_back(static_cast<FaceIndex>(face_index));
  }
  return face_indices;
}

geometry::SurfaceSupportIndex buildRoomSupportIndex(const RoomsData& rooms, const GeometryData& geometry,
                                                    RoomIndex room, FaceType ignore_flags) {
  const RoomSupportPredicateContext context{&rooms, &geometry, room, ignore_flags};
  return geometry::buildSurfaceSupportIndex(geometry, {roomSupportFace, &context});
}

ArxVector3 offsetRoomDistanceSample(const geometry::SurfaceSupportHit& hit, const RoomDistanceOptions& options) {
  ArxVector3 position = hit.position;
  if (hit.normal.y > 0.0f)
    position.y += options.sample_height_offset;
  else
    position.y -= options.sample_height_offset;
  return position;
}

bool sampleOffsetClear(const RoomsData& rooms, const GeometryData& geometry, RoomIndex room,
                       const ArxVector3& support_position, const ArxVector3& sample_position) {
  for (FaceIndex face_index : faceIndicesForRoom(rooms, geometry, room, kRoomDistanceIgnoreFlags)) {
    std::array<ArxVector3, 3> vertices = geometry::facePositions(geometry, geometry.faces[face_index]);
    double t = 0.0;
    if (!geometry::segmentTriangleIntersectionT(
            support_position, sample_position, vertices[0], vertices[1], vertices[2], t))
      continue;
    if (t > kSampleOffsetEndpointEpsilon && t < 1.0f - kSampleOffsetEndpointEpsilon) return false;
  }
  return true;
}

void addSampleNodes(const RoomsData& rooms, const GeometryData& geometry, const RoomDistanceOptions& options,
                    RoomDistanceGenerationGraph& graph, RoomDistanceGenDiagnostics* diagnostics) {
  if (diagnostics) diagnostics->sampled_points_by_room.assign(rooms.definitions.size(), {});
  if (options.sample_spacing <= 0.0f || !std::isfinite(options.sample_spacing)) return;
  const std::size_t count = std::min(rooms.definitions.size(), graph.rooms.size());
  for (std::size_t room_index = 0; room_index < count; ++room_index) {
    RoomIndex room = static_cast<RoomIndex>(room_index);
    if (graph.rooms[room_index].portal_nodes.size() < 2U) continue;
    geometry::SurfaceSupportIndex support = buildRoomSupportIndex(rooms, geometry, room);
    if (!support.hasBounds()) continue;
    const ArxAabb& bounds = support.bounds();
    const int min_x = spatial::cellCoord(bounds.min.x, options.sample_spacing);
    const int max_x = spatial::cellCoord(bounds.max.x, options.sample_spacing);
    const int min_z = spatial::cellCoord(bounds.min.z, options.sample_spacing);
    const int max_z = spatial::cellCoord(bounds.max.z, options.sample_spacing);
    for (int z = min_z; z <= max_z; ++z) {
      const float sample_z = (static_cast<float>(z) + 0.5f) * options.sample_spacing;
      for (int x = min_x; x <= max_x; ++x) {
        const float sample_x = (static_cast<float>(x) + 0.5f) * options.sample_spacing;
        std::vector<geometry::SurfaceSupportHit> hits = support.hitsAt(sample_x, sample_z);
        geometry::mergeSurfaceSupportHits(hits);
        for (const geometry::SurfaceSupportHit& hit : hits) {
          ArxVector3 position = offsetRoomDistanceSample(hit, options);
          if (!sampleOffsetClear(rooms, geometry, room, hit.position, position)) continue;
          if (diagnostics) diagnostics->sampled_points_by_room[room].push_back({position, room});
          addRoomNode(graph.rooms[room], {position, room});
        }
      }
    }
  }
}

}  // namespace pistoris::rooms
