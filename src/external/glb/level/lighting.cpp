// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#include "lighting.h"

#include "arx_pistoris/arx_math.h"
#include "arx_pistoris/flags.h"
#include "arx_pistoris/level.hpp"
#include "arx_pistoris/pistoris_types.h"

#include "coordinates.h"
#include "external/glb/utils/level/tokens.h"
#include "external/glb/utils/node.h"
#include "modules/lights.h"
#include "utils/name_tokens.h"

#include <array>
#include <cmath>
#include <cstddef>
#include <format>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace pistoris::glb_level {
namespace {

struct LightFlagName {
  LightFlags bit;
  std::string_view name;
};

constexpr std::array kLightFlagNames = {
    LightFlagName{kLightFlagSemidynamic, "SEMIDYNAMIC"},
    LightFlagName{kLightFlagExtinguishable, "EXTINGUISHABLE"},
    LightFlagName{kLightFlagStartExtinguished, "STARTEXTINGUISHED"},
    LightFlagName{kLightFlagSpawnFire, "SPAWNFIRE"},
    LightFlagName{kLightFlagSpawnSmoke, "SPAWNSMOKE"},
    LightFlagName{kLightFlagOff, "OFF"},
    LightFlagName{kLightFlagColorLegacy, "COLORLEGACY"},
    LightFlagName{kLightFlagNoCasted, "NOCASTED"},
    LightFlagName{kLightFlagFixFlareSize, "FIXFLARESIZE"},
    LightFlagName{kLightFlagFireplace, "FIREPLACE"},
    LightFlagName{kLightFlagNoIgnit, "NOIGNIT"},
    LightFlagName{kLightFlagFlare, "FLARE"},
};

constexpr std::string_view kSettingsPrefix = "SETTINGS__";
constexpr std::string_view kFlagsPrefix = "FLAGS__";
constexpr std::string_view kEffectPrefix = "EFFECT__";
constexpr std::string_view kLightPrefix = "arx_light__";

bool zero(const ArxColor3& value) { return value.r == 0.0f && value.g == 0.0f && value.b == 0.0f; }

bool parseColor(std::string_view text, ArxColor3& out) {
  std::size_t first = text.find('_');
  if (first == std::string_view::npos) return false;
  std::size_t second = text.find('_', first + 1);
  if (second == std::string_view::npos) return false;
  float r = 0.0f;
  float g = 0.0f;
  float b = 0.0f;
  if (!parseFloatToken(text.substr(0, first), r) || !parseFloatToken(text.substr(first + 1, second - first - 1), g) ||
      !parseFloatToken(text.substr(second + 1), b))
    return false;
  out = {r, g, b};
  return true;
}

}  // namespace

bool isReservedLightName(std::string_view name) { return name.starts_with(kLightPrefix); }

std::string lightNodeName(const Light& light, const Level::GlbExportOptions& options) {
  std::vector<std::string> storage = {"arx_light"};
  if (light.fallstart != light.fallend * 0.5f)
    storage.push_back(std::format("FALLSTART_{}", toGlbLength(light.fallstart, options)));
  if (light.fallend != 0.0f) storage.push_back(std::format("FALLEND_{}", toGlbLength(light.fallend, options)));
  storage.push_back(light.name);
  std::vector<std::string_view> tokens(storage.begin(), storage.end());
  return joinDoubleUnderscore(tokens);
}

std::string lightSettingsHelperName(std::string_view name, const Light& light) {
  std::string rgb = std::format("RGB_{}_{}_{}", light.color.r, light.color.g, light.color.b);
  std::string intensity = std::format("INTENSITY_{}", light.intensity);
  return joinDoubleUnderscore({"SETTINGS", rgb, intensity, name});
}

std::string lightFlagHelperName(std::string_view name, LightFlags flags) {
  std::vector<std::string_view> tokens = {"FLAGS"};
  for (const LightFlagName& entry : kLightFlagNames) {
    if ((flags & entry.bit) == 0) continue;
    tokens.push_back(entry.name);
  }
  tokens.push_back(name);
  return joinDoubleUnderscore(tokens);
}

