// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#pragma once

#include <string>
#include <string_view>

namespace pistoris {

bool isLegacyTeoExtension(std::string_view extension) noexcept;
bool normalizeEntityClassPath(std::string_view source, std::string& out, std::string_view& removed_extension);

}  // namespace pistoris
