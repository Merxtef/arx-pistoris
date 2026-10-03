// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#pragma once

#include <cstdint>
#include <string_view>

namespace cli {

enum class ResourceReadResult : std::uint8_t;

void reportRequiredReadFailure(ResourceReadResult result, std::string_view description, std::string_view path);

}  // namespace cli
