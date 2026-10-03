// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#pragma once

#include "arx_pistoris/ambiance.h"
#include "arx_pistoris/ambiance.hpp"

#include "api/c/internal.h"

struct arx_pistoris_ambiance {
  pistoris::Ambiance value;
};

namespace pistoris::c_api {

inline bool valid(const ArxAmbiancePannedTrackInput& track) noexcept { return valid(track.keys, track.key_count); }

inline bool valid(const ArxAmbiancePositionedTrackInput& track) noexcept { return valid(track.keys, track.key_count); }

}  // namespace pistoris::c_api
