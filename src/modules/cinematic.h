// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#pragma once

#include "arx_pistoris/base/indices.h"
#include "arx_pistoris/base/math.hpp"

#include <cstddef>
#include <cstdint>
#include <vector>

namespace pistoris {

struct SoundsData;

enum class CinematicInterpolation : std::int8_t {
  kNone = -1,
  kBezier = 0,
  kLinear = 1,
};

enum class CinematicBaseEffect : std::uint8_t {
  kNone,
  kFadeIn,
  kFadeOut,
  kBlur,
};

enum class CinematicPostEffect : std::uint8_t {
  kNone,
  kFlash,
  kSuppressFlash,
};

struct CinematicColor {
  std::uint8_t r = 255;
  std::uint8_t g = 255;
  std::uint8_t b = 255;
};

struct CinematicLight {
  ArxVector3 position = {};
  float fall_in = 0.0f;
  float fall_out = 0.0f;
  ArxColor3 color = {};
  float intensity = -1.0f;
  float random_intensity = 0.0f;
};

struct CinematicIllustration {
  TextureIndex texture = kNoTexture;
  std::int32_t subdivision_scale = 1;
};

struct CinematicKeyframe {
  std::int32_t frame = 0;
  CinematicIllustrationIndex illustration = kInvalidCinematicIllustrationIndex;
  ArxVector3 camera_position = {};
  float camera_roll = 0.0f;
  CinematicColor color = {};
  CinematicColor secondary_color = {};
  CinematicColor flash_color = {};
  float flash_decay = 0.0f;
  CinematicLight light;
  float outgoing_speed = 1.0f;
  SoundHandle sound = kNoSoundHandle;
  CinematicInterpolation interpolation = CinematicInterpolation::kLinear;
  CinematicBaseEffect base_effect = CinematicBaseEffect::kNone;
  CinematicPostEffect post_effect = CinematicPostEffect::kNone;
  bool crossfade = false;
  bool dream = false;
  bool light_active = false;
};

struct CinematicData {
  std::vector<CinematicIllustration> illustrations;
  std::int32_t end_frame = 0;
  float fps = 25.0f;
  std::vector<CinematicKeyframe> keyframes;
};

namespace cinematic {

enum class Error : std::uint8_t {
  kNone,
  kNoIllustrations,
  kTooManyIllustrations,
  kBadIllustration,
  kBadIllustrationScale,
  kBadTimeline,
  kBadKeyCount,
  kBadKeyFrame,
  kBadKeyIllustration,
  kBadKeySound,
  kBadKeyTransform,
  kBadKeyTiming,
  kBadKeyInterpolation,
  kBadKeyEffect,
  kBadIndex,
};

Error validateIllustrationCount(std::size_t count) noexcept;
Error validateIllustration(const CinematicIllustration& illustration, std::size_t texture_count) noexcept;
Error validateKeyframe(const CinematicKeyframe& keyframe, std::size_t illustration_count, const SoundsData& sounds,
                       bool terminal) noexcept;
Error validate(const CinematicData& cinematic, std::size_t texture_count, const SoundsData& sounds) noexcept;

void setIllustration(CinematicData& cinematic, CinematicIllustrationIndex index,
                     CinematicIllustration illustration) noexcept;
CinematicIllustrationIndex addIllustration(CinematicData& cinematic, CinematicIllustration illustration);
void removeIllustration(CinematicData& cinematic, CinematicIllustrationIndex index) noexcept;
void clearIllustrations(CinematicData& cinematic) noexcept;
void setKeyframe(CinematicData& cinematic, std::size_t index, CinematicKeyframe keyframe) noexcept;
void insertKeyframe(CinematicData& cinematic, std::size_t index, CinematicKeyframe keyframe);
void removeKeyframe(CinematicData& cinematic, std::size_t index) noexcept;
void clearKeyframes(CinematicData& cinematic) noexcept;

}  // namespace cinematic
}  // namespace pistoris
