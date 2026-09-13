// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#pragma once

#include "arx_pistoris/native/amb.hpp"
#include "arx_pistoris/sound.hpp"

#include <cstdint>
#include <vector>

namespace pistoris {

struct NativeAmbianceBundle {
  amb::Data amb;
  std::vector<SoundFile> sound_files;
};

struct AmbianceGlbBundle {
  std::vector<std::uint8_t> glb;
  std::vector<SoundFile> sound_files;
};

}  // namespace pistoris
