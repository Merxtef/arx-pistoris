// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#pragma once

#include "arx_pistoris/level.hpp"

#include "modules/navigation.h"
#include "modules/rooms.h"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace pistoris {
struct RoomsData;
}  // namespace pistoris

namespace pistoris::glb_level {

inline constexpr float kRoomParentOffset = 0.0f;
inline constexpr float kAnchorParentOffset = 10.0f;
inline constexpr float kPortalParentOffset = 20.0f;
inline constexpr float kLightParentOffset = 30.0f;
inline constexpr float kEntityParentOffset = 40.0f;
inline constexpr float kZoneParentOffset = 50.0f;
inline constexpr float kPathParentOffset = 60.0f;
inline constexpr float kFogParentOffset = 70.0f;

bool isReservedAnchorName(std::string_view name);
struct ParsedAnchorName {
  std::string name;
  std::optional<float> radius;
  std::optional<float> height;
  std::int16_t flags = 0;
};
std::string anchorNodeName(const Anchor& anchor, std::size_t ordinal, const Level::GlbExportOptions& options);
std::optional<ParsedAnchorName> parseAnchorNodeName(std::string_view node_name);
bool isReservedRoomName(std::string_view name);
std::string roomNodeName(const Room& room);
std::optional<std::string> roomNameFromNode(std::string_view node_name);
bool isReservedPortalName(std::string_view name);
struct ParsedPortalName {
  std::string name;
  std::string room_1;
  std::string room_2;
};
std::string portalNodeName(const Portal& portal, const RoomsData& rooms);
std::optional<ParsedPortalName> parsePortalNodeName(std::string_view node_name);
ArxVector3 bottomCenter(const ArxAabb& bounds);

}  // namespace pistoris::glb_level
