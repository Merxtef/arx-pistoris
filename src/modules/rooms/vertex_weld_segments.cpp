// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#include "arx_pistoris/indices.h"

#include "modules/geometry.h"
#include "modules/rooms.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <vector>

namespace pistoris::rooms {

Error collectVertexWeldSegments(const GeometryData& geometry, const RoomsData& rooms, float radius,
                                std::vector<std::vector<VertexIndex>>& room_vertices,
                                std::vector<VertexIndex>& protected_vertices) {
  room_vertices.clear();
  protected_vertices.clear();
  Error error = rooms::validateFaceRooms(rooms, geometry.faces.size());
  if (error != Error::kNone) return error;

  room_vertices.resize(rooms.definitions.size());
  for (std::size_t face_index = 0; face_index < geometry.faces.size(); ++face_index) {
    const RoomIndex room = rooms.face_rooms[face_index];
    for (const Corner& corner : geometry.faces[face_index].corners) {
      if (corner.vertex >= geometry.vertices.size()) return Error::kBadFaceVertex;
      room_vertices[room].push_back(corner.vertex);
    }
  }

  std::vector<std::uint8_t> protected_mask(geometry.vertices.size(), 0);
  std::vector<RoomIndex> first_room(geometry.vertices.size(), kInvalidRoomIndex);
  for (std::size_t room = 0; room < room_vertices.size(); ++room) {
    std::vector<VertexIndex>& vertices = room_vertices[room];
    std::sort(vertices.begin(), vertices.end());
    vertices.erase(std::unique(vertices.begin(), vertices.end()), vertices.end());
    for (VertexIndex vertex : vertices) {
      if (first_room[vertex] == kInvalidRoomIndex) {
        first_room[vertex] = static_cast<RoomIndex>(room);
      } else if (first_room[vertex] != room) {
        protected_mask[vertex] = 1;
      }
    }
  }

  std::vector<std::vector<PortalIndex>> room_portals(rooms.definitions.size());
  if (rooms.portals.size() > static_cast<std::size_t>(kInvalidPortalIndex)) return Error::kTooManyPortals;
  for (std::size_t portal_index = 0; portal_index < rooms.portals.size(); ++portal_index) {
    const Portal& portal = rooms.portals[portal_index];
    error = rooms::validatePortal(portal, rooms.definitions.size());
    if (error != Error::kNone) return error;
    const PortalIndex index = static_cast<PortalIndex>(portal_index);
    room_portals[portal.room_1].push_back(index);
    room_portals[portal.room_2].push_back(index);
  }

  const double radius_squared = static_cast<double>(radius) * radius;
  for (std::size_t room = 0; room < room_vertices.size(); ++room) {
    for (VertexIndex vertex : room_vertices[room]) {
      for (PortalIndex portal : room_portals[room]) {
        if (pointPortalDistanceSquared(geometry.vertices[vertex].position, rooms.portals[portal]) <= radius_squared) {
          protected_mask[vertex] = 1;
          break;
        }
      }
    }
  }

  for (std::size_t vertex = 0; vertex < protected_mask.size(); ++vertex) {
    if (protected_mask[vertex] != 0) protected_vertices.push_back(static_cast<VertexIndex>(vertex));
  }
  return Error::kNone;
}

}  // namespace pistoris::rooms
