// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#pragma once

#include "arx_pistoris/base/indices.h"

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace pistoris {

struct NativeSoundBakeOptions {
  // Include encoded sidecars in output bundle
  bool include_files = true;
};

struct SoundFile {
  SoundIndex source_sound = kNoSound;
  std::string path;
  std::vector<std::uint8_t> encoded_audio;
};

struct SoundSourceReference {
  SoundIndex sound = kNoSound;
  std::string path;
};

struct AnimationSoundFile {
  std::size_t animation_index = 0;
  SoundFile file;
};

struct AnimationSoundSourceReference {
  std::size_t animation_index = 0;
  SoundSourceReference reference;
};

}  // namespace pistoris
