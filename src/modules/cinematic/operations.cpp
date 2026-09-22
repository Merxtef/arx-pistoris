// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#include "arx_pistoris/base/indices.h"

#include "modules/cinematic.h"

#include <cassert>
#include <cstddef>

namespace pistoris::cinematic {

void setIllustration(CinematicData& cinematic, CinematicIllustrationIndex index,
                     CinematicIllustration illustration) noexcept {
  assert(static_cast<std::size_t>(index) < cinematic.illustrations.size());
  cinematic.illustrations[index] = illustration;
}

CinematicIllustrationIndex addIllustration(CinematicData& cinematic, CinematicIllustration illustration) {
  assert(cinematic.illustrations.size() < static_cast<std::size_t>(kInvalidCinematicIllustrationIndex));
  const auto index = static_cast<CinematicIllustrationIndex>(cinematic.illustrations.size());
  cinematic.illustrations.push_back(illustration);
  return index;
}

void removeIllustration(CinematicData& cinematic, CinematicIllustrationIndex index) noexcept {
  assert(static_cast<std::size_t>(index) < cinematic.illustrations.size());
  cinematic.illustrations.erase(cinematic.illustrations.begin() + static_cast<std::ptrdiff_t>(index));
  for (CinematicKeyframe& key : cinematic.keyframes) {
    if (key.illustration > index && key.illustration != kInvalidCinematicIllustrationIndex) --key.illustration;
  }
}

void clearIllustrations(CinematicData& cinematic) noexcept { cinematic.illustrations.clear(); }

void setKeyframe(CinematicData& cinematic, std::size_t index, CinematicKeyframe keyframe) noexcept {
  assert(index < cinematic.keyframes.size());
  cinematic.keyframes[index] = keyframe;
}

void insertKeyframe(CinematicData& cinematic, std::size_t index, CinematicKeyframe keyframe) {
  assert(index <= cinematic.keyframes.size());
  cinematic.keyframes.insert(cinematic.keyframes.begin() + static_cast<std::ptrdiff_t>(index), keyframe);
}

void removeKeyframe(CinematicData& cinematic, std::size_t index) noexcept {
  assert(index < cinematic.keyframes.size());
  cinematic.keyframes.erase(cinematic.keyframes.begin() + static_cast<std::ptrdiff_t>(index));
}

void clearKeyframes(CinematicData& cinematic) noexcept { cinematic.keyframes.clear(); }

}  // namespace pistoris::cinematic
