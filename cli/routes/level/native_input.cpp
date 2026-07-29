// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#include "routes/level/native_input.h"

#include "arx_pistoris/paths.hpp"
#include "arx_pistoris/pistoris.hpp"
#include "arx_pistoris/pistoris_types.h"

#include "formats/classification.h"
#include "formats/format.h"

#include <cstdint>
#include <cstring>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace cli::level {
namespace {

std::string_view textView(const std::vector<std::uint8_t>& buffer) {
  return {reinterpret_cast<const char*>(buffer.data()), buffer.size()};
}

}  // namespace

ArxReturnCode decodeFts(const ClassifiedPath& input, pistoris::Fts& out) {
  return input.facts.format == Format::kJson ? pistoris::importJson(textView(input.buffer), out)
                                             : pistoris::readFts(input.buffer, out);
}

ArxReturnCode decodeLlf(const ClassifiedPath& input, pistoris::Llf& out) {
  return input.facts.format == Format::kJson ? pistoris::importJson(textView(input.buffer), out)
                                             : pistoris::readLlf(input.buffer, out);
}

ArxReturnCode decodeDlf(const ClassifiedPath& input, pistoris::Dlf& out,
                        std::optional<pistoris::Llf>* embedded_lighting) {
  if (input.facts.format == Format::kJson) {
    if (embedded_lighting) embedded_lighting->reset();
    return pistoris::importJson(textView(input.buffer), out);
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
  pistoris::paths::dlfSceneFromLevelName(level_name, dlf.scene_path);
  dlf.scene_path.push_back('/');
}

}  // namespace cli::level
