// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#pragma once

#include "arx_pistoris/base/location.hpp"
#include "arx_pistoris/base/result.hpp"
#include "arx_pistoris/model/location.hpp"

#include <cstdint>
#include <variant>

namespace pistoris {

enum class AmbianceElement : std::uint8_t {
  kResource,
  kTrack,
  kKey,
  kSound,
};

using AmbianceLocation = ResourceLocation<AmbianceElement>;

template <class T>
using AmbianceResult = Result<T, AmbianceLocation>;

using AmbianceGlbExportLocation = std::variant<AmbianceLocation, ModelLocation>;

template <class T>
using AmbianceGlbExportResult = Result<T, AmbianceGlbExportLocation>;

}  // namespace pistoris
