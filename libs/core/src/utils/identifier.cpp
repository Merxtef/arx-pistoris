// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#include "utils/identifier.h"

#include "utils/unique_value.h"

#include <algorithm>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace pistoris {
namespace {

char lowerAscii(char value) noexcept {
  if (value >= 'A' && value <= 'Z') return static_cast<char>(value - 'A' + 'a');
  return value;
}

bool identifierCharacter(char value, const IdentifierPolicy& policy) noexcept {
  return (value >= 'A' && value <= 'Z') || (value >= 'a' && value <= 'z') || (value >= '0' && value <= '9') ||
         value == '_' || value == '-' || (policy.allow_brackets && (value == '[' || value == ']')) ||
         (policy.allow_dots && value == '.') || (policy.allow_spaces && value == ' ') ||
         (policy.allow_parentheses && (value == '(' || value == ')')) || (policy.allow_ampersands && value == '&');
}

IdentifierNormalization normalizeIdentifierImpl(std::string_view requested, const IdentifierPolicy& policy) {
  std::string normalized;
  normalized.reserve(std::min(requested.size(), policy.max_length));
  IdentifierRepair repairs = IdentifierRepair::kNone;
  bool segment_start = true;
  bool previous_underscore = false;
  bool previous_replacement = false;
  for (char input : requested) {
    char output = input;
    if (output == '\\' && policy.allow_path_separators) {
      output = '/';
      repairs |= IdentifierRepair::kSeparators;
    }
    if (output == '/' && policy.allow_path_separators) {
      while (!normalized.empty() && (normalized.back() == '_' || (policy.allow_spaces && normalized.back() == ' '))) {
        const char removed = normalized.back();
        normalized.pop_back();
        repairs |= removed == '_' ? IdentifierRepair::kUnderscores : IdentifierRepair::kCharacters;
      }
      if (!normalized.empty() && normalized.back() == '.') {
        normalized.back() = '-';
        repairs |= IdentifierRepair::kCharacters;
      }
      if (normalized.empty() || normalized.back() == '/') {
        repairs |= IdentifierRepair::kSeparators;
        continue;
      }
      if (normalized.size() == policy.max_length) {
        repairs |= IdentifierRepair::kLength;
        continue;
      }
      normalized.push_back('/');
      segment_start = true;
      previous_underscore = false;
      previous_replacement = false;
      continue;
    }
    if (policy.letter_case == IdentifierCase::kLower) {
      const char lowered = lowerAscii(output);
      if (lowered != output) repairs |= IdentifierRepair::kCase;
      output = lowered;
    }
    if (!identifierCharacter(output, policy)) {
      repairs |= IdentifierRepair::kCharacters;
      if (previous_replacement) continue;
      output = '-';
      previous_replacement = true;
    } else {
      previous_replacement = false;
    }
    if (output == '.' && segment_start) {
      output = '-';
      repairs |= IdentifierRepair::kCharacters;
    }
    if (output == '_') {
      if (segment_start || previous_underscore) {
        repairs |= IdentifierRepair::kUnderscores;
        continue;
      }
      previous_underscore = true;
    } else {
      segment_start = false;
      previous_underscore = false;
    }
    if (normalized.size() == policy.max_length) {
      repairs |= IdentifierRepair::kLength;
      continue;
    }
    normalized.push_back(output);
  }
  while (!normalized.empty() &&
         (normalized.back() == '_' || normalized.back() == '/' || (policy.allow_spaces && normalized.back() == ' '))) {
    const char removed = normalized.back();
    normalized.pop_back();
    repairs |= removed == '_'   ? IdentifierRepair::kUnderscores
               : removed == '/' ? IdentifierRepair::kSeparators
                                : IdentifierRepair::kCharacters;
  }
  if (!normalized.empty() && normalized.back() == '.') {
    normalized.back() = '-';
    repairs |= IdentifierRepair::kCharacters;
  }
  if (normalized.empty() && !policy.allow_empty) {
    normalized = "unnamed";
    if (normalized.size() > policy.max_length) normalized.resize(policy.max_length);
    repairs |= IdentifierRepair::kEmpty;
  }
  return {std::move(normalized), repairs};
}

std::optional<std::string> compactIdentifier(std::size_t value, std::size_t max_length) {
  constexpr std::string_view kDigits = "0123456789abcdefghijklmnopqrstuvwxyz";
  std::string result;
  do {
    if (result.size() == max_length) return std::nullopt;
    result.push_back(kDigits[value % kDigits.size()]);
    value /= kDigits.size();
  } while (value != 0);
  std::ranges::reverse(result);
  return result;
}

std::size_t fallbackOrdinalStart(std::size_t max_length) noexcept {
  if (max_length <= 2U) return 1U;
  std::size_t start = 1U;
  for (std::size_t digit = 1U; digit < max_length - 1U; ++digit) {
    if (start > std::numeric_limits<std::size_t>::max() / 10U) return std::numeric_limits<std::size_t>::max();
    start *= 10U;
  }
  return start;
}

std::optional<std::string> suffixedIdentifier(std::string_view base, std::size_t suffix, std::size_t max_length) {
  const std::string suffix_text = "_" + std::to_string(suffix);
  if (suffix_text.size() >= max_length) return compactIdentifier(suffix - fallbackOrdinalStart(max_length), max_length);
  std::string prefix(base.substr(0, std::min(base.size(), max_length - suffix_text.size())));
  while (!prefix.empty() && (prefix.back() == '_' || prefix.back() == '/')) prefix.pop_back();
  return prefix + suffix_text;
}

}  // namespace

