// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#include "arx_pistoris/indices.h"

#include "modules/rooms.h"
#include "utils/unique_name.h"

#include <cassert>
#include <cstddef>
#include <span>
#include <string>
#include <unordered_set>
#include <utility>

namespace pistoris::rooms {

RoomIndex addRoom(RoomsData& rooms, std::string name) {
  if (rooms.definitions.size() >= static_cast<std::size_t>(kInvalidRoomIndex)) return kInvalidRoomIndex;
  RoomIndex index = static_cast<RoomIndex>(rooms.definitions.size());
  rooms.definitions.push_back({std::move(name)});
  return index;
}

void addFaceRooms(RoomsData& rooms, std::span<const RoomIndex> face_rooms) {
  rooms.face_rooms.insert(rooms.face_rooms.end(), face_rooms.begin(), face_rooms.end());
}

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
  if (rooms.portals.size() >= static_cast<std::size_t>(kInvalidPortalIndex)) return kInvalidPortalIndex;
  PortalIndex index = static_cast<PortalIndex>(rooms.portals.size());
  rooms.portals.push_back(std::move(portal));
  return index;
}

std::size_t makePortalNamesUnique(std::span<Portal> portals) {
  std::unordered_set<std::string> unavailable;
  unavailable.reserve(portals.size());
  for (const Portal& portal : portals) unavailable.insert(portal.name);

  std::unordered_set<std::string> assigned;
  assigned.reserve(portals.size());
  std::size_t renamed = 0;
  for (Portal& portal : portals) {
    if (assigned.insert(portal.name).second) continue;
    portal.name = makeUniqueName(portal.name, unavailable);
    unavailable.insert(portal.name);
    assigned.insert(portal.name);
    ++renamed;
  }
  return renamed;
}

}  // namespace pistoris::rooms