std::string lightEffectHelperName(std::string_view name, const Light& light, const Level::GlbExportOptions& options) {
  std::vector<std::string> storage;
  std::vector<std::string_view> tokens = {"EFFECT"};
  auto append = [&](std::string_view key, auto value) { storage.push_back(std::format("{}_{}", key, value)); };
  if (!zero(light.flicker))
    append("FLICKER", std::format("{}_{}_{}", light.flicker.r, light.flicker.g, light.flicker.b));
  if (light.effect_radius != 0.0f) append("RADIUS", toGlbLength(light.effect_radius, options));
  if (light.effect_frequency != 0.0f) append("FREQUENCY", light.effect_frequency);
  if (light.effect_size != 0.0f) append("SIZE", light.effect_size);
  if (light.effect_speed != 0.0f) append("SPEED", light.effect_speed);
  if (light.flare_size != 0.0f) append("FLARESIZE", light.flare_size);
  for (const std::string& token : storage) tokens.push_back(token);
  tokens.push_back(name);
  return joinDoubleUnderscore(tokens);
}

ArxReturnCode parseLightName(std::string_view node_name, std::string_view resource_name, std::size_t ordinal,
                             float fallend, ParsedLightName& out) {
  ParsedLightName parsed;
  std::string_view effective = !node_name.empty() ? node_name : resource_name;
  const bool reserved_name = effective.starts_with(kLightPrefix);
  if (reserved_name) effective.remove_prefix(kLightPrefix.size());
  if (!reserved_name) {
    parsed.name = effective.empty() ? std::format("light_{}", ordinal) : std::string(effective);
    parsed.generated = effective.empty();
    parsed.fallend = fallend;
    parsed.has_fallend = fallend > 0.0f;
    parsed.fallstart = parsed.fallend == 0.0f ? 0.0f : parsed.fallend * 0.5f;
    out = std::move(parsed);
    return ARX_OK;
  }
  std::vector<std::string_view> tokens;
  splitDoubleUnderscore(effective, tokens);
  const std::string_view base = tokens.back();
  if (base.empty() || hasDoubleUnderscore(base)) return ARX_GLB_BAD_LEVEL_LIGHT;

  std::string_view fallstart_text;
  std::string_view fallend_text;
  bool fallstart_seen = false;
  bool fallend_seen = false;
  for (std::string_view token : std::span<const std::string_view>(tokens).first(tokens.size() - 1)) {
    if (token.starts_with("FALLSTART_")) {
      if (fallstart_seen || fallend_seen) return ARX_GLB_BAD_LEVEL_LIGHT;
      fallstart_seen = true;
      fallstart_text = token.substr(10);
    } else if (token.starts_with("FALLEND_")) {
      if (fallend_seen) return ARX_GLB_BAD_LEVEL_LIGHT;
      fallend_seen = true;
      fallend_text = token.substr(8);
    } else {
      return ARX_GLB_BAD_LEVEL_LIGHT;
    }
  }

  parsed.name = base;

  parsed.fallend = fallend;
  parsed.has_fallend = fallend > 0.0f;
  if (fallend_seen) {
    float value = 0.0f;
    if (!parseFloatToken(fallend_text, value) || value <= 0.0f) return ARX_GLB_BAD_LEVEL_LIGHT;
    parsed.fallend = value;
    parsed.has_fallend = true;
  }

  parsed.fallstart = parsed.fallend == 0.0f ? 0.0f : parsed.fallend * 0.5f;
  if (fallstart_seen) {
    float value = 0.0f;
    if (parseFloatToken(fallstart_text, value) && value >= 0.0f && value < parsed.fallend) {
      parsed.fallstart = value;
    } else {
      parsed.repaired = true;
    }
  }
  out = std::move(parsed);
  return ARX_OK;
}

