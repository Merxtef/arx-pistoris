// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#pragma once

#include "utils/unique_value.h"

#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace pistoris {

enum class IdentifierCase : std::uint8_t {
  kPreserve,
  kLower,
};

enum class IdentifierRepair : std::uint8_t {
  kNone = 0,
  kCase = 1U << 0U,
  kCharacters = 1U << 1U,
  kUnderscores = 1U << 2U,
  kSeparators = 1U << 3U,
  kLength = 1U << 4U,
  kEmpty = 1U << 5U,
  kDuplicate = 1U << 6U,
};

constexpr IdentifierRepair operator|(IdentifierRepair left, IdentifierRepair right) noexcept {
  return static_cast<IdentifierRepair>(static_cast<std::uint8_t>(left) | static_cast<std::uint8_t>(right));
}

constexpr IdentifierRepair& operator|=(IdentifierRepair& left, IdentifierRepair right) noexcept {
  left = left | right;
  return left;
}

constexpr bool hasIdentifierRepair(IdentifierRepair value, IdentifierRepair repair) noexcept {
  return (static_cast<std::uint8_t>(value) & static_cast<std::uint8_t>(repair)) != 0;
}

struct IdentifierPolicy {
  IdentifierCase letter_case = IdentifierCase::kPreserve;
  std::size_t max_length = 1023;
  bool allow_empty = false;
  bool allow_path_separators = false;
  bool allow_brackets = false;
  bool allow_dots = false;
  bool allow_spaces = false;
  bool allow_parentheses = false;
  bool allow_ampersands = false;
  bool ascii_case_insensitive = false;
};

struct IdentifierNormalization {
  std::string value;
  IdentifierRepair repair = IdentifierRepair::kNone;
};

struct IdentifierRepairSummary {
  std::size_t changed = 0;
  std::size_t normalized = 0;
  std::size_t deduplicated = 0;
  bool exhausted = false;
};

class IdentifierUniquifier {
 public:
  explicit IdentifierUniquifier(IdentifierPolicy policy = {});

  void reserve(std::size_t count, std::size_t occupied_count = 0);
  void occupy(std::string_view name);
  void add(std::string& name);
  IdentifierRepairSummary apply(std::span<IdentifierRepair> repairs = {});

 private:
  struct IdentityHash {
    using is_transparent = void;  // NOLINT(readability-identifier-naming)

    bool ascii_case_insensitive = false;
    std::size_t operator()(std::string_view value) const noexcept;
  };

  struct IdentityEqual {
    using is_transparent = void;  // NOLINT(readability-identifier-naming)

    bool ascii_case_insensitive = false;
    bool operator()(std::string_view left, std::string_view right) const noexcept;
  };

  struct Entry {
    std::string* name = nullptr;
  };

  IdentifierPolicy policy_;
  std::vector<Entry> names_;
  ValueUniquifier<IdentityHash, IdentityEqual> values_;
};

IdentifierNormalization normalizeIdentifier(std::string_view requested, const IdentifierPolicy& policy = {});
IdentifierRepair repairIdentifier(std::string& name, const IdentifierPolicy& policy = {});
bool isIdentifier(std::string_view name, const IdentifierPolicy& policy = {}) noexcept;

}  // namespace pistoris
