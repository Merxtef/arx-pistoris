// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#pragma once

#include "arx_pistoris/cinematic.hpp"
#include "arx_pistoris/cinematic/sound.hpp"
#include "arx_pistoris/native/cin.hpp"
#include "arx_pistoris/native/text.hpp"

#include "resources/cinematic_sound_io.h"

#include <string>
#include <variant>
#include <vector>

namespace cli::cinematic {

struct NativeCinematic {
  pistoris::Cin cinematic;
  pistoris::NativeTextMode text_mode = pistoris::NativeTextMode::kUtf8;
};

struct IntermediateCinematic {
  IntermediateCinematic() = default;
  IntermediateCinematic(const IntermediateCinematic&) = delete;
  IntermediateCinematic& operator=(const IntermediateCinematic&) = delete;
  IntermediateCinematic(IntermediateCinematic&&) = delete;
  IntermediateCinematic& operator=(IntermediateCinematic&&) = delete;

  pistoris::Cinematic cinematic;
  std::vector<std::string> illustration_sources;
  std::vector<pistoris::CinematicSoundSourceReference> sound_sources;
  CinematicSoundSourceFormat sound_source_format = CinematicSoundSourceFormat::kCin;
};

using CinematicInput = std::variant<std::monostate, NativeCinematic, IntermediateCinematic>;

}  // namespace cli::cinematic
