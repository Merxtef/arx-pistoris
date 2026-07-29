// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

namespace pistoris::glb_level {

std::optional<std::uint32_t> parseUnsignedToken(std::string_view token);
std::optional<std::int32_t> parseSignedToken(std::string_view token);
bool parseFloatToken(std::string_view token, float& out);
std::string formatFloatToken(float value);

}  // namespace pistoris::glb_level
