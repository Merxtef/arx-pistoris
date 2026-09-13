// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#pragma once

#include "arx_pistoris/level.hpp"
#include "arx_pistoris/native/dlf.hpp"
#include "arx_pistoris/native/fts.hpp"
#include "arx_pistoris/native/llf.hpp"
#include "arx_pistoris/texture.hpp"

#include <optional>
#include <string>
#include <variant>
#include <vector>

namespace cli::level {

struct NativeLevelFiles {
  NativeLevelFiles() = default;
  NativeLevelFiles(const NativeLevelFiles&) = delete;
  NativeLevelFiles& operator=(const NativeLevelFiles&) = delete;
  NativeLevelFiles(NativeLevelFiles&&) = delete;
  NativeLevelFiles& operator=(NativeLevelFiles&&) = delete;

  std::optional<pistoris::Fts> fts;
  std::optional<pistoris::Llf> llf;
  std::optional<pistoris::Dlf> dlf;
};

struct IntermediateLevel {
  IntermediateLevel() = default;
  IntermediateLevel(const IntermediateLevel&) = delete;
  IntermediateLevel& operator=(const IntermediateLevel&) = delete;
  IntermediateLevel(IntermediateLevel&&) = delete;
  IntermediateLevel& operator=(IntermediateLevel&&) = delete;

  pistoris::Level level;
  std::vector<std::string> texture_source_paths;
  pistoris::ArxVector3 source_fts_offset = {};
};

using LevelInput = std::variant<std::monostate, NativeLevelFiles, IntermediateLevel>;

}  // namespace cli::level
