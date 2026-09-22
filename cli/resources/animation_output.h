// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#pragma once

#include "resources/selector.h"

#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace cli {

struct AnimationOutputIdentity {
  std::string name;
  std::string resource_path;
};

bool buildAnimationTargets(std::span<const AnimationOutputIdentity> animations, const OutputTarget& base,
                           std::string_view resource_directory, Format output_format,
                           std::span<const std::string_view> reserved_paths, std::vector<OutputTarget>& out,
                           std::string& error);

bool buildGameAnimationTargets(std::span<const AnimationOutputIdentity> animations, std::string_view fallback_type,
                               std::span<const std::string_view> reserved_paths, std::vector<OutputTarget>& out,
                               std::string& error);

}  // namespace cli
