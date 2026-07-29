// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#include "modules/rooms.h"
#include "utils/packed_index.h"

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

void initializeRoomDistances(RoomDistances& distances, std::size_t room_count) {
  const std::size_t expected = roomDistancePairCount(room_count);
  distances.assign(expected, {});
}

}  // namespace pistoris::rooms
