// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#include "arx/llf.h"

#include "arx_pistoris/native/fts.hpp"
#include "arx_pistoris/native/llf.hpp"
#include "arx_pistoris/pistoris_types.h"

#include "arx/conversion/level/internal.h"
#include "modules/lights.h"

#include <algorithm>
#include <cstddef>
#include <format>
#include <utility>
#include <vector>

namespace pistoris::arx_level_conversion {

void buildLlfModules(const fts::Data& fts, const llf::Data& llf, LightingData& out, NativeBuildWarnings& warnings) {
  out.lights.reserve(llf.lights.size());
  for (std::size_t i = 0; i < llf.lights.size(); ++i) {
    const llf::Light& source = llf.lights[i];
    Light light;
    light.name = std::format("light_{}", i);
    light.position = {source.position.x + fts.scene.Mscenepos.x,
                      source.position.y + fts.scene.Mscenepos.y,
                      source.position.z + fts.scene.Mscenepos.z};
    light.color = source.color;
    light.fallstart = source.fallstart;
    light.fallend = source.fallend;
    if (light.fallstart == light.fallend && light.fallend > 0.0f) {
      light.fallstart = std::max(light.fallstart - 1.0f, 0.0f);
      ++warnings.equal_light_falloffs;
    }
    light.intensity = source.intensity;
    light.flicker = source.flicker;
    light.effect_radius = source.effect_radius;
    light.effect_frequency = source.effect_frequency;
    light.effect_size = source.effect_size;
    light.effect_speed = source.effect_speed;
    light.flare_size = source.flare_size;
    light.flags = source.flags;
    out.lights.push_back(std::move(light));
  }
}

ArxReturnCode bakeLlf(const LightingData& lighting, const std::vector<ArxColor3>& baked_colors, llf::Data& out) {
  llf::Data llf;
  llf.colors = baked_colors;
  llf.lights.reserve(lighting.lights.size());
  for (const Light& source : lighting.lights) {
    llf.lights.push_back({source.position,
                          source.color,
                          source.fallstart,
                          source.fallend,
                          source.intensity,
                          source.flicker,
                          source.effect_radius,
                          source.effect_frequency,
                          source.effect_size,
                          source.effect_speed,
                          source.flare_size,
                          source.flags});
  }
  ArxReturnCode rc = validateLlf(&llf);
  if (rc != ARX_OK) return rc;
  out = std::move(llf);
  return ARX_OK;
}

}  // namespace pistoris::arx_level_conversion
