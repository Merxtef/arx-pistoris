// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#include "external/material_name.h"

#include "arx_pistoris/base/flags.h"
#include "arx_pistoris/runtime/types.h"

#include "modules/textures.h"
#include "utils/log.h"
#include "utils/path.h"
#include "utils/unique_value.h"

#include <array>
#include <cassert>
#include <charconv>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#include <string_view>
#include <system_error>
#include <unordered_set>
#include <vector>

namespace pistoris::material_names {
namespace {

struct FlagEntry {
  FaceType bit;
  std::string_view name;
};

constexpr std::array kFlagNames = {
    FlagEntry{kFaceBitNoShadow, "NO_SHADOW"},
    FlagEntry{kFaceBitDoublesided, "DOUBLESIDED"},
    FlagEntry{kFaceBitTrans, "TRANS"},
    FlagEntry{kFaceBitWater, "WATER"},
    FlagEntry{kFaceBitGlow, "GLOW"},
    FlagEntry{kFaceBitIgnore, "IGNORE"},
    FlagEntry{kFaceBitQuad, "QUAD"},
    FlagEntry{kFaceBitTiled, "TILED"},
    FlagEntry{kFaceBitMetal, "METAL"},
    FlagEntry{kFaceBitHide, "HIDE"},
    FlagEntry{kFaceBitStone, "STONE"},
    FlagEntry{kFaceBitWood, "WOOD"},
    FlagEntry{kFaceBitGravel, "GRAVEL"},
    FlagEntry{kFaceBitEarth, "EARTH"},
    FlagEntry{kFaceBitNocol, "NOCOL"},
    FlagEntry{kFaceBitLava, "LAVA"},
    FlagEntry{kFaceBitClimb, "CLIMB"},
    FlagEntry{kFaceBitFall, "FALL"},
    FlagEntry{kFaceBitNopath, "NOPATH"},
    FlagEntry{kFaceBitNodraw, "NODRAW"},
    FlagEntry{kFaceBitPrecisePath, "PRECISE_PATH"},
    FlagEntry{kFaceBitNoClimb, "NO_CLIMB"},
    FlagEntry{kFaceBitAngular, "ANGULAR"},
    FlagEntry{kFaceBitAngularIdx0, "ANGULAR_IDX0"},
    FlagEntry{kFaceBitAngularIdx1, "ANGULAR_IDX1"},
    FlagEntry{kFaceBitAngularIdx2, "ANGULAR_IDX2"},
    FlagEntry{kFaceBitAngularIdx3, "ANGULAR_IDX3"},
    FlagEntry{kFaceBitLateMip, "LATE_MIP"},
};

constexpr std::string_view kTransvalPrefix = "TRANSVAL_";

bool parseTransval(std::string_view text, float& out) noexcept {
  if (text.empty()) return false;
  float value = 0.0f;
  const auto [end, error] = std::from_chars(text.data(), text.data() + text.size(), value, std::chars_format::general);
  if (error != std::errc{} || end != text.data() + text.size() || !std::isfinite(value)) return false;
  out = value == 0.0f ? 0.0f : value;
  return true;
}

std::string transvalText(float value) {
  if (value == 0.0f) value = 0.0f;
  std::array<char, 64> buffer{};
  const auto [end, error] =
      std::to_chars(buffer.data(), buffer.data() + buffer.size(), value, std::chars_format::general);
  return error == std::errc{} ? std::string(buffer.data(), end) : std::string("0");
}

}  // namespace

DecodeError decode(std::string_view name, bool allow_unknown_tokens, Decoded& out, DecodeInfo* out_info) noexcept {
  out = {};
  DecodeInfo info;
  const std::size_t first_separator = name.find("__");
  out.fallback_stem = name.substr(0, first_separator);
  if (out.fallback_stem.empty()) return DecodeError::kBadName;
  if (first_separator == std::string_view::npos) {
    if (out_info) *out_info = info;
    return DecodeError::kNone;
  }

  std::size_t begin = first_separator + 2U;
  while (begin <= name.size()) {
    const std::size_t end = name.find("__", begin);
    const std::string_view token = name.substr(begin, end == std::string_view::npos ? end : end - begin);
    if (token.empty()) return DecodeError::kBadName;
    if (token.starts_with(kTransvalPrefix)) {
      if (out.transval) return DecodeError::kDuplicateTransval;
      float value = 0.0f;
      if (!parseTransval(token.substr(kTransvalPrefix.size()), value)) return DecodeError::kBadTransval;
      out.transval = value;
    } else {
      bool matched = false;
      for (const FlagEntry& entry : kFlagNames) {
        if (token != entry.name) continue;
        matched = true;
        if ((out.flags & entry.bit) != 0)
          ++info.duplicate_flags;
        else
          out.flags |= entry.bit;
        break;
      }
      if (!matched) {
        if (!allow_unknown_tokens) return DecodeError::kUnknownToken;
        if (info.unknown_tokens == 0) info.first_unknown_token = token;
        ++info.unknown_tokens;
      }
    }
    if (end == std::string_view::npos) break;
    begin = end + 2U;
  }
  if (out_info) *out_info = info;
  return DecodeError::kNone;
}

std::string encode(std::string_view fallback_stem, FaceType flags, float transval) {
  std::string result(fallback_stem.empty() ? std::string_view("no_tex") : fallback_stem);
  for (const FlagEntry& entry : kFlagNames) {
    if ((flags & entry.bit) == 0) continue;
    result += "__";
    result += entry.name;
  }
  if ((flags & kFaceBitTrans) != 0) {
    result += "__";
    result += kTransvalPrefix;
    result += transvalText(transval);
  }
  return result;
}

std::string fallbackStem(std::string_view texture_path, bool* normalized_delimiter) {
  const std::string_view filename = pathFilename(texture_path);
  std::string result;
  result.reserve(filename.size());
  bool normalized = false;
  bool previous_underscore = false;
  for (char value : filename) {
    if (value == '_' && previous_underscore) {
      normalized = true;
      continue;
    }
    result.push_back(value);
    previous_underscore = value == '_';
  }
  if (!result.empty() && result.back() == '_') {
    result.pop_back();
    normalized = true;
  }
  if (normalized_delimiter) *normalized_delimiter = normalized;
  return result.empty() ? std::string("texture") : result;
}

std::vector<std::string> fallbackStems(std::span<const Texture> textures, std::span<const std::uint8_t> referenced,
                                       std::span<const std::string_view> reserved, std::string_view log_prefix) {
  assert(textures.size() == referenced.size());
  std::vector<std::string> stems(textures.size());
  std::unordered_set<std::string> unavailable;
  unavailable.reserve(textures.size() * 2U + reserved.size());
  for (std::string_view stem : reserved) unavailable.emplace(stem);
  for (std::size_t index = 0; index < textures.size(); ++index) {
    if (referenced[index] == 0) continue;
    bool normalized_delimiter = false;
    stems[index] = fallbackStem(textures[index].path, &normalized_delimiter);
    unavailable.insert(stems[index]);
    if (normalized_delimiter) {
      log(ARX_LOG_WARN,
          "{}: texture '{}' uses material fallback stem '{}' in all referencing materials",
          log_prefix,
          textures[index].path,
          stems[index]);
    }
  }

  std::unordered_set<std::string> assigned;
  assigned.reserve(textures.size() + reserved.size());
  for (std::string_view stem : reserved) assigned.emplace(stem);
  for (std::size_t index = 0; index < stems.size(); ++index) {
    std::string& stem = stems[index];
    if (referenced[index] == 0 || assigned.insert(stem).second) continue;
    const std::string original = stem;
    stem = makeUniqueName(original, unavailable);
    unavailable.insert(stem);
    assigned.insert(stem);
    log(ARX_LOG_WARN,
        "{}: material fallback stem '{}' normalized to '{}' for texture '{}'",
        log_prefix,
        original,
        stem,
        textures[index].path);
  }
  return stems;
}

}  // namespace pistoris::material_names
