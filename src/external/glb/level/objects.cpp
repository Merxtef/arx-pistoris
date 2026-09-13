// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#include "objects.h"

#include "arx_pistoris/base/math.h"
#include "arx_pistoris/level.hpp"

#include "coordinates.h"
#include "external/glb/utils/tokens.h"
#include "modules/navigation.h"
#include "modules/rooms.h"
#include "utils/name_tokens.h"

#include <cstddef>
#include <format>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace pistoris::glb_level {

using glb::parseFloatToken;

constexpr std::string_view kAnchorPrefix = "arx_anchor__";
constexpr std::string_view kRoomPrefix = "arx_room__";
constexpr std::string_view kPortalPrefix = "arx_portal__";

bool isReservedAnchorName(std::string_view name) { return name.starts_with(kAnchorPrefix); }

std::string anchorNodeName(const Anchor& anchor, std::size_t ordinal, const Level::GlbExportOptions& options) {
  std::string fallback_name = std::format("anchor_{}", ordinal);
  std::string_view anchor_name = anchor.name.empty() ? std::string_view(fallback_name) : std::string_view(anchor.name);
  std::vector<std::string> storage = {"arx_anchor"};
  if (anchor.radius != kDefaultAnchorRadius) {
    storage.push_back(std::format("RADIUS_{}", toGlbLength(anchor.radius, options)));
  }
  if (anchor.height != kDefaultAnchorHeight) {
    storage.push_back(std::format("HEIGHT_{}", toGlbLength(-anchor.height, options)));
  }
  if ((anchor.flags & kAnchorFlagBlocked) != 0) storage.emplace_back("BLOCKED");
  storage.emplace_back(anchor_name);
  std::vector<std::string_view> tokens(storage.begin(), storage.end());
  return joinDoubleUnderscore(tokens);
}

std::optional<ParsedAnchorName> parseAnchorNodeName(std::string_view node_name) {
  if (!isReservedAnchorName(node_name)) return std::nullopt;
  std::string_view remaining = node_name.substr(kAnchorPrefix.size());
  std::vector<std::string_view> tokens;
  splitDoubleUnderscore(remaining, tokens);
  std::string_view name = tokens.back();
  if (name.empty()) return std::nullopt;

  ParsedAnchorName parsed;
  parsed.name = std::string(name);
  bool radius_seen = false;
  bool height_seen = false;
  bool blocked_seen = false;
  for (std::string_view token : std::span<const std::string_view>(tokens).first(tokens.size() - 1)) {
    float value = 0.0f;
    if (token.starts_with("RADIUS_")) {
      if (radius_seen || !parseFloatToken(token.substr(7), value) || value < 0.0f) return std::nullopt;
      parsed.radius = value;
      radius_seen = true;
    } else if (token.starts_with("HEIGHT_")) {
      if (height_seen || !parseFloatToken(token.substr(7), value) || value < 0.0f) return std::nullopt;
      parsed.height = value;
      height_seen = true;
    } else if (token == "BLOCKED") {
      if (blocked_seen) return std::nullopt;
      parsed.flags |= kAnchorFlagBlocked;
      blocked_seen = true;
    } else {
      return std::nullopt;
    }
  }
  return parsed;
}

bool isReservedPortalName(std::string_view name) { return name.starts_with(kPortalPrefix); }

bool isReservedRoomName(std::string_view name) { return name.starts_with(kRoomPrefix); }

std::string roomNodeName(const Room& room) { return std::format("arx_room__{}", room.name); }

std::optional<std::string> roomNameFromNode(std::string_view node_name) {
  if (!isReservedRoomName(node_name)) return std::nullopt;
  const std::string_view name = node_name.substr(kRoomPrefix.size());
  if (name.empty() || hasDoubleUnderscore(name)) return std::nullopt;
  return std::string(name);
}

std::string portalNodeName(const Portal& portal, const RoomsData& rooms) {
  std::string_view room_1 = portal.room_1 < rooms.definitions.size()
                                ? std::string_view(rooms.definitions[portal.room_1].name)
                                : std::string_view{};
  std::string_view room_2 = portal.room_2 < rooms.definitions.size()
                                ? std::string_view(rooms.definitions[portal.room_2].name)
                                : std::string_view{};
  return joinDoubleUnderscore({"arx_portal", room_1, room_2, portal.name});
}

std::optional<ParsedPortalName> parsePortalNodeName(std::string_view node_name) {
  if (!isReservedPortalName(node_name)) return std::nullopt;
  std::string_view remaining = node_name.substr(kPortalPrefix.size());
  std::vector<std::string_view> tokens;
  splitDoubleUnderscore(remaining, tokens);
  if (tokens.size() != 3) return std::nullopt;
  ParsedPortalName parsed;
  parsed.room_1 = std::string(tokens[0]);
  parsed.room_2 = std::string(tokens[1]);
  parsed.name = std::string(tokens[2]);
  if (parsed.name.empty() || parsed.room_1.empty() || parsed.room_2.empty()) return std::nullopt;
  return parsed;
}

ArxVector3 bottomCenter(const ArxAabb& bounds) {
  return {(bounds.min.x + bounds.max.x) * 0.5f, bounds.max.y, (bounds.min.z + bounds.max.z) * 0.5f};
}

}  // namespace pistoris::glb_level
