// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#pragma once

#include <array>
#include <cstdint>
#include <optional>
#include <span>
#include <string_view>
#include <vector>

namespace pistoris::image {

inline constexpr std::uint32_t kMaxDimension = 8192;

enum class Format : std::uint8_t {
  kUnknown = 0,
  kJpeg,
  kPng,
  kBmp,
  kTga,
};

using FormatFlags = std::uint8_t;

constexpr FormatFlags formatFlag(Format format) noexcept {
  return format == Format::kUnknown ? 0 : static_cast<FormatFlags>(1U << (static_cast<unsigned>(format) - 1U));
}

inline constexpr FormatFlags kFormatsAll =
    formatFlag(Format::kJpeg) | formatFlag(Format::kPng) | formatFlag(Format::kBmp) | formatFlag(Format::kTga);

enum class Error : std::uint8_t {
  kNone,
  kMalformed,
  kOutOfMemory,
};

enum class BmpColorKey : std::uint8_t {
  kNone,
  kBinary,
  kAntialiased,
};

enum class QuarterTurn : std::uint8_t {
  kNone,
  kClockwise90,
  kClockwise180,
  kClockwise270,
};

struct Info {
  Format format = Format::kUnknown;
  std::uint32_t width = 0;
  std::uint32_t height = 0;
  std::uint8_t components = 0;
};

struct Placement {
  std::int64_t x = 0;
  std::int64_t y = 0;
  std::uint32_t width = 0;
  std::uint32_t height = 0;
};

enum class FitMode : std::uint8_t {
  kCenter = 0,
  kTopLeft,
  kTopRight,
  kBottomLeft,
  kBottomRight,
  kStretch,
};

Format detectFormat(std::span<const std::uint8_t> encoded) noexcept;
Error inspectMetadata(std::span<const std::uint8_t> encoded, Info& out) noexcept;
Error inspect(std::span<const std::uint8_t> encoded, Info* out = nullptr) noexcept;
bool hasAlpha(const Info& info) noexcept;
std::string_view extension(Format format) noexcept;

Error decodeResizedRgb(std::span<const std::uint8_t> encoded, std::uint32_t width, std::uint32_t height,
                       std::vector<std::uint8_t>& out);
Error encodeRgbPng(std::uint32_t width, std::uint32_t height, std::span<const std::uint8_t> rgb,
                   std::vector<std::uint8_t>& out);

Error transcodeToPng(std::span<const std::uint8_t> encoded, std::vector<std::uint8_t>& out, Info* out_info = nullptr,
                     BmpColorKey bmp_color_key = BmpColorKey::kNone);
Error normalizeToPowerOfTwo(std::span<const std::uint8_t> encoded, std::vector<std::uint8_t>& out,
                            Info* out_info = nullptr, bool* out_rescaled = nullptr,
                            BmpColorKey bmp_color_key = BmpColorKey::kNone);
Error fitToPng(std::span<const std::uint8_t> encoded, std::uint32_t width, std::uint32_t height, FitMode mode,
               std::vector<std::uint8_t>& out, BmpColorKey bmp_color_key = BmpColorKey::kNone);
Error placeToPng(std::span<const std::uint8_t> encoded, std::uint32_t canvas_width, std::uint32_t canvas_height,
                 Placement placement, std::array<std::uint8_t, 4> fill, std::vector<std::uint8_t>& out,
                 std::optional<std::array<std::uint8_t, 4>> border_color = std::nullopt,
                 BmpColorKey bmp_color_key = BmpColorKey::kNone);
Error rotateQuarterTurnToPng(std::span<const std::uint8_t> encoded, QuarterTurn rotation,
                             std::vector<std::uint8_t>& out);

}  // namespace pistoris::image
