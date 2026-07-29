// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#include "io/paths.h"

#include "arx_pistoris/paths.hpp"

#include <string>
#include <string_view>

namespace {

char lowerAscii(char value) {
  if (value >= 'A' && value <= 'Z') return static_cast<char>(value - 'A' + 'a');
  return value;
}

}  // namespace

std::string sanitizeFilename(std::string_view name) { return pistoris::paths::sanitizePortableFilename(name); }

bool isPortableReservedFilename(std::string_view name) {
  name = name.substr(0, name.find('.'));
  std::string lower;
  lower.reserve(name.size());
  for (char value : name) lower.push_back(lowerAscii(value));

  if (lower == "con" || lower == "prn" || lower == "aux" || lower == "nul" || lower == "clock$" || lower == "conin$" ||
      lower == "conout$") {
    return true;
  }
  return lower.size() == 4 && (lower.starts_with("com") || lower.starts_with("lpt")) && lower[3] >= '1' &&
         lower[3] <= '9';
}

const char* pathFilename(const char* path) {
  const char* last = path;
  for (const char* p = path; *p; ++p)
    if (*p == '/' || *p == '\\') last = p + 1;
  return last;
}

const char* fileExtension(const char* path) {
  const char* dot = nullptr;
  const char* p;
  for (p = path; *p; ++p) {
    if (*p == '/' || *p == '\\')
      dot = nullptr;
    else if (*p == '.')
      dot = p;
  }
  return dot ? dot : p;
}
