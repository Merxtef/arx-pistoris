// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#include "arx_pistoris/base/indices.h"

#include "modules/geometry.h"
#include "modules/rooms.h"
#include "modules/rooms/internal.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <span>
#include <utility>
#include <vector>

namespace pistoris::rooms {

Error collectVertexWeldSegments(const GeometryData& geometry, const RoomsData& rooms, float radius,
                                VertexWeldSegments& out) {
  if (!(radius > 0.0f) || !std::isfinite(radius)) return Error::kInvalidOptions;
  Error error = rooms::validateFaceRooms(rooms, geometry.faces.size());
  if (error != Error::kNone) return error;

  VertexWeldSegments collected;
  collected.offsets.assign(rooms.definitions.size() + 1U, 0);
  for (std::size_t face_index = 0; face_index < geometry.faces.size(); ++face_index) {
    const RoomIndex room = rooms.face_rooms[face_index];
    for (const Corner& corner : geometry.faces[face_index].corners) {
      if (corner.vertex >= geometry.vertices.size()) return Error::kBadFaceVertex;
      ++collected.offsets[static_cast<std::size_t>(room) + 1U];
    }
  }
  for (std::size_t room = 1; room < collected.offsets.size(); ++room)
    collected.offsets[room] += collected.offsets[room - 1U];
  collected.vertices.resize(collected.offsets.back());
  std::vector<std::size_t> room_write = collected.offsets;
  for (std::size_t face_index = 0; face_index < geometry.faces.size(); ++face_index) {
    const RoomIndex room = rooms.face_rooms[face_index];
    for (const Corner& corner : geometry.faces[face_index].corners)
      collected.vertices[room_write[room]++] = corner.vertex;
  }

  std::vector<std::uint8_t> protected_mask(geometry.vertices.size(), 0);
  std::vector<RoomIndex> first_room(geometry.vertices.size(), kInvalidRoomIndex);
  std::size_t source_begin = 0;
  std::size_t compact_write = 0;
  for (std::size_t room = 0; room < rooms.definitions.size(); ++room) {
    const std::size_t source_end = collected.offsets[room + 1U];
    auto begin = collected.vertices.begin() + static_cast<std::ptrdiff_t>(source_begin);
    auto end = collected.vertices.begin() + static_cast<std::ptrdiff_t>(source_end);
    std::sort(begin, end);
    const auto unique_end = std::unique(begin, end);
    collected.offsets[room] = compact_write;
    for (auto current = begin; current != unique_end; ++current) {
      const VertexIndex vertex = *current;
      collected.vertices[compact_write++] = vertex;
      if (first_room[vertex] == kInvalidRoomIndex) {
        first_room[vertex] = static_cast<RoomIndex>(room);
      } else if (first_room[vertex] != room) {
        protected_mask[vertex] = 1;
      }
    }
    source_begin = source_end;
  }
  collected.offsets.back() = compact_write;
  collected.vertices.resize(compact_write);

  RoomPortalIndex portal_index;
  error = buildRoomPortalIndex(rooms, portal_index);
  if (error != Error::kNone) return error;

  const double radius_squared = static_cast<double>(radius) * radius;
  for (std::size_t room = 0; room < rooms.definitions.size(); ++room) {
    const std::span<const VertexIndex> vertices =
        std::span<const VertexIndex>(collected.vertices)
            .subspan(collected.offsets[room], collected.offsets[room + 1U] - collected.offsets[room]);
    const std::span<const PortalIndex> portals = portal_index.roomPortals(static_cast<RoomIndex>(room));
    for (VertexIndex vertex : vertices) {
      for (PortalIndex portal : portals) {
        if (pointPortalDistanceSquared(geometry.vertices[vertex].position, rooms.portals[portal]) <= radius_squared) {
          protected_mask[vertex] = 1;
          break;
        }
      }
    }
  }

  collected.protected_vertices.reserve(
      static_cast<std::size_t>(std::count(protected_mask.begin(), protected_mask.end(), std::uint8_t{1})));
  for (std::size_t vertex = 0; vertex < protected_mask.size(); ++vertex) {
    if (protected_mask[vertex] != 0) collected.protected_vertices.push_back(static_cast<VertexIndex>(vertex));
  }
  out = std::move(collected);
  return Error::kNone;
}

}  // namespace pistoris::rooms
