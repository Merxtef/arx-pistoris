// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#pragma once

#include "arx_pistoris/cinematic.h"
#include "arx_pistoris/cinematic.hpp"
#include "arx_pistoris/cinematic/sound.hpp"

#include <vector>

struct arx_pistoris_cinematic {
  pistoris::Cinematic value;
};

struct arx_pistoris_cinematic_sound_files {
  std::vector<pistoris::CinematicSoundFile> value;
};

struct arx_pistoris_cinematic_sound_source_references {
  std::vector<pistoris::CinematicSoundSourceReference> value;
};
