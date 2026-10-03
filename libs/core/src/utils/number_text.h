// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#pragma once

#include <string_view>

namespace pistoris {

bool parseFiniteFloat(std::string_view text, float& out) noexcept;

}  // namespace pistoris
