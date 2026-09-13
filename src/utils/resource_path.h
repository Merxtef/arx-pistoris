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

enum class ResourcePathRepair : std::uint8_t {
  kNone = 0,
  kCase = 1U << 0U,
  kSeparators = 1U << 1U,
  kCharacters = 1U << 2U,
  kTrailing = 1U << 3U,
  kReserved = 1U << 4U,
  kLength = 1U << 5U,
  kDuplicate = 1U << 6U,
};

constexpr ResourcePathRepair operator|(ResourcePathRepair left, ResourcePathRepair right) noexcept {
  return static_cast<ResourcePathRepair>(static_cast<std::uint8_t>(left) | static_cast<std::uint8_t>(right));
}

constexpr ResourcePathRepair& operator|=(ResourcePathRepair& left, ResourcePathRepair right) noexcept {
  left = left | right;
  return left;
}

constexpr bool hasResourcePathRepair(ResourcePathRepair value, ResourcePathRepair repair) noexcept {
  return (static_cast<std::uint8_t>(value) & static_cast<std::uint8_t>(repair)) != 0;
}

constexpr bool hasStructuralResourcePathRepair(ResourcePathRepair value) noexcept {
  return hasResourcePathRepair(value, ResourcePathRepair::kCharacters) ||
         hasResourcePathRepair(value, ResourcePathRepair::kTrailing) ||
         hasResourcePathRepair(value, ResourcePathRepair::kReserved) ||
         hasResourcePathRepair(value, ResourcePathRepair::kLength) ||
         hasResourcePathRepair(value, ResourcePathRepair::kDuplicate);
}

enum class ResourcePathError : std::uint8_t {
  kNone,
  kBadPath,
};

struct ResourcePathRepairSummary {
  std::size_t changed = 0;
  std::size_t normalized = 0;
  std::size_t deduplicated = 0;
};

struct ResourcePathNormalization {
  std::string value;
  ResourcePathRepair repair = ResourcePathRepair::kNone;
  ResourcePathError error = ResourcePathError::kNone;
};

struct ResourcePathIdentityHash {
  using is_transparent = void;  // NOLINT(readability-identifier-naming)

  std::size_t operator()(std::string_view path) const noexcept;
};

struct ResourcePathIdentityEqual {
  using is_transparent = void;  // NOLINT(readability-identifier-naming)

  bool operator()(std::string_view left, std::string_view right) const noexcept;
};

class ResourcePathUniquifier {
 public:
  void reserve(std::size_t count, std::size_t occupied_count = 0);
  ResourcePathError occupy(std::string_view path);
  void add(std::string& path);
  ResourcePathError apply(ResourcePathRepairSummary* summary = nullptr, std::span<ResourcePathRepair> repairs = {});

 private:
  std::vector<std::string*> paths_;
  ValueUniquifier<ResourcePathIdentityHash, ResourcePathIdentityEqual> values_;
};

ResourcePathNormalization normalizeResourcePath(std::string_view requested);
ResourcePathNormalization normalizeResourceDirectory(std::string_view requested);
bool isPortableResourcePathComponent(std::string_view component) noexcept;
bool isResourcePath(std::string_view path) noexcept;
void normalizeResourcePathIdentity(std::string& path) noexcept;
std::string resourcePathIdentityKey(std::string_view path);

}  // namespace pistoris
