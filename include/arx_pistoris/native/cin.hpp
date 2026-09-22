// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#pragma once

#include "arx_pistoris/base/math.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace pistoris {

inline constexpr std::array<char, 4> kCinMagic = {'K', 'F', 'A', '\0'};
constexpr std::int32_t kCinVersion175 = 0x0001004b;
constexpr std::int32_t kCinVersion = 0x0001004c;
constexpr std::size_t kCinMaxSounds = 256;

namespace cin {

struct Bitmap {
  std::int32_t subdivision_scale = 1;
  std::string path;
};

struct Sound {
  std::string path;
  bool speech = false;
};

struct Light {
  ArxVector3 position = {};
  float fall_in = 0.0f;
  float fall_out = 0.0f;
  ArxColor3 color = {};
  float intensity = -1.0f;
  float random_intensity = 0.0f;
};

struct Keyframe {
  std::int32_t frame = 0;
  std::int32_t bitmap = 0;
  std::uint32_t effects = 0;
  std::int16_t interpolation = 1;
  std::int16_t crossfade = 0;
  ArxVector3 camera_position = {};
  float camera_roll = 0.0f;
  std::uint32_t color = 0xffffffffU;
  std::uint32_t secondary_color = 0xffffffffU;
  std::uint32_t flash_color = 0xffffffffU;
  float flash_decay = 0.0f;
  Light light;
  ArxVector3 bitmap_position = {};
  float bitmap_roll = 0.0f;
  float outgoing_speed = 1.0f;
  std::int32_t sound = -1;
};

struct Data {
  std::vector<Bitmap> bitmaps;
  std::vector<Sound> sounds;
  std::int32_t end_frame = 0;
  float fps = 25.0f;
  std::vector<Keyframe> keyframes;
};

}  // namespace cin

using Cin = cin::Data;

}  // namespace pistoris
