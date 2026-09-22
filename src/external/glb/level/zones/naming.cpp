// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#include "arx_pistoris/base/math.h"
#include "arx_pistoris/base/status.h"

#include "external/glb/level/zones.h"
#include "external/glb/utils/tokens.h"
#include "internal.h"
#include "modules/scene.h"
#include "utils/name_tokens.h"

#include <cstddef>
#include <format>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace pistoris::glb_level::zone_internal {

using glb::formatFloatToken;
using glb::parseFloatToken;
using glb::parseUnsignedToken;

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

ArxReturnCode parseSettings(std::span<const std::string_view> settings, Settings& out) {
  if (settings.empty()) return ARX_GLB_BAD_LEVEL_ZONE;
  bool color_seen = false;
  bool farclip_seen = false;
  bool volume_seen = false;
  for (std::string_view setting : settings) {
    if (setting.empty()) return ARX_GLB_BAD_LEVEL_ZONE;
    if (setting.starts_with("RGB_")) {
      if (color_seen) return ARX_GLB_BAD_LEVEL_ZONE;
      const std::string_view values = setting.substr(4);
      ArxColor3 color{};
      if (!glb::parseColor3Token(values, color)) return ARX_GLB_BAD_LEVEL_ZONE;
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

std::string settingsHelperName(const Settings& settings, std::string_view label) {
  std::vector<std::string> storage;
  std::vector<std::string_view> tokens = {"SETTINGS"};
  const auto& color = settings.color;
  if (color) storage.push_back("RGB_" + glb::formatColor3Token(*color));
  const auto& farclip = settings.farclip;
  if (farclip) storage.push_back("FARCLIP_" + formatFloatToken(*farclip));
  const auto& volume = settings.volume;
  if (volume && *volume != 100.0f) storage.push_back("VOLUME_" + formatFloatToken(*volume));
  if (storage.empty()) return {};
  for (const std::string& token : storage) tokens.push_back(token);
  tokens.push_back(label);
  return joinDoubleUnderscore(tokens);
}

}  // namespace pistoris::glb_level::zone_internal

namespace pistoris::glb_level {

bool isReservedZoneName(std::string_view name) { return name.starts_with("arx_zone__"); }

}  // namespace pistoris::glb_level
