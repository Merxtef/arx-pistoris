// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#pragma once

#include <cstddef>
#include <string>
#include <string_view>

namespace pistoris {

bool normalizeNativeResourcePath(std::string_view source, std::string& out);
bool normalizeNativeResourceStem(std::string_view source, std::string& out);
bool encodeNativeResourceStem(std::string_view source, std::string& out);
bool encodeNativeResourceStem(std::string_view source, std::size_t capacity, std::string& out);

}  // namespace pistoris
