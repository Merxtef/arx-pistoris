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
// Source: https://github.com/arx/ArxLibertatis/blob/cbdaea8/src/graphics/GraphicsTypes.h
/*
 * Modified for arx-pistoris:
 * Copyright (C) 2026 Merxtef
 */

#pragma once

#include <cstdint>
namespace pistoris {

using FaceType = std::uint32_t;

enum FaceTypeBitmask : FaceType {
  kFaceBitNoShadow    = 1U << 0,
  kFaceBitDoublesided = 1U << 1,
  kFaceBitTrans       = 1U << 2,
  kFaceBitWater       = 1U << 3,
  kFaceBitGlow        = 1U << 4,
  kFaceBitIgnore      = 1U << 5,
  kFaceBitQuad        = 1U << 6,
  kFaceBitTiled       = 1U << 7,
  kFaceBitMetal       = 1U << 8,
  kFaceBitHide        = 1U << 9,
  kFaceBitStone       = 1U << 10,
  kFaceBitWood        = 1U << 11,
  kFaceBitGravel      = 1U << 12,
  kFaceBitEarth       = 1U << 13,
  kFaceBitNocol       = 1U << 14,
  kFaceBitLava        = 1U << 15,
  kFaceBitClimb       = 1U << 16,
  kFaceBitFall        = 1U << 17,
  kFaceBitNopath      = 1U << 18,
  kFaceBitNodraw      = 1U << 19,
  kFaceBitPrecisePath = 1U << 20,
  kFaceBitNoClimb     = 1U << 21,
  kFaceBitAngular     = 1U << 22,
  kFaceBitAngularIdX0 = 1U << 23,
  kFaceBitAngularIdX1 = 1U << 24,
  kFaceBitAngularIdX2 = 1U << 25,
  kFaceBitAngularIdX3 = 1U << 26,
  kFaceBitLateMip     = 1U << 27,
  kFaceBitsAll        = (1U << 28) - 1,
};

}  // namespace pistoris
