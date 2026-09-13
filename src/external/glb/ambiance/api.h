// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#pragma once

#include "arx_pistoris/ambiance.hpp"
#include "arx_pistoris/base/status.h"
#include "arx_pistoris/sound.hpp"

#include <cstdint>
#include <span>
#include <vector>

namespace pistoris {

struct AmbianceModules;
struct ModelModules;

ArxReturnCode importAmbianceFromGlb(std::span<const std::uint8_t> glb, const Ambiance::GlbImportOptions& options,
                                    AmbianceModules& out, std::vector<SoundSourceReference>* sound_sources = nullptr);
ArxReturnCode exportAmbianceToGlb(const AmbianceModules& ambiance, const Ambiance::GlbExportOptions& options,
                                  const ModelModules* reference_model, std::vector<std::uint8_t>& out);

}  // namespace pistoris
