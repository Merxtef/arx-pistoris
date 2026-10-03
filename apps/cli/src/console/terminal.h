// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#pragma once

#include <cstdio>

namespace cli {

bool terminalSupportsColor(std::FILE* output) noexcept;

}  // namespace cli
