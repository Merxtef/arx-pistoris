// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#include "console/style.h"

#include "console/terminal.h"

#include <cstdio>
#include <string_view>

namespace {

const char* escapeCode(cli::ConsoleStyle style) {
  if (style.bold) {
    switch (style.color) {
      case cli::ConsoleColor::kDefault:
        return "\x1b[1m";
      case cli::ConsoleColor::kGray:
        return "\x1b[1;90m";
      case cli::ConsoleColor::kRed:
        return "\x1b[1;31m";
      case cli::ConsoleColor::kGreen:
        return "\x1b[1;32m";
      case cli::ConsoleColor::kYellow:
        return "\x1b[1;33m";
      case cli::ConsoleColor::kMagenta:
        return "\x1b[1;35m";
      case cli::ConsoleColor::kCyan:
        return "\x1b[1;36m";
      case cli::ConsoleColor::kBrightMagenta:
        return "\x1b[1;95m";
    }
  }

  switch (style.color) {
    case cli::ConsoleColor::kDefault:
      return nullptr;
    case cli::ConsoleColor::kGray:
      return "\x1b[90m";
    case cli::ConsoleColor::kRed:
      return "\x1b[31m";
    case cli::ConsoleColor::kGreen:
      return "\x1b[32m";
    case cli::ConsoleColor::kYellow:
      return "\x1b[33m";
    case cli::ConsoleColor::kMagenta:
      return "\x1b[35m";
    case cli::ConsoleColor::kCyan:
      return "\x1b[36m";
    case cli::ConsoleColor::kBrightMagenta:
      return "\x1b[95m";
  }
  return nullptr;
}

}  // namespace

namespace cli {

void writeStyled(std::FILE* output, ConsoleStyle style, std::string_view text) {
  const char* escape = terminalSupportsColor(output) ? escapeCode(style) : nullptr;
  if (escape) std::fputs(escape, output);
  std::fwrite(text.data(), 1, text.size(), output);
  if (escape) std::fputs("\x1b[0m", output);
}

}  // namespace cli