ArxReturnCode parseLightSettings(const cgltf_node& node, bool real_light, bool has_point_light, const ArxColor3& color,
                                 float intensity, Light& light) {
  light.color = real_light ? ArxColor3{1.0f, 1.0f, 1.0f} : ArxColor3{};
  light.intensity = real_light ? 1.0f : 0.0f;
  if (has_point_light) {
    light.color = color;
    light.intensity = intensity;
  }
  const ArxColor3 default_color = light.color;
  const float default_intensity = light.intensity;
  bool selected = false;
  for (std::size_t child_index = 0; child_index < node.children_count; ++child_index) {
    const cgltf_node* child = node.children[child_index];
    if (child == nullptr || child->name == nullptr) continue;
    std::string_view name(child->name);
    if (!name.starts_with(kSettingsPrefix)) continue;
    const std::size_t label_separator = name.rfind("__");
    if (label_separator == std::string_view::npos || label_separator < kSettingsPrefix.size() ||
        label_separator + 2 == name.size())
      return ARX_GLB_BAD_LEVEL_LIGHT;
    if (!glb::simpleEmptyNode(*child)) return ARX_GLB_BAD_LEVEL_LIGHT;

    ArxColor3 parsed_color = default_color;
    float parsed_intensity = default_intensity;
    bool seen_rgb = false;
    bool seen_intensity = false;
    std::vector<std::string_view> tokens;
    splitDoubleUnderscore(name.substr(kSettingsPrefix.size(), label_separator - kSettingsPrefix.size()), tokens);
    for (std::string_view token : tokens) {
      if (token.starts_with("RGB_")) {
        ArxColor3 value{};
        if (seen_rgb || !parseColor(token.substr(4), value) || value.r < 0.0f || value.r > 1.0f || value.g < 0.0f ||
            value.g > 1.0f || value.b < 0.0f || value.b > 1.0f)
          return ARX_GLB_BAD_LEVEL_LIGHT;
        parsed_color = value;
        seen_rgb = true;
      } else if (token.starts_with("INTENSITY_")) {
        float value = 0.0f;
        if (seen_intensity || !parseFloatToken(token.substr(10), value) || value < 0.0f) return ARX_GLB_BAD_LEVEL_LIGHT;
        parsed_intensity = value;
        seen_intensity = true;
      } else {
        return ARX_GLB_BAD_LEVEL_LIGHT;
      }
    }
    if (!selected) {
      light.color = parsed_color;
      light.intensity = parsed_intensity;
      selected = true;
    }
  }
  return ARX_OK;
}

ArxReturnCode parseLightFlags(const cgltf_node& node, LightFlags& out) {
  out = 0;
  bool selected = false;
  for (std::size_t child_index = 0; child_index < node.children_count; ++child_index) {
    const cgltf_node* child = node.children[child_index];
    if (child == nullptr || child->name == nullptr) continue;
    std::string_view name(child->name);
    if (!name.starts_with(kFlagsPrefix)) continue;
    const std::size_t label_separator = name.rfind("__");
    if (label_separator == std::string_view::npos || label_separator < kFlagsPrefix.size() ||
        label_separator + 2 == name.size())
      return ARX_GLB_BAD_LEVEL_LIGHT;
    if (!glb::simpleEmptyNode(*child)) return ARX_GLB_BAD_LEVEL_LIGHT;

    LightFlags flags = 0;
    std::vector<std::string_view> tokens;
    splitDoubleUnderscore(name.substr(kFlagsPrefix.size(), label_separator - kFlagsPrefix.size()), tokens);
    for (std::string_view token : tokens) {
      const LightFlagName* hit = nullptr;
      for (const LightFlagName& entry : kLightFlagNames) {
        if (entry.name == token) {
          hit = &entry;
          break;
        }
      }
      if (hit == nullptr) return ARX_GLB_BAD_LEVEL_LIGHT;
      flags |= hit->bit;
    }
    if (!selected) {
      out = flags;
      selected = true;
    }
  }
  return ARX_OK;
}

