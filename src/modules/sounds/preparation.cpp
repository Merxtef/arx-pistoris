// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#include "arx_pistoris/base/indices.h"
#include "arx_pistoris/runtime/types.h"

#include "modules/sounds.h"
#include "modules/sounds/internal.h"
#include "utils/audio.h"
#include "utils/log.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <new>
#include <span>
#include <utility>
#include <vector>

namespace pistoris::sounds {
namespace {

constexpr std::size_t kNoRequest = std::numeric_limits<std::size_t>::max();

bool validOptions(const AudioPreparationOptions& options) noexcept {
  if (options.channels != ChannelMode::kPreserve && options.channels != ChannelMode::kMono) return false;
  return options.accepted_formats != 0 && (options.accepted_formats & ~kAudioFormatsAll) == 0 &&
         options.fallback_format == AudioFormat::kWav;
}

bool needsConversion(const AudioPreparationOptions& options, const AudioInfo& source) noexcept {
  const bool accepted = (options.accepted_formats & audioFormatFlag(source.format)) != 0;
  return !accepted || (options.channels == ChannelMode::kMono && source.channels > 1);
}

AudioInfo outputInfo(const AudioPreparationOptions& options, const AudioInfo& source) noexcept {
  AudioInfo result = source;
  if (needsConversion(options, source)) result.format = options.fallback_format;
  if (options.channels == ChannelMode::kMono) result.channels = 1;
  return result;
}

struct AudioPlan {
  SoundIndex sound = kNoSound;
  AudioInfo source;
  bool convert_preserved = false;
  bool convert_mono = false;
  std::size_t last_preserved = kNoRequest;
  std::size_t last_mono = kNoRequest;
  std::vector<std::uint8_t> preserved;
  std::vector<std::uint8_t> mono;
};

}  // namespace

Error prepareAudio(const SoundsData& sounds, std::span<const AudioPreparationRequest> requests,
                   std::vector<PreparedAudio>& out) {
  try {
    std::vector<PreparedAudio> prepared(requests.size());
    std::vector<std::size_t> plan_by_sound(sounds.sounds.size(), kNoRequest);
    std::vector<AudioPlan> plans;
    plans.reserve(std::min(sounds.sounds.size(), requests.size()));

    for (std::size_t index = 0; index < requests.size(); ++index) {
      const AudioPreparationRequest& request = requests[index];
      if (static_cast<std::size_t>(request.sound) >= sounds.sounds.size()) {
        log(ARX_LOG_DEBUG,
            "Sound preparation: request {} references sound {} with sound count {}",
            index,
            request.sound,
            sounds.sounds.size());
        return Error::kBadIndex;
      }
      if (!validOptions(request.options)) {
        log(ARX_LOG_DEBUG,
            "Sound preparation: request {} for sound {} has invalid options: formats {:#x}, channels {}, fallback {}",
            index,
            request.sound,
            request.options.accepted_formats,
            static_cast<int>(request.options.channels),
            static_cast<int>(request.options.fallback_format));
        return Error::kInvalidOptions;
      }
      const Sound& sound = sounds.sounds[request.sound];
      if (sound.encoded_audio.empty()) {
        log(ARX_LOG_DEBUG,
            "Sound preparation: request {} sound {} '{}' has no encoded audio",
            index,
            request.sound,
            sound.path);
        return Error::kBadAudio;
      }

      std::size_t& plan_index = plan_by_sound[request.sound];
      if (plan_index == kNoRequest) {
        AudioInfo source_info;
        const Error error = inspectEncodedAudio(sound.encoded_audio, source_info);
        if (error != Error::kNone) {
          log(ARX_LOG_DEBUG,
              "Sound preparation: sound {} '{}' inspection failed: {} bytes, error {}",
              request.sound,
              sound.path,
              sound.encoded_audio.size(),
              static_cast<int>(error));
          return error;
        }
        plan_index = plans.size();
        AudioPlan plan;
        plan.sound = request.sound;
        plan.source = source_info;
        plans.push_back(std::move(plan));
      }

      AudioPlan& plan = plans[plan_index];
      PreparedAudio& target = prepared[index];
      target.source = plan.source;
      target.output = outputInfo(request.options, plan.source);
      if (!request.options.include_bytes) continue;
      if (!needsConversion(request.options, plan.source)) {
        target.bytes.borrowed = sound.encoded_audio;
      } else if (request.options.channels == ChannelMode::kMono && plan.source.channels > 1) {
        plan.convert_mono = true;
        plan.last_mono = index;
      } else {
        plan.convert_preserved = true;
        plan.last_preserved = index;
      }
    }

    for (AudioPlan& plan : plans) {
      const Sound& sound = sounds.sounds[plan.sound];
      Error error = Error::kNone;
      if (plan.convert_preserved || plan.convert_mono) {
        error = audioError(audio::transcodeToPcm16WavVariants(sound.encoded_audio,
                                                              plan.convert_preserved,
                                                              plan.convert_mono,
                                                              &plan.preserved,
                                                              plan.convert_mono ? &plan.mono : nullptr));
      } else {
        error = audioError(audio::validate(sound.encoded_audio));
      }
      if (error != Error::kNone) {
        log(ARX_LOG_DEBUG,
            "Sound preparation: sound {} '{}' conversion failed: preserved {}, mono {}, error {}",
            plan.sound,
            sound.path,
            plan.convert_preserved,
            plan.convert_mono,
            static_cast<int>(error));
        return error;
      }
    }

    for (std::size_t index = 0; index < requests.size(); ++index) {
      const AudioPreparationRequest& request = requests[index];
      if (!request.options.include_bytes) continue;
      AudioPlan& plan = plans[plan_by_sound[request.sound]];
      if (!needsConversion(request.options, plan.source)) continue;
      PreparedBytes& bytes = prepared[index].bytes;
      if (request.options.channels == ChannelMode::kMono && plan.source.channels > 1) {
        if (index == plan.last_mono)
          bytes.converted = std::move(plan.mono);
        else
          bytes.converted = plan.mono;
      } else if (index == plan.last_preserved) {
        bytes.converted = std::move(plan.preserved);
      } else {
        bytes.converted = plan.preserved;
      }
    }
    out = std::move(prepared);
    return Error::kNone;
  } catch (const std::bad_alloc&) {
    return Error::kOutOfMemory;
  }
}

}  // namespace pistoris::sounds
