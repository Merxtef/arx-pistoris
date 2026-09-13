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

struct AnimationOutputIdentity {
  std::string_view name;
  std::string_view resource_path;
};

AnimationOutputIdentity nativeAnimationOutputIdentity(const pistoris::tea::Data& animation,
                                                      std::string_view resource_path = {}) noexcept;

bool buildAnimationTargets(std::span<const AnimationOutputIdentity> animations, const OutputTarget& base,
                           std::string_view resource_directory, Format output_format,
                           std::span<const std::string_view> reserved_paths, std::vector<OutputTarget>& out,
                           std::string& error);

bool buildAnimationTargets(std::span<const pistoris::tea::Data> teas, const OutputTarget& base,
                           std::string_view resource_directory, Format output_format,
                           std::span<const std::string_view> reserved_paths, std::vector<OutputTarget>& out,
                           std::string& error);

bool buildNativeAnimationTargets(std::span<const pistoris::tea::Data> teas, const OutputTarget& base,
                                 std::string_view resource_directory, std::vector<OutputTarget>& out,
                                 std::string& error);

bool buildGameAnimationTargets(std::span<const AnimationOutputIdentity> animations, std::string_view fallback_type,
                               std::span<const std::string_view> reserved_paths, std::vector<OutputTarget>& out,
                               std::string& error);

}  // namespace cli
