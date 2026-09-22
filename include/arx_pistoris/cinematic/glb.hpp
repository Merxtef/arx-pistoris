// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#pragma once

#include "arx_pistoris/cinematic/sound.hpp"

#include <cstdint>
#include <vector>

namespace pistoris {

struct CinematicGlbBundle {
  std::vector<std::uint8_t> glb;
  std::vector<CinematicSoundFile> sound_files;
};

}  // namespace pistoris
