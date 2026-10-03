// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#pragma once

#include "doctest/doctest.h"

#include "arx_pistoris/sound.h"

#include <algorithm>
#include <cstddef>
#include <string_view>

namespace test_support {

struct SoundEquivalenceOptions {
  bool compare_paths = true;
  bool compare_encoded_audio = true;
};

inline std::string_view soundStringView(ArxStringView value) {
  return value.size == 0 ? std::string_view{} : std::string_view(value.data, value.size);
}

template <class Asset>
void checkSoundsEquivalent(const Asset& lhs, const Asset& rhs, SoundEquivalenceOptions options = {}) {
  CHECK(lhs.soundCount() == rhs.soundCount());
  if (lhs.soundCount() != rhs.soundCount()) return;

  for (std::size_t index = 0; index < lhs.soundCount(); ++index) {
    ArxSoundView lhs_sound{};
    ArxSoundView rhs_sound{};
    if constexpr (requires { lhs.sounds(); }) {
      lhs_sound = lhs.sounds()[index];
      rhs_sound = rhs.sounds()[index];
    } else {
      const ArxReturnCode lhs_status = lhs.copySoundViews(index, 1, &lhs_sound);
      const ArxReturnCode rhs_status = rhs.copySoundViews(index, 1, &rhs_sound);
      CHECK(lhs_status == ARX_OK);
      CHECK(rhs_status == ARX_OK);
      if (lhs_status != ARX_OK || rhs_status != ARX_OK) return;
    }
    if (options.compare_paths) CHECK(soundStringView(lhs_sound.path) == soundStringView(rhs_sound.path));
    if (!options.compare_encoded_audio) continue;
    CHECK(lhs_sound.encoded_audio.size == rhs_sound.encoded_audio.size);
    if (lhs_sound.encoded_audio.size != rhs_sound.encoded_audio.size) continue;
    if (lhs_sound.encoded_audio.size != 0)
      CHECK(std::equal(lhs_sound.encoded_audio.data,
                       lhs_sound.encoded_audio.data + lhs_sound.encoded_audio.size,
                       rhs_sound.encoded_audio.data));
  }
}

}  // namespace test_support
