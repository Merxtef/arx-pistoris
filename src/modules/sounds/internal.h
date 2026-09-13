// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#pragma once

#include "modules/sounds.h"
#include "utils/audio.h"

namespace pistoris::sounds {

inline Error audioError(audio::Error error) noexcept {
  switch (error) {
    case audio::Error::kNone:
      return Error::kNone;
    case audio::Error::kMalformed:
      return Error::kBadAudio;
    case audio::Error::kUnsupportedChannels:
      return Error::kUnsupportedChannels;
    case audio::Error::kTooLarge:
      return Error::kAudioTooLarge;
    case audio::Error::kOutOfMemory:
      return Error::kOutOfMemory;
  }
  return Error::kBadAudio;
}

}  // namespace pistoris::sounds
