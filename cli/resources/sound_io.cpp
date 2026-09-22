// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#include "resources/sound_io.h"

#include "arx_pistoris/ambiance.hpp"
#include "arx_pistoris/animation.hpp"
#include "arx_pistoris/base/indices.h"
#include "arx_pistoris/base/status.h"
#include "arx_pistoris/cinematic/sound.hpp"
#include "arx_pistoris/native/amb.hpp"
#include "arx_pistoris/native/tea.hpp"
#include "arx_pistoris/native/text.hpp"
#include "arx_pistoris/paths.hpp"
#include "arx_pistoris/runtime.hpp"
#include "arx_pistoris/runtime/types.h"
#include "arx_pistoris/sound.h"
#include "arx_pistoris/sound.hpp"

#include "base/resource_path.h"
#include "console/diagnostics.h"
#include "console/logging.h"
#include "io/native_text.h"
#include "io/path_location.h"
#include "io/service.h"
#include "media/encoded.h"
#include "resources/layout.h"
#include "resources/resource_output.h"
#include "resources/selector.h"

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <span>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>

namespace cli {
namespace {

struct SoundLookup {
  std::string path;
  bool loaded = false;
  bool failed = false;
};

struct NativeSoundLookup {
  pistoris::SoundFile file;
  bool loaded = false;
  bool failed = false;
};

struct SoundSourceLookup {
  pistoris::SoundIndex sound = pistoris::kNoSound;
  std::string path;
};

bool setNativeAudio(NativeSoundLookup& sound, std::vector<std::uint8_t> encoded) {
  sound.file.encoded_audio = std::move(encoded);
  return true;
}

std::string logicalSoundPath(std::string_view path) { return resourcePathKey(path); }

template <class Apply>
AudioCandidateResult tryLoad(IoService& io, const PathLocation* base, std::string_view path, std::string_view owner,
                             Apply&& apply, AudioLookupMode mode = AudioLookupMode::kExact) {
  media::PreparedAudio prepared;
  const AudioCandidateResult result = loadAudioCandidate(io, base, path, mode, owner, prepared);
  if (result != AudioCandidateResult::kLoaded) return result;
  if (apply(std::move(prepared.encoded))) return AudioCandidateResult::kLoaded;
  log(ARX_LOG_WARN,
      "%.*s sound data could not be assigned: %.*s",
      static_cast<int>(owner.size()),
      owner.data(),
      static_cast<int>(path.size()),
      path.data());
  return AudioCandidateResult::kFailed;
}

void warnMissingSound(IoService& io, std::string_view owner, std::string_view path) {
  if (io.hasReadMounts()) {
    log(ARX_LOG_WARN,
        "%.*s sound not found: %.*s",
        static_cast<int>(owner.size()),
        owner.data(),
        static_cast<int>(path.size()),
        path.data());
  } else {
    log(ARX_LOG_WARN,
        "Referenced %.*s sound was not found because there are no readable mount folders: %.*s",
        static_cast<int>(owner.size()),
        owner.data(),
        static_cast<int>(path.size()),
        path.data());
  }
}

template <class Owner>
bool copyLookups(const Owner& owner, std::string_view owner_name, std::vector<SoundLookup>& out) {
  std::vector<ArxSoundView> views(owner.soundCount());
  const ArxReturnCode rc = owner.copySoundViews(0, views.size(), views.data());
  if (rc != ARX_OK) {
    diagnostic(DiagnosticCode::kResourceInputInvalid,
               "%.*s sound projection failed: %s (code %d)",
               static_cast<int>(owner_name.size()),
               owner_name.data(),
               pistoris::errorString(rc),
               static_cast<int>(rc));
    return false;
  }
  out.clear();
  out.reserve(views.size());
  for (const ArxSoundView& view : views)
    out.push_back({std::string(view.path.data, view.path.size), view.encoded_audio.size != 0});
  return true;
}

template <class Owner>
bool loadSoundDataImpl(Owner& owner, std::string_view owner_name, IoService& io, const SoundInput& input,
                       std::span<const pistoris::SoundSourceReference> sources) {
  const std::size_t sound_count = owner.soundCount();
  for (const pistoris::SoundSourceReference& source : sources) {
    if (source.sound < sound_count) continue;
    diagnostic(DiagnosticCode::kResourceInputInvalid,
               "%.*s sound source references invalid sound index %u for %zu sounds",
               static_cast<int>(owner_name.size()),
               owner_name.data(),
               static_cast<unsigned>(source.sound),
               sound_count);
    return false;
  }

  std::vector<SoundLookup> sounds;
  if (!copyLookups(owner, owner_name, sounds)) return false;
  std::size_t loaded = 0;
  if (input.use_format_sources) {
    for (const pistoris::SoundSourceReference& source : sources) {
      if (source.sound >= sounds.size() || sounds[source.sound].loaded || source.path.empty()) continue;
      const AudioCandidateResult result = tryLoad(
          io,
          &input.source_base,
          source.path,
          owner_name,
          [&](std::vector<std::uint8_t> encoded) {
            return owner.setSoundData(source.sound, {encoded.data(), encoded.size()}) == ARX_OK;
          },
          AudioLookupMode::kFormatPriority);
      if (result == AudioCandidateResult::kLoaded) {
        sounds[source.sound].loaded = true;
        ++loaded;
      } else if (result == AudioCandidateResult::kFailed) {
        sounds[source.sound].failed = true;
      }
    }
  }
  for (std::size_t index = 0; index < sounds.size(); ++index) {
    if (sounds[index].loaded || sounds[index].path.empty()) continue;
    const AudioCandidateResult result =
        tryLoad(io, nullptr, sounds[index].path, owner_name, [&](std::vector<std::uint8_t> encoded) {
          return owner.setSoundData(static_cast<pistoris::SoundIndex>(index), {encoded.data(), encoded.size()}) ==
                 ARX_OK;
        });
    if (result == AudioCandidateResult::kLoaded) {
      sounds[index].loaded = true;
      ++loaded;
      continue;
    }
    if (result == AudioCandidateResult::kFailed) sounds[index].failed = true;
    if (!sounds[index].failed) warnMissingSound(io, owner_name, sounds[index].path);
  }
  if (loaded != 0)
    log(ARX_LOG_INFO, "loaded %zu %.*s sound(s)", loaded, static_cast<int>(owner_name.size()), owner_name.data());
  return true;
}

std::string teaSoundPath(std::string_view sample) {
  std::string result(pistoris::paths::soundDirectory());
  result += '/';
  result += sample;
  result = logicalSoundPath(result);
  const std::size_t slash = result.find_last_of('/');
  const std::size_t dot = result.find_last_of('.');
  if (dot != std::string::npos && (slash == std::string::npos || dot > slash + 1U)) result.resize(dot);
  result += ".wav";
  return result;
}

bool soundOutputTarget(IoService& io, const SoundOutput& output, std::string_view path, PathLocation& out,
                       DiagnosticCode failure_code, std::string_view owner) {
  if (output.layout == ResourceLayout::kGame) {
    out.path = path;
    out.address = PathAddress::kMountRelative;
    return true;
  }
  std::string error;
  if (io.appendPathLocation(output.base, path, out, error)) return true;
  diagnostic(failure_code,
             "Cannot resolve %.*s sound output '%.*s': %s",
             static_cast<int>(owner.size()),
             owner.data(),
             static_cast<int>(path.size()),
             path.data(),
             error.c_str());
  return false;
}

template <class SoundFile>
bool addSoundFileOutputsImpl(ResourceOutputPlan& plan, IoService& io, const SoundOutput& output,
                             std::span<const SoundFile> files, ResourceAssetId asset, DiagnosticCode failure_code,
                             std::string_view owner) {
  for (const SoundFile& file : files) {
    if (file.encoded_audio.empty()) {
      diagnostic(failure_code,
                 "Cannot write %.*s sound '%s': encoded audio data is empty",
                 static_cast<int>(owner.size()),
                 owner.data(),
                 file.path.c_str());
      return false;
    }
    PathLocation target;
    if (!soundOutputTarget(io, output, file.path, target, failure_code, owner)) return false;
    plan.add(ResourceFileKind::kAudio, std::move(target), file.encoded_audio.data(), file.encoded_audio.size(), asset);
  }
  return true;
}

}  // namespace

AudioCandidateResult loadAudioCandidate(IoService& io, const PathLocation* base, std::string_view path,
                                        std::string_view owner, media::PreparedAudio& out) {
  return loadAudioCandidate(io, base, path, AudioLookupMode::kExact, owner, out);
}

AudioCandidateResult loadAudioCandidate(IoService& io, const PathLocation* base, std::string_view path,
                                        AudioLookupMode mode, std::string_view owner, media::PreparedAudio& out) {
  PathLocation lookup_base;
  if (base) {
    lookup_base = *base;
  } else {
    lookup_base.address = PathAddress::kMountRelative;
  }

  std::vector<std::uint8_t> encoded;
  std::string resolved;
  const ResourceReadResult result =
      io.readAudio(lookup_base, normalizeResourceSeparators(path), mode, encoded, nullptr, &resolved);
  switch (result) {
    case ResourceReadResult::kSuccess:
      if (media::prepareAudio(std::move(encoded), out) == ARX_OK) return AudioCandidateResult::kLoaded;
      log(ARX_LOG_WARN,
          "%.*s sound data is invalid: %.*s -> %s",
          static_cast<int>(owner.size()),
          owner.data(),
          static_cast<int>(path.size()),
          path.data(),
          resolved.c_str());
      return AudioCandidateResult::kFailed;
    case ResourceReadResult::kReadFailed:
      log(ARX_LOG_WARN,
          "%.*s sound could not be read: %.*s -> %s",
          static_cast<int>(owner.size()),
          owner.data(),
          static_cast<int>(path.size()),
          path.data(),
          resolved.c_str());
      return AudioCandidateResult::kFailed;
    case ResourceReadResult::kInvalidPath:
      log(ARX_LOG_WARN,
          "%.*s sound path cannot be resolved: %.*s",
          static_cast<int>(owner.size()),
          owner.data(),
          static_cast<int>(path.size()),
          path.data());
      return AudioCandidateResult::kFailed;
    case ResourceReadResult::kNotFound:
      return AudioCandidateResult::kNotFound;
  }
  return AudioCandidateResult::kFailed;
}

AudioCandidateResult loadAudioCandidate(IoService& io, const PathLocation& location, std::string_view display_path,
                                        std::string_view owner, media::PreparedAudio& out) {
  std::vector<std::uint8_t> encoded;
  std::string resolved;
  const ResourceReadResult result = io.readPath(location, encoded, &resolved);
  switch (result) {
    case ResourceReadResult::kSuccess:
      if (media::prepareAudio(std::move(encoded), out) == ARX_OK) return AudioCandidateResult::kLoaded;
      log(ARX_LOG_WARN,
          "%.*s sound data is invalid: %.*s -> %s",
          static_cast<int>(owner.size()),
          owner.data(),
          static_cast<int>(display_path.size()),
          display_path.data(),
          resolved.c_str());
      return AudioCandidateResult::kFailed;
    case ResourceReadResult::kReadFailed:
      log(ARX_LOG_WARN,
          "%.*s sound could not be read: %.*s -> %s",
          static_cast<int>(owner.size()),
          owner.data(),
          static_cast<int>(display_path.size()),
          display_path.data(),
          resolved.c_str());
      return AudioCandidateResult::kFailed;
    case ResourceReadResult::kInvalidPath:
      log(ARX_LOG_WARN,
          "%.*s sound path cannot be resolved: %.*s",
          static_cast<int>(owner.size()),
          owner.data(),
          static_cast<int>(display_path.size()),
          display_path.data());
      return AudioCandidateResult::kFailed;
    case ResourceReadResult::kNotFound:
      return AudioCandidateResult::kNotFound;
  }
  return AudioCandidateResult::kFailed;
}

bool loadSoundData(pistoris::Ambiance& ambiance, IoService& io, const SoundInput& input,
                   std::span<const pistoris::SoundSourceReference> sources) {
  return loadSoundDataImpl(ambiance, "Ambiance", io, input, sources);
}

bool loadSoundData(pistoris::Animation& animation, IoService& io, const SoundInput& input,
                   std::span<const pistoris::SoundSourceReference> sources) {
  return loadSoundDataImpl(animation, "Animation", io, input, sources);
}

void loadNativeSoundFiles(const pistoris::amb::Data& ambiance, pistoris::NativeTextMode text_mode, IoService& io,
                          const SoundInput& input, std::vector<pistoris::SoundFile>& out) {
  std::vector<NativeSoundLookup> sounds;
  std::vector<SoundSourceLookup> sources;
  std::unordered_map<std::string, pistoris::SoundIndex> by_path;
  sounds.reserve(ambiance.tracks.size());
  sources.reserve(ambiance.tracks.size());
  by_path.reserve(ambiance.tracks.size());
  for (const pistoris::amb::Track& track : ambiance.tracks) {
    std::string utf8_path;
    const ArxReturnCode rc = io_detail::nativeTextToUtf8(track.sample_path, text_mode, utf8_path);
    if (rc != ARX_OK) {
      log(ARX_LOG_WARN,
          "Ambiance native sound path cannot be decoded; skipping referenced audio (code %d)",
          static_cast<int>(rc));
      continue;
    }
    std::string key = logicalSoundPath(utf8_path);
    auto [entry, inserted] = by_path.emplace(key, static_cast<pistoris::SoundIndex>(sounds.size()));
    if (inserted) sounds.push_back({{entry->second, std::move(key), {}}, false});
    sources.push_back({entry->second, std::move(utf8_path)});
  }

  if (input.use_format_sources) {
    for (const SoundSourceLookup& source : sources) {
      NativeSoundLookup& sound = sounds[source.sound];
      if (sound.loaded || source.path.empty()) continue;
      const AudioCandidateResult result = tryLoad(
          io,
          &input.source_base,
          source.path,
          "Ambiance",
          [&](std::vector<std::uint8_t> bytes) { return setNativeAudio(sound, std::move(bytes)); },
          AudioLookupMode::kFormatPriority);
      sound.loaded = result == AudioCandidateResult::kLoaded;
      sound.failed = sound.failed || result == AudioCandidateResult::kFailed;
    }
  }

  for (NativeSoundLookup& sound : sounds) {
    AudioCandidateResult result = AudioCandidateResult::kNotFound;
    if (!sound.loaded) {
      result = tryLoad(io, nullptr, sound.file.path, "Ambiance", [&](std::vector<std::uint8_t> bytes) {
        return setNativeAudio(sound, std::move(bytes));
      });
      sound.loaded = result == AudioCandidateResult::kLoaded;
      sound.failed = sound.failed || result == AudioCandidateResult::kFailed;
    }
    if (sound.loaded) continue;
    if (!sound.failed && result == AudioCandidateResult::kNotFound) warnMissingSound(io, "Ambiance", sound.file.path);
  }

  out.clear();
  out.reserve(sounds.size());
  for (NativeSoundLookup& sound : sounds)
    if (sound.loaded) out.push_back(std::move(sound.file));
}

void loadNativeSoundFiles(const pistoris::tea::Data& animation, pistoris::NativeTextMode text_mode, IoService& io,
                          const SoundInput& input, std::vector<pistoris::SoundFile>& out) {
  std::vector<NativeSoundLookup> sounds;
  std::unordered_map<std::string, pistoris::SoundIndex> by_path;
  sounds.reserve(animation.keyframes.size());
  by_path.reserve(animation.keyframes.size());
  for (const pistoris::tea::Keyframe& keyframe : animation.keyframes) {
    if (!keyframe.sample) continue;
    const char* end = static_cast<const char*>(std::memchr(keyframe.sample->name, '\0', sizeof(keyframe.sample->name)));
    const std::size_t size =
        end ? static_cast<std::size_t>(end - keyframe.sample->name) : sizeof(keyframe.sample->name);
    const std::string_view raw(keyframe.sample->name, size);
    std::string utf8_path;
    const ArxReturnCode text_rc = io_detail::nativeTextToUtf8(raw, text_mode, utf8_path);
    if (text_rc != ARX_OK) {
      log(ARX_LOG_WARN,
          "Animation native sound path cannot be decoded; skipping referenced audio (code %d)",
          static_cast<int>(text_rc));
      continue;
    }
    const std::string path = teaSoundPath(utf8_path);
    auto [entry, inserted] = by_path.emplace(path, static_cast<pistoris::SoundIndex>(sounds.size()));
    if (!inserted) continue;
    sounds.push_back({{entry->second, path, {}}, false});
    NativeSoundLookup& sound = sounds.back();
    log(ARX_LOG_DEBUG,
        "Animation native sound lookup: sample '%.*s', format-relative {}, base '%s', runtime '%s'",
        static_cast<int>(utf8_path.size()),
        utf8_path.data(),
        input.use_format_sources ? "enabled" : "disabled",
        input.source_base.path.c_str(),
        path.c_str());
    if (input.use_format_sources) {
      std::string format_path(utf8_path);
      const std::size_t slash = format_path.find_last_of("/\\");
      const std::size_t dot = format_path.find_last_of('.');
      if (dot != std::string::npos && (slash == std::string::npos || dot > slash + 1U)) format_path.resize(dot);
      format_path += ".wav";
      const AudioCandidateResult result = tryLoad(
          io,
          &input.source_base,
          format_path,
          "Animation",
          [&](std::vector<std::uint8_t> bytes) { return setNativeAudio(sound, std::move(bytes)); },
          AudioLookupMode::kFormatPriority);
      sound.loaded = result == AudioCandidateResult::kLoaded;
      sound.failed = result == AudioCandidateResult::kFailed;
    }
  }
  for (NativeSoundLookup& sound : sounds) {
    AudioCandidateResult result = AudioCandidateResult::kNotFound;
    if (!sound.loaded) {
      result = tryLoad(io, nullptr, sound.file.path, "Animation", [&](std::vector<std::uint8_t> bytes) {
        return setNativeAudio(sound, std::move(bytes));
      });
      sound.loaded = result == AudioCandidateResult::kLoaded;
      sound.failed = sound.failed || result == AudioCandidateResult::kFailed;
    }
    if (sound.loaded) continue;
    if (!sound.failed && result == AudioCandidateResult::kNotFound) warnMissingSound(io, "Animation", sound.file.path);
  }
  out.clear();
  out.reserve(sounds.size());
  for (NativeSoundLookup& sound : sounds)
    if (sound.loaded) out.push_back(std::move(sound.file));
}

bool addSoundFileOutputs(ResourceOutputPlan& plan, IoService& io, const SoundOutput& output,
                         std::span<const pistoris::SoundFile> files, ResourceAssetId asset, DiagnosticCode failure_code,
                         std::string_view owner) {
  return addSoundFileOutputsImpl(plan, io, output, files, asset, failure_code, owner);
}

bool addSoundFileOutputs(ResourceOutputPlan& plan, IoService& io, const SoundOutput& output,
                         std::span<const pistoris::CinematicSoundFile> files, ResourceAssetId asset,
                         DiagnosticCode failure_code, std::string_view owner) {
  return addSoundFileOutputsImpl(plan, io, output, files, asset, failure_code, owner);
}

}  // namespace cli
