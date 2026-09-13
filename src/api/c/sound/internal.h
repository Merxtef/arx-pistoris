// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#pragma once

#include "arx_pistoris/sound.h"
#include "arx_pistoris/sound.hpp"

#include "api/c/internal.h"

#include <vector>

struct arx_pistoris_sound_files {
  std::vector<pistoris::SoundFile> value;
};

struct arx_pistoris_sound_source_references {
  std::vector<pistoris::SoundSourceReference> value;
};

struct arx_pistoris_animation_sound_files {
  std::vector<pistoris::AnimationSoundFile> value;
};

struct arx_pistoris_animation_sound_source_references {
  std::vector<pistoris::AnimationSoundSourceReference> value;
};
