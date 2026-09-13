// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#include "resources/level_json.h"

#include "base/ascii.h"

#include <cstddef>
#include <cstdint>
#include <limits>
#include <string>
#include <string_view>

namespace cli {

bool parseLevelFtsJsonPath(std::string_view path, LevelJsonPath& out) {
  const std::size_t separator = path.find_last_of("/\\");
  const std::string_view filename = separator == std::string_view::npos ? path : path.substr(separator + 1);
  constexpr std::string_view kSuffix = ".fts.json";
  if (filename.size() <= 5 + kSuffix.size() || !endsWithAsciiInsensitive(filename, kSuffix)) return false;
  for (std::size_t index = 0; index < 5; ++index)
    if (lowerAscii(filename[index]) != std::string_view("level")[index]) return false;

  const std::string_view digits = filename.substr(5, filename.size() - 5 - kSuffix.size());
  if (digits.empty()) return false;
  std::uint32_t level = 0;
  for (char value : digits) {
    if (value < '0' || value > '9') return false;
    const std::uint32_t digit = static_cast<std::uint32_t>(value - '0');
    if (level > (std::numeric_limits<std::uint32_t>::max() - digit) / 10U) return false;
    level = level * 10U + digit;
  }
  out.level = level;
  out.stem.assign(filename.substr(0, filename.size() - kSuffix.size()));
  return true;
}

std::string levelDlfJsonFilename(const LevelJsonPath& path) { return path.stem + ".dlf.json"; }

std::string levelLlfJsonFilename(const LevelJsonPath& path) { return path.stem + ".llf.json"; }

}  // namespace cli
