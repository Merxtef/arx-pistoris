// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#pragma once

#include "arx_pistoris/base/status.h"
#include "arx_pistoris/cinematic/types.h"

#include "modules/cinematic.h"
#include "modules/sounds.h"
#include "modules/textures.h"

namespace pistoris {

struct CinematicModules;

namespace cinematic_detail {

ArxReturnCode errorCode(cinematic::Error error) noexcept;
ArxReturnCode textureError(textures::Error error) noexcept;
ArxReturnCode soundError(sounds::Error error) noexcept;
ArxReturnCode validateStructure(const CinematicModules& modules) noexcept;
ArxReturnCode internalKeyframe(const ArxCinematicKeyframe& source, CinematicKeyframe& out) noexcept;
ArxCinematicKeyframe publicKeyframe(const CinematicKeyframe& source) noexcept;

}  // namespace cinematic_detail
}  // namespace pistoris
