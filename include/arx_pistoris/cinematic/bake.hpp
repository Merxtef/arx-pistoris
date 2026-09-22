// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#pragma once

#include "arx_pistoris/base/image.h"
#include "arx_pistoris/cinematic/sound.hpp"
#include "arx_pistoris/native/cin.hpp"
#include "arx_pistoris/native/text.hpp"
#include "arx_pistoris/texture.hpp"

#include <vector>

namespace pistoris {

struct NativeCinematicBakeOptions {
  // Include encoded illustration sidecars
  bool include_illustration_files = true;
  // Include encoded sound sidecars
  bool include_sound_files = true;
  // UNKNOWN retains BMP and game-compatible TGA, using TGA otherwise; BMP or TGA forces that format.
  ArxImageFormat illustration_format = ARX_IMAGE_FORMAT_UNKNOWN;
  NativeTextMode text_mode = NativeTextMode::kAuto;
};

struct NativeCinematicBundle {
  cin::Data cin;
  std::vector<NativeTextureFile> illustration_files;
  std::vector<CinematicSoundFile> sound_files;
};

}  // namespace pistoris
