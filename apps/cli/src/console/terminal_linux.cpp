// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#include "console/terminal.h"

#include <cstdio>
#include <cstdlib>
#include <stdio.h>
#include <string_view>
#include <unistd.h>

namespace {

bool detectColorSupport(std::FILE* output) {
  if (std::getenv("NO_COLOR")) return false;
  if (const char* term = std::getenv("TERM"); term && std::string_view(term) == "dumb") return false;
  const int descriptor = fileno(output);
  return descriptor >= 0 && isatty(descriptor) == 1;
}

}  // namespace

namespace cli {

bool terminalSupportsColor(std::FILE* output) noexcept {
  if (output == stdout) {
    static const bool kSupported = detectColorSupport(stdout);
    return kSupported;
  }
  if (output == stderr) {
    static const bool kSupported = detectColorSupport(stderr);
    return kSupported;
  }
  return detectColorSupport(output);
}

}  // namespace cli
