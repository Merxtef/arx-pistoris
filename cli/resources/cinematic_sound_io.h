// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#pragma once

#include "arx_pistoris/cinematic/sound.hpp"
#include "arx_pistoris/native/cin.hpp"
#include "arx_pistoris/native/text.hpp"
#include "arx_pistoris/sound.hpp"

#include "resources/sound_io.h"

#include <cstdint>
#include <span>
#include <vector>

namespace pistoris {
class Cinematic;
}

namespace cli {

class IoService;

enum class CinematicSoundSourceFormat : std::uint8_t {
  kCin,
  kGlb,
};

bool prepareCinematicSounds(pistoris::Cinematic& cinematic, IoService& io, const SoundInput& input,
                            std::span<pistoris::CinematicSoundSourceReference> sources,
                            CinematicSoundSourceFormat format, bool load_files);
void loadNativeCinematicSoundFiles(const pistoris::cin::Data& cinematic, pistoris::NativeTextMode text_mode,
                                   IoService& io, const SoundInput& input, std::vector<pistoris::SoundFile>& out);

}  // namespace cli
