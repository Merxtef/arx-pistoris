// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#pragma once

#include "arx_pistoris/tea_data.hpp"

#include "state.h"

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

bool readFile(const char* path, std::vector<std::uint8_t>& out);
bool readFileOptional(const char* path, std::vector<std::uint8_t>& out);
bool writeFile(cli::State& state, const char* path, const void* data, std::size_t size);

bool isFtl(const std::vector<std::uint8_t>& buf);
bool isTea(const std::vector<std::uint8_t>& buf);
bool isGlb(const std::vector<std::uint8_t>& buf);

std::string sanitizeFilename(std::string_view name);
const char* pathFilename(const char* path);
const char* fileExtension(const char* path);
