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
// Copyright (c) 1999-2001 ARKANE Studios SA. All rights reserved
// Source: https://github.com/arx/ArxLibertatis/blob/1dfb6c4/src/animation/AnimationFormat.h
/*
 * Modified for arx-pistoris:
 * Copyright (C) 2026 Merxtef
 */

#pragma once
#include "arx_pistoris/arx_math.hpp"

#include <cstdint>
#include <optional>
#include <vector>

namespace pistoris {

constexpr char kTeaMagic[20]        = "Theo Animation File";  // 20-byte null-terminated identity
constexpr uint32_t kTeaVersion      = 2014;                   // minimum accepted, also written
constexpr uint32_t kTeaVersionAlt   = 2015;                   // >= this: 288-byte keyframe header
constexpr int32_t kTeaFlagFrameNone = -1;
constexpr int32_t kTeaFlagFrameStep = 9;      // footstep trigger
constexpr float kTeaFps             = 24.0f;  // fixed playback rate

constexpr std::size_t kTeaMaxGroups    = 1024;     // sanity cap
constexpr std::size_t kTeaMaxKeyframes = 0x10000;  // sanity cap

namespace tea {

struct Sample {
  char name[256] = {};
};
static_assert(sizeof(Sample) == 256);  // read from file directly

struct GroupAnim {
  int32_t key_group    = 0;  // stored but unchecked at runtime
  ArxQuat quat         = {};
  ArxVector3 translate = {};
  ArxVector3 zoom      = {};
};

struct Keyframe {
  int32_t num_frame  = 0;
  int32_t flag_frame = kTeaFlagFrameNone;
  std::optional<ArxVector3> translate;  // present iff key_move != 0
  std::optional<ArxQuat> quat;          // present iff key_orient != 0
  std::vector<GroupAnim> groups;        // num_groups entries
  std::optional<Sample> sample;         // present iff num_sample != -1; audio bytes dropped
};

struct Data {
  int32_t num_frames = 0;   // animation duration (num_frames * 1_000_000 / 24 us)
  int32_t num_groups = 0;   // bone group count; redundant when keyframes non-empty
  char name[256]     = {};  // anim_name; null-terminated, may be empty
  std::vector<Keyframe> keyframes;
};

}  // namespace tea

}  // namespace pistoris
