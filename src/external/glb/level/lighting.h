// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#pragma once

#include "arx_pistoris/base/status.h"
#include "arx_pistoris/level.hpp"

#include "cgltf/cgltf.h"
#include "modules/lights.h"

#include <cstddef>
#include <string>
#include <string_view>

namespace pistoris::glb_level {

struct ParsedLightName {
  std::string name;
  float fallstart = 0.0f;
  float fallend = 0.0f;
  bool has_fallend = false;
  bool generated = false;
  bool repaired = false;
};

bool isReservedLightName(std::string_view name);
std::string lightNodeName(const Light& light, const Level::GlbExportOptions& options);
std::string lightSettingsHelperName(std::string_view name, const Light& light);
std::string lightFlagHelperName(std::string_view name, LightFlags flags);
std::string lightEffectHelperName(std::string_view name, const Light& light, const Level::GlbExportOptions& options);
ArxReturnCode parseLightName(std::string_view node_name, std::string_view resource_name, std::size_t ordinal,
                             float fallend, ParsedLightName& out);
ArxReturnCode parseLightSettings(const cgltf_node& node, bool real_light, bool has_point_light, const ArxColor3& color,
                                 float intensity, Light& light);
ArxReturnCode parseLightFlags(const cgltf_node& node, LightFlags& out);
ArxReturnCode parseLightEffects(const cgltf_node& node, Light& light);
bool hasEffectFields(const Light& light);
bool hasSettingsFields(const Light& light);

}  // namespace pistoris::glb_level
