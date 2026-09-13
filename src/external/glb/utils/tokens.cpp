// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#include "tokens.h"

#include <array>
#include <charconv>
#include <cmath>
#include <cstdint>
#include <limits>
#include <optional>
#include <string>
#include <string_view>
#include <system_error>

namespace pistoris::glb {

std::optional<std::uint32_t> parseUnsignedToken(std::string_view token) {
  if (token.empty()) return std::nullopt;
  std::uint32_t value = 0;
  const auto [end, error] = std::from_chars(token.data(), token.data() + token.size(), value);
  if (error != std::errc{} || end != token.data() + token.size()) return std::nullopt;
  return value;
}

std::optional<std::int32_t> parseSignedToken(std::string_view token) {
  if (token.empty()) return std::nullopt;
  std::int32_t value = 0;
  const auto [end, error] = std::from_chars(token.data(), token.data() + token.size(), value);
  if (error != std::errc{} || end != token.data() + token.size()) return std::nullopt;
  return value;
}

bool parseFloatToken(std::string_view token, float& out) {
  if (token.empty()) return false;
  float value = 0.0f;
  const auto [end, error] =
      std::from_chars(token.data(), token.data() + token.size(), value, std::chars_format::general);
  if (error != std::errc{} || end != token.data() + token.size() || !std::isfinite(value)) return false;
  out = value;
  return true;
}

std::string formatFloatToken(float value) {
  std::array<char, 64> buffer{};
  const auto [end, error] = std::to_chars(buffer.data(),
                                          buffer.data() + buffer.size(),
                                          value,
                                          std::chars_format::general,
                                          std::numeric_limits<float>::max_digits10);
  return error == std::errc{} ? std::string(buffer.data(), end) : std::string("0");
}

}  // namespace pistoris::glb
