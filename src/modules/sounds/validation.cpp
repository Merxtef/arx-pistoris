// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#include "arx_pistoris/base/indices.h"
#include "arx_pistoris/base/status.h"
#include "arx_pistoris/runtime/types.h"
#include "arx_pistoris/sound.hpp"

#include "modules/sounds.h"
#include "modules/sounds/internal.h"
#include "utils/audio.h"
#include "utils/identifier.h"
#include "utils/log.h"
#include "utils/resource_path.h"

#include <cstddef>
#include <cstdint>
#include <functional>
#include <new>
#include <span>
#include <string>
#include <string_view>
#include <unordered_set>

namespace pistoris::sounds {
namespace {

Error validatePaths(std::span<const std::string> paths, SoundKind kind) {
  const Error count_error = validateSoundCount(paths.size());
  if (count_error != Error::kNone) return count_error;
  std::unordered_set<std::string_view, ResourcePathIdentityHash, ResourcePathIdentityEqual> identities;
  identities.reserve(paths.size());
  for (std::size_t index = 0; index < paths.size(); ++index) {
    if (!validPath(paths[index])) {
      log(ARX_LOG_DEBUG,
          "Sound validation: kind {} path {} is invalid: '{}'",
          static_cast<int>(kind),
          index,
          paths[index]);
      return Error::kBadPath;
    }
    if (!identities.insert(paths[index]).second) {
      log(ARX_LOG_DEBUG,
          "Sound validation: kind {} path {} duplicates '{}'",
          static_cast<int>(kind),
          index,
          paths[index]);
      return Error::kDuplicatePath;
    }
  }
  return Error::kNone;
}

struct EncodingIdentity {
  SoundHandle sound = kNoSoundHandle;
  LanguageId language = kSoundEffects;

  bool operator==(const EncodingIdentity&) const = default;
};

struct EncodingIdentityHash {
  std::size_t operator()(const EncodingIdentity& value) const noexcept {
    return std::hash<SoundHandle>{}(value.sound) ^ (std::hash<LanguageId>{}(value.language) << 1U);
  }
};

}  // namespace

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

Error validateStructure(const SoundsData& sounds) noexcept {
  try {
    Error error = validatePaths(sounds.effect_paths, SoundKind::kEffect);
    if (error != Error::kNone) return error;
    error = validatePaths(sounds.speech_paths, SoundKind::kSpeech);
    if (error != Error::kNone) return error;

    std::unordered_set<std::string_view, ResourcePathIdentityHash, ResourcePathIdentityEqual> language_names;
    language_names.reserve(sounds.languages.size());
    for (const auto& [id, name] : sounds.languages) {
      if (id == kSoundEffects || id == kInvalidLanguageId || !isIdentifier(name) ||
          !isPortableResourcePathComponent(name) || !language_names.insert(name).second)
        return Error::kBadLanguage;
    }

    std::unordered_set<EncodingIdentity, EncodingIdentityHash> encodings;
    encodings.reserve(sounds.encodings.size());
    for (const SoundEncoding& encoding : sounds.encodings) {
      SoundKind kind = SoundKind::kEffect;
      if (!validHandle(sounds, encoding.sound) || soundHandleKind(encoding.sound, kind) != ARX_OK)
        return Error::kBadIndex;
      if ((kind == SoundKind::kEffect && encoding.language != kSoundEffects) ||
          (kind == SoundKind::kSpeech &&
           (encoding.language == kSoundEffects || !sounds.languages.contains(encoding.language))))
        return Error::kBadLanguage;
      if (encoding.encoded_audio.empty()) return Error::kBadAudio;
      if (!encodings.insert({encoding.sound, encoding.language}).second) return Error::kDuplicateEncoding;
    }
    return Error::kNone;
  } catch (const std::bad_alloc&) {
    return Error::kOutOfMemory;
  }
}

Error validateAudio(const SoundsData& sounds) noexcept {
  for (const SoundEncoding& encoding : sounds.encodings) {
    const Error error = audioError(audio::validate(encoding.encoded_audio));
    if (error != Error::kNone) {
      log(ARX_LOG_DEBUG,
          "Sound validation: handle {} language {} has invalid encoded audio: {} bytes, error {}",
          encoding.sound,
          encoding.language,
          encoding.encoded_audio.size(),
          static_cast<int>(error));
      return error;
    }
  }
  return Error::kNone;
}

Error validate(const SoundsData& sounds) noexcept {
  const Error structure_error = validateStructure(sounds);
  return structure_error == Error::kNone ? validateAudio(sounds) : structure_error;
}

}  // namespace pistoris::sounds
