// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#pragma once

#include "arx_pistoris/animation.hpp"
#include "arx_pistoris/native/tea.hpp"
#include "arx_pistoris/sound.hpp"

#include <variant>
#include <vector>

namespace cli::animation {

struct NativeAnimation {
  pistoris::Tea animation;
  std::vector<pistoris::SoundFile> sound_files;
};

struct IntermediateAnimation {
  pistoris::Animation animation;
  std::vector<pistoris::SoundSourceReference> sound_sources;
};

using AnimationInput = std::variant<std::monostate, NativeAnimation, IntermediateAnimation>;

}  // namespace cli::animation
