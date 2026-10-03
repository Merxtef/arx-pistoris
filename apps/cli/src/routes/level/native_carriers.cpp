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
#include "routes/conversion_failure.h"

#include <cassert>
#include <cstdint>
#include <cstring>
#include <optional>
#include <string>
#include <utility>

namespace cli::level {

namespace {

template <class Result, class Publish>
ArxReturnCode publishDecoded(Result&& result, Publish&& publish, std::string* failure) {
  if (result) {
    std::forward<Publish>(publish)(std::move(*result));
  } else if (failure) {
    *failure = conversionFailureDescription(result);
  }
  return result.code();
}

}  // namespace

ArxReturnCode decodeFts(const ClassifiedPath& input, pistoris::NativeTextMode text_mode, pistoris::Fts& out,
                        std::string* failure) {
  if (input.facts.format == Format::kJson) {
    auto result = pistoris::fromFtsJson(byteStringView(input.buffer), text_mode);
    return publishDecoded(
        std::move(result), [&](pistoris::FtsJsonImport&& decoded) { out = std::move(decoded.fts); }, failure);
  }
  auto result = pistoris::readFts(input.buffer);
  return publishDecoded(std::move(result), [&](pistoris::Fts&& decoded) { out = std::move(decoded); }, failure);
}

ArxReturnCode decodeLlf(const ClassifiedPath& input, pistoris::Llf& out, std::string* failure) {
  if (input.facts.format == Format::kJson) {
    auto result = pistoris::fromLlfJson(byteStringView(input.buffer));
    return publishDecoded(std::move(result), [&](pistoris::Llf&& decoded) { out = std::move(decoded); }, failure);
  }
  auto result = pistoris::readLlf(input.buffer);
  return publishDecoded(std::move(result), [&](pistoris::Llf&& decoded) { out = std::move(decoded); }, failure);
}

ArxReturnCode decodeDlf(const ClassifiedPath& input, pistoris::NativeTextMode text_mode, pistoris::Dlf& out,
                        std::optional<pistoris::Llf>* embedded_lighting, std::string* failure) {
  if (input.facts.format == Format::kJson) {
    if (embedded_lighting) embedded_lighting->reset();
    auto result = pistoris::fromDlfJson(byteStringView(input.buffer), text_mode);
    return publishDecoded(std::move(result), [&](pistoris::Dlf&& decoded) { out = std::move(decoded); }, failure);
  }
  auto result = pistoris::readDlf(input.buffer);
  return publishDecoded(
      std::move(result),
      [&](pistoris::DlfBundle&& decoded) {
        out = std::move(decoded.dlf);
        if (embedded_lighting) *embedded_lighting = std::move(decoded.embedded_lighting);
      },
      failure);
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
