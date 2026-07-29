// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#pragma once

#include "arx_pistoris/native/tea.hpp"

#include "resources/selector.h"

#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace cli {

bool buildAnimationTargets(std::span<const pistoris::tea::Data> teas, const OutputTarget& base,
                           std::string_view resource_directory, Format output_format,
                           std::span<const OutputTarget> reserved_targets, std::vector<OutputTarget>& out,
                           std::string& error);

bool buildNativeAnimationTargets(std::span<const pistoris::tea::Data> teas, const OutputTarget& base,
                                 std::string_view resource_directory, std::vector<OutputTarget>& out,
                                 std::string& error);

}  // namespace cli
