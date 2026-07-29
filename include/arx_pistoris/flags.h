/*
 * Copyright 2011-2022 Arx Libertatis Team (see the AUTHORS file)
 *
 * This file is part of Arx Libertatis.
 *
 * Arx Libertatis is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * Arx Libertatis is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Arx Libertatis.  If not, see <http://www.gnu.org/licenses/>.
 */
/* Based on:
===========================================================================
ARX FATALIS GPL Source Code
Copyright (C) 1999-2010 Arkane Studios SA, a ZeniMax Media company.

This file is part of the Arx Fatalis GPL Source Code ('Arx Fatalis Source Code').

Arx Fatalis Source Code is free software: you can redistribute it and/or modify it under the terms
of the GNU General Public License as published by the Free Software Foundation, either version 3 of
the License, or (at your option) any later version.

Arx Fatalis Source Code is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License along with Arx Fatalis Source
Code.  If not, see <http://www.gnu.org/licenses/>.

In addition, the Arx Fatalis Source Code is also subject to certain additional terms. You should
have received a copy of these additional terms immediately following the terms and conditions of the
GNU General Public License which accompanied the Arx Fatalis Source Code. If not, please request a
copy in writing from Arkane Studios at the address below.

If you have questions concerning this license or the applicable additional terms, you may contact in
writing Arkane Studios, c/o ZeniMax Media Inc., Suite 120, Rockville, Maryland 20850 USA.
===========================================================================
*/
// Code: Cyril Meynier
//       Didier Pedreno (update Light Model version 1)
//
// Copyright (c) 1999-2000 ARKANE Studios SA. All rights reserved
// Sources:
// https://github.com/arx/ArxLibertatis/blob/cbdaea8bf53118c767aafc22a94c358585242f48/src/graphics/GraphicsTypes.h
// https://github.com/arx/ArxLibertatis/blob/cbdaea8bf53118c767aafc22a94c358585242f48/src/scene/Light.h
/*
 * Modified for arx-pistoris:
 * Copyright (C) 2026 Merxtef
 */

#ifndef ARX_PISTORIS_FLAGS_H
#define ARX_PISTORIS_FLAGS_H

#include <stdint.h>

// NOLINTBEGIN(readability-identifier-naming, performance-enum-size)

typedef uint32_t ArxFaceType;
typedef uint32_t ArxLightFlags;

enum {
  ARX_FACE_BIT_NO_SHADOW = 1U << 0,
  ARX_FACE_BIT_DOUBLESIDED = 1U << 1,
  ARX_FACE_BIT_TRANS = 1U << 2,
  ARX_FACE_BIT_WATER = 1U << 3,
  ARX_FACE_BIT_GLOW = 1U << 4,
  ARX_FACE_BIT_IGNORE = 1U << 5,
  ARX_FACE_BIT_QUAD = 1U << 6,
  ARX_FACE_BIT_TILED = 1U << 7,
  ARX_FACE_BIT_METAL = 1U << 8,
  ARX_FACE_BIT_HIDE = 1U << 9,
  ARX_FACE_BIT_STONE = 1U << 10,
  ARX_FACE_BIT_WOOD = 1U << 11,
  ARX_FACE_BIT_GRAVEL = 1U << 12,
  ARX_FACE_BIT_EARTH = 1U << 13,
  ARX_FACE_BIT_NOCOL = 1U << 14,
  ARX_FACE_BIT_LAVA = 1U << 15,
  ARX_FACE_BIT_CLIMB = 1U << 16,
  ARX_FACE_BIT_FALL = 1U << 17,
  ARX_FACE_BIT_NOPATH = 1U << 18,
  ARX_FACE_BIT_NODRAW = 1U << 19,
  ARX_FACE_BIT_PRECISE_PATH = 1U << 20,
  ARX_FACE_BIT_NO_CLIMB = 1U << 21,
  ARX_FACE_BIT_ANGULAR = 1U << 22,
  ARX_FACE_BIT_ANGULAR_IDX0 = 1U << 23,
  ARX_FACE_BIT_ANGULAR_IDX1 = 1U << 24,
  ARX_FACE_BIT_ANGULAR_IDX2 = 1U << 25,
  ARX_FACE_BIT_ANGULAR_IDX3 = 1U << 26,
  ARX_FACE_BIT_LATE_MIP = 1U << 27,
  ARX_FACE_BITS_ALL = (1U << 28) - 1U
};

