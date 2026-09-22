// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#ifndef ARX_PISTORIS_CINEMATIC_TYPES_H
#define ARX_PISTORIS_CINEMATIC_TYPES_H

#include "arx_pistoris/base/audio.h"
#include "arx_pistoris/base/indices.h"
#include "arx_pistoris/base/math.h"
#include "arx_pistoris/base/string_view.h"

#include <stdint.h>

// Public C-compatible Cinematic value types
// NOLINTBEGIN(readability-identifier-naming, performance-enum-size)

#ifdef __cplusplus
#define ARX_PISTORIS_DETAIL_CXX_DEFAULT(value) = value
#else
#define ARX_PISTORIS_DETAIL_CXX_DEFAULT(value)
#endif

typedef enum ArxCinematicInterpolation {
  ARX_CINEMATIC_INTERPOLATION_NONE = -1,
  ARX_CINEMATIC_INTERPOLATION_BEZIER = 0,
  ARX_CINEMATIC_INTERPOLATION_LINEAR = 1,
} ArxCinematicInterpolation;

typedef enum ArxCinematicBaseEffect {
  ARX_CINEMATIC_BASE_EFFECT_NONE = 0,
  ARX_CINEMATIC_BASE_EFFECT_FADE_IN,
  ARX_CINEMATIC_BASE_EFFECT_FADE_OUT,
  ARX_CINEMATIC_BASE_EFFECT_BLUR,
} ArxCinematicBaseEffect;

typedef enum ArxCinematicPostEffect {
  ARX_CINEMATIC_POST_EFFECT_NONE = 0,
  ARX_CINEMATIC_POST_EFFECT_FLASH,
  // Suppress a flash carried across an earlier key
  ARX_CINEMATIC_POST_EFFECT_SUPPRESS_FLASH,
} ArxCinematicPostEffect;

typedef struct ArxCinematicColor {
  uint8_t r ARX_PISTORIS_DETAIL_CXX_DEFAULT(255);
  uint8_t g ARX_PISTORIS_DETAIL_CXX_DEFAULT(255);
  uint8_t b ARX_PISTORIS_DETAIL_CXX_DEFAULT(255);
} ArxCinematicColor;

typedef struct ArxCinematicLight {
  ArxVector3 position ARX_PISTORIS_DETAIL_CXX_DEFAULT({});
  float fall_in ARX_PISTORIS_DETAIL_CXX_DEFAULT(0.0f);
  float fall_out ARX_PISTORIS_DETAIL_CXX_DEFAULT(0.0f);
  ArxColor3 color;
  float intensity ARX_PISTORIS_DETAIL_CXX_DEFAULT(-1.0f);
  float random_intensity ARX_PISTORIS_DETAIL_CXX_DEFAULT(0.0f);
} ArxCinematicLight;

typedef struct ArxCinematicIllustration {
  ArxTextureIndex texture ARX_PISTORIS_DETAIL_CXX_DEFAULT(ARX_NO_TEXTURE);
  int32_t subdivision_scale ARX_PISTORIS_DETAIL_CXX_DEFAULT(1);
} ArxCinematicIllustration;

typedef struct ArxCinematicKeyframe {
  int32_t frame ARX_PISTORIS_DETAIL_CXX_DEFAULT(0);
  ArxCinematicIllustrationIndex illustration ARX_PISTORIS_DETAIL_CXX_DEFAULT(ARX_INVALID_CINEMATIC_ILLUSTRATION);
  ArxVector3 camera_position ARX_PISTORIS_DETAIL_CXX_DEFAULT({});
  float camera_roll ARX_PISTORIS_DETAIL_CXX_DEFAULT(0.0f);
  ArxCinematicColor color;
  ArxCinematicColor secondary_color;
  ArxCinematicColor flash_color;
  float flash_decay ARX_PISTORIS_DETAIL_CXX_DEFAULT(0.0f);
  ArxCinematicLight light;
  float outgoing_speed ARX_PISTORIS_DETAIL_CXX_DEFAULT(1.0f);
  ArxSoundHandle sound ARX_PISTORIS_DETAIL_CXX_DEFAULT(ARX_NO_SOUND_HANDLE);
  ArxCinematicInterpolation interpolation ARX_PISTORIS_DETAIL_CXX_DEFAULT(ARX_CINEMATIC_INTERPOLATION_LINEAR);
  ArxCinematicBaseEffect base_effect ARX_PISTORIS_DETAIL_CXX_DEFAULT(ARX_CINEMATIC_BASE_EFFECT_NONE);
  ArxCinematicPostEffect post_effect ARX_PISTORIS_DETAIL_CXX_DEFAULT(ARX_CINEMATIC_POST_EFFECT_NONE);
  uint8_t crossfade ARX_PISTORIS_DETAIL_CXX_DEFAULT(0);
  uint8_t dream ARX_PISTORIS_DETAIL_CXX_DEFAULT(0);
  uint8_t light_active ARX_PISTORIS_DETAIL_CXX_DEFAULT(0);
} ArxCinematicKeyframe;

typedef struct ArxCinematicSoundView {
  ArxSoundHandle handle ARX_PISTORIS_DETAIL_CXX_DEFAULT(ARX_NO_SOUND_HANDLE);
  ArxStringView path;
} ArxCinematicSoundView;

typedef struct ArxCinematicLanguageView {
  ArxLanguageId id ARX_PISTORIS_DETAIL_CXX_DEFAULT(ARX_INVALID_LANGUAGE_ID);
  ArxStringView name;
} ArxCinematicLanguageView;

typedef struct ArxCinematicSoundEncodingView {
  ArxSoundHandle sound ARX_PISTORIS_DETAIL_CXX_DEFAULT(ARX_NO_SOUND_HANDLE);
  ArxLanguageId language ARX_PISTORIS_DETAIL_CXX_DEFAULT(ARX_SOUND_EFFECTS_LANGUAGE_ID);
  ArxEncodedAudioView encoded_audio;
} ArxCinematicSoundEncodingView;

#undef ARX_PISTORIS_DETAIL_CXX_DEFAULT

// NOLINTEND(readability-identifier-naming, performance-enum-size)

#endif /* ARX_PISTORIS_CINEMATIC_TYPES_H */
