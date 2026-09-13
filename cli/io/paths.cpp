// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#include "io/paths.h"

#include "arx_pistoris/paths.hpp"

#include "base/ascii.h"

#include <string>
#include <string_view>

std::string sanitizeFilename(std::string_view name) { return pistoris::paths::sanitizePortableFilename(name); }

bool isPortableReservedFilename(std::string_view name) {
  name = name.substr(0, name.find('.'));
  std::string lower;
  lower.reserve(name.size());
  for (char value : name) lower.push_back(cli::lowerAscii(value));

  if (lower == "con" || lower == "prn" || lower == "aux" || lower == "nul" || lower == "clock$" || lower == "conin$" ||
      lower == "conout$") {
    return true;
  }
  return lower.size() == 4 && (lower.starts_with("com") || lower.starts_with("lpt")) && lower[3] >= '1' &&
         lower[3] <= '9';
}
