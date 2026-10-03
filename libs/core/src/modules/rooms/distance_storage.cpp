// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#include "arx_pistoris/base/indices.h"

#include "modules/rooms.h"
#include "utils/packed_index.h"

#include <cassert>
#include <cstddef>

namespace pistoris::rooms {

std::size_t roomDistancePairCount(std::size_t room_count) { return packed::symmetricPairCount(room_count); }

std::size_t roomDistancePairIndex(std::size_t first_room, std::size_t second_room) {
  return packed::symmetricPairIndex(first_room, second_room);
}

bool hasCompleteRoomDistances(const RoomDistances& distances, std::size_t room_count) {
  const std::size_t expected = roomDistancePairCount(room_count);
  return distances.size() == expected;
}

RoomDistance roomDistance(const RoomsData& rooms, RoomIndex low_room, RoomIndex high_room) noexcept {
  assert(low_room < high_room);
  assert(static_cast<std::size_t>(high_room) < rooms.definitions.size());
  assert(rooms.distances.empty() || hasCompleteRoomDistances(rooms.distances, rooms.definitions.size()));
  if (rooms.distances.empty()) return {};
  return rooms.distances[roomDistancePairIndex(low_room, high_room)];
}

void resetRoomDistances(RoomDistances& distances, std::size_t room_count) {
  const std::size_t expected = roomDistancePairCount(room_count);
  distances.assign(expected, {});
}

}  // namespace pistoris::rooms
