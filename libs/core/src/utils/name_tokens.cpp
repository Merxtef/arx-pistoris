// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#include "utils/name_tokens.h"

#include <cstddef>
#include <initializer_list>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace pistoris {

namespace {

constexpr std::string_view kSeparator = "__";

}  // namespace

bool hasDoubleUnderscore(std::string_view text) noexcept { return text.find(kSeparator) != std::string_view::npos; }

bool hasEmbeddedNull(std::string_view text) noexcept { return text.find('\0') != std::string_view::npos; }

bool validSemanticString(std::string_view text) noexcept {
  return !hasEmbeddedNull(text) && !hasDoubleUnderscore(text);
}

void splitDoubleUnderscore(std::string_view text, std::vector<std::string_view>& out) {
  out.clear();
  std::size_t begin = 0;
  while (true) {
    std::size_t separator = text.find(kSeparator, begin);
    if (separator == std::string_view::npos) {
      out.push_back(text.substr(begin));
      return;
    }
    out.push_back(text.substr(begin, separator - begin));
    begin = separator + kSeparator.size();
  }
}

std::string joinDoubleUnderscore(std::span<const std::string_view> tokens) {
  std::size_t size = 0;
  for (std::string_view token : tokens) size += token.size();
  if (!tokens.empty()) size += (tokens.size() - 1) * kSeparator.size();

  std::string result;
  result.reserve(size);
  for (std::size_t i = 0; i < tokens.size(); ++i) {
    if (i != 0) result += kSeparator;
    result += tokens[i];
  }
  return result;
}

std::string joinDoubleUnderscore(std::initializer_list<std::string_view> tokens) {
  return joinDoubleUnderscore(std::span<const std::string_view>(tokens.begin(), tokens.size()));
}

}  // namespace pistoris
