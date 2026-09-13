// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#include "resources/sound_io.h"

#include "arx_pistoris/ambiance.hpp"
#include "arx_pistoris/animation.hpp"
#include "arx_pistoris/base/indices.h"
#include "arx_pistoris/base/status.h"
#include "arx_pistoris/native/amb.hpp"
#include "arx_pistoris/native/tea.hpp"
#include "arx_pistoris/paths.hpp"
#include "arx_pistoris/runtime.hpp"
#include "arx_pistoris/runtime/types.h"
#include "arx_pistoris/sound.h"
#include "arx_pistoris/sound.hpp"

#include "base/resource_path.h"
#include "console/diagnostics.h"
#include "console/logging.h"
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
  std::string_view path;
};

enum class SoundLoadResult : std::uint8_t {
  kLoaded,
  kNotFound,
  kFailed,
};

bool setNativeAudio(NativeSoundLookup& sound, std::vector<std::uint8_t> encoded) {
  media::PreparedAudio prepared;
  if (media::prepareAudio(std::move(encoded), prepared) != ARX_OK) return false;
  sound.file.encoded_audio = std::move(prepared.encoded);
  return true;
}

std::string logicalSoundPath(std::string_view path) { return resourcePathKey(path); }

ResourceReadResult readFormatSound(IoService& io, const PathLocation& base, std::string_view path,
                                   std::vector<std::uint8_t>& out, std::string& resolved) {
  PathLocation location;
  std::string error;
  if (!io.appendPathLocation(base, normalizeResourceSeparators(path), location, error))
    return ResourceReadResult::kInvalidPath;
  return io.readPath(location, out, &resolved);
}

template <class Apply>
SoundLoadResult tryLoad(IoService& io, const PathLocation* base, std::string_view path, std::string_view owner,
                        Apply&& apply) {
  std::vector<std::uint8_t> encoded;
  std::string resolved;
  const ResourceReadResult result = base ? readFormatSound(io, *base, path, encoded, resolved)
                                         : io.readResource(logicalSoundPath(path), encoded, &resolved);
  switch (result) {
    case ResourceReadResult::kSuccess:
      if (apply(std::move(encoded))) return SoundLoadResult::kLoaded;
      log(ARX_LOG_WARN,
          "%.*s sound data is invalid: %.*s -> %s",
          static_cast<int>(owner.size()),
          owner.data(),
          static_cast<int>(path.size()),
          path.data(),
          resolved.c_str());
      return SoundLoadResult::kFailed;
    case ResourceReadResult::kReadFailed:
      log(ARX_LOG_WARN,
          "%.*s sound could not be read: %.*s -> %s",
          static_cast<int>(owner.size()),
          owner.data(),
          static_cast<int>(path.size()),
          path.data(),
          resolved.c_str());
      return SoundLoadResult::kFailed;
    case ResourceReadResult::kInvalidPath:
      log(ARX_LOG_WARN,
          "%.*s sound path cannot be resolved: %.*s",
          static_cast<int>(owner.size()),
          owner.data(),
          static_cast<int>(path.size()),
          path.data());
      return SoundLoadResult::kFailed;
    case ResourceReadResult::kNotFound:
      return SoundLoadResult::kNotFound;
  }
  return SoundLoadResult::kFailed;
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
      const SoundLoadResult result =
          tryLoad(io, &input.source_base, source.path, owner_name, [&](std::vector<std::uint8_t> encoded) {
            return owner.setSoundData(source.sound, {encoded.data(), encoded.size()}) == ARX_OK;
          });
      if (result == SoundLoadResult::kLoaded) {
        sounds[source.sound].loaded = true;
        ++loaded;
      } else if (result == SoundLoadResult::kFailed) {
        sounds[source.sound].failed = true;
      }
    }
  }
  for (std::size_t index = 0; index < sounds.size(); ++index) {
    if (sounds[index].loaded || sounds[index].path.empty()) continue;
    const SoundLoadResult result =
        tryLoad(io, nullptr, sounds[index].path, owner_name, [&](std::vector<std::uint8_t> encoded) {
          return owner.setSoundData(static_cast<pistoris::SoundIndex>(index), {encoded.data(), encoded.size()}) ==
                 ARX_OK;
        });
    if (result == SoundLoadResult::kLoaded) {
      sounds[index].loaded = true;
      ++loaded;
      continue;
    }
    if (result == SoundLoadResult::kFailed) sounds[index].failed = true;
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

}  // namespace

bool loadSoundData(pistoris::Ambiance& ambiance, IoService& io, const SoundInput& input,
                   std::span<const pistoris::SoundSourceReference> sources) {
  return loadSoundDataImpl(ambiance, "Ambiance", io, input, sources);
}

