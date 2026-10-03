// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#pragma once

#include "arx_pistoris/base/indices.h"
#include "arx_pistoris/base/location.hpp"
#include "arx_pistoris/base/result.hpp"

#include <cstddef>
#include <cstdint>
#include <string>

namespace pistoris {

enum class CinematicElement : std::uint8_t {
  kResource,
  kTimeline,
  kIllustration,
  kKeyframe,
  kTexture,
  kSound,
  kLanguage,
  kSoundEncoding,
};

struct CinematicLocation {
  std::string resource_path;
  std::size_t input_index = kNoInputIndex;
  CinematicElement element = CinematicElement::kResource;
  std::size_t index = kNoElementIndex;
  std::size_t subindex = kNoElementIndex;
  std::string label;
  SoundHandle sound_handle = kNoSoundHandle;
  LanguageId language_id = kInvalidLanguageId;
};

template <class T>
using CinematicResult = Result<T, CinematicLocation>;

}  // namespace pistoris
