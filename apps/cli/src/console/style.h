// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#pragma once

#include <cstdint>
#include <cstdio>
#include <string_view>

namespace cli {

enum class ConsoleColor : std::uint8_t {
  kDefault,
  kGray,
  kRed,
  kGreen,
  kYellow,
  kMagenta,
  kCyan,
  kBrightMagenta,
};

struct ConsoleStyle {
  ConsoleColor color = ConsoleColor::kDefault;
  bool bold = false;
};

void writeStyled(std::FILE* output, ConsoleStyle style, std::string_view text);

}  // namespace cli
