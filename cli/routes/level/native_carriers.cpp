// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#include "routes/level/native_carriers.h"

#include "arx_pistoris/base/status.h"
#include "arx_pistoris/native.hpp"
#include "arx_pistoris/native/text.hpp"
#include "arx_pistoris/paths.hpp"

#include "base/bytes.h"
#include "formats/classification.h"
#include "formats/format.h"

#include <cassert>
#include <cstdint>
#include <cstring>
#include <optional>
#include <string>

namespace cli::level {

ArxReturnCode decodeFts(const ClassifiedPath& input, pistoris::NativeTextMode text_mode, pistoris::Fts& out) {
  return input.facts.format == Format::kJson ? pistoris::fromJson(byteStringView(input.buffer), out, text_mode)
                                             : pistoris::readFts(input.buffer, out);
}

ArxReturnCode decodeLlf(const ClassifiedPath& input, pistoris::Llf& out) {
  return input.facts.format == Format::kJson ? pistoris::fromJson(byteStringView(input.buffer), out)
                                             : pistoris::readLlf(input.buffer, out);
}

ArxReturnCode decodeDlf(const ClassifiedPath& input, pistoris::NativeTextMode text_mode, pistoris::Dlf& out,
                        std::optional<pistoris::Llf>* embedded_lighting) {
  if (input.facts.format == Format::kJson) {
    if (embedded_lighting) embedded_lighting->reset();
    return pistoris::fromJson(byteStringView(input.buffer), out, text_mode);
  }
  return pistoris::readDlf(input.buffer, out, embedded_lighting);
}

void applyLevelNumber(std::uint32_t level, pistoris::Fts& fts, pistoris::Dlf* dlf) {
  const std::string fts_path = pistoris::paths::levelFts(level);
  std::memcpy(fts.header.path, fts_path.c_str(), fts_path.size() + 1U);
  if (dlf) applyLevelNumber(level, *dlf);
}

void applyLevelNumber(std::uint32_t level, pistoris::Dlf& dlf) {
  const std::string level_name = "level" + std::to_string(level);
  std::string scene_path;
  [[maybe_unused]] const bool path_generated = pistoris::paths::dlfSceneFromLevelName(level_name, scene_path);
  assert(path_generated);
  assert(scene_path.size() < sizeof(dlf.scene_path));
  std::memcpy(dlf.scene_path, scene_path.c_str(), scene_path.size() + 1U);
  std::memset(dlf.scene_path + scene_path.size() + 1U, 0, sizeof(dlf.scene_path) - scene_path.size() - 1U);
}

}  // namespace cli::level