ArxReturnCode parseLightEffects(const cgltf_node& node, Light& light) {
  light.flicker = {};
  light.effect_radius = 0.0f;
  light.effect_frequency = 0.0f;
  light.effect_size = 0.0f;
  light.effect_speed = 0.0f;
  light.flare_size = 0.0f;
  bool selected = false;
  for (std::size_t child_index = 0; child_index < node.children_count; ++child_index) {
    const cgltf_node* child = node.children[child_index];
    if (child == nullptr || child->name == nullptr) continue;
    std::string_view name(child->name);
    if (!name.starts_with(kEffectPrefix)) continue;
    const std::size_t label_separator = name.rfind("__");
    if (label_separator == std::string_view::npos || label_separator < kEffectPrefix.size() ||
        label_separator + 2 == name.size())
      return ARX_GLB_BAD_LEVEL_LIGHT;
    if (!glb::simpleEmptyNode(*child)) return ARX_GLB_BAD_LEVEL_LIGHT;

    ArxColor3 flicker = {};
    float effect_radius = 0.0f;
    float effect_frequency = 0.0f;
    float effect_size = 0.0f;
    float effect_speed = 0.0f;
    float flare_size = 0.0f;
    bool seen_flicker = false;
    bool seen_radius = false;
    bool seen_frequency = false;
    bool seen_size = false;
    bool seen_speed = false;
    bool seen_flaresize = false;
    std::vector<std::string_view> tokens;
    splitDoubleUnderscore(name.substr(kEffectPrefix.size(), label_separator - kEffectPrefix.size()), tokens);
    for (std::string_view token : tokens) {
      if (token.starts_with("FLICKER_")) {
        if (seen_flicker || !parseColor(token.substr(8), flicker)) return ARX_GLB_BAD_LEVEL_LIGHT;
        seen_flicker = true;
      } else if (token.starts_with("RADIUS_")) {
        if (seen_radius || !parseFloatToken(token.substr(7), effect_radius)) return ARX_GLB_BAD_LEVEL_LIGHT;
        seen_radius = true;
      } else if (token.starts_with("FREQUENCY_")) {
        if (seen_frequency || !parseFloatToken(token.substr(10), effect_frequency)) return ARX_GLB_BAD_LEVEL_LIGHT;
        seen_frequency = true;
      } else if (token.starts_with("SIZE_")) {
        if (seen_size || !parseFloatToken(token.substr(5), effect_size)) return ARX_GLB_BAD_LEVEL_LIGHT;
        seen_size = true;
      } else if (token.starts_with("SPEED_")) {
        if (seen_speed || !parseFloatToken(token.substr(6), effect_speed)) return ARX_GLB_BAD_LEVEL_LIGHT;
        seen_speed = true;
      } else if (token.starts_with("FLARESIZE_")) {
        if (seen_flaresize || !parseFloatToken(token.substr(10), flare_size)) return ARX_GLB_BAD_LEVEL_LIGHT;
        seen_flaresize = true;
      } else {
        return ARX_GLB_BAD_LEVEL_LIGHT;
      }
    }
    if (!selected) {
      light.flicker = flicker;
      light.effect_radius = effect_radius;
      light.effect_frequency = effect_frequency;
      light.effect_size = effect_size;
      light.effect_speed = effect_speed;
      light.flare_size = flare_size;
      selected = true;
    }
  }
  return ARX_OK;
}

bool hasEffectFields(const Light& light) {
  return !zero(light.flicker) || light.effect_radius != 0.0f || light.effect_frequency != 0.0f ||
         light.effect_size != 0.0f || light.effect_speed != 0.0f || light.flare_size != 0.0f;
}

bool hasSettingsFields(const Light& light) {
  if (light.fallend == 0.0f) return !zero(light.color) || light.intensity != 0.0f;
  return !std::isfinite(light.intensity) || light.intensity != 1.0f || light.color.r != 1.0f || light.color.g != 1.0f ||
         light.color.b != 1.0f;
}

}  // namespace pistoris::glb_level