enum {
  ARX_LIGHT_FLAG_SEMIDYNAMIC = 0x0001,
  ARX_LIGHT_FLAG_EXTINGUISHABLE = 0x0002,
  ARX_LIGHT_FLAG_START_EXTINGUISHED = 0x0004,
  ARX_LIGHT_FLAG_SPAWN_FIRE = 0x0008,
  ARX_LIGHT_FLAG_SPAWN_SMOKE = 0x0010,
  ARX_LIGHT_FLAG_OFF = 0x0020,
  ARX_LIGHT_FLAG_COLOR_LEGACY = 0x0040,
  ARX_LIGHT_FLAG_NO_CASTED = 0x0080,
  ARX_LIGHT_FLAG_FIX_FLARE_SIZE = 0x0100,
  ARX_LIGHT_FLAG_FIREPLACE = 0x0200,
  ARX_LIGHT_FLAG_NO_IGNIT = 0x0400,
  ARX_LIGHT_FLAG_FLARE = 0x0800,
  ARX_LIGHT_FLAGS_ALL = 0x0fff
};

#ifdef __cplusplus
namespace pistoris {

using FaceType = ::ArxFaceType;
using LightFlags = ::ArxLightFlags;

enum FaceTypeBitmask : FaceType {
  kFaceBitNoShadow = ARX_FACE_BIT_NO_SHADOW,
  kFaceBitDoublesided = ARX_FACE_BIT_DOUBLESIDED,
  kFaceBitTrans = ARX_FACE_BIT_TRANS,
  kFaceBitWater = ARX_FACE_BIT_WATER,
  kFaceBitGlow = ARX_FACE_BIT_GLOW,
  kFaceBitIgnore = ARX_FACE_BIT_IGNORE,
  kFaceBitQuad = ARX_FACE_BIT_QUAD,
  kFaceBitTiled = ARX_FACE_BIT_TILED,
  kFaceBitMetal = ARX_FACE_BIT_METAL,
  kFaceBitHide = ARX_FACE_BIT_HIDE,
  kFaceBitStone = ARX_FACE_BIT_STONE,
  kFaceBitWood = ARX_FACE_BIT_WOOD,
  kFaceBitGravel = ARX_FACE_BIT_GRAVEL,
  kFaceBitEarth = ARX_FACE_BIT_EARTH,
  kFaceBitNocol = ARX_FACE_BIT_NOCOL,
  kFaceBitLava = ARX_FACE_BIT_LAVA,
  kFaceBitClimb = ARX_FACE_BIT_CLIMB,
  kFaceBitFall = ARX_FACE_BIT_FALL,
  kFaceBitNopath = ARX_FACE_BIT_NOPATH,
  kFaceBitNodraw = ARX_FACE_BIT_NODRAW,
  kFaceBitPrecisePath = ARX_FACE_BIT_PRECISE_PATH,
  kFaceBitNoClimb = ARX_FACE_BIT_NO_CLIMB,
  kFaceBitAngular = ARX_FACE_BIT_ANGULAR,
  kFaceBitAngularIdX0 = ARX_FACE_BIT_ANGULAR_IDX0,
  kFaceBitAngularIdX1 = ARX_FACE_BIT_ANGULAR_IDX1,
  kFaceBitAngularIdX2 = ARX_FACE_BIT_ANGULAR_IDX2,
  kFaceBitAngularIdX3 = ARX_FACE_BIT_ANGULAR_IDX3,
  kFaceBitLateMip = ARX_FACE_BIT_LATE_MIP,
  kFaceBitsAll = ARX_FACE_BITS_ALL,
};

enum LightFlagBitmask : uint16_t {
  kLightFlagSemidynamic = ARX_LIGHT_FLAG_SEMIDYNAMIC,
  kLightFlagExtinguishable = ARX_LIGHT_FLAG_EXTINGUISHABLE,
  kLightFlagStartExtinguished = ARX_LIGHT_FLAG_START_EXTINGUISHED,
  kLightFlagSpawnFire = ARX_LIGHT_FLAG_SPAWN_FIRE,
  kLightFlagSpawnSmoke = ARX_LIGHT_FLAG_SPAWN_SMOKE,
  kLightFlagOff = ARX_LIGHT_FLAG_OFF,
  kLightFlagColorLegacy = ARX_LIGHT_FLAG_COLOR_LEGACY,
  kLightFlagNoCasted = ARX_LIGHT_FLAG_NO_CASTED,
  kLightFlagFixFlareSize = ARX_LIGHT_FLAG_FIX_FLARE_SIZE,
  kLightFlagFireplace = ARX_LIGHT_FLAG_FIREPLACE,
  kLightFlagNoIgnit = ARX_LIGHT_FLAG_NO_IGNIT,
  kLightFlagFlare = ARX_LIGHT_FLAG_FLARE,
  kLightFlagsAll = ARX_LIGHT_FLAGS_ALL,
};

}  // namespace pistoris
#endif

// NOLINTEND(readability-identifier-naming, performance-enum-size)

#endif /* ARX_PISTORIS_FLAGS_H */
