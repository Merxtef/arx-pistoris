// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#pragma once

#include "arx_pistoris/native/cin.hpp"

#include "native/cin_resource_path.h"

#include <array>
#include <cassert>
#include <cstdint>
#include <cstring>
#include <string>
#include <string_view>
#include <type_traits>
#include <vector>

template <class T>
void appendCinValue(std::vector<std::uint8_t>& out, const T& value) {
  static_assert(std::is_trivially_copyable_v<T>);
  const std::size_t offset = out.size();
  out.resize(offset + sizeof(T));
  std::memcpy(out.data() + offset, &value, sizeof(T));
}

inline void appendCinString(std::vector<std::uint8_t>& out, std::string_view value) {
  out.insert(out.end(), value.begin(), value.end());
  out.push_back(0);
}

inline pistoris::cin::Keyframe makeCinKey(std::int32_t frame) {
  pistoris::cin::Keyframe key;
  key.frame = frame;
  key.bitmap = 0;
  key.camera_position = {10.0f + static_cast<float>(frame), 20.0f, 30.0f};
  key.outgoing_speed = 1.0f;
  return key;
}

inline pistoris::Cin makeCinData() {
  pistoris::Cin result;
  result.bitmaps = {{2, "graph/interface/illustrations/test"}};
  result.sounds = {{"cinematic/effect", false}};
  result.end_frame = 10;
  result.fps = 25.0f;
  result.keyframes = {makeCinKey(0), makeCinKey(10)};
  result.keyframes.front().sound = 0;
  return result;
}

inline void appendCinLight(std::vector<std::uint8_t>& out, const pistoris::cin::Light& light) {
  appendCinValue(out, light.position);
  appendCinValue(out, light.fall_in);
  appendCinValue(out, light.fall_out);
  appendCinValue(out, light.color);
  appendCinValue(out, light.intensity);
  appendCinValue(out, light.random_intensity);
  appendCinValue(out, std::uint64_t{0});
}

inline void appendCinKey(std::vector<std::uint8_t>& out, const pistoris::cin::Keyframe& key, std::int32_t version) {
  appendCinValue(out, key.frame);
  appendCinValue(out, key.bitmap);
  appendCinValue(out, key.effects);
  appendCinValue(out, key.interpolation);
  appendCinValue(out, key.crossfade);
  appendCinValue(out, key.camera_position);
  appendCinValue(out, key.camera_roll);
  appendCinValue(out, key.color);
  appendCinValue(out, key.secondary_color);
  appendCinValue(out, key.flash_color);
  if (version == pistoris::kCinVersion175) appendCinValue(out, key.sound);
  appendCinValue(out, key.flash_decay);
  appendCinLight(out, key.light);
  appendCinValue(out, key.bitmap_position);
  appendCinValue(out, key.bitmap_roll);
  appendCinValue(out, key.outgoing_speed);
  if (version == pistoris::kCinVersion) {
    std::array<std::int32_t, 16> sounds;
    sounds.fill(-1);
    sounds[3] = key.sound;
    appendCinValue(out, sounds);
  }
}

inline std::vector<std::uint8_t> makeCinBytes(const pistoris::Cin& data = makeCinData(),
                                              std::int32_t version = pistoris::kCinVersion) {
  std::vector<std::uint8_t> out;
  appendCinValue(out, std::array{'K', 'F', 'A', '\0'});
  appendCinValue(out, version);
  appendCinString(out, {});
  appendCinValue(out, static_cast<std::int32_t>(data.bitmaps.size()));
  for (const pistoris::cin::Bitmap& bitmap : data.bitmaps) {
    appendCinValue(out, bitmap.subdivision_scale);
    std::string path;
    const bool encoded = pistoris::encodeCinIllustrationPath(bitmap.path, path);
    assert(encoded);
    (void)encoded;
    appendCinString(out, path);
  }
  appendCinValue(out, static_cast<std::int32_t>(data.sounds.size()));
  for (const pistoris::cin::Sound& sound : data.sounds) {
    if (version == pistoris::kCinVersion) appendCinValue(out, std::uint16_t{0});
    std::string path;
    const bool encoded = pistoris::encodeCinSoundPath(sound, path);
    assert(encoded);
    (void)encoded;
    appendCinString(out, path);
  }
  appendCinValue(out, std::int32_t{0});
  appendCinValue(out, data.end_frame);
  appendCinValue(out, 0.0f);
  appendCinValue(out, data.fps);
  appendCinValue(out, static_cast<std::int32_t>(data.keyframes.size()));
  appendCinValue(out, std::int32_t{1});
  for (const pistoris::cin::Keyframe& key : data.keyframes) appendCinKey(out, key, version);
  return out;
}
