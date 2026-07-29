// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#include "arx_pistoris/indices.h"

#include "modules/rooms.h"
#include "utils/math/finite.h"
#include "utils/name_tokens.h"

#include <cstddef>
#include <span>
#include <string_view>
#include <unordered_set>

namespace pistoris::rooms {
namespace {

Error portalError(PortalValidation validation) {
  switch (validation) {
    case PortalValidation::kValid:
      return Error::kNone;
    case PortalValidation::kBadShape:
      return Error::kBadPortalShape;
    case PortalValidation::kBadVertex:
      return Error::kBadPortalVertex;
    case PortalValidation::kDegenerate:
      return Error::kDegeneratePortal;
    case PortalValidation::kNonPlanar:
      return Error::kNonPlanarPortal;
    case PortalValidation::kInconsistentOrientation:
      return Error::kInconsistentPortalOrientation;
    case PortalValidation::kSelfIntersecting:
      return Error::kSelfIntersectingPortal;
  }
  return Error::kBadPortalShape;
}

Error validatePortalDefinition(const Portal& portal) {
  if (portal.name.empty() || !validSemanticString(portal.name)) return Error::kBadPortalName;
  return portalError(validatePortalGeometry(portal));
}

}  // namespace

Error validateRoom(const Room& room) noexcept {
  return room.name.empty() || !validSemanticString(room.name) ? Error::kBadRoomName : Error::kNone;
}

Error validateRooms(const RoomsData& rooms) {
  if (rooms.definitions.size() > static_cast<std::size_t>(kInvalidRoomIndex)) return Error::kTooManyRooms;
  if (rooms.definitions.empty()) return Error::kNoRooms;
  std::unordered_set<std::string_view> names;
  names.reserve(rooms.definitions.size());
  for (const Room& room : rooms.definitions) {
    Error error = validateRoom(room);
    if (error != Error::kNone) return error;
    if (!names.insert(room.name).second) return Error::kDuplicateRoomName;
  }
  return Error::kNone;
}

Error validateFaceRooms(std::span<const RoomIndex> face_rooms, std::size_t face_count, std::size_t room_count) {
  if (face_rooms.size() != face_count) return Error::kBadFaceRoomCount;
  for (RoomIndex room : face_rooms) {
    if (room >= room_count) return Error::kBadFaceRoomIndex;
  }
  return Error::kNone;
}

Error validateFaceRooms(const RoomsData& rooms, std::size_t face_count) {
  return validateFaceRooms(rooms.face_rooms, face_count, rooms.definitions.size());
}

Error validateRoomDistances(const RoomDistances& distances, const RoomsData& rooms) {
  if (distances.empty()) return Error::kNone;
  if (!hasCompleteRoomDistances(distances, rooms.definitions.size())) return Error::kBadRoomDistanceCount;

  std::size_t index = 0;
  for (RoomIndex high_room = 1; high_room < rooms.definitions.size(); ++high_room) {
    for (RoomIndex low_room = 0; low_room < high_room; ++low_room, ++index) {
      const RoomDistance& distance = distances[index];
      if (!math::finite(distance.distance)) return Error::kBadRoomDistance;

      if (distance.distance > 0.0f) {
        if (distance.low_room_portal >= rooms.portals.size() || distance.high_room_portal >= rooms.portals.size())
          return Error::kBadRoomDistance;
        const Portal& low_portal = rooms.portals[distance.low_room_portal];
        const Portal& high_portal = rooms.portals[distance.high_room_portal];
        if ((low_portal.room_1 != low_room && low_portal.room_2 != low_room) ||
            (high_portal.room_1 != high_room && high_portal.room_2 != high_room))
          return Error::kBadRoomDistance;
        continue;
      }

      const bool invalid =
          distance.low_room_portal == kInvalidPortalIndex && distance.high_room_portal == kInvalidPortalIndex;
      if (invalid) continue;
      if (distance.low_room_portal != distance.high_room_portal || distance.low_room_portal >= rooms.portals.size() ||
          !connectsRooms(rooms.portals[distance.low_room_portal], low_room, high_room))
        return Error::kBadRoomDistance;
    }
  }
  return Error::kNone;
}

Error validateRoomDistances(const RoomsData& rooms) { return validateRoomDistances(rooms.distances, rooms); }

Error validatePortalDefinitions(const RoomsData& rooms) {
  if (rooms.portals.size() > static_cast<std::size_t>(kInvalidPortalIndex)) return Error::kTooManyPortals;
  std::unordered_set<std::string_view> names;
  names.reserve(rooms.portals.size());
  for (const Portal& portal : rooms.portals) {
    Error error = validatePortalDefinition(portal);
    if (error != Error::kNone) return error;
    if (!names.insert(portal.name).second) return Error::kDuplicatePortalName;
  }
  return Error::kNone;
}

Error validatePortalRoomRefs(const Portal& portal, std::size_t room_count) {
  if (portal.room_1 >= room_count || portal.room_2 >= room_count || portal.room_1 == portal.room_2)
    return Error::kBadPortalRoom;
  return Error::kNone;
}

Error validatePortalRoomRefs(const RoomsData& rooms) {
  for (const Portal& portal : rooms.portals) {
    Error error = validatePortalRoomRefs(portal, rooms.definitions.size());
    if (error != Error::kNone) return error;
  }
  return Error::kNone;
}

Error validatePortal(const Portal& portal, std::size_t room_count) {
  if (portal.name.empty() || !validSemanticString(portal.name)) return Error::kBadPortalName;
  Error error = validatePortalRoomRefs(portal, room_count);
  if (error != Error::kNone) return error;
  return validatePortalDefinition(portal);
}

Error validatePortals(const RoomsData& rooms) {
  if (rooms.portals.size() > static_cast<std::size_t>(kInvalidPortalIndex)) return Error::kTooManyPortals;
  std::unordered_set<std::string_view> names;
  names.reserve(rooms.portals.size());
  for (const Portal& portal : rooms.portals) {
    Error error = validatePortal(portal, rooms.definitions.size());
    if (error != Error::kNone) return error;
    if (!names.insert(portal.name).second) return Error::kDuplicatePortalName;
  }
  return Error::kNone;
}

Error validate(const RoomsData& rooms, std::size_t face_count) {
  Error error = validateRooms(rooms);
  if (error != Error::kNone) return error;
  error = validateFaceRooms(rooms, face_count);
  if (error != Error::kNone) return error;
  error = validatePortals(rooms);
  if (error != Error::kNone) return error;
  error = validateRoomDistances(rooms);
  if (error != Error::kNone) return error;
  return Error::kNone;
}

}  // namespace pistoris::rooms
