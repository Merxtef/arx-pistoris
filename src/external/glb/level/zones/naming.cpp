// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#include "arx_pistoris/arx_math.h"
#include "arx_pistoris/pistoris_types.h"

#include "external/glb/level/zones.h"
#include "external/glb/utils/level/tokens.h"
#include "internal.h"
#include "modules/scene.h"
#include "utils/log.h"
#include "utils/name_tokens.h"

#include <cstddef>
#include <format>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace pistoris::glb_level::zone_internal {

void logFailure(std::size_t node_index, std::string_view name, std::string_view reason) {
  log(ARX_LOG_DEBUG, std::format("GLB -> Level object failure: zone node {} '{}' {}", node_index, name, reason));
}

ParsedName parseName(std::string_view name, std::size_t node_index) {
  constexpr std::string_view kPrefix = "arx_zone__";
  ParsedName parsed;
  const std::string_view rest = name.substr(kPrefix.size());
  std::vector<std::string_view> tokens;
  splitDoubleUnderscore(rest, tokens);
  std::string parsed_name(rest);
  if (tokens.size() > 1) {
    if (const auto ordinal = parseUnsignedToken(tokens[0])) {
      parsed.ordinal = ordinal;
      parsed_name = joinDoubleUnderscore(std::span<const std::string_view>(tokens).subspan(1));
    }
  }
  parsed.name = parsed_name.empty() ? std::format("zone_{}", node_index) : std::move(parsed_name);
  return parsed;
}

std::string nodeName(const Zone& zone, std::size_t ordinal) {
  return std::format("arx_zone__{:03}__{}", ordinal, zone.name);
}

ArxReturnCode parseSettings(std::string_view payload, Settings& out) {
  if (payload.empty()) return ARX_GLB_BAD_LEVEL_ZONE;
  bool color_seen = false;
  bool farclip_seen = false;
  bool volume_seen = false;
  std::vector<std::string_view> tokens;
  splitDoubleUnderscore(payload, tokens);
  for (std::string_view setting : tokens) {
    if (setting.empty()) return ARX_GLB_BAD_LEVEL_ZONE;
    if (setting.starts_with("RGB_")) {
      if (color_seen) return ARX_GLB_BAD_LEVEL_ZONE;
      const std::string_view values = setting.substr(4);
      const std::size_t first = values.find('_');
      const std::size_t second = first == std::string_view::npos ? first : values.find('_', first + 1);
      if (first == std::string_view::npos || second == std::string_view::npos ||
          values.find('_', second + 1) != std::string_view::npos)
        return ARX_GLB_BAD_LEVEL_ZONE;
      ArxColor3 color{};
      if (!parseFloatToken(values.substr(0, first), color.r) ||
          !parseFloatToken(values.substr(first + 1, second - first - 1), color.g) ||
          !parseFloatToken(values.substr(second + 1), color.b))
        return ARX_GLB_BAD_LEVEL_ZONE;
      out.color = color;
      color_seen = true;
    } else if (setting.starts_with("FARCLIP_")) {
      if (farclip_seen) return ARX_GLB_BAD_LEVEL_ZONE;
      float value = 0.0f;
      if (!parseFloatToken(setting.substr(8), value)) return ARX_GLB_BAD_LEVEL_ZONE;
      out.farclip = value;
      farclip_seen = true;
    } else if (setting.starts_with("VOLUME_")) {
      if (volume_seen) return ARX_GLB_BAD_LEVEL_ZONE;
      float value = 0.0f;
      if (!parseFloatToken(setting.substr(7), value)) return ARX_GLB_BAD_LEVEL_ZONE;
      out.volume = value;
      volume_seen = true;
    } else {
      return ARX_GLB_BAD_LEVEL_ZONE;
    }
  }
  return ARX_OK;
}

std::string settingsHelperName(const Zone& zone) {
  std::vector<std::string> storage;
  std::vector<std::string_view> tokens = {"SETTINGS"};
  const auto& color = zone.color;
  if (color)
    storage.push_back(std::format(
        "RGB_{}_{}_{}", formatFloatToken(color->r), formatFloatToken(color->g), formatFloatToken(color->b)));
  const auto& farclip = zone.farclip;
  if (farclip) storage.push_back("FARCLIP_" + formatFloatToken(*farclip));
  const auto& ambiance = zone.ambiance;
  if (ambiance && ambiance->volume != 100.0f) storage.push_back("VOLUME_" + formatFloatToken(ambiance->volume));
  if (storage.empty()) return {};
  for (const std::string& token : storage) tokens.push_back(token);
  tokens.push_back(zone.name);
  return joinDoubleUnderscore(tokens);
}

}  // namespace pistoris::glb_level::zone_internal

namespace pistoris::glb_level {

bool isReservedZoneName(std::string_view name) { return name.starts_with("arx_zone__"); }

}  // namespace pistoris::glb_level
