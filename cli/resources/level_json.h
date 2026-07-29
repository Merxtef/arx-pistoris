// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#pragma once

#include <cstdint>
#include <string>
#include <string_view>

namespace cli {

struct LevelJsonPath {
  std::uint32_t level = 0;
  std::string stem;
};

bool parseLevelFtsJsonPath(std::string_view path, LevelJsonPath& out);
std::string levelDlfJsonFilename(const LevelJsonPath& path);
std::string levelLlfJsonFilename(const LevelJsonPath& path);

}  // namespace cli
