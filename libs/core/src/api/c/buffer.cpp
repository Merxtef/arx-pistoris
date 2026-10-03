// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#include "arx_pistoris/base/buffer.h"

#include <cstdint>

// NOLINTBEGIN(readability-identifier-naming)

void arx_pistoris_free_string(char* value) noexcept { delete[] value; }

void arx_pistoris_free_bytes(std::uint8_t* value) noexcept { delete[] value; }

// NOLINTEND(readability-identifier-naming)
