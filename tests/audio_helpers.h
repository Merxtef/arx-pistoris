// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#pragma once

#include <cstdint>
#include <vector>

inline std::vector<std::uint8_t> makePcm16Wav(std::uint16_t channels) {
  constexpr std::uint32_t kSampleRate = 8000;
  constexpr std::uint32_t kFrames = 4;
  const std::uint32_t data_size = kFrames * channels * sizeof(std::int16_t);
  std::vector<std::uint8_t> result;
  result.reserve(44U + data_size);
  const auto bytes = [&](const char* value, std::size_t count) { result.insert(result.end(), value, value + count); };
  const auto u16 = [&](std::uint16_t value) {
    result.push_back(static_cast<std::uint8_t>(value));
    result.push_back(static_cast<std::uint8_t>(value >> 8U));
  };
  const auto u32 = [&](std::uint32_t value) {
    result.push_back(static_cast<std::uint8_t>(value));
    result.push_back(static_cast<std::uint8_t>(value >> 8U));
    result.push_back(static_cast<std::uint8_t>(value >> 16U));
    result.push_back(static_cast<std::uint8_t>(value >> 24U));
  };

  bytes("RIFF", 4);
  u32(36U + data_size);
  bytes("WAVEfmt ", 8);
  u32(16);
  u16(1);
  u16(channels);
  u32(kSampleRate);
  u32(kSampleRate * channels * sizeof(std::int16_t));
  u16(static_cast<std::uint16_t>(channels * sizeof(std::int16_t)));
  u16(16);
  bytes("data", 4);
  u32(data_size);
  for (std::uint32_t frame = 0; frame < kFrames; ++frame) {
    for (std::uint16_t channel = 0; channel < channels; ++channel) {
      const std::int16_t sample = static_cast<std::int16_t>((frame + 1U) * (channel == 0 ? 1000 : -500));
      u16(static_cast<std::uint16_t>(sample));
    }
  }
  return result;
}
