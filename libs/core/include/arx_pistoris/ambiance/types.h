// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#ifndef ARX_PISTORIS_AMBIANCE_TYPES_H
#define ARX_PISTORIS_AMBIANCE_TYPES_H

#include "arx_pistoris/base/indices.h"
#include "arx_pistoris/base/string_view.h"

#include <stddef.h>
#include <stdint.h>

// Public C-compatible Ambiance value types
// NOLINTBEGIN(readability-identifier-naming, performance-enum-size)

#ifdef __cplusplus
#define ARX_PISTORIS_DETAIL_CXX_DEFAULT(value) = value
#else
#define ARX_PISTORIS_DETAIL_CXX_DEFAULT(value)
#endif

typedef uint32_t ArxAmbianceAutomationMode;
enum {
  ARX_AMBIANCE_AUTOMATION_CONSTANT = 0,
  ARX_AMBIANCE_AUTOMATION_STEP = 1,
  ARX_AMBIANCE_AUTOMATION_RANDOM_STEP = 2,
  ARX_AMBIANCE_AUTOMATION_INTERPOLATED = 3,
  ARX_AMBIANCE_AUTOMATION_RANDOM_INTERPOLATED = 4
};

typedef uint32_t ArxAmbianceTrackKind;
enum { ARX_AMBIANCE_TRACK_PANNED = 0, ARX_AMBIANCE_TRACK_POSITIONED = 1 };

typedef struct ArxAmbianceAutomation {
  // Constant value or first dynamic endpoint
  float first;
  // Second dynamic endpoint; ignored in constant mode
  float second;
  // Dynamic interval; ignored in constant mode
  uint32_t interval_ms;
  ArxAmbianceAutomationMode mode ARX_PISTORIS_DETAIL_CXX_DEFAULT(ARX_AMBIANCE_AUTOMATION_CONSTANT);
} ArxAmbianceAutomation;

typedef struct ArxAmbiancePannedKey {
  uint32_t start_delay_ms;
  uint32_t play_count ARX_PISTORIS_DETAIL_CXX_DEFAULT(1U);
  uint32_t delay_min_ms;
  uint32_t delay_max_ms;
  ArxAmbianceAutomation volume;
  ArxAmbianceAutomation pitch;
  ArxAmbianceAutomation pan;
} ArxAmbiancePannedKey;

typedef struct ArxAmbiancePositionedKey {
  uint32_t start_delay_ms;
  uint32_t play_count ARX_PISTORIS_DETAIL_CXX_DEFAULT(1U);
  uint32_t delay_min_ms;
  uint32_t delay_max_ms;
  ArxAmbianceAutomation volume;
  ArxAmbianceAutomation pitch;
  ArxAmbianceAutomation x;
  ArxAmbianceAutomation y;
  ArxAmbianceAutomation z;
} ArxAmbiancePositionedKey;

typedef struct ArxAmbianceTrack {
  ArxSoundIndex sound;
  ArxAmbianceTrackKind kind;
  size_t key_count;
} ArxAmbianceTrack;

typedef struct ArxAmbiancePannedTrackInput {
  ArxSoundIndex sound;
  const ArxAmbiancePannedKey* keys;
  size_t key_count;
} ArxAmbiancePannedTrackInput;

typedef struct ArxAmbiancePositionedTrackInput {
  ArxSoundIndex sound;
  const ArxAmbiancePositionedKey* keys;
  size_t key_count;
} ArxAmbiancePositionedTrackInput;

#undef ARX_PISTORIS_DETAIL_CXX_DEFAULT

// NOLINTEND(readability-identifier-naming, performance-enum-size)

#endif /* ARX_PISTORIS_AMBIANCE_TYPES_H */
