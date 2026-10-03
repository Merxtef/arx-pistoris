// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#pragma once

#include "modules/rooms.h"

#include <cstddef>
#include <span>
#include <string>

// NOLINTNEXTLINE(readability-identifier-naming)
struct cgltf_data;

namespace pistoris::glb {
struct NodeGraph;
}

namespace pistoris::glb_level {

struct ImportUnits;

std::string roomDistanceRoomMetadataJson(std::size_t id);
std::string roomDistancePortalMetadataJson(std::size_t id);
std::string roomDistanceRootMetadataJson(const RoomsData& rooms);

void restoreRoomDistanceMetadata(const cgltf_data& data, const glb::NodeGraph& graph,
                                 std::span<const std::size_t> room_nodes, std::span<const std::size_t> portal_nodes,
                                 const ImportUnits& units, RoomsData& rooms);

}  // namespace pistoris::glb_level
