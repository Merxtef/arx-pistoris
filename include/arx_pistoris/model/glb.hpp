// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#pragma once

#include "arx_pistoris/sound.hpp"

#include <cstdint>
#include <vector>

namespace pistoris {

struct ModelGlbBundle {
  std::vector<std::uint8_t> glb;
  std::vector<AnimationSoundFile> sound_files;
};

}  // namespace pistoris
