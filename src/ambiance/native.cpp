// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#include "arx_pistoris/ambiance.hpp"
#include "arx_pistoris/ambiance/bake.hpp"
#include "arx_pistoris/base/indices.h"
#include "arx_pistoris/base/status.h"
#include "arx_pistoris/native/amb.hpp"
#include "arx_pistoris/runtime/types.h"
#include "arx_pistoris/sound.hpp"

#include "ambiance/data.h"
#include "ambiance/internal.h"
#include "api/status_boundary.h"
#include "modules/ambiance.h"
#include "modules/sounds.h"
#include "native/amb.h"
#include "utils/log.h"
#include "utils/resource_path.h"

#include <cstddef>
#include <cstdint>
#include <limits>
#include <string>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <variant>
#include <vector>

namespace pistoris {
namespace {

Automation internalSetting(const amb::Setting& source) {
  if (source.min == source.max) return ConstantAutomation{source.min};

  DynamicAutomationMode mode;
  const bool random = (source.flags & amb::kSettingRandom) != 0;
  const bool interpolated = (source.flags & amb::kSettingInterpolate) != 0;
  if (random && interpolated) {
    mode = DynamicAutomationMode::kRandomInterpolated;
  } else if (random) {
    mode = DynamicAutomationMode::kRandomStep;
  } else if (interpolated) {
    mode = DynamicAutomationMode::kInterpolated;
  } else {
    mode = DynamicAutomationMode::kStep;
  }
  return DynamicAutomation{source.min, source.max, source.interval_ms, mode};
}

amb::Setting nativeSetting(const Automation& source) {
  if (const auto* constant = std::get_if<ConstantAutomation>(&source)) return {constant->value, constant->value, 0, 0};

  const auto* dynamic = std::get_if<DynamicAutomation>(&source);
  if (!dynamic) return {};
  amb::SettingFlags flags = 0;
  switch (dynamic->mode) {
    case DynamicAutomationMode::kStep:
      break;
    case DynamicAutomationMode::kRandomStep:
      flags = amb::kSettingRandom;
      break;
    case DynamicAutomationMode::kInterpolated:
      flags = amb::kSettingInterpolate;
      break;
    case DynamicAutomationMode::kRandomInterpolated:
      flags = amb::kSettingRandom | amb::kSettingInterpolate;
      break;
  }
  return {dynamic->first, dynamic->second, dynamic->interval_ms, flags};
}

void setCommon(AmbianceKeyCommon& out, const amb::Key& source) {
  out.start_delay_ms = source.start_ms;
  out.play_count = source.loop_minus_one + 1U;
  out.delay_min_ms = source.delay_min_ms;
  out.delay_max_ms = source.delay_max_ms;
  out.volume = internalSetting(source.volume);
  out.pitch = internalSetting(source.pitch);
}

void setCommon(amb::Key& out, const AmbianceKeyCommon& source) {
  out.start_ms = source.start_delay_ms;
  out.loop_minus_one = source.play_count - 1U;
  out.delay_min_ms = source.delay_min_ms;
  out.delay_max_ms = source.delay_max_ms;
  out.volume = nativeSetting(source.volume);
  out.pitch = nativeSetting(source.pitch);
}

PannedAmbianceKey internalPannedKey(const amb::Key& source) {
  PannedAmbianceKey result;
  setCommon(result, source);
  result.pan = internalSetting(source.pan);
  return result;
}

PositionedAmbianceKey internalPositionedKey(const amb::Key& source) {
  PositionedAmbianceKey result;
  setCommon(result, source);
  result.x = internalSetting(source.x);
  result.y = internalSetting(source.y);
  result.z = internalSetting(source.z);
  return result;
}

amb::Key nativeKey(const PannedAmbianceKey& source) {
  amb::Key result;
  setCommon(result, source);
  result.pan = nativeSetting(source.pan);
  return result;
}

amb::Key nativeKey(const PositionedAmbianceKey& source) {
  amb::Key result;
  setCommon(result, source);
  result.x = nativeSetting(source.x);
  result.y = nativeSetting(source.y);
  result.z = nativeSetting(source.z);
  return result;
}

AmbianceTrack internalTrack(const amb::Track& source, SoundIndex sound) {
  AmbianceTrack result;
  result.sound = sound;
  if ((source.flags & amb::kTrackPosition) != 0) {
    std::vector<PositionedAmbianceKey> keys;
    keys.reserve(source.keys.size());
    for (const amb::Key& key : source.keys) keys.push_back(internalPositionedKey(key));
    result.keys = std::move(keys);
  } else {
    std::vector<PannedAmbianceKey> keys;
    keys.reserve(source.keys.size());
    for (const amb::Key& key : source.keys) keys.push_back(internalPannedKey(key));
    result.keys = std::move(keys);
  }
  return result;
}

bool zeroPan(const AmbianceTrack& track) noexcept {
  const auto* keys = std::get_if<std::vector<PannedAmbianceKey>>(&track.keys);
  if (!keys) return false;
  for (const PannedAmbianceKey& key : *keys) {
    const auto* pan = std::get_if<ConstantAutomation>(&key.pan);
    if (!pan || pan->value != 0.0f) return false;
  }
  return true;
}

std::string wavPath(std::string_view source) {
  std::string result(source);
  normalizeResourcePathIdentity(result);
  const std::size_t slash = result.find_last_of('/');
  const std::size_t dot = result.find_last_of('.');
  if (dot != std::string::npos && (slash == std::string::npos || dot > slash + 1U)) result.resize(dot);
  result += ".wav";
  return result;
}

bool isWavPath(std::string_view path) noexcept { return path.ends_with(".wav"); }

struct SoundUse {
  bool stereo = false;
  bool mono = false;
};

struct ProjectedSound {
  std::string stereo_path;
  std::string mono_path;
};

struct AudioPreparations {
  std::size_t preserved = std::numeric_limits<std::size_t>::max();
  std::size_t mono = std::numeric_limits<std::size_t>::max();
};

void appendSoundFile(std::vector<SoundFile>& files, SoundIndex source, std::string path,
                     sounds::PreparedAudio& prepared) {
  SoundFile file{source, std::move(path), {}};
  if (prepared.bytes.converted.empty()) {
    file.encoded_audio.assign(prepared.bytes.borrowed.begin(), prepared.bytes.borrowed.end());
  } else {
    file.encoded_audio = std::move(prepared.bytes.converted);
  }
  files.push_back(std::move(file));
}

ArxReturnCode projectSounds(const AmbianceModules& modules, bool include_files, std::vector<ProjectedSound>& paths,
                            std::vector<SoundFile>& files) {
  std::vector<SoundUse> uses(modules.sounds.sounds.size());
  for (const AmbianceTrack& track : modules.ambiance.tracks) {
    if (track.sound >= uses.size()) return ARX_AMBIANCE_BAD_TRACK_SOUND;
    if (zeroPan(track))
      uses[track.sound].stereo = true;
    else
      uses[track.sound].mono = true;
  }

  paths.resize(modules.sounds.sounds.size());
  std::vector<sounds::AudioPreparationRequest> requests;
  std::vector<AudioPreparations> preparation(modules.sounds.sounds.size());
  requests.reserve(modules.sounds.sounds.size() * 2U);
  for (std::size_t index = 0; index < modules.sounds.sounds.size(); ++index) {
    const Sound& sound = modules.sounds.sounds[index];
    const SoundUse use = uses[index];
    if (sound.encoded_audio.empty()) continue;
    const bool preserve_wav = isWavPath(sound.path);
    const bool include_preserved = use.stereo || (preserve_wav && use.mono);
    if (include_preserved || (!use.stereo && !use.mono)) {
      preparation[index].preserved = requests.size();
      requests.push_back({static_cast<SoundIndex>(index),
                          {sounds::audioFormatFlag(sounds::AudioFormat::kWav),
                           sounds::AudioFormat::kWav,
                           sounds::ChannelMode::kPreserve,
                           include_files && include_preserved}});
    }
    if (use.mono) {
      preparation[index].mono = requests.size();
      requests.push_back({static_cast<SoundIndex>(index),
                          {sounds::audioFormatFlag(sounds::AudioFormat::kWav),
                           sounds::AudioFormat::kWav,
                           sounds::ChannelMode::kMono,
                           include_files}});
    }
  }

  std::vector<sounds::PreparedAudio> prepared;
  ArxReturnCode rc = ambiance_detail::soundErrorCode(sounds::prepareAudio(modules.sounds, requests, prepared));
  if (rc != ARX_OK) return rc;
  if (include_files) files.reserve(modules.ambiance.tracks.size() + modules.sounds.sounds.size());

  constexpr std::uint8_t kPrimary = 1U << 0U;
  constexpr std::uint8_t kSecondary = 1U << 1U;
  constexpr std::uint8_t kShared = 1U << 2U;
  std::vector<std::uint8_t> projection(modules.sounds.sounds.size(), 0);
  for (std::size_t index = 0; index < modules.sounds.sounds.size(); ++index) {
    const Sound& sound = modules.sounds.sounds[index];
    const SoundUse use = uses[index];
    if (!use.stereo && !use.mono) continue;

    if (sound.encoded_audio.empty()) {
      paths[index].stereo_path = sound.path;
      projection[index] = kPrimary | kShared;
      continue;
    }

    const AudioPreparations indices = preparation[index];
    const std::size_t info_index =
        indices.preserved != std::numeric_limits<std::size_t>::max() ? indices.preserved : indices.mono;
    if (info_index >= prepared.size()) return ARX_INTERNAL_ERROR;
    const std::string base = wavPath(sound.path);
    if (prepared[info_index].source.channels == 1) {
      paths[index].stereo_path = base;
      projection[index] = kPrimary | kShared;
      continue;
    }

    if (indices.preserved != std::numeric_limits<std::size_t>::max()) {
      paths[index].stereo_path = base;
      projection[index] |= kPrimary;
    }
    if (indices.mono != std::numeric_limits<std::size_t>::max()) {
      paths[index].mono_path = base;
      projection[index] |= kSecondary;
    }
  }

  ResourcePathUniquifier uniquifier;
  uniquifier.reserve(modules.sounds.sounds.size() * 2U, modules.sounds.sounds.size());
  for (std::size_t index = 0; index < modules.sounds.sounds.size(); ++index) {
    const Sound& sound = modules.sounds.sounds[index];
    if ((uses[index].stereo || uses[index].mono) || !isWavPath(sound.path)) continue;
    if (uniquifier.occupy(sound.path) != ResourcePathError::kNone) return ARX_AMBIANCE_BAD_SOUND_PATH;
  }
  for (std::size_t index = 0; index < projection.size(); ++index)
    if ((projection[index] & kPrimary) != 0 && isWavPath(modules.sounds.sounds[index].path))
      uniquifier.add(paths[index].stereo_path);
  for (std::size_t index = 0; index < projection.size(); ++index)
    if ((projection[index] & kPrimary) != 0 && !isWavPath(modules.sounds.sounds[index].path))
      uniquifier.add(paths[index].stereo_path);
  for (std::size_t index = 0; index < projection.size(); ++index)
    if ((projection[index] & kSecondary) != 0) uniquifier.add(paths[index].mono_path);
  if (uniquifier.apply() != ResourcePathError::kNone) return ARX_AMBIANCE_BAD_SOUND_PATH;

  for (std::size_t index = 0; index < projection.size(); ++index) {
    if ((projection[index] & kShared) != 0) paths[index].mono_path = paths[index].stereo_path;
    if (!include_files || modules.sounds.sounds[index].encoded_audio.empty()) continue;
    const AudioPreparations indices = preparation[index];
    const std::size_t info_index =
        indices.preserved != std::numeric_limits<std::size_t>::max() ? indices.preserved : indices.mono;
    if (info_index >= prepared.size()) continue;
    if (prepared[info_index].source.channels == 1) {
      appendSoundFile(files, static_cast<SoundIndex>(index), paths[index].stereo_path, prepared[info_index]);
      continue;
    }
    if (indices.preserved != std::numeric_limits<std::size_t>::max())
      appendSoundFile(files, static_cast<SoundIndex>(index), paths[index].stereo_path, prepared[indices.preserved]);
    if (indices.mono != std::numeric_limits<std::size_t>::max())
      appendSoundFile(files, static_cast<SoundIndex>(index), paths[index].mono_path, prepared[indices.mono]);
  }
  return ARX_OK;
}

amb::Track nativeTrack(const AmbianceTrack& source, bool master, std::string path) {
  amb::Track result;
  result.sample_path = std::move(path);
  if (master) result.flags |= amb::kTrackMaster;
  if (const auto* keys = std::get_if<std::vector<PannedAmbianceKey>>(&source.keys)) {
    result.keys.reserve(keys->size());
    for (const PannedAmbianceKey& key : *keys) result.keys.push_back(nativeKey(key));
  } else if (const auto* keys = std::get_if<std::vector<PositionedAmbianceKey>>(&source.keys)) {
    result.flags |= amb::kTrackPosition;
    result.keys.reserve(keys->size());
    for (const PositionedAmbianceKey& key : *keys) result.keys.push_back(nativeKey(key));
  }
  return result;
}

}  // namespace

ArxReturnCode Ambiance::importNative(Ambiance& out, const amb::Data& native,
                                     std::vector<SoundSourceReference>* sound_sources) noexcept {
  return api_detail::statusBoundary([&]() -> ArxReturnCode {
    ArxReturnCode rc = validateAmb(&native);
    if (rc != ARX_OK) return rc;

    Ambiance result;
    std::vector<Sound> sounds;
    sounds.reserve(native.tracks.size());
    std::unordered_map<std::string, SoundIndex, ResourcePathIdentityHash, ResourcePathIdentityEqual> identities;
    identities.reserve(native.tracks.size());
    std::unordered_set<std::string_view> source_spellings;
    source_spellings.reserve(native.tracks.size());
    std::vector<const std::string*> original_sound_paths;
    original_sound_paths.reserve(native.tracks.size());
    std::vector<SoundSourceReference> sources;
    if (sound_sources) sources.reserve(native.tracks.size());

    result.data_->ambiance.tracks.reserve(native.tracks.size());
    for (std::size_t index = 0; index < native.tracks.size(); ++index) {
      const amb::Track& track = native.tracks[index];
      std::string path = track.sample_path;
      normalizeResourcePathIdentity(path);
      auto [entry, inserted] = identities.try_emplace(path, static_cast<SoundIndex>(sounds.size()));
      if (inserted) {
        original_sound_paths.push_back(&entry->first);
        sounds.push_back({std::move(path), {}});
      }
      const SoundIndex sound = entry->second;
      result.data_->ambiance.tracks.push_back(internalTrack(track, sound));
      if (sound_sources && source_spellings.insert(track.sample_path).second)
        sources.push_back({sound, track.sample_path});
      if ((track.flags & amb::kTrackMaster) != 0)
        result.data_->ambiance.master_track = static_cast<AmbianceTrackIndex>(index);
    }
    ResourcePathUniquifier sound_paths;
    sound_paths.reserve(sounds.size());
    for (Sound& sound : sounds) sound_paths.add(sound.path);
    std::vector<ResourcePathRepair> sound_repairs(sounds.size());
    if (sound_paths.apply(nullptr, sound_repairs) != ResourcePathError::kNone) return ARX_AMBIANCE_BAD_SOUND_PATH;
    for (std::size_t index = 0; index < sound_repairs.size(); ++index) {
      const ResourcePathRepair repair = sound_repairs[index];
      if (!hasResourcePathRepair(repair, ResourcePathRepair::kCharacters) &&
          !hasResourcePathRepair(repair, ResourcePathRepair::kTrailing) &&
          !hasResourcePathRepair(repair, ResourcePathRepair::kReserved) &&
          !hasResourcePathRepair(repair, ResourcePathRepair::kLength) &&
          !hasResourcePathRepair(repair, ResourcePathRepair::kDuplicate))
        continue;
      log(ARX_LOG_WARN,
          "AMB -> Ambiance: sound path '{}' normalized to '{}'",
          *original_sound_paths[index],
          sounds[index].path);
    }
    rc = ambiance_detail::soundErrorCode(sounds::validate(sounds));
    if (rc != ARX_OK) return rc;
    sounds::replaceSounds(result.data_->sounds, std::move(sounds));
    rc = result.validate();
    if (rc != ARX_OK) return rc;
    out.swap(result);
    if (sound_sources) *sound_sources = std::move(sources);
    return ARX_OK;
  });
}

ArxReturnCode Ambiance::bakeNative(amb::Data& out) const noexcept {
  NativeAmbianceBundle bundle;
  const ArxReturnCode rc = bakeNativeBundle({.include_files = false}, bundle);
  if (rc != ARX_OK) return rc;
  out = std::move(bundle.amb);
  return ARX_OK;
}

ArxReturnCode Ambiance::bakeNativeBundle(const NativeSoundBakeOptions& options,
                                         NativeAmbianceBundle& out) const noexcept {
  return api_detail::statusBoundary([&]() -> ArxReturnCode {
    ArxReturnCode rc = ambiance_detail::validateStructure(static_cast<const AmbianceModules&>(*data_));
    if (rc != ARX_OK) return rc;

    NativeAmbianceBundle result;
    std::vector<ProjectedSound> projected;
    rc = projectSounds(
        static_cast<const AmbianceModules&>(*data_), options.include_files, projected, result.sound_files);
    if (rc != ARX_OK) return rc;
    result.amb.tracks.reserve(data_->ambiance.tracks.size());
    for (std::size_t index = 0; index < data_->ambiance.tracks.size(); ++index) {
      const AmbianceTrack& track = data_->ambiance.tracks[index];
      const std::string& path = zeroPan(track) ? projected[track.sound].stereo_path : projected[track.sound].mono_path;
      if (path.empty()) return ARX_INTERNAL_ERROR;
      result.amb.tracks.push_back(nativeTrack(track, index == data_->ambiance.master_track, path));
    }
    rc = validateAmb(&result.amb);
    if (rc != ARX_OK) return rc;
    out = std::move(result);
    ambiance_detail::warnAboutMasterTiming(static_cast<const AmbianceModules&>(*data_));
    return ARX_OK;
  });
}

}  // namespace pistoris
