// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#pragma once

#include "arx_pistoris/base/location.hpp"
#include "arx_pistoris/base/result.hpp"
#include "arx_pistoris/model/location.hpp"
#include "arx_pistoris/native/location.hpp"

#include <cstdint>
#include <variant>

namespace pistoris {

enum class LevelElement : std::uint8_t {
  kResource,
  kMinimap,
  kLoadingScreen,
  kVertex,
  kFace,
  kTexture,
  kRoom,
  kPortal,
  kRoomDistance,
  kNavSurfaceVertex,
  kNavSurfaceTriangle,
  kAnchor,
  kAnchorConnection,
  kLight,
  kPlayerSpawn,
  kEntity,
  kFog,
  kZone,
  kZonePerimeterPoint,
  kPath,
  kPathNode,
};

using LevelLocation = ResourceLocation<LevelElement>;
using LevelNativeLocation = std::variant<FtsLocation, LlfLocation, DlfLocation>;
using LevelGlbExportLocation = std::variant<LevelLocation, ModelLocation>;

template <class T>
using LevelResult = Result<T, LevelLocation>;

template <class T>
using LevelNativeResult = Result<T, LevelNativeLocation>;

template <class T>
using LevelGlbExportResult = Result<T, LevelGlbExportLocation>;

}  // namespace pistoris
