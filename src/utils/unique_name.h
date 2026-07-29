// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#pragma once

#include <string>
#include <string_view>
#include <unordered_set>

namespace pistoris {

// Comparison semantics belong to the caller: normalize names before populating unavailable when needed
std::string makeUniqueName(std::string_view requested, const std::unordered_set<std::string>& unavailable);
bool isPortableReservedName(std::string_view name) noexcept;
bool isPortableName(std::string_view name) noexcept;
std::string makeUniquePortableName(std::string_view requested, const std::unordered_set<std::string>& unavailable = {});

}  // namespace pistoris
