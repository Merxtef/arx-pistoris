// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#include "arx_pistoris/base/indices.h"
#include "arx_pistoris/runtime/types.h"

#include "modules/sounds.h"
#include "modules/sounds/internal.h"
#include "utils/audio.h"
#include "utils/log.h"

#include <cstddef>
#include <cstdint>
#include <functional>
#include <limits>
#include <new>
#include <span>
#include <unordered_map>
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

struct AudioIdentity {
  SoundHandle sound = kNoSoundHandle;
  LanguageId language = kSoundEffects;

  bool operator==(const AudioIdentity&) const = default;
};

struct AudioIdentityHash {
  std::size_t operator()(const AudioIdentity& value) const noexcept {
    return std::hash<SoundHandle>{}(value.sound) ^ (std::hash<LanguageId>{}(value.language) << 1U);
  }
};

struct AudioPlan {
  AudioIdentity identity;
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
    std::vector<std::size_t> request_plans(requests.size(), kNoRequest);
    std::unordered_map<AudioIdentity, std::size_t, AudioIdentityHash> plans_by_audio;
    plans_by_audio.reserve(requests.size());
    std::vector<AudioPlan> plans;
    plans.reserve(requests.size());

    for (std::size_t request_index = 0; request_index < requests.size(); ++request_index) {
      const AudioPreparationRequest& request = requests[request_index];
      if (!validHandle(sounds, request.sound)) return Error::kBadIndex;
      if (!validOptions(request.options)) return Error::kInvalidOptions;
      const std::span<const std::uint8_t> audio = encodedAudio(sounds, request.sound, request.language);
      if (audio.empty()) {
        log(ARX_LOG_DEBUG,
            "Sound preparation: request {} handle {} language {} has no encoded audio",
            request_index,
            request.sound,
            request.language);
        return Error::kBadAudio;
      }

      const AudioIdentity identity{request.sound, request.language};
      auto [entry, inserted] = plans_by_audio.try_emplace(identity, plans.size());
      if (inserted) {
        AudioInfo source;
        const Error error = inspectEncodedAudio(audio, source);
        if (error != Error::kNone) return error;
        plans.emplace_back();
        plans.back().identity = identity;
        plans.back().source = source;
      }
      const std::size_t plan_index = entry->second;
      request_plans[request_index] = plan_index;
      AudioPlan& plan = plans[plan_index];
      PreparedAudio& target = prepared[request_index];
      target.source = plan.source;
      target.output = outputInfo(request.options, plan.source);
      if (!request.options.include_bytes) continue;
      if (!needsConversion(request.options, plan.source)) {
        target.bytes.borrowed = audio;
      } else if (request.options.channels == ChannelMode::kMono && plan.source.channels > 1) {
        plan.convert_mono = true;
        plan.last_mono = request_index;
      } else {
        plan.convert_preserved = true;
        plan.last_preserved = request_index;
      }
    }

    for (AudioPlan& plan : plans) {
      const std::span<const std::uint8_t> audio = encodedAudio(sounds, plan.identity.sound, plan.identity.language);
      Error error = Error::kNone;
      if (plan.convert_preserved || plan.convert_mono) {
        error = audioError(audio::transcodeToPcm16WavVariants(audio,
                                                              plan.convert_preserved,
                                                              plan.convert_mono,
                                                              &plan.preserved,
                                                              plan.convert_mono ? &plan.mono : nullptr));
      } else {
        error = audioError(audio::validate(audio));
      }
      if (error != Error::kNone) return error;
    }

    for (std::size_t request_index = 0; request_index < requests.size(); ++request_index) {
      const AudioPreparationRequest& request = requests[request_index];
      if (!request.options.include_bytes) continue;
      AudioPlan& plan = plans[request_plans[request_index]];
      if (!needsConversion(request.options, plan.source)) continue;
      PreparedBytes& bytes = prepared[request_index].bytes;
      if (request.options.channels == ChannelMode::kMono && plan.source.channels > 1) {
        if (request_index == plan.last_mono)
          bytes.converted = std::move(plan.mono);
        else
          bytes.converted = plan.mono;
      } else if (request_index == plan.last_preserved) {
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
