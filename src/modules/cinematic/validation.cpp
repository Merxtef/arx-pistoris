// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#include "arx_pistoris/base/indices.h"
#include "arx_pistoris/runtime/types.h"

#include "modules/cinematic.h"
#include "modules/sounds.h"
#include "utils/cinematic_constraints.h"
#include "utils/log.h"
#include "utils/math/finite.h"

#include <cstddef>
#include <cstdint>
#include <limits>
#include <span>

namespace pistoris::cinematic {
namespace {

bool validInterpolation(CinematicInterpolation value) noexcept {
  return value == CinematicInterpolation::kNone || value == CinematicInterpolation::kBezier ||
         value == CinematicInterpolation::kLinear;
}

bool validBaseEffect(CinematicBaseEffect value) noexcept {
  return value == CinematicBaseEffect::kNone || value == CinematicBaseEffect::kFadeIn ||
         value == CinematicBaseEffect::kFadeOut || value == CinematicBaseEffect::kBlur;
}

bool validPostEffect(CinematicPostEffect value) noexcept {
  return value == CinematicPostEffect::kNone || value == CinematicPostEffect::kFlash ||
         value == CinematicPostEffect::kSuppressFlash;
}

bool finite(const CinematicLight& light) noexcept {
  return math::finite(light.position) && math::finite(light.fall_in) && math::finite(light.fall_out) &&
         math::finite(light.color) && math::finite(light.intensity) && math::finite(light.random_intensity);
}

bool finite(const CinematicKeyframe& key) noexcept {
  return math::finite(key.camera_position) && math::finite(key.camera_roll) && math::finite(key.flash_decay) &&
         finite(key.light) && math::finite(key.outgoing_speed);
}

}  // namespace

Error validateIllustrationCount(std::size_t count) noexcept {
  if (count == 0) return Error::kNoIllustrations;
  return count > static_cast<std::size_t>(kInvalidCinematicIllustrationIndex) ? Error::kTooManyIllustrations
                                                                              : Error::kNone;
}

Error validateIllustration(const CinematicIllustration& illustration, std::size_t texture_count) noexcept {
  if (static_cast<std::size_t>(illustration.texture) >= texture_count) return Error::kBadIllustration;
  return illustration.subdivision_scale > 0 ? Error::kNone : Error::kBadIllustrationScale;
}

Error validateKeyframe(const CinematicKeyframe& key, std::size_t illustration_count, const SoundsData& sounds,
                       bool terminal) noexcept {
  if (static_cast<std::size_t>(key.illustration) >= illustration_count) return Error::kBadKeyIllustration;
  if (key.sound != kNoSoundHandle && !sounds::validHandle(sounds, key.sound)) return Error::kBadKeySound;
  if (!finite(key)) return Error::kBadKeyTransform;
  if (!terminal && key.outgoing_speed <= 0.0f) return Error::kBadKeyTiming;
  if (!validInterpolation(key.interpolation)) return Error::kBadKeyInterpolation;
  if (!validBaseEffect(key.base_effect) || !validPostEffect(key.post_effect)) return Error::kBadKeyEffect;
  return Error::kNone;
}

Error validate(const CinematicData& data, std::size_t texture_count, const SoundsData& sounds) noexcept {
  Error error = validateIllustrationCount(data.illustrations.size());
  if (error != Error::kNone) return error;
  for (std::size_t index = 0; index < data.illustrations.size(); ++index) {
    error = validateIllustration(data.illustrations[index], texture_count);
    if (error != Error::kNone) {
      log(ARX_LOG_DEBUG, "Cinematic validation: illustration {} is invalid", index);
      return error;
    }
  }

  if (data.end_frame <= 0 || !math::finite(data.fps) || data.fps <= 0.0f) return Error::kBadTimeline;
  if (data.keyframes.size() < 2 ||
      data.keyframes.size() > static_cast<std::size_t>(std::numeric_limits<std::int32_t>::max()))
    return Error::kBadKeyCount;
  if (data.keyframes.front().frame != 0) return Error::kBadKeyFrame;

  std::int32_t previous = -1;
  for (std::size_t index = 0; index < data.keyframes.size(); ++index) {
    const CinematicKeyframe& key = data.keyframes[index];
    if (key.frame <= previous || key.frame > data.end_frame) return Error::kBadKeyFrame;
    error = validateKeyframe(key, data.illustrations.size(), sounds, index + 1U == data.keyframes.size());
    if (error != Error::kNone) {
      log(ARX_LOG_DEBUG, "Cinematic validation: keyframe {} at frame {} is invalid", index, key.frame);
      return error;
    }
    previous = key.frame;
  }
  return cinematic_constraints::validDuration(data.fps, std::span<const CinematicKeyframe>(data.keyframes))
             ? Error::kNone
             : Error::kBadKeyTiming;
}

}  // namespace pistoris::cinematic
