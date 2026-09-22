// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#pragma once

#include "arx_pistoris/base/status.h"
#include "arx_pistoris/cinematic/sound.hpp"

#include <cstdint>
#include <span>
#include <vector>

namespace pistoris {

struct CinematicModules;

ArxReturnCode importCinematicFromGlb(std::span<const std::uint8_t> glb, CinematicModules& out,
                                     std::vector<CinematicSoundSourceReference>* sound_sources = nullptr);
ArxReturnCode exportCinematicToGlb(const CinematicModules& cinematic, std::vector<std::uint8_t>& out);

}  // namespace pistoris