bool loadSoundData(pistoris::Animation& animation, IoService& io, const SoundInput& input,
                   std::span<const pistoris::SoundSourceReference> sources) {
  return loadSoundDataImpl(animation, "Animation", io, input, sources);
}

void loadNativeSoundFiles(const pistoris::amb::Data& ambiance, IoService& io, const SoundInput& input,
                          std::vector<pistoris::SoundFile>& out) {
  std::vector<NativeSoundLookup> sounds;
  std::vector<SoundSourceLookup> sources;
  std::unordered_map<std::string, pistoris::SoundIndex> by_path;
  sounds.reserve(ambiance.tracks.size());
  sources.reserve(ambiance.tracks.size());
  by_path.reserve(ambiance.tracks.size());
  for (const pistoris::amb::Track& track : ambiance.tracks) {
    std::string key = logicalSoundPath(track.sample_path);
    auto [entry, inserted] = by_path.emplace(key, static_cast<pistoris::SoundIndex>(sounds.size()));
    if (inserted) sounds.push_back({{entry->second, std::move(key), {}}, false});
    sources.push_back({entry->second, track.sample_path});
  }

  if (input.use_format_sources) {
    for (const SoundSourceLookup& source : sources) {
      NativeSoundLookup& sound = sounds[source.sound];
      if (sound.loaded || source.path.empty()) continue;
      const SoundLoadResult result =
          tryLoad(io, &input.source_base, source.path, "Ambiance", [&](std::vector<std::uint8_t> bytes) {
            return setNativeAudio(sound, std::move(bytes));
          });
      sound.loaded = result == SoundLoadResult::kLoaded;
      sound.failed = sound.failed || result == SoundLoadResult::kFailed;
    }
  }

  for (NativeSoundLookup& sound : sounds) {
    SoundLoadResult result = SoundLoadResult::kNotFound;
    if (!sound.loaded) {
      result = tryLoad(io, nullptr, sound.file.path, "Ambiance", [&](std::vector<std::uint8_t> bytes) {
        return setNativeAudio(sound, std::move(bytes));
      });
      sound.loaded = result == SoundLoadResult::kLoaded;
      sound.failed = sound.failed || result == SoundLoadResult::kFailed;
    }
    if (sound.loaded) continue;
    if (!sound.failed && result == SoundLoadResult::kNotFound) warnMissingSound(io, "Ambiance", sound.file.path);
  }

  out.clear();
  out.reserve(sounds.size());
  for (NativeSoundLookup& sound : sounds)
    if (sound.loaded) out.push_back(std::move(sound.file));
}

void loadNativeSoundFiles(const pistoris::tea::Data& animation, IoService& io, const SoundInput& input,
                          std::vector<pistoris::SoundFile>& out) {
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
    const std::string path = teaSoundPath(raw);
    auto [entry, inserted] = by_path.emplace(path, static_cast<pistoris::SoundIndex>(sounds.size()));
    if (!inserted) continue;
    sounds.push_back({{entry->second, path, {}}, false});
    NativeSoundLookup& sound = sounds.back();
    if (input.use_format_sources) {
      std::string format_path(raw);
      const std::size_t slash = format_path.find_last_of("/\\");
      const std::size_t dot = format_path.find_last_of('.');
      if (dot != std::string::npos && (slash == std::string::npos || dot > slash + 1U)) format_path.resize(dot);
      format_path += ".wav";
      const SoundLoadResult result =
          tryLoad(io, &input.source_base, format_path, "Animation", [&](std::vector<std::uint8_t> bytes) {
            return setNativeAudio(sound, std::move(bytes));
          });
      sound.loaded = result == SoundLoadResult::kLoaded;
      sound.failed = result == SoundLoadResult::kFailed;
    }
  }
  for (NativeSoundLookup& sound : sounds) {
    SoundLoadResult result = SoundLoadResult::kNotFound;
    if (!sound.loaded) {
      result = tryLoad(io, nullptr, sound.file.path, "Animation", [&](std::vector<std::uint8_t> bytes) {
        return setNativeAudio(sound, std::move(bytes));
      });
      sound.loaded = result == SoundLoadResult::kLoaded;
      sound.failed = sound.failed || result == SoundLoadResult::kFailed;
    }
    if (sound.loaded) continue;
    if (!sound.failed && result == SoundLoadResult::kNotFound) warnMissingSound(io, "Animation", sound.file.path);
  }
  out.clear();
  out.reserve(sounds.size());
  for (NativeSoundLookup& sound : sounds)
    if (sound.loaded) out.push_back(std::move(sound.file));
}

bool addSoundFileOutputs(ResourceOutputPlan& plan, IoService& io, const SoundOutput& output,
                         std::span<const pistoris::SoundFile> files, ResourceAssetId asset, DiagnosticCode failure_code,
                         std::string_view owner) {
  for (const pistoris::SoundFile& file : files) {
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

}  // namespace cli
