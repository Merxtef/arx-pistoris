// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#pragma once

#include "arx_pistoris/base/flags.h"

#include "modules/textures.h"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace pistoris::material_names {

enum class DecodeError : std::uint8_t {
  kNone,
  kBadName,
  kBadTransval,
  kDuplicateTransval,
  kUnknownToken,
};

struct Decoded {
  std::string_view fallback_stem;
  FaceType flags = 0;
  std::optional<float> transval;
};

struct DecodeInfo {
  std::size_t duplicate_flags = 0;
  std::size_t unknown_tokens = 0;
  std::string_view first_unknown_token;
};

DecodeError decode(std::string_view name, bool allow_unknown_tokens, Decoded& out,
                   DecodeInfo* out_info = nullptr) noexcept;
std::string encode(std::string_view fallback_stem, FaceType flags, float transval);

std::string fallbackStem(std::string_view texture_path, bool* normalized_delimiter = nullptr);
std::vector<std::string> fallbackStems(std::span<const Texture> textures, std::span<const std::uint8_t> referenced,
                                       std::span<const std::string_view> reserved, std::string_view log_prefix);

}  // namespace pistoris::material_names