IdentifierUniquifier::IdentifierUniquifier(IdentifierPolicy policy)
    : policy_(policy),
      values_(IdentityHash{policy.ascii_case_insensitive}, IdentityEqual{policy.ascii_case_insensitive}) {
  assert(policy_.max_length != 0);
  policy_.max_length = std::max(policy_.max_length, std::size_t{1});
}

std::size_t IdentifierUniquifier::IdentityHash::operator()(std::string_view value) const noexcept {
  std::size_t hash = 0;
  for (char character : value) {
    if (ascii_case_insensitive) character = lowerAscii(character);
    hash = hash * 131U + static_cast<unsigned char>(character);
  }
  return hash;
}

bool IdentifierUniquifier::IdentityEqual::operator()(std::string_view left, std::string_view right) const noexcept {
  if (left.size() != right.size()) return false;
  for (std::size_t index = 0; index < left.size(); ++index) {
    if (ascii_case_insensitive) {
      if (lowerAscii(left[index]) != lowerAscii(right[index])) return false;
    } else if (left[index] != right[index]) {
      return false;
    }
  }
  return true;
}

IdentifierNormalization normalizeIdentifier(std::string_view requested, const IdentifierPolicy& policy) {
  assert(policy.max_length != 0);
  IdentifierPolicy effective = policy;
  effective.max_length = std::max(effective.max_length, std::size_t{1});
  return normalizeIdentifierImpl(requested, effective);
}

IdentifierRepair repairIdentifier(std::string& name, const IdentifierPolicy& policy) {
  if (isIdentifier(name, policy)) return IdentifierRepair::kNone;
  IdentifierNormalization normalized = normalizeIdentifier(name, policy);
  name = std::move(normalized.value);
  return normalized.repair;
}

void IdentifierUniquifier::reserve(std::size_t count, std::size_t occupied_count) {
  names_.reserve(count);
  values_.reserve(count + occupied_count);
}

void IdentifierUniquifier::occupy(std::string_view name) {
  IdentifierNormalization normalized = normalizeIdentifier(name, policy_);
  if (!normalized.value.empty()) values_.occupy(std::move(normalized.value));
}

void IdentifierUniquifier::add(std::string& name) { names_.push_back({&name}); }

IdentifierRepairSummary IdentifierUniquifier::apply(std::span<IdentifierRepair> repairs) {
  if (!repairs.empty() && repairs.size() != names_.size()) return {};

  std::vector<IdentifierNormalization> candidates;
  candidates.reserve(names_.size());
  for (const Entry& entry : names_) candidates.push_back(normalizeIdentifier(*entry.name, policy_));

  std::vector<std::string*> values;
  values.reserve(candidates.size());
  for (IdentifierNormalization& candidate : candidates)
    if (!candidate.value.empty() || !policy_.allow_empty) values.push_back(&candidate.value);
  std::vector<std::uint8_t> duplicates(values.size(), 0);
  IdentifierRepairSummary summary;
  const ValueUniquifierResult result = values_.apply(
      std::span(values),
      [&](std::string_view value, std::size_t suffix) { return suffixedIdentifier(value, suffix, policy_.max_length); },
      duplicates);
  if (result.error == ValueUniquifierError::kExhausted) {
    summary.exhausted = true;
    return summary;
  }
  summary.deduplicated = result.changed;
  std::size_t value_index = 0;
  for (IdentifierNormalization& candidate : candidates) {
    if (candidate.value.empty() && policy_.allow_empty) continue;
    if (duplicates[value_index++] != 0) candidate.repair |= IdentifierRepair::kDuplicate;
  }
  for (const IdentifierNormalization& candidate : candidates) {
    if (candidate.repair != IdentifierRepair::kNone && candidate.repair != IdentifierRepair::kDuplicate)
      ++summary.normalized;
  }
  summary.changed = static_cast<std::size_t>(
      std::count_if(candidates.begin(), candidates.end(), [](const IdentifierNormalization& candidate) {
        return candidate.repair != IdentifierRepair::kNone;
      }));

  for (std::size_t index = 0; index < names_.size(); ++index) {
    *names_[index].name = std::move(candidates[index].value);
    if (!repairs.empty()) repairs[index] = candidates[index].repair;
  }
  return summary;
}

bool isIdentifier(std::string_view name, const IdentifierPolicy& policy) noexcept {
  assert(policy.max_length != 0);
  const std::size_t max_length = std::max(policy.max_length, std::size_t{1});
  if (name.empty()) return policy.allow_empty;
  if (name.size() > max_length) return false;
  bool segment_start = true;
  bool previous_underscore = false;
  bool previous_dot = false;
  bool previous_space = false;
  for (char value : name) {
    if (value == '/') {
      if (!policy.allow_path_separators || segment_start || previous_underscore || previous_dot || previous_space)
        return false;
      segment_start = true;
      previous_underscore = false;
      previous_dot = false;
      previous_space = false;
      continue;
    }
    if (value == '\\' || !identifierCharacter(value, policy)) return false;
    if (policy.letter_case == IdentifierCase::kLower && value >= 'A' && value <= 'Z') return false;
    if (value == '.' && segment_start) return false;
    if (value == '_') {
      if (segment_start || previous_underscore) return false;
      previous_underscore = true;
    } else {
      segment_start = false;
      previous_underscore = false;
    }
    previous_dot = value == '.';
    previous_space = value == ' ';
  }
  return !segment_start && !previous_underscore && !previous_dot && !previous_space;
}

}  // namespace pistoris
