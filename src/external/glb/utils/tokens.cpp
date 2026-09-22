// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#include "tokens.h"

#include "arx_pistoris/base/math.h"

#include <array>
#include <charconv>
#include <cmath>
#include <cstddef>
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

bool parseColor3Token(std::string_view token, ArxColor3& out) {
  const std::size_t first = token.find('_');
  const std::size_t second = first == std::string_view::npos ? first : token.find('_', first + 1U);
  if (first == std::string_view::npos || second == std::string_view::npos ||
      token.find('_', second + 1U) != std::string_view::npos)
    return false;
  ArxColor3 parsed{};
  if (!parseFloatToken(token.substr(0, first), parsed.r) ||
      !parseFloatToken(token.substr(first + 1U, second - first - 1U), parsed.g) ||
      !parseFloatToken(token.substr(second + 1U), parsed.b))
    return false;
  out = parsed;
  return true;
}

std::string formatColor3Token(const ArxColor3& value) {
  return formatFloatToken(value.r) + '_' + formatFloatToken(value.g) + '_' + formatFloatToken(value.b);
}

}  // namespace pistoris::glb
