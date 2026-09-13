// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#include "arx_pistoris/base/indices.h"

#include "modules/rooms.h"
#include "utils/identifier.h"

#include <algorithm>
#include <cassert>
#include <cstddef>
#include <span>
#include <utility>
#include <vector>

namespace pistoris::rooms {
namespace {

bool samePortalTopology(const Portal& lhs, const Portal& rhs) noexcept {
  if (lhs.room_1 != rhs.room_1 || lhs.room_2 != rhs.room_2 || lhs.shape != rhs.shape) return false;
  const std::size_t count = portalVertexCount(lhs.shape);
  for (std::size_t index = 0; index < count; ++index)
    if (lhs.vertices[index].x != rhs.vertices[index].x || lhs.vertices[index].y != rhs.vertices[index].y ||
        lhs.vertices[index].z != rhs.vertices[index].z)
      return false;
  return true;
}

}  // namespace

void setRoom(RoomsData& rooms, RoomIndex index, Room room) noexcept {
  assert(static_cast<std::size_t>(index) < rooms.definitions.size());
  rooms.definitions[index] = std::move(room);
}

RoomIndex addRoom(RoomsData& rooms, Room room) {
  assert(rooms.definitions.size() < static_cast<std::size_t>(kInvalidRoomIndex));
  const RoomIndex index = static_cast<RoomIndex>(rooms.definitions.size());
  rooms.definitions.push_back(std::move(room));
  rooms.distances.clear();
  return index;
}

void removeRoom(RoomsData& rooms, RoomIndex index) noexcept {
  assert(static_cast<std::size_t>(index) < rooms.definitions.size());
  assert(std::ranges::find(rooms.face_rooms, index) == rooms.face_rooms.end());
  rooms.definitions.erase(rooms.definitions.begin() + static_cast<std::ptrdiff_t>(index));
  std::erase_if(rooms.portals,
                [index](const Portal& portal) { return portal.room_1 == index || portal.room_2 == index; });
  for (Portal& portal : rooms.portals) {
    if (portal.room_1 > index) --portal.room_1;
    if (portal.room_2 > index) --portal.room_2;
  }
  for (RoomIndex& room : rooms.face_rooms)
    if (room > index) --room;
  rooms.distances.clear();
}

void appendFaceRooms(RoomsData& rooms, std::span<const RoomIndex> face_rooms) {
  rooms.face_rooms.insert(rooms.face_rooms.end(), face_rooms.begin(), face_rooms.end());
}

void reserveFaceRoomCapacity(RoomsData& rooms, std::size_t capacity) { rooms.face_rooms.reserve(capacity); }

void truncateFaceRooms(RoomsData& rooms, std::size_t size) noexcept {
  while (rooms.face_rooms.size() > size) rooms.face_rooms.pop_back();
}

void setFaceRoom(RoomsData& rooms, FaceIndex face, RoomIndex room) noexcept {
  assert(static_cast<std::size_t>(face) < rooms.face_rooms.size());
  assert(static_cast<std::size_t>(room) < rooms.definitions.size());
  rooms.face_rooms[face] = room;
}

void removeFaceRoom(RoomsData& rooms, FaceIndex face) noexcept {
  assert(static_cast<std::size_t>(face) < rooms.face_rooms.size());
  rooms.face_rooms.erase(rooms.face_rooms.begin() + static_cast<std::ptrdiff_t>(face));
}

void replaceFaceRooms(RoomsData& rooms, std::vector<RoomIndex>&& face_rooms) noexcept {
  rooms.face_rooms = std::move(face_rooms);
}

void clearFaceRooms(RoomsData& rooms) noexcept { rooms.face_rooms.clear(); }

void remapFaceRooms(RoomsData& rooms, std::span<const FaceIndex> face_remap) noexcept {
  if (face_remap.empty()) return;
  assert(face_remap.size() == rooms.face_rooms.size());
  std::size_t next_face = 0;
  for (std::size_t old = 0; old < face_remap.size(); ++old) {
    const FaceIndex next = face_remap[old];
    if (next == kInvalidFaceIndex) continue;
    assert(next == next_face);
    if (next_face != old) rooms.face_rooms[next_face] = rooms.face_rooms[old];
    ++next_face;
  }
  while (rooms.face_rooms.size() > next_face) rooms.face_rooms.pop_back();
}

PortalIndex addPortal(RoomsData& rooms, Portal portal) {
  assert(rooms.portals.size() < static_cast<std::size_t>(kInvalidPortalIndex));
  const PortalIndex index = static_cast<PortalIndex>(rooms.portals.size());
  rooms.portals.push_back(std::move(portal));
  rooms.distances.clear();
  return index;
}

void setPortal(RoomsData& rooms, PortalIndex index, Portal portal) noexcept {
  assert(static_cast<std::size_t>(index) < rooms.portals.size());
  const bool topology_changed = !samePortalTopology(rooms.portals[index], portal);
  rooms.portals[index] = std::move(portal);
  if (topology_changed) rooms.distances.clear();
}

void removePortal(RoomsData& rooms, PortalIndex index) noexcept {
  assert(static_cast<std::size_t>(index) < rooms.portals.size());
  rooms.portals.erase(rooms.portals.begin() + static_cast<std::ptrdiff_t>(index));
  rooms.distances.clear();
}

void setRoomDistance(RoomsData& rooms, RoomIndex low_room, RoomIndex high_room, RoomDistance distance) {
  assert(rooms.distances.empty() || rooms.distances.size() == roomDistancePairCount(rooms.definitions.size()));
  if (rooms.distances.empty()) resetRoomDistances(rooms.distances, rooms.definitions.size());
  rooms.distances[roomDistancePairIndex(low_room, high_room)] = distance;
}

void replaceRoomDistances(RoomsData& rooms, RoomDistances&& distances) noexcept {
  rooms.distances = std::move(distances);
}

void clearRoomDistances(RoomsData& rooms) noexcept { rooms.distances.clear(); }

std::size_t repairPortalNames(std::span<Portal> portals) {
  IdentifierUniquifier names;
  names.reserve(portals.size());
  for (Portal& portal : portals) names.add(portal.name);
  const IdentifierRepairSummary summary = names.apply();
  assert(!summary.exhausted);
  return summary.changed;
}

namespace {

template <class Value>
void repairName(std::span<const Value> existing, Value& candidate, std::size_t ignored) {
  IdentifierUniquifier names;
  names.reserve(1, existing.size());
  for (std::size_t index = 0; index < existing.size(); ++index)
    if (index != ignored) names.occupy(existing[index].name);
  names.add(candidate.name);
  [[maybe_unused]] const IdentifierRepairSummary summary = names.apply();
  assert(!summary.exhausted);
}

}  // namespace

void repairRoomName(const RoomsData& rooms, Room& room, RoomIndex ignored) {
  repairName<Room>(rooms.definitions, room, ignored);
}

void repairPortalName(const RoomsData& rooms, Portal& portal, PortalIndex ignored) {
  repairName<Portal>(rooms.portals, portal, ignored);
}

}  // namespace pistoris::rooms
