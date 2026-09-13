// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#include "modules/sounds.h"
#include "modules/sounds/internal.h"
#include "utils/audio.h"

#include <cstdint>
#include <span>

namespace pistoris::sounds {
namespace {

AudioFormat audioFormat(audio::Format format) noexcept {
  switch (format) {
    case audio::Format::kWav:
      return AudioFormat::kWav;
    case audio::Format::kMp3:
      return AudioFormat::kMp3;
    case audio::Format::kOggVorbis:
      return AudioFormat::kOggVorbis;
    case audio::Format::kUnknown:
      return AudioFormat::kUnknown;
  }
  return AudioFormat::kUnknown;
}

}  // namespace

Error inspectEncodedAudio(std::span<const std::uint8_t> encoded_audio, AudioInfo& out) noexcept {
  audio::Info info;
  const Error error = audioError(audio::inspect(encoded_audio, &info));
  if (error == Error::kNone) out = {audioFormat(info.format), info.channels, info.sample_rate, info.frame_count};
  return error;
}

}  // namespace pistoris::sounds
