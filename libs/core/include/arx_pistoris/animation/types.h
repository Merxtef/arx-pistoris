// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#ifndef ARX_PISTORIS_ANIMATION_TYPES_H
#define ARX_PISTORIS_ANIMATION_TYPES_H

#include "arx_pistoris/base/indices.h"
#include "arx_pistoris/base/math.h"

#include <stddef.h>
#include <stdint.h>

// Public C-compatible Animation value types
// NOLINTBEGIN(readability-identifier-naming)

#ifdef __cplusplus
#define ARX_PISTORIS_DETAIL_CXX_DEFAULT(value) = value
#else
#define ARX_PISTORIS_DETAIL_CXX_DEFAULT(value)
#endif

typedef struct ArxAnimationGroupTransform {
  ArxQuat rotation;
  ArxVector3 translation;
  ArxVector3 scale ARX_PISTORIS_DETAIL_CXX_DEFAULT((ArxVector3{1.0f, 1.0f, 1.0f}));
} ArxAnimationGroupTransform;

typedef struct ArxAnimationKeyframe {
  uint32_t frame;
  ArxVector3 root_translation;
  ArxQuat root_rotation;
  uint8_t footstep ARX_PISTORIS_DETAIL_CXX_DEFAULT(0);
  ArxSoundIndex sound ARX_PISTORIS_DETAIL_CXX_DEFAULT(ARX_NO_SOUND);
} ArxAnimationKeyframe;

typedef struct ArxAnimationKeyframeInput {
  ArxAnimationKeyframe keyframe;
  const ArxAnimationGroupTransform* group_transforms ARX_PISTORIS_DETAIL_CXX_DEFAULT(nullptr);
  size_t group_count ARX_PISTORIS_DETAIL_CXX_DEFAULT(0);
} ArxAnimationKeyframeInput;

typedef struct ArxAnimationConversionReport {
  size_t converted ARX_PISTORIS_DETAIL_CXX_DEFAULT(0);
  size_t skipped ARX_PISTORIS_DETAIL_CXX_DEFAULT(0);
} ArxAnimationConversionReport;

#undef ARX_PISTORIS_DETAIL_CXX_DEFAULT

// NOLINTEND(readability-identifier-naming)

#endif /* ARX_PISTORIS_ANIMATION_TYPES_H */
