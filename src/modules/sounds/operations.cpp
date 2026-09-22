// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#include "arx_pistoris/base/indices.h"
#include "arx_pistoris/base/status.h"
#include "arx_pistoris/sound.hpp"

#include "modules/sounds.h"
#include "utils/path.h"
#include "utils/resource_path.h"

#include <algorithm>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <new>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace pistoris::sounds {
namespace {

std::vector<std::string>& paths(SoundsData& sounds, SoundKind kind) noexcept {
  return kind == SoundKind::kSpeech ? sounds.speech_paths : sounds.effect_paths;
}

const std::vector<std::string>& paths(const SoundsData& sounds, SoundKind kind) noexcept {
  return kind == SoundKind::kSpeech ? sounds.speech_paths : sounds.effect_paths;
}

bool decode(SoundHandle handle, SoundKind& kind, SoundIndex& index) noexcept {
  return soundHandleKind(handle, kind) == ARX_OK && soundHandleIndex(handle, index) == ARX_OK;
}

auto findEncoding(SoundsData& sounds, SoundHandle handle, LanguageId language) noexcept {
  return std::ranges::find_if(sounds.encodings, [=](const SoundEncoding& encoding) {
    return encoding.sound == handle && encoding.language == language;
  });
}

auto findEncoding(const SoundsData& sounds, SoundHandle handle, LanguageId language) noexcept {
  return std::ranges::find_if(sounds.encodings, [=](const SoundEncoding& encoding) {
    return encoding.sound == handle && encoding.language == language;
  });
}

Error repairPathsImpl(std::span<const std::string> existing, SoundIndex ignored, std::span<Sound> candidates,
                      PathRepairInfo* out_info) {
  ResourcePathUniquifier uniquifier;
  uniquifier.reserve(candidates.size(), existing.size());
  for (std::size_t index = 0; index < existing.size(); ++index) {
    if (index == static_cast<std::size_t>(ignored)) continue;
    if (uniquifier.occupy(existing[index]) != ResourcePathError::kNone) return Error::kBadPath;
  }
  std::vector<std::string> repaired;
  repaired.reserve(candidates.size());
  for (const Sound& sound : candidates) {
    repaired.push_back(sound.path);
    uniquifier.add(repaired.back());
  }
  if (uniquifier.apply() != ResourcePathError::kNone) return Error::kBadPath;

  PathRepairInfo result;
  if (out_info) {
    for (std::size_t index = 0; index < candidates.size(); ++index) {
      if (candidates[index].path == repaired[index]) continue;
      result.repairs.push_back({candidates[index].path, repaired[index]});
    }
  }
  for (std::size_t index = 0; index < candidates.size(); ++index) candidates[index].path = std::move(repaired[index]);
  if (out_info) *out_info = std::move(result);
  return Error::kNone;
}

void removePath(SoundsData& sounds, SoundKind kind, SoundIndex removed) noexcept {
  std::vector<std::string>& list = paths(sounds, kind);
  list.erase(list.begin() + static_cast<std::ptrdiff_t>(removed));
  std::erase_if(sounds.encodings, [&](SoundEncoding& encoding) {
    SoundKind encoding_kind = SoundKind::kEffect;
    SoundIndex encoding_index = kNoSound;
    if (!decode(encoding.sound, encoding_kind, encoding_index) || encoding_kind != kind) return false;
    if (encoding_index == removed) return true;
    if (encoding_index > removed) {
      const ArxReturnCode rc = soundHandle(kind, encoding_index - 1U, encoding.sound);
      assert(rc == ARX_OK);
      (void)rc;
    }
    return false;
  });
}

}  // namespace

std::size_t count(const SoundsData& sounds, SoundKind kind) noexcept { return paths(sounds, kind).size(); }

SoundHandle effectHandle(SoundIndex index) noexcept {
  if (index == kNoSound) return kNoSoundHandle;
  SoundHandle result = kNoSoundHandle;
  const ArxReturnCode rc = soundHandle(SoundKind::kEffect, index, result);
  assert(rc == ARX_OK);
  (void)rc;
  return result;
}

bool validHandle(const SoundsData& sounds, SoundHandle handle) noexcept {
  SoundKind kind = SoundKind::kEffect;
  SoundIndex index = kNoSound;
  return decode(handle, kind, index) && static_cast<std::size_t>(index) < count(sounds, kind);
}

bool effectIndex(SoundHandle handle, SoundIndex& out) noexcept {
  SoundKind kind = SoundKind::kEffect;
  return soundHandleKind(handle, kind) == ARX_OK && kind == SoundKind::kEffect &&
         soundHandleIndex(handle, out) == ARX_OK;
}

std::string_view path(const SoundsData& sounds, SoundHandle handle) noexcept {
  SoundKind kind = SoundKind::kEffect;
  SoundIndex index = kNoSound;
  if (!decode(handle, kind, index) || static_cast<std::size_t>(index) >= count(sounds, kind)) return {};
  return paths(sounds, kind)[index];
}

