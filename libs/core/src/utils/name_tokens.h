// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#pragma once

#include <initializer_list>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace pistoris {

bool hasDoubleUnderscore(std::string_view text) noexcept;
bool hasEmbeddedNull(std::string_view text) noexcept;
bool validSemanticString(std::string_view text) noexcept;
void splitDoubleUnderscore(std::string_view text, std::vector<std::string_view>& out);
std::string joinDoubleUnderscore(std::span<const std::string_view> tokens);
std::string joinDoubleUnderscore(std::initializer_list<std::string_view> tokens);

}  // namespace pistoris
