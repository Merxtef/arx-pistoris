
// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#include "native/lighting_layout.h"

#include "arx_pistoris/base/flags.h"
#include "arx_pistoris/native/llf.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>

namespace pistoris::native_lighting {
namespace {

std::uint8_t colorByte(float value) {
  return static_cast<std::uint8_t>(std::lround(std::clamp(value, 0.0f, 1.0f) * 255.0f));
}

}  // namespace

llf::Light decode(const Light& source) {
  return {
      source.position,
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
      static_cast<LightFlags>(source.flags),
  };
}

Light encode(const llf::Light& source) {
  Light result;
  result.position = source.position;
  result.color = source.color;
  result.fallstart = source.fallstart;
  result.fallend = source.fallend;
  result.intensity = source.intensity;
  result.flicker = source.flicker;
  result.effect_radius = source.effect_radius;
  result.effect_frequency = source.effect_frequency;
  result.effect_size = source.effect_size;
  result.effect_speed = source.effect_speed;
  result.flare_size = source.flare_size;
  result.flags = static_cast<std::int32_t>(source.flags);
  return result;
}

ArxColor3 decodeColor(std::uint32_t bgra) {
  constexpr float kScale = 1.0f / 255.0f;
  return {
      static_cast<float>((bgra >> 16U) & 0xffU) * kScale,
      static_cast<float>((bgra >> 8U) & 0xffU) * kScale,
      static_cast<float>(bgra & 0xffU) * kScale,
  };
}

std::uint32_t encodeColor(const ArxColor3& color) {
  return static_cast<std::uint32_t>(colorByte(color.b)) | (static_cast<std::uint32_t>(colorByte(color.g)) << 8U) |
         (static_cast<std::uint32_t>(colorByte(color.r)) << 16U) | 0xff000000U;
}

}  // namespace pistoris::native_lighting
