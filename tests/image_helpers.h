// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

inline std::vector<std::uint8_t> makeTestBmp(std::uint8_t red = 255, std::uint8_t green = 0, std::uint8_t blue = 0) {
  return {
      'B', 'M', 58, 0, 0, 0, 0, 0, 0, 0, 54, 0, 0, 0, 40, 0, 0, 0, 1, 0, 0, 0, 1, 0, 0, 0,    1,     0,   24,
      0,   0,   0,  0, 0, 0, 0, 4, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, blue, green, red, 0,
  };
}

inline std::vector<std::uint8_t> makeTestTga(std::uint8_t red = 0, std::uint8_t green = 255, std::uint8_t blue = 0) {
  return {
      0, 0, 2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 1, 0, 24, 0x20, blue, green, red,
  };
}

inline std::vector<std::uint8_t> makeSolidTestBmp(std::uint32_t width, std::uint32_t height, std::uint8_t red = 255,
                                                  std::uint8_t green = 0, std::uint8_t blue = 0) {
  const std::uint32_t row_bytes = (width * 3U + 3U) & ~3U;
  const std::uint32_t data_bytes = row_bytes * height;
  std::vector<std::uint8_t> bytes(54U + data_bytes, 0);
  auto write_u16 = [&](std::size_t offset, std::uint16_t value) {
    bytes[offset] = static_cast<std::uint8_t>(value);
    bytes[offset + 1] = static_cast<std::uint8_t>(value >> 8U);
  };
  auto write_u32 = [&](std::size_t offset, std::uint32_t value) {
    for (std::size_t i = 0; i < 4; ++i) bytes[offset + i] = static_cast<std::uint8_t>(value >> (i * 8U));
  };
  bytes[0] = 'B';
  bytes[1] = 'M';
  write_u32(2, static_cast<std::uint32_t>(bytes.size()));
  write_u32(10, 54);
  write_u32(14, 40);
  write_u32(18, width);
  write_u32(22, height);
  write_u16(26, 1);
  write_u16(28, 24);
  write_u32(34, data_bytes);
  for (std::uint32_t y = 0; y < height; ++y) {
    for (std::uint32_t x = 0; x < width; ++x) {
      const std::size_t offset = 54U + static_cast<std::size_t>(y) * row_bytes + static_cast<std::size_t>(x) * 3U;
      bytes[offset + 0] = blue;
      bytes[offset + 1] = green;
      bytes[offset + 2] = red;
    }
  }
  return bytes;
}

inline std::vector<std::uint8_t> makeTestNpotBmp() {
  constexpr std::uint32_t kWidth = 3;
  constexpr std::uint32_t kHeight = 2;
  constexpr std::uint32_t kRowBytes = 12;
  constexpr std::uint32_t kDataBytes = kRowBytes * kHeight;
  std::vector<std::uint8_t> bytes(54U + kDataBytes, 0);
  auto write_u16 = [&](std::size_t offset, std::uint16_t value) {
    bytes[offset] = static_cast<std::uint8_t>(value);
    bytes[offset + 1] = static_cast<std::uint8_t>(value >> 8U);
  };
  auto write_u32 = [&](std::size_t offset, std::uint32_t value) {
    for (std::size_t i = 0; i < 4; ++i) bytes[offset + i] = static_cast<std::uint8_t>(value >> (i * 8U));
  };
  bytes[0] = 'B';
  bytes[1] = 'M';
  write_u32(2, static_cast<std::uint32_t>(bytes.size()));
  write_u32(10, 54);
  write_u32(14, 40);
  write_u32(18, kWidth);
  write_u32(22, kHeight);
  write_u16(26, 1);
  write_u16(28, 24);
  write_u32(34, kDataBytes);

  constexpr std::uint8_t kPixels[6][3] = {
      {0, 0, 255},
      {0, 255, 0},
      {255, 0, 0},
      {0, 255, 255},
      {255, 0, 255},
      {255, 255, 0},
  };
  for (std::size_t y = 0; y < kHeight; ++y) {
    for (std::size_t x = 0; x < kWidth; ++x) {
      const std::size_t target = 54U + y * kRowBytes + x * 3U;
      const std::size_t source = y * kWidth + x;
      bytes[target + 0] = kPixels[source][0];
      bytes[target + 1] = kPixels[source][1];
      bytes[target + 2] = kPixels[source][2];
    }
  }
  return bytes;
}
