// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#pragma once

#include <cstdint>
#include <string>
#include <string_view>

namespace pistoris {

enum class InteractiveKind : std::uint8_t {
  kUnknown,
  kItem,
  kNpc,
  kFix,
  kCamera,
  kMarker,
};

bool isLegacyTeoExtension(std::string_view extension) noexcept;
bool normalizeEntityClassPath(std::string_view source, std::string& out, std::string_view& removed_extension,
                              bool* discarded_prefix = nullptr);
InteractiveKind classifyEntityClassPath(std::string_view normalized_path) noexcept;

}  // namespace pistoris
