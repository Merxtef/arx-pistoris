// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#include "utils/number_text.h"

#include "fast_float/fast_float.h"

#include <cmath>
#include <string_view>
#include <system_error>

namespace pistoris {

bool parseFiniteFloat(std::string_view text, float& out) noexcept {
  if (text.empty()) return false;
  float value = 0.0f;
  const auto [end, error] = fast_float::from_chars(text.data(), text.data() + text.size(), value);
  if (error != std::errc{} || end != text.data() + text.size() || !std::isfinite(value)) return false;
  out = value;
  return true;
}

}  // namespace pistoris
