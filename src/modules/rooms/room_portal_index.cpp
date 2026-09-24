// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#include "arx_pistoris/base/indices.h"

#include "modules/rooms.h"
#include "modules/rooms/internal.h"

#include <cstddef>
#include <span>
#include <utility>
#include <vector>

namespace pistoris::rooms {

std::span<const PortalIndex> RoomPortalIndex::roomPortals(RoomIndex room) const noexcept {
  if (static_cast<std::size_t>(room) + 1U >= offsets_.size()) return {};
  return std::span<const PortalIndex>(portals_).subspan(offsets_[room], offsets_[room + 1U] - offsets_[room]);
}

Error buildRoomPortalIndex(const RoomsData& rooms, RoomPortalIndex& out) {
  Error error = validatePortalCount(rooms.portals.size());
  if (error != Error::kNone) return error;

  RoomPortalIndex built;
  built.offsets_.assign(rooms.definitions.size() + 1U, 0);
  for (const Portal& portal : rooms.portals) {
    error = validatePortal(portal, rooms.definitions.size());
    if (error != Error::kNone) return error;
    ++built.offsets_[static_cast<std::size_t>(portal.room_1) + 1U];
    ++built.offsets_[static_cast<std::size_t>(portal.room_2) + 1U];
  }
  for (std::size_t room = 1; room < built.offsets_.size(); ++room) built.offsets_[room] += built.offsets_[room - 1U];

  built.portals_.resize(built.offsets_.back());
  std::vector<std::size_t> write_offsets = built.offsets_;
  for (std::size_t portal = 0; portal < rooms.portals.size(); ++portal) {
    const Portal& value = rooms.portals[portal];
    const PortalIndex index = static_cast<PortalIndex>(portal);
    built.portals_[write_offsets[value.room_1]++] = index;
    built.portals_[write_offsets[value.room_2]++] = index;
  }
  out = std::move(built);
  return Error::kNone;
}

}  // namespace pistoris::rooms
