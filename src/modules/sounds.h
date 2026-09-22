// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#pragma once

#include "arx_pistoris/base/indices.h"
#include "arx_pistoris/sound.hpp"

#include "utils/prepared_bytes.h"

#include <cstddef>
#include <cstdint>
#include <map>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace pistoris {

struct Sound {
  std::string path;
  std::vector<std::uint8_t> encoded_audio;
};

struct SoundEncoding {
  SoundHandle sound = kNoSoundHandle;
  LanguageId language = kSoundEffects;
  std::vector<std::uint8_t> encoded_audio;
};

struct SoundsData {  // NOLINT(bugprone-exception-escape): MSVC debug STL container move
  std::vector<std::string> effect_paths;
  std::vector<std::string> speech_paths;
  std::map<LanguageId, std::string> languages;
  std::vector<SoundEncoding> encodings;
};

namespace sounds {

enum class Error : std::uint8_t {
  kNone,
  kInvalidOptions,
  kTooManySounds,
  kBadPath,
  kBadAudio,
  kUnsupportedChannels,
  kAudioTooLarge,
  kDuplicatePath,
  kBadKind,
  kBadLanguage,
  kDuplicateEncoding,
  kBadIndex,
  kOutOfMemory,
};

enum class AudioFormat : std::uint8_t {
  kUnknown = 0,
  kWav,
  kMp3,
  kOggVorbis,
};

using AudioFormatFlags = std::uint8_t;

constexpr AudioFormatFlags audioFormatFlag(AudioFormat format) noexcept {
  return format == AudioFormat::kUnknown ? 0
                                         : static_cast<AudioFormatFlags>(1U << (static_cast<unsigned>(format) - 1U));
}

inline constexpr AudioFormatFlags kAudioFormatsAll =
    audioFormatFlag(AudioFormat::kWav) | audioFormatFlag(AudioFormat::kMp3) | audioFormatFlag(AudioFormat::kOggVorbis);

enum class ChannelMode : std::uint8_t {
  kPreserve,
  kMono,
};

struct AudioInfo {
  AudioFormat format = AudioFormat::kUnknown;
  std::uint32_t channels = 0;
  std::uint32_t sample_rate = 0;
  std::uint64_t frame_count = 0;
};

struct AudioPreparationOptions {
  AudioFormatFlags accepted_formats = kAudioFormatsAll;
  AudioFormat fallback_format = AudioFormat::kWav;
  ChannelMode channels = ChannelMode::kPreserve;
  bool include_bytes = true;
};

struct AudioPreparationRequest {
  SoundHandle sound = kNoSoundHandle;
  LanguageId language = kSoundEffects;
  AudioPreparationOptions options;
};

struct PreparedAudio {
  PreparedBytes bytes;
  AudioInfo source;
  AudioInfo output;
};

struct PathRepairInfo {
  struct Repair {
    std::string original;
    std::string repaired;
  };

  std::vector<Repair> repairs;
};

using PathRebaseInfo = PathRepairInfo;

// --- Validation ---

Error validateSoundCount(std::size_t count) noexcept;
Error validateSound(const Sound& sound) noexcept;
Error validateEncodedAudio(std::span<const std::uint8_t> encoded_audio) noexcept;
Error validateStructure(const SoundsData& sounds) noexcept;
Error validateAudio(const SoundsData& sounds) noexcept;
Error validate(const SoundsData& sounds) noexcept;
bool validPath(std::string_view path) noexcept;

// --- Queries ---

Error inspectEncodedAudio(std::span<const std::uint8_t> encoded_audio, AudioInfo& out) noexcept;
std::size_t count(const SoundsData& sounds, SoundKind kind) noexcept;
bool validHandle(const SoundsData& sounds, SoundHandle handle) noexcept;
bool effectIndex(SoundHandle handle, SoundIndex& out) noexcept;
std::string_view path(const SoundsData& sounds, SoundHandle handle) noexcept;
std::span<const std::uint8_t> encodedAudio(const SoundsData& sounds, SoundHandle handle,
                                           LanguageId language = kSoundEffects) noexcept;
SoundHandle effectHandle(SoundIndex index) noexcept;

// --- Mutation ---

void setSound(SoundsData& sounds, SoundIndex index, Sound sound);
SoundIndex addSound(SoundsData& sounds, Sound sound);
void setEncodedAudio(SoundsData& sounds, SoundIndex index, std::vector<std::uint8_t> encoded_audio);
void clearEncodedAudio(SoundsData& sounds, SoundIndex index) noexcept;
void removeSound(SoundsData& sounds, SoundIndex index) noexcept;
void replaceSounds(SoundsData& sounds, std::vector<Sound>&& replacement);
SoundHandle addPath(SoundsData& sounds, SoundKind kind, std::string path);
void setPath(SoundsData& sounds, SoundHandle handle, std::string path) noexcept;
void removeSound(SoundsData& sounds, SoundHandle handle) noexcept;
void setLanguage(SoundsData& sounds, LanguageId language, std::string name);
void setEncodedAudio(SoundsData& sounds, SoundHandle handle, LanguageId language,
                     std::vector<std::uint8_t> encoded_audio);
void clearEncodedAudio(SoundsData& sounds, SoundHandle handle, LanguageId language) noexcept;
void removeLanguage(SoundsData& sounds, LanguageId language) noexcept;

// --- Repair ---

Error repairPath(const SoundsData& sounds, Sound& sound, SoundIndex ignored, PathRepairInfo* out_info = nullptr);
Error repairPaths(const SoundsData& sounds, std::span<Sound> candidates, PathRepairInfo* out_info = nullptr);
Error repairPaths(std::span<Sound> sounds, PathRepairInfo* out_info = nullptr);
Error repairPath(const SoundsData& sounds, SoundKind kind, std::string& path, SoundIndex ignored,
                 PathRepairInfo* out_info = nullptr);

// --- Transformation ---

Error rebasePaths(SoundsData& sounds, std::string_view directory, PathRebaseInfo* out_info = nullptr);
Error rebasePaths(SoundsData& sounds, SoundKind kind, std::string_view directory, PathRebaseInfo* out_info = nullptr);
Error compact(SoundsData& sounds, SoundKind kind, std::span<const std::uint8_t> used,
              std::vector<SoundIndex>& out_remap, std::size_t& out_removed);

// --- Generation ---

Error prepareAudio(const SoundsData& sounds, std::span<const AudioPreparationRequest> requests,
                   std::vector<PreparedAudio>& out);

}  // namespace sounds
}  // namespace pistoris
