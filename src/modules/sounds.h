// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#pragma once

#include "arx_pistoris/base/indices.h"

#include "utils/prepared_bytes.h"

#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace pistoris {

struct Sound {
  std::string path;
  std::vector<std::uint8_t> encoded_audio;
};

struct SoundsData {
  std::vector<Sound> sounds;
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
  SoundIndex sound = kNoSound;
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
Error validateStructure(std::span<const Sound> sounds) noexcept;
Error validateAudio(std::span<const Sound> sounds) noexcept;
Error validate(std::span<const Sound> sounds) noexcept;
bool validPath(std::string_view path) noexcept;

// --- Queries ---

Error inspectEncodedAudio(std::span<const std::uint8_t> encoded_audio, AudioInfo& out) noexcept;

// --- Mutation ---

void setSound(SoundsData& sounds, SoundIndex index, Sound sound) noexcept;
SoundIndex addSound(SoundsData& sounds, Sound sound);
void setEncodedAudio(SoundsData& sounds, SoundIndex index, std::vector<std::uint8_t> encoded_audio) noexcept;
void clearEncodedAudio(SoundsData& sounds, SoundIndex index) noexcept;
void removeSound(SoundsData& sounds, SoundIndex index) noexcept;
void replaceSounds(SoundsData& sounds, std::vector<Sound>&& replacement) noexcept;

// --- Repair ---

Error repairPath(const SoundsData& sounds, Sound& sound, SoundIndex ignored, PathRepairInfo* out_info = nullptr);
Error repairPaths(const SoundsData& sounds, std::span<Sound> candidates, PathRepairInfo* out_info = nullptr);
Error repairPaths(std::span<Sound> sounds, PathRepairInfo* out_info = nullptr);

// --- Transformation ---

Error rebasePaths(SoundsData& sounds, std::string_view directory, PathRebaseInfo* out_info = nullptr);

// --- Generation ---

Error prepareAudio(const SoundsData& sounds, std::span<const AudioPreparationRequest> requests,
                   std::vector<PreparedAudio>& out);

}  // namespace sounds
}  // namespace pistoris
