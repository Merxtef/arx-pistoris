// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#include "arx_pistoris/base/indices.h"

#include "modules/sounds.h"
#include "utils/path.h"
#include "utils/resource_path.h"

#include <cassert>
#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace pistoris::sounds {

void setSound(SoundsData& sounds, SoundIndex index, Sound sound) noexcept {
  assert(static_cast<std::size_t>(index) < sounds.sounds.size());
  assert(validPath(sound.path));
  sounds.sounds[index] = std::move(sound);
}

SoundIndex addSound(SoundsData& sounds, Sound sound) {
  assert(sounds.sounds.size() < static_cast<std::size_t>(kNoSound));
  assert(validPath(sound.path));
  const SoundIndex index = static_cast<SoundIndex>(sounds.sounds.size());
  sounds.sounds.push_back(std::move(sound));
  return index;
}

void setEncodedAudio(SoundsData& sounds, SoundIndex index, std::vector<std::uint8_t> encoded_audio) noexcept {
  assert(static_cast<std::size_t>(index) < sounds.sounds.size());
  assert(!encoded_audio.empty());
  sounds.sounds[index].encoded_audio = std::move(encoded_audio);
}

void clearEncodedAudio(SoundsData& sounds, SoundIndex index) noexcept {
  assert(static_cast<std::size_t>(index) < sounds.sounds.size());
  sounds.sounds[index].encoded_audio.clear();
}

void removeSound(SoundsData& sounds, SoundIndex index) noexcept {
  assert(static_cast<std::size_t>(index) < sounds.sounds.size());
  sounds.sounds.erase(sounds.sounds.begin() + static_cast<std::ptrdiff_t>(index));
}

void replaceSounds(SoundsData& sounds, std::vector<Sound>&& replacement) noexcept {
  sounds.sounds = std::move(replacement);
}

namespace {

Error repairPathsImpl(std::span<const Sound> existing, SoundIndex ignored, std::span<Sound> candidates,
                      PathRepairInfo* out_info) {
  ResourcePathUniquifier uniquifier;
  uniquifier.reserve(candidates.size(), existing.size());
  for (std::size_t index = 0; index < existing.size(); ++index) {
    if (index == static_cast<std::size_t>(ignored)) continue;
    if (uniquifier.occupy(existing[index].path) != ResourcePathError::kNone) return Error::kBadPath;
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

}  // namespace

Error repairPath(const SoundsData& sounds, Sound& sound, SoundIndex ignored, PathRepairInfo* out_info) {
  return repairPathsImpl(sounds.sounds, ignored, std::span<Sound>(&sound, 1), out_info);
}

Error repairPaths(const SoundsData& sounds, std::span<Sound> candidates, PathRepairInfo* out_info) {
  return repairPathsImpl(sounds.sounds, kNoSound, candidates, out_info);
}

Error repairPaths(std::span<Sound> sounds, PathRepairInfo* out_info) {
  return repairPathsImpl({}, kNoSound, sounds, out_info);
}

Error rebasePaths(SoundsData& sounds, std::string_view directory, PathRebaseInfo* out_info) {
  ResourcePathNormalization normalized_directory = normalizeResourceDirectory(directory);
  if (normalized_directory.error != ResourcePathError::kNone) return Error::kBadPath;
  std::string folder = std::move(normalized_directory.value);
  if (!folder.empty()) folder.push_back('/');

  std::vector<std::string> paths;
  paths.reserve(sounds.sounds.size());
  ResourcePathUniquifier uniquifier;
  uniquifier.reserve(sounds.sounds.size());
  for (const Sound& sound : sounds.sounds) {
    const std::string_view base = pathFilename(sound.path);
    if (base.empty()) return Error::kBadPath;
    paths.push_back(folder + std::string(base));
    uniquifier.add(paths.back());
  }
  if (uniquifier.apply() != ResourcePathError::kNone) return Error::kBadPath;
  for (const std::string& path : paths)
    if (!validPath(path)) return Error::kBadPath;

  PathRebaseInfo info;
  if (out_info) {
    info.repairs.reserve(sounds.sounds.size() + 1U);
    if (hasStructuralResourcePathRepair(normalized_directory.repair))
      info.repairs.push_back({std::string(directory), folder.substr(0, folder.size() - 1U)});
    for (std::size_t index = 0; index < sounds.sounds.size(); ++index) {
      if (pathFilename(sounds.sounds[index].path) == pathFilename(paths[index])) continue;
      info.repairs.push_back({sounds.sounds[index].path, paths[index]});
    }
  }
  for (std::size_t index = 0; index < paths.size(); ++index) sounds.sounds[index].path = std::move(paths[index]);
  if (out_info) *out_info = std::move(info);
  return Error::kNone;
}

}  // namespace pistoris::sounds
