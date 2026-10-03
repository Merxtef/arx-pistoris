// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#pragma once

#include "arx_pistoris/base/math.h"

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

namespace pistoris::glb {

std::optional<std::uint32_t> parseUnsignedToken(std::string_view token);
std::optional<std::int32_t> parseSignedToken(std::string_view token);
bool parseFloatToken(std::string_view token, float& out);
std::string formatFloatToken(float value);
bool parseColor3Token(std::string_view token, ArxColor3& out);
std::string formatColor3Token(const ArxColor3& value);

}  // namespace pistoris::glb
