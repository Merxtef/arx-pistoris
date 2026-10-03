// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#pragma once

#include "arx_pistoris/ambiance/types.h"
#include "arx_pistoris/base/status.h"

#include "modules/ambiance.h"
#include "modules/sounds.h"

namespace pistoris {

struct AmbianceModules;

namespace ambiance_detail {

ArxReturnCode errorCode(ambiance::Error error) noexcept;
ArxReturnCode soundErrorCode(sounds::Error error) noexcept;
ArxReturnCode validateStructure(const AmbianceModules& modules) noexcept;
bool internalAutomation(const ArxAmbianceAutomation& source, Automation& out) noexcept;
ArxAmbianceAutomation publicAutomation(const Automation& source) noexcept;
bool internalTrack(const ArxAmbiancePannedTrackInput& source, AmbianceTrack& out);
bool internalTrack(const ArxAmbiancePositionedTrackInput& source, AmbianceTrack& out);
ArxAmbiancePannedKey publicKey(const PannedAmbianceKey& source) noexcept;
ArxAmbiancePositionedKey publicKey(const PositionedAmbianceKey& source) noexcept;
void warnAboutMasterTiming(const AmbianceModules& modules) noexcept;

}  // namespace ambiance_detail
}  // namespace pistoris
