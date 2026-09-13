// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#pragma once

#include <string_view>

namespace pistoris {

bool resolvesThroughDefaultLooseRoot(std::string_view lookup_base, std::string_view stored_path) noexcept;

}  // namespace pistoris