std::span<const std::uint8_t> encodedAudio(const SoundsData& sounds, SoundHandle handle, LanguageId language) noexcept {
  const auto found = findEncoding(sounds, handle, language);
  return found == sounds.encodings.end() ? std::span<const std::uint8_t>{} : found->encoded_audio;
}

void setSound(SoundsData& sounds, SoundIndex index, Sound sound) {
  assert(static_cast<std::size_t>(index) < sounds.effect_paths.size());
  assert(validPath(sound.path));
  sounds.effect_paths[index] = std::move(sound.path);
  const SoundHandle handle = effectHandle(index);
  clearEncodedAudio(sounds, handle, kSoundEffects);
  if (!sound.encoded_audio.empty()) setEncodedAudio(sounds, handle, kSoundEffects, std::move(sound.encoded_audio));
}

SoundIndex addSound(SoundsData& sounds, Sound sound) {
  assert(sounds.effect_paths.size() < static_cast<std::size_t>(kNoSound));
  assert(validPath(sound.path));
  const SoundIndex index = static_cast<SoundIndex>(sounds.effect_paths.size());
  sounds.effect_paths.push_back(std::move(sound.path));
  if (!sound.encoded_audio.empty())
    sounds.encodings.push_back({effectHandle(index), kSoundEffects, std::move(sound.encoded_audio)});
  return index;
}

void setEncodedAudio(SoundsData& sounds, SoundIndex index, std::vector<std::uint8_t> encoded_audio) {
  assert(static_cast<std::size_t>(index) < sounds.effect_paths.size());
  setEncodedAudio(sounds, effectHandle(index), kSoundEffects, std::move(encoded_audio));
}

void clearEncodedAudio(SoundsData& sounds, SoundIndex index) noexcept {
  assert(static_cast<std::size_t>(index) < sounds.effect_paths.size());
  clearEncodedAudio(sounds, effectHandle(index), kSoundEffects);
}

void removeSound(SoundsData& sounds, SoundIndex index) noexcept {
  assert(static_cast<std::size_t>(index) < sounds.effect_paths.size());
  removePath(sounds, SoundKind::kEffect, index);
}

void replaceSounds(SoundsData& sounds, std::vector<Sound>&& replacement) {
  std::erase_if(sounds.encodings, [](const SoundEncoding& encoding) {
    SoundKind kind = SoundKind::kEffect;
    return soundHandleKind(encoding.sound, kind) != ARX_OK || kind == SoundKind::kEffect;
  });
  sounds.effect_paths.clear();
  sounds.effect_paths.reserve(replacement.size());
  for (Sound& sound : replacement) {
    const SoundIndex index = static_cast<SoundIndex>(sounds.effect_paths.size());
    sounds.effect_paths.push_back(std::move(sound.path));
    if (!sound.encoded_audio.empty())
      sounds.encodings.push_back({effectHandle(index), kSoundEffects, std::move(sound.encoded_audio)});
  }
}

SoundHandle addPath(SoundsData& sounds, SoundKind kind, std::string value) {
  std::vector<std::string>& list = paths(sounds, kind);
  assert(list.size() < static_cast<std::size_t>(kNoSound));
  assert(validPath(value));
  SoundHandle handle = kNoSoundHandle;
  const ArxReturnCode rc = soundHandle(kind, static_cast<SoundIndex>(list.size()), handle);
  assert(rc == ARX_OK);
  (void)rc;
  list.push_back(std::move(value));
  return handle;
}

void setPath(SoundsData& sounds, SoundHandle handle, std::string value) noexcept {
  SoundKind kind = SoundKind::kEffect;
  SoundIndex index = kNoSound;
  const bool decoded = decode(handle, kind, index);
  assert(decoded && static_cast<std::size_t>(index) < count(sounds, kind));
  if (!decoded || static_cast<std::size_t>(index) >= count(sounds, kind)) return;
  assert(validPath(value));
  paths(sounds, kind)[index] = std::move(value);
}

void removeSound(SoundsData& sounds, SoundHandle handle) noexcept {
  SoundKind kind = SoundKind::kEffect;
  SoundIndex index = kNoSound;
  const bool decoded = decode(handle, kind, index);
  assert(decoded && static_cast<std::size_t>(index) < count(sounds, kind));
  if (!decoded || static_cast<std::size_t>(index) >= count(sounds, kind)) return;
  removePath(sounds, kind, index);
}

void setLanguage(SoundsData& sounds, LanguageId language, std::string name) {
  assert(language != kSoundEffects && language != kInvalidLanguageId);
  sounds.languages.insert_or_assign(language, std::move(name));
}

void setEncodedAudio(SoundsData& sounds, SoundHandle handle, LanguageId language,
                     std::vector<std::uint8_t> encoded_audio) {
  assert(validHandle(sounds, handle));
  assert(!encoded_audio.empty());
  const auto found = findEncoding(sounds, handle, language);
  if (found == sounds.encodings.end())
    sounds.encodings.push_back({handle, language, std::move(encoded_audio)});
  else
    found->encoded_audio = std::move(encoded_audio);
}

