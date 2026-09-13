// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#pragma once

#include "doctest/doctest.h"

#include "arx_pistoris/sound.h"

#include <algorithm>
#include <cstddef>
#include <string_view>
#include <vector>

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
  std::vector<ArxSoundView> lhs_sounds(lhs.soundCount());
  std::vector<ArxSoundView> rhs_sounds(rhs.soundCount());
  const ArxReturnCode lhs_status = lhs.copySoundViews(0, lhs_sounds.size(), lhs_sounds.data());
  const ArxReturnCode rhs_status = rhs.copySoundViews(0, rhs_sounds.size(), rhs_sounds.data());
  CHECK(lhs_status == ARX_OK);
  CHECK(rhs_status == ARX_OK);
  if (lhs_status != ARX_OK || rhs_status != ARX_OK) return;

  for (std::size_t index = 0; index < lhs_sounds.size(); ++index) {
    if (options.compare_paths)
      CHECK(soundStringView(lhs_sounds[index].path) == soundStringView(rhs_sounds[index].path));
    if (!options.compare_encoded_audio) continue;
    CHECK(lhs_sounds[index].encoded_audio.size == rhs_sounds[index].encoded_audio.size);
    if (lhs_sounds[index].encoded_audio.size != rhs_sounds[index].encoded_audio.size) continue;
    if (lhs_sounds[index].encoded_audio.size != 0)
      CHECK(std::equal(lhs_sounds[index].encoded_audio.data,
                       lhs_sounds[index].encoded_audio.data + lhs_sounds[index].encoded_audio.size,
                       rhs_sounds[index].encoded_audio.data));
  }
}

}  // namespace test_support
