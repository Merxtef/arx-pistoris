// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#include "arx_pistoris/base/indices.h"
#include "arx_pistoris/runtime/types.h"

#include "modules/sounds.h"
#include "modules/sounds/internal.h"
#include "utils/audio.h"
#include "utils/log.h"
#include "utils/resource_path.h"

#include <cstddef>
#include <cstdint>
#include <new>
#include <span>
#include <string_view>
#include <unordered_set>

namespace pistoris::sounds {
bool validPath(std::string_view path) noexcept { return isResourcePath(path); }

Error validateSound(const Sound& sound) noexcept {
  if (!validPath(sound.path)) return Error::kBadPath;
  if (sound.encoded_audio.empty()) return Error::kNone;
  return audioError(audio::validate(sound.encoded_audio));
}

Error validateSoundCount(std::size_t count) noexcept {
  return count > static_cast<std::size_t>(kNoSound) ? Error::kTooManySounds : Error::kNone;
}

Error validateEncodedAudio(std::span<const std::uint8_t> encoded_audio) noexcept {
  if (encoded_audio.empty()) return Error::kBadAudio;
  return audioError(audio::validate(encoded_audio));
}

Error validateStructure(std::span<const Sound> sound_list) noexcept {
  const Error count_error = validateSoundCount(sound_list.size());
  if (count_error != Error::kNone) {
    log(ARX_LOG_DEBUG,
        "Sound validation: count {} exceeds limit {}",
        sound_list.size(),
        static_cast<std::size_t>(kNoSound));
    return count_error;
  }
  try {
    std::unordered_set<std::string_view, ResourcePathIdentityHash, ResourcePathIdentityEqual> identities;
    identities.reserve(sound_list.size());
    for (std::size_t index = 0; index < sound_list.size(); ++index) {
      const Sound& sound = sound_list[index];
      if (!validPath(sound.path)) {
        log(ARX_LOG_DEBUG, "Sound validation: sound {} has invalid path '{}'", index, sound.path);
        return Error::kBadPath;
      }
      if (!identities.insert(sound.path).second) {
        log(ARX_LOG_DEBUG, "Sound validation: sound {} duplicates path '{}'", index, sound.path);
        return Error::kDuplicatePath;
      }
    }
  } catch (const std::bad_alloc&) {
    return Error::kOutOfMemory;
  }
  return Error::kNone;
}

Error validateAudio(std::span<const Sound> sound_list) noexcept {
  for (std::size_t index = 0; index < sound_list.size(); ++index) {
    const Sound& sound = sound_list[index];
    if (sound.encoded_audio.empty()) continue;
    const Error error = audioError(audio::validate(sound.encoded_audio));
    if (error != Error::kNone) {
      log(ARX_LOG_DEBUG,
          "Sound validation: sound {} '{}' has invalid encoded audio: {} bytes, error {}",
          index,
          sound.path,
          sound.encoded_audio.size(),
          static_cast<int>(error));
      return error;
    }
  }
  return Error::kNone;
}

Error validate(std::span<const Sound> sound_list) noexcept {
  const Error error = validateStructure(sound_list);
  return error == Error::kNone ? validateAudio(sound_list) : error;
}

}  // namespace pistoris::sounds