void clearEncodedAudio(SoundsData& sounds, SoundHandle handle, LanguageId language) noexcept {
  const auto found = findEncoding(sounds, handle, language);
  if (found != sounds.encodings.end()) sounds.encodings.erase(found);
}

void removeLanguage(SoundsData& sounds, LanguageId language) noexcept {
  if (language == kSoundEffects) return;
  sounds.languages.erase(language);
  std::erase_if(sounds.encodings, [=](const SoundEncoding& encoding) { return encoding.language == language; });
}

Error repairPath(const SoundsData& sounds, Sound& sound, SoundIndex ignored, PathRepairInfo* out_info) {
  return repairPathsImpl(sounds.effect_paths, ignored, std::span<Sound>(&sound, 1), out_info);
}

Error repairPaths(const SoundsData& sounds, std::span<Sound> candidates, PathRepairInfo* out_info) {
  return repairPathsImpl(sounds.effect_paths, kNoSound, candidates, out_info);
}

Error repairPaths(std::span<Sound> sound_list, PathRepairInfo* out_info) {
  return repairPathsImpl({}, kNoSound, sound_list, out_info);
}

Error repairPath(const SoundsData& sounds, SoundKind kind, std::string& value, SoundIndex ignored,
                 PathRepairInfo* out_info) {
  Sound candidate{value, {}};
  const Error error = repairPathsImpl(paths(sounds, kind), ignored, std::span<Sound>(&candidate, 1), out_info);
  if (error == Error::kNone) value = std::move(candidate.path);
  return error;
}

Error rebasePaths(SoundsData& sounds, std::string_view directory, PathRebaseInfo* out_info) {
  return rebasePaths(sounds, SoundKind::kEffect, directory, out_info);
}

Error rebasePaths(SoundsData& sounds, SoundKind kind, std::string_view directory, PathRebaseInfo* out_info) {
  ResourcePathNormalization normalized_directory = normalizeResourceDirectory(directory);
  if (normalized_directory.error != ResourcePathError::kNone) return Error::kBadPath;
  std::string folder = std::move(normalized_directory.value);
  if (!folder.empty()) folder.push_back('/');

  std::vector<std::string> rebased;
  std::vector<std::string>& source_paths = paths(sounds, kind);
  rebased.reserve(source_paths.size());
  ResourcePathUniquifier uniquifier;
  uniquifier.reserve(source_paths.size());
  for (const std::string& source : source_paths) {
    const std::string_view base = pathFilename(source);
    if (base.empty()) return Error::kBadPath;
    rebased.push_back(folder + std::string(base));
    uniquifier.add(rebased.back());
  }
  if (uniquifier.apply() != ResourcePathError::kNone) return Error::kBadPath;
  for (const std::string& value : rebased)
    if (!validPath(value)) return Error::kBadPath;

  PathRebaseInfo info;
  if (out_info) {
    info.repairs.reserve(source_paths.size() + 1U);
    if (hasStructuralResourcePathRepair(normalized_directory.repair))
      info.repairs.push_back({std::string(directory), folder.substr(0, folder.size() - 1U)});
    for (std::size_t index = 0; index < source_paths.size(); ++index) {
      if (pathFilename(source_paths[index]) == pathFilename(rebased[index])) continue;
      info.repairs.push_back({source_paths[index], rebased[index]});
    }
  }
  source_paths = std::move(rebased);
  if (out_info) *out_info = std::move(info);
  return Error::kNone;
}

Error compact(SoundsData& sounds, SoundKind kind, std::span<const std::uint8_t> used,
              std::vector<SoundIndex>& out_remap, std::size_t& out_removed) {
  std::vector<std::string>& list = paths(sounds, kind);
  if (used.size() != list.size()) return Error::kInvalidOptions;

  std::vector<SoundIndex> remap;
  try {
    remap.assign(used.size(), kNoSound);
  } catch (const std::bad_alloc&) {
    return Error::kOutOfMemory;
  }
  SoundIndex next = 0;
  for (std::size_t index = 0; index < used.size(); ++index)
    if (used[index] != 0) remap[index] = next++;

  const std::size_t original_size = list.size();
  std::size_t destination = 0;
  for (std::size_t source = 0; source < original_size; ++source) {
    if (used[source] == 0) continue;
    if (source != destination) list[destination] = std::move(list[source]);
    ++destination;
  }
  list.resize(destination);

  std::erase_if(sounds.encodings, [&](SoundEncoding& encoding) {
    SoundKind encoding_kind = SoundKind::kEffect;
    SoundIndex source = kNoSound;
    if (!decode(encoding.sound, encoding_kind, source) || encoding_kind != kind) return false;
    if (source >= remap.size() || remap[source] == kNoSound) return true;
    const ArxReturnCode rc = soundHandle(kind, remap[source], encoding.sound);
    assert(rc == ARX_OK);
    (void)rc;
    return false;
  });

  out_removed = original_size - destination;
  out_remap = std::move(remap);
  return Error::kNone;
}

}  // namespace pistoris::sounds
