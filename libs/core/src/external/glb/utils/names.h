// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#pragma once

#include "utils/name_tokens.h"

#include <cstdint>
#include <initializer_list>
#include <optional>
#include <span>
#include <string_view>
#include <utility>
#include <vector>

namespace pistoris::glb {

struct LabeledValue {
  std::string_view value;
  std::string_view label;
};

enum class LabelPresence : std::uint8_t {
  kPresent,
  kMissing,
};

struct ParsedLabel {
  std::string_view text;
  LabelPresence presence = LabelPresence::kMissing;
};

struct ConventionOptions {
  std::initializer_list<std::string_view> exact;
  std::initializer_list<std::string_view> valued_prefixes;
};

std::optional<LabeledValue> splitRequiredLabel(std::string_view text) noexcept;
bool looksLikeConventionToken(std::string_view label) noexcept;
void reportConventionLabel(std::string_view context, std::string_view name, const ParsedLabel& label);

template <typename Value, typename Parse>
bool parseRecoverableLabel(std::string_view name, Value& out, ParsedLabel* label, ConventionOptions options,
                           Parse parse) {
  std::vector<std::string_view> tokens;
  splitDoubleUnderscore(name, tokens);

  Value parsed = out;
  if (parse(std::span<const std::string_view>(tokens), parsed)) {
    out = std::move(parsed);
    if (label != nullptr) *label = {};
    return true;
  }
  if (tokens.size() < 2 || tokens.back().empty()) return false;

  const std::string_view terminal = tokens.back();
  for (std::string_view exact : options.exact)
    if (terminal == exact) return false;
  for (std::string_view prefix : options.valued_prefixes)
    if (terminal.starts_with(prefix)) return false;

  tokens.pop_back();
  parsed = out;
  if (!parse(std::span<const std::string_view>(tokens), parsed)) return false;
  out = std::move(parsed);
  if (label != nullptr) *label = {terminal, LabelPresence::kPresent};
  return true;
}

}  // namespace pistoris::glb
