// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#pragma once

#include "arx_pistoris/base/indices.h"

#include <cstdint>
#include <string>
#include <vector>

namespace pistoris {

struct CinematicSoundSourceReference {
  SoundHandle sound = kNoSoundHandle;
  std::string path;
};

struct CinematicSoundFile {
  SoundHandle source_sound = kNoSoundHandle;
  LanguageId language = kSoundEffects;
  std::string path;
  std::vector<std::uint8_t> encoded_audio;
};

}  // namespace pistoris
