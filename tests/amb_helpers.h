// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#pragma once

#include "arx_pistoris/native/amb.hpp"

#include <cstdint>
#include <cstring>
#include <span>
#include <string_view>
#include <type_traits>
#include <utility>
#include <vector>

template <class T>
void appendAmbValue(std::vector<std::uint8_t>& out, const T& value) {
  static_assert(std::is_trivially_copyable_v<T>);
  const std::size_t offset = out.size();
  out.resize(offset + sizeof(T));
  std::memcpy(out.data() + offset, &value, sizeof(T));
}

inline void appendAmbString(std::vector<std::uint8_t>& out, std::string_view value) {
  out.insert(out.end(), value.begin(), value.end());
  out.push_back(0);
}

inline pistoris::amb::Setting makeAmbSetting(float value) { return {value, value, 0, 0}; }

inline pistoris::amb::Setting makeAmbSetting(float min, float max, std::uint32_t interval_ms,
                                             pistoris::amb::SettingFlags flags) {
  return {min, max, interval_ms, flags};
}

inline pistoris::amb::Key makeAmbKey(std::uint32_t start_ms) {
  pistoris::amb::Key key;
  key.start_ms = start_ms;
  key.loop_minus_one = 1;
  key.delay_min_ms = 10;
  key.delay_max_ms = 20;
  key.volume = makeAmbSetting(0.25f, 0.75f, 100, pistoris::amb::kSettingInterpolate);
  key.pitch = makeAmbSetting(1.0f);
  key.pan = makeAmbSetting(0.0f);
  key.x = makeAmbSetting(10.0f);
  key.y = makeAmbSetting(20.0f);
  key.z = makeAmbSetting(30.0f);
  return key;
}

inline pistoris::amb::Data makeAmbData() {
  pistoris::amb::Track track;
  track.sample_path = "sfx/ambiance/test.wav";
  track.flags = pistoris::amb::kTrackMaster | pistoris::amb::kTrackPosition;
  track.keys = {makeAmbKey(100), makeAmbKey(200)};
  return {{std::move(track)}};
}

inline void appendAmbSetting(std::vector<std::uint8_t>& out, const pistoris::amb::Setting& setting) {
  appendAmbValue(out, setting.min);
  appendAmbValue(out, setting.max);
  appendAmbValue(out, setting.interval_ms);
  appendAmbValue(out, setting.flags);
}

inline void appendAmbKey(std::vector<std::uint8_t>& out, const pistoris::amb::Key& key, bool with_padding) {
  if (with_padding) appendAmbValue(out, std::uint32_t{0});
  appendAmbValue(out, key.start_ms);
  appendAmbValue(out, key.loop_minus_one);
  appendAmbValue(out, key.delay_min_ms);
  appendAmbValue(out, key.delay_max_ms);
  appendAmbSetting(out, key.volume);
  appendAmbSetting(out, key.pitch);
  appendAmbSetting(out, key.pan);
  appendAmbSetting(out, key.x);
  appendAmbSetting(out, key.y);
  appendAmbSetting(out, key.z);
}

inline std::vector<std::uint8_t> makeAmbBytes(const pistoris::amb::Data& data, std::uint32_t version,
                                              std::span<const std::string_view> track_names = {}) {
  std::vector<std::uint8_t> out;
  appendAmbValue(out, pistoris::kAmbMagic);
  appendAmbValue(out, version);
  appendAmbValue(out, static_cast<std::uint32_t>(data.tracks.size()));

  for (std::size_t track_index = 0; track_index < data.tracks.size(); ++track_index) {
    const pistoris::amb::Track& track = data.tracks[track_index];
    appendAmbString(out, track.sample_path);

    if (version == pistoris::kAmbVersion1000) {
      appendAmbKey(out, track.keys.front(), false);
      appendAmbValue(out, track.flags);
      continue;
    }

    if (version >= pistoris::kAmbVersion1002) {
      appendAmbString(out, track_index < track_names.size() ? track_names[track_index] : std::string_view{});
    }
    appendAmbValue(out, track.flags);
    appendAmbValue(out, static_cast<std::uint32_t>(track.keys.size()));
    if (version == pistoris::kAmbVersion1003) {
      for (auto key = track.keys.rbegin(); key != track.keys.rend(); ++key) appendAmbKey(out, *key, true);
    } else {
      for (const pistoris::amb::Key& key : track.keys) appendAmbKey(out, key, true);
    }
  }

  return out;
}

inline std::vector<std::uint8_t> makeAmbBytes(std::uint32_t version = pistoris::kAmbVersion,
                                              std::string_view track_name = {}) {
  const pistoris::amb::Data data = makeAmbData();
  return makeAmbBytes(data, version, std::span(&track_name, 1));
}
