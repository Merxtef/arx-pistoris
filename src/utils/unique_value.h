// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#pragma once

#include <algorithm>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <optional>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

namespace pistoris {

namespace detail {

inline std::string_view uniqueValueView(const std::string& value) noexcept { return value; }
inline std::string_view uniqueValueView(std::string* const& value) noexcept { return *value; }

inline std::string& uniqueValue(std::string& value) noexcept { return value; }
inline std::string& uniqueValue(std::string*& value) noexcept { return *value; }

}  // namespace detail

enum class ValueUniquifierError : std::uint8_t {
  kNone,
  kExhausted,
};

struct ValueUniquifierResult {
  std::size_t changed = 0;
  ValueUniquifierError error = ValueUniquifierError::kNone;
};

template <class Hash, class Equal>
class ValueUniquifier {
 public:
  explicit ValueUniquifier(Hash hash = {}, Equal equal = {}) : states_(0, std::move(hash), std::move(equal)) {}

  void reserve(std::size_t capacity) { states_.reserve(capacity); }

  void occupy(std::string value) {
    auto [entry, inserted] = states_.try_emplace(std::move(value), State::kOccupied);
    if (!inserted) entry->second = State::kOccupied;
  }

  template <class Value, class Suffix>
  ValueUniquifierResult apply(std::span<Value> values, Suffix&& suffix, std::span<std::uint8_t> changed = {}) {
    assert(changed.empty() || changed.size() == values.size());
    std::fill(changed.begin(), changed.end(), 0);
    clearTransient();
    try {
      if (values.size() > states_.max_size() - states_.size()) throw std::length_error("too many unique values");
      states_.reserve(states_.size() + values.size());
      for (const Value& value : values) {
        const std::string_view view = detail::uniqueValueView(value);
        if (!states_.contains(view)) states_.emplace(std::string(view), State::kAvailable);
      }

      struct Replacement {
        std::size_t index = 0;
        std::string value;
      };
      std::vector<Replacement> replacements;
      for (std::size_t index = 0; index < values.size(); ++index) {
        const std::string& value = detail::uniqueValue(values[index]);
        auto state = states_.find(std::string_view(value));
        if (state != states_.end() && state->second == State::kAvailable) {
          state->second = State::kClaimed;
          continue;
        }
        for (std::size_t ordinal = 1;;) {
          std::optional<std::string> candidate = suffix(value, ordinal);
          if (!candidate) {
            clearTransient();
            return {.error = ValueUniquifierError::kExhausted};
          }
          if (states_.try_emplace(*candidate, State::kClaimed).second) {
            replacements.push_back({index, std::move(*candidate)});
            break;
          }
          if (ordinal == std::numeric_limits<std::size_t>::max()) {
            clearTransient();
            return {.error = ValueUniquifierError::kExhausted};
          }
          ++ordinal;
        }
      }
      for (Replacement& replacement : replacements) {
        detail::uniqueValue(values[replacement.index]) = std::move(replacement.value);
        if (!changed.empty()) changed[replacement.index] = true;
      }
      const std::size_t changed_count = replacements.size();
      clearTransient();
      return {.changed = changed_count};
    } catch (...) {
      clearTransient();
      throw;
    }
  }

 private:
  enum class State : std::uint8_t {
    kOccupied,
    kAvailable,
    kClaimed,
  };

  void clearTransient() {
    std::erase_if(states_, [](const auto& entry) { return entry.second != State::kOccupied; });
  }

  std::unordered_map<std::string, State, Hash, Equal> states_;
};

inline std::string makeUniqueName(std::string_view requested, const std::unordered_set<std::string>& unavailable) {
  std::string prefix(requested);
  if (!unavailable.contains(prefix)) return prefix;
  if (!prefix.ends_with('_')) prefix.push_back('_');
  for (std::size_t suffix = 1;; ++suffix) {
    std::string candidate = prefix + std::to_string(suffix);
    if (!unavailable.contains(candidate)) return candidate;
  }
}

}  // namespace pistoris
