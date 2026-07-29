// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#pragma once

#include <cstdint>
#include <vector>

bool readFile(const char* path, std::vector<std::uint8_t>& out);
bool readFileOptional(const char* path, std::vector<std::uint8_t>& out);
