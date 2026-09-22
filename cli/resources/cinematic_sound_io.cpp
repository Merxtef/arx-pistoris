// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#include "resources/cinematic_sound_io.h"

#include "arx_pistoris/base/indices.h"
#include "arx_pistoris/base/status.h"
#include "arx_pistoris/cinematic.hpp"
#include "arx_pistoris/cinematic/sound.hpp"
#include "arx_pistoris/cinematic/types.h"
#include "arx_pistoris/native/cin.hpp"
#include "arx_pistoris/native/text.hpp"
#include "arx_pistoris/runtime.hpp"
#include "arx_pistoris/runtime/types.h"
#include "arx_pistoris/sound.hpp"

#include "base/ascii.h"
#include "base/resource_path.h"
#include "console/diagnostics.h"
#include "console/logging.h"
#include "io/native_text.h"
#include "io/path_location.h"
#include "io/service.h"
#include "media/encoded.h"
#include "resources/sound_io.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

namespace cli {
namespace {

struct StableSound {
  pistoris::SoundKind kind = pistoris::SoundKind::kEffect;
  pistoris::SoundIndex index = pistoris::kNoSound;

  friend bool operator==(const StableSound&, const StableSound&) = default;
};

struct StableSoundHash {
  std::size_t operator()(StableSound value) const noexcept {
    return (static_cast<std::size_t>(value.index) << 1U) | (value.kind == pistoris::SoundKind::kSpeech ? 1U : 0U);
  }
};

struct PreparedSource {
  StableSound sound;
  std::string authored_path;
  std::string canonical_path;
  std::optional<std::size_t> preferred_extension;
};

struct LoadState {
  bool loaded = false;
  bool failed = false;
};

struct NativeSpeechCandidate {
  StableSound sound;
  std::string language;
  std::string relative_path;
  PathLocation location;
  std::size_t extension = 0;
};

std::string identity(std::string_view value) { return asciiLower(normalizeResourceSeparators(value)); }

bool stableSound(pistoris::SoundHandle handle, StableSound& out) {
  return pistoris::soundHandleKind(handle, out.kind) == ARX_OK &&
         pistoris::soundHandleIndex(handle, out.index) == ARX_OK;
}

bool currentHandle(const pistoris::Cinematic& cinematic, StableSound sound, pistoris::SoundHandle& out) {
  if (sound.index >= cinematic.soundCount(sound.kind)) return false;
  return pistoris::soundHandle(sound.kind, sound.index, out) == ARX_OK;
}

std::optional<std::size_t> knownExtension(std::string_view path) {
  const std::size_t separator = path.find_last_of("/\\");
  const std::size_t dot = path.find_last_of('.');
  if (dot == std::string_view::npos || (separator != std::string_view::npos && dot < separator)) return std::nullopt;
  const std::span<const std::string_view> extensions = media::audioLookupExtensions();
  for (std::size_t index = 0; index < extensions.size(); ++index)
    if (equalAsciiInsensitive(path.substr(dot), extensions[index])) return index;
  return std::nullopt;
}

std::string stripKnownExtension(std::string_view path, std::optional<std::size_t>& extension) {
  extension = knownExtension(path);
  if (!extension) return normalizeResourceSeparators(path);
  return normalizeResourceSeparators(path.substr(0, path.size() - media::audioLookupExtensions()[*extension].size()));
}

bool splitLanguageDecoration(std::string& path, std::string& language) {
  if (path.empty() || path.back() != ']') return false;
  const std::size_t separator = path.find_last_of('/');
  const std::size_t open = path.find_last_of('[');
  if (open == std::string::npos || open == 0 || (separator != std::string::npos && open < separator) ||
      open + 2U > path.size())
    return false;
  language.assign(path, open + 1U, path.size() - open - 2U);
  path.resize(open);
  return !language.empty();
}

bool canonicalizeGlbSources(pistoris::Cinematic& cinematic, std::span<pistoris::CinematicSoundSourceReference> sources,
                            std::vector<PreparedSource>& out) {
  out.clear();
  out.reserve(sources.size());
  std::unordered_map<StableSound, std::string, StableSoundHash> paths;
  std::unordered_map<std::string, StableSound> sounds_by_identity;
  std::unordered_set<std::string> warned_decorations;
  for (pistoris::CinematicSoundSourceReference& source : sources) {
    StableSound sound;
    if (!stableSound(source.sound, sound) || sound.index >= cinematic.soundCount(sound.kind)) {
      diagnostic(DiagnosticCode::kCinematicInputFailed, "GLB sound source has an invalid handle");
      return false;
    }

    PreparedSource prepared;
    prepared.sound = sound;
    prepared.authored_path = source.path;
    prepared.canonical_path = stripKnownExtension(source.path, prepared.preferred_extension);
    if (sound.kind == pistoris::SoundKind::kSpeech && prepared.preferred_extension) {
      std::string undecorated = prepared.canonical_path;
      std::string discarded_language;
      if (splitLanguageDecoration(undecorated, discarded_language)) {
        prepared.canonical_path = std::move(undecorated);
        if (warned_decorations.insert(source.path).second) {
          log(ARX_LOG_WARN,
              "Cinematic GLB speech reference includes a language suffix; using '%s' as the logical path: %s",
              prepared.canonical_path.c_str(),
              source.path.c_str());
        }
      }
    }

    const std::string key = identity(prepared.canonical_path);
    std::string kind_key = sound.kind == pistoris::SoundKind::kSpeech ? "speech:" : "effect:";
    kind_key += key;
    const auto [identity_entry, identity_inserted] = sounds_by_identity.emplace(kind_key, sound);
    if (!identity_inserted && identity_entry->second != sound) {
      diagnostic(DiagnosticCode::kCinematicInputFailed,
                 "Distinct GLB sounds resolve to the same logical path: %s",
                 prepared.canonical_path.c_str());
      return false;
    }
    auto [entry, inserted] = paths.emplace(sound, key);
    if (!inserted && entry->second != key) {
      diagnostic(DiagnosticCode::kCinematicInputFailed,
                 "GLB sound references for one sound resolve to different logical paths");
      return false;
    }
    out.push_back(std::move(prepared));
  }

  pistoris::Cinematic probe(cinematic);
  std::unordered_set<StableSound, StableSoundHash> applied;
  for (const PreparedSource& source : out) {
    if (!applied.insert(source.sound).second) continue;
    pistoris::SoundHandle handle = pistoris::kNoSoundHandle;
    if (!currentHandle(probe, source.sound, handle)) return false;
    const ArxReturnCode rc = probe.setSoundPath(handle, source.canonical_path);
    if (rc != ARX_OK) {
      diagnostic(DiagnosticCode::kCinematicInputFailed,
                 "GLB sound path canonicalization failed for '%s': %s (code %d)",
                 source.authored_path.c_str(),
                 pistoris::errorString(rc),
                 static_cast<int>(rc));
      return false;
    }
  }

  applied.clear();
  for (const PreparedSource& source : out) {
    if (!applied.insert(source.sound).second) continue;
    pistoris::SoundHandle handle = pistoris::kNoSoundHandle;
    if (!currentHandle(cinematic, source.sound, handle)) return false;
    const ArxReturnCode rc = cinematic.setSoundPath(handle, source.canonical_path);
    if (rc != ARX_OK) {
      diagnostic(DiagnosticCode::kCinematicInputFailed,
                 "GLB sound path canonicalization failed: %s (code %d)",
                 pistoris::errorString(rc),
                 static_cast<int>(rc));
      return false;
    }
  }
  for (pistoris::CinematicSoundSourceReference& source : sources) {
    StableSound sound;
    if (!stableSound(source.sound, sound) || !currentHandle(cinematic, sound, source.sound)) return false;
  }
  return true;
}

bool nativeSources(const pistoris::Cinematic& cinematic, std::span<pistoris::CinematicSoundSourceReference> sources,
                   std::vector<PreparedSource>& out) {
  out.clear();
  out.reserve(sources.size());
  for (pistoris::CinematicSoundSourceReference& source : sources) {
    StableSound sound;
    if (!stableSound(source.sound, sound) || sound.index >= cinematic.soundCount(sound.kind)) {
      diagnostic(DiagnosticCode::kCinematicInputFailed, "CIN sound source has an invalid handle");
      return false;
    }
    out.push_back({sound, source.path, source.path, std::nullopt});
  }
  return true;
}

bool setEffectData(pistoris::Cinematic& cinematic, StableSound sound, media::PreparedAudio&& prepared) {
  pistoris::SoundHandle handle = pistoris::kNoSoundHandle;
  if (!currentHandle(cinematic, sound, handle)) return false;
  return cinematic.setSoundData(handle, pistoris::kSoundEffects, {prepared.encoded.data(), prepared.encoded.size()}) ==
         ARX_OK;
}

std::string languageKey(std::string_view language) { return asciiLower(language); }

bool copyLanguages(const pistoris::Cinematic& cinematic, std::unordered_map<std::string, pistoris::LanguageId>& out) {
  std::vector<ArxCinematicLanguageView> views(cinematic.languageCount());
  const ArxReturnCode rc = cinematic.copyLanguages(0, views.size(), views.data());
  if (rc != ARX_OK) return false;
  out.clear();
  out.reserve(views.size());
  for (const ArxCinematicLanguageView& view : views) {
    out.emplace(languageKey({view.name.data, view.name.size}), view.id);
  }
  return true;
}

bool setSpeechData(pistoris::Cinematic& cinematic, StableSound sound, std::string_view language,
                   media::PreparedAudio&& prepared, std::unordered_map<std::string, pistoris::LanguageId>& languages,
                   bool& rejected) {
  rejected = false;
  const std::string key = languageKey(language);
  pistoris::LanguageId language_id = pistoris::kInvalidLanguageId;
  const auto existing = languages.find(key);
  if (existing != languages.end()) {
    language_id = existing->second;
  } else {
    const ArxReturnCode rc = cinematic.addLanguage(language, language_id);
    if (rc != ARX_OK) {
      log(ARX_LOG_WARN,
          "Cinematic speech language is invalid and was ignored: %.*s",
          static_cast<int>(language.size()),
          language.data());
      rejected = true;
      return true;
    }
    languages.emplace(key, language_id);
  }

  pistoris::SoundHandle handle = pistoris::kNoSoundHandle;
  if (!currentHandle(cinematic, sound, handle)) return false;
  const ArxReturnCode rc =
      cinematic.setSoundData(handle, language_id, {prepared.encoded.data(), prepared.encoded.size()});
  if (rc == ARX_OK) return true;
  diagnostic(DiagnosticCode::kCinematicInputFailed,
             "Cinematic speech data assignment failed: %s (code %d)",
             pistoris::errorString(rc),
             static_cast<int>(rc));
  return false;
}

AudioCandidateResult loadCandidate(IoService& io, const SoundInput& input, std::string_view path, AudioLookupMode mode,
                                   media::PreparedAudio& out) {
  return loadAudioCandidate(io, input.use_format_sources ? &input.source_base : nullptr, path, mode, "Cinematic", out);
}

void warnMissing(std::string_view path) {
  log(ARX_LOG_WARN, "Cinematic sound not found: %.*s", static_cast<int>(path.size()), path.data());
}

bool loadEffects(pistoris::Cinematic& cinematic, IoService& io, const SoundInput& input,
                 CinematicSoundSourceFormat format, std::span<const PreparedSource> sources,
                 std::unordered_map<StableSound, LoadState, StableSoundHash>& states) {
  for (const PreparedSource& source : sources) {
    if (source.sound.kind != pistoris::SoundKind::kEffect) continue;
    LoadState& state = states[source.sound];
    if (state.loaded) continue;
    std::string path =
        format == CinematicSoundSourceFormat::kCin ? "sfx/" + source.canonical_path + ".wav" : source.authored_path;
    AudioLookupMode mode = AudioLookupMode::kFormatPriority;
    if (format == CinematicSoundSourceFormat::kCin && !input.use_format_sources) mode = AudioLookupMode::kExact;
    media::PreparedAudio prepared;
    const AudioCandidateResult result = loadCandidate(io, input, path, mode, prepared);
    if (result == AudioCandidateResult::kFailed) {
      state.failed = true;
      continue;
    }
    if (result == AudioCandidateResult::kNotFound) continue;
    if (!setEffectData(cinematic, source.sound, std::move(prepared))) {
      diagnostic(DiagnosticCode::kCinematicInputFailed, "Cinematic sound data assignment failed");
      return false;
    }
    state.loaded = true;
  }
  return true;
}

std::size_t componentCount(std::string_view path) {
  if (path.empty()) return 0;
  return 1U + static_cast<std::size_t>(std::ranges::count(path, '/'));
}

bool speechDirectory(const SoundInput& input, IoService& io, std::string_view relative, PathLocation& out) {
  std::string path = "speech";
  if (!relative.empty()) {
    path.push_back('/');
    path += relative;
  }
  if (!input.use_format_sources) {
    out = {.path = std::move(path), .address = PathAddress::kMountRelative};
    return true;
  }
  std::string error;
  if (io.appendPathLocation(input.source_base, path, out, error)) return true;
  diagnostic(DiagnosticCode::kCinematicInputFailed, "Cannot resolve Cinematic speech directory: %s", error.c_str());
  return false;
}

bool splitNativeSpeechFile(std::string_view relative, bool allow_format_sources, std::string& language,
                           std::string& logical, std::size_t& extension) {
  const std::size_t separator = relative.find('/');
  if (separator == std::string_view::npos || separator == 0 || separator + 1U >= relative.size()) return false;
  const std::optional<std::size_t> known = knownExtension(relative);
  if (!known || (!allow_format_sources && *known != 0)) return false;
  const std::string_view suffix = media::audioLookupExtensions()[*known];
  language.assign(relative.substr(0, separator));
  logical.assign(relative.substr(separator + 1U, relative.size() - separator - 1U - suffix.size()));
  extension = *known;
  return !logical.empty();
}

bool discoverNativeSpeech(IoService& io, const SoundInput& input, std::span<const PreparedSource> sources,
                          bool& enumerated, std::vector<NativeSpeechCandidate>& out) {
  std::unordered_map<std::string, StableSound> by_path;
  std::uint32_t max_depth = 0;
  for (const PreparedSource& source : sources) {
    if (source.sound.kind != pistoris::SoundKind::kSpeech) continue;
    by_path.try_emplace(identity(source.canonical_path), source.sound);
    const std::size_t depth = componentCount(source.canonical_path) + 1U;
    max_depth = std::max(
        max_depth, static_cast<std::uint32_t>(std::min<std::size_t>(depth, std::numeric_limits<std::uint32_t>::max())));
  }
  out.clear();
  enumerated = true;
  if (by_path.empty()) return true;

  PathLocation directory;
  if (!speechDirectory(input, io, {}, directory)) return false;
  std::vector<EnumeratedFile> files;
  if (io.enumerateFiles(directory, max_depth, files) != ResourceEnumerationResult::kSuccess) {
    log(ARX_LOG_WARN, "Cinematic speech directory could not be enumerated");
    enumerated = false;
    return true;
  }

  for (EnumeratedFile& file : files) {
    std::string language;
    std::string logical;
    std::size_t extension = 0;
    if (!splitNativeSpeechFile(file.relative_path, input.use_format_sources, language, logical, extension)) continue;
    const auto sound = by_path.find(identity(logical));
    if (sound == by_path.end()) continue;
    out.push_back(
        {sound->second, std::move(language), std::move(file.relative_path), std::move(file.location), extension});
  }
  std::ranges::stable_sort(out, [](const NativeSpeechCandidate& lhs, const NativeSpeechCandidate& rhs) {
    if (lhs.sound.kind != rhs.sound.kind) return lhs.sound.kind < rhs.sound.kind;
    if (lhs.sound.index != rhs.sound.index) return lhs.sound.index < rhs.sound.index;
    const std::string left = languageKey(lhs.language);
    const std::string right = languageKey(rhs.language);
    if (left != right) return left < right;
    if (lhs.extension != rhs.extension) return lhs.extension < rhs.extension;
    return lhs.relative_path < rhs.relative_path;
  });
  return true;
}

bool loadNativeSpeech(pistoris::Cinematic& cinematic, IoService& io, const SoundInput& input,
                      std::span<const PreparedSource> sources,
                      std::unordered_map<StableSound, LoadState, StableSoundHash>& states,
                      std::unordered_map<std::string, pistoris::LanguageId>& languages) {
  bool enumerated = true;
  std::vector<NativeSpeechCandidate> candidates;
  if (!discoverNativeSpeech(io, input, sources, enumerated, candidates)) return false;
  if (!enumerated) {
    for (auto& [sound, state] : states) {
      if (sound.kind == pistoris::SoundKind::kSpeech) state.failed = true;
    }
    return true;
  }

  std::unordered_set<std::string> assigned;
  for (const NativeSpeechCandidate& candidate : candidates) {
    const std::string assignment = std::to_string(static_cast<unsigned>(candidate.sound.index)) + ':' +
                                   std::to_string(static_cast<unsigned>(candidate.sound.kind)) + ':' +
                                   languageKey(candidate.language);
    if (assigned.contains(assignment)) continue;
    media::PreparedAudio prepared;
    const AudioCandidateResult loaded =
        loadAudioCandidate(io, candidate.location, candidate.relative_path, "Cinematic", prepared);
    LoadState& state = states[candidate.sound];
    if (loaded == AudioCandidateResult::kFailed) {
      state.failed = true;
      continue;
    }
    if (loaded != AudioCandidateResult::kLoaded) continue;
    bool rejected = false;
    if (!setSpeechData(cinematic, candidate.sound, candidate.language, std::move(prepared), languages, rejected))
      return false;
    if (rejected) {
      state.failed = true;
      continue;
    }
    assigned.insert(assignment);
    state.loaded = true;
  }
  return true;
}

bool splitGlbSpeechFile(std::string_view filename, std::string_view expected_stem, std::string& language,
                        std::size_t& extension) {
  const std::optional<std::size_t> known = knownExtension(filename);
  if (!known) return false;
  const std::string_view ext = media::audioLookupExtensions()[*known];
  const std::string_view stem = filename.substr(0, filename.size() - ext.size());
  if (stem.size() <= expected_stem.size() + 2U || stem[expected_stem.size()] != '[' || stem.back() != ']' ||
      !equalAsciiInsensitive(stem.substr(0, expected_stem.size()), expected_stem))
    return false;
  language.assign(stem.substr(expected_stem.size() + 1U, stem.size() - expected_stem.size() - 2U));
  extension = *known;
  return !language.empty();
}

std::size_t extensionPriority(std::size_t extension, std::optional<std::size_t> preferred) {
  if (!preferred) return extension;
  if (extension == *preferred) return 0;
  return 1U + extension;
}

bool loadDiscoveredGlbSpeech(pistoris::Cinematic& cinematic, IoService& io, const SoundInput& input,
                             const PreparedSource& source,
                             std::unordered_map<std::string, pistoris::LanguageId>& languages,
                             std::unordered_set<std::string>& assigned, LoadState& state) {
  const std::size_t separator = source.canonical_path.find_last_of('/');
  const std::string_view parent = separator == std::string::npos
                                      ? std::string_view{}
                                      : std::string_view(source.canonical_path).substr(0, separator);
  const std::string_view stem = separator == std::string::npos
                                    ? std::string_view(source.canonical_path)
                                    : std::string_view(source.canonical_path).substr(separator + 1U);
  PathLocation directory;
  std::string error;
  if (parent.empty()) {
    directory = input.source_base;
  } else if (!io.appendPathLocation(input.source_base, parent, directory, error)) {
    diagnostic(DiagnosticCode::kCinematicInputFailed, "Cannot resolve Cinematic speech source: %s", error.c_str());
    return false;
  }
  std::vector<EnumeratedFile> files;
  const ResourceEnumerationResult enumeration = io.enumerateFiles(directory, 1, files);
  if (enumeration != ResourceEnumerationResult::kSuccess) {
    log(ARX_LOG_WARN, "Cinematic GLB speech directory could not be enumerated");
    state.failed = true;
    return true;
  }

  struct Match {
    const EnumeratedFile* file = nullptr;
    std::string language;
    std::size_t extension = 0;
  };
  std::vector<Match> matches;
  for (const EnumeratedFile& file : files) {
    std::string language;
    std::size_t extension = 0;
    if (splitGlbSpeechFile(resourceFilename(file.relative_path), stem, language, extension))
      matches.push_back({&file, std::move(language), extension});
  }
  std::ranges::stable_sort(matches, [&](const Match& lhs, const Match& rhs) {
    const std::string left = languageKey(lhs.language);
    const std::string right = languageKey(rhs.language);
    if (left != right) return left < right;
    return extensionPriority(lhs.extension, source.preferred_extension) <
           extensionPriority(rhs.extension, source.preferred_extension);
  });

  for (const Match& match : matches) {
    const std::string key = languageKey(match.language);
    if (assigned.contains(key)) continue;
    media::PreparedAudio prepared;
    const AudioCandidateResult loaded =
        loadAudioCandidate(io, match.file->location, match.file->relative_path, "Cinematic", prepared);
    if (loaded == AudioCandidateResult::kFailed) {
      state.failed = true;
      continue;
    }
    if (loaded != AudioCandidateResult::kLoaded) continue;
    bool rejected = false;
    if (!setSpeechData(cinematic, source.sound, match.language, std::move(prepared), languages, rejected)) return false;
    if (rejected) {
      state.failed = true;
      continue;
    }
    assigned.insert(key);
    state.loaded = true;
  }
  return true;
}

bool loadGlbSpeech(pistoris::Cinematic& cinematic, IoService& io, const SoundInput& input,
                   std::span<const PreparedSource> sources,
                   std::unordered_map<StableSound, LoadState, StableSoundHash>& states,
                   std::unordered_map<std::string, pistoris::LanguageId>& languages) {
  std::unordered_map<StableSound, std::unordered_set<std::string>, StableSoundHash> assigned;
  for (const PreparedSource& source : sources) {
    if (source.sound.kind != pistoris::SoundKind::kSpeech) continue;
    LoadState& state = states[source.sound];
    std::unordered_set<std::string>& sound_languages = assigned[source.sound];
    if (!loadDiscoveredGlbSpeech(cinematic, io, input, source, languages, sound_languages, state)) return false;
  }
  return true;
}

void nativeCarrierSources(const pistoris::cin::Data& cinematic, pistoris::NativeTextMode text_mode,
                          std::vector<PreparedSource>& out) {
  std::vector<bool> used(cinematic.sounds.size(), false);
  for (const pistoris::cin::Keyframe& keyframe : cinematic.keyframes) {
    if (keyframe.sound >= 0 && static_cast<std::size_t>(keyframe.sound) < used.size())
      used[static_cast<std::size_t>(keyframe.sound)] = true;
  }

  out.clear();
  out.reserve(cinematic.sounds.size());
  std::unordered_set<std::string> identities;
  for (std::size_t index = 0; index < cinematic.sounds.size(); ++index) {
    if (!used[index]) continue;
    const pistoris::cin::Sound& sound = cinematic.sounds[index];
    std::string path;
    const ArxReturnCode rc = io_detail::nativeTextToUtf8(sound.path, text_mode, path);
    if (rc != ARX_OK) {
      log(ARX_LOG_WARN,
          "Cinematic native sound path cannot be decoded; skipping referenced audio (code %d)",
          static_cast<int>(rc));
      continue;
    }
    const pistoris::SoundKind kind = sound.speech ? pistoris::SoundKind::kSpeech : pistoris::SoundKind::kEffect;
    std::string key = kind == pistoris::SoundKind::kSpeech ? "speech:" : "effect:";
    key += identity(path);
    if (!identities.insert(std::move(key)).second) continue;
    out.push_back({{kind, static_cast<pistoris::SoundIndex>(index)}, path, std::move(path), std::nullopt});
  }
}

std::string encodedAudioPath(std::string_view stem, const media::PreparedAudio& audio) {
  std::string path(stem);
  path += media::audioExtension(audio.info.format);
  return path;
}

void loadNativeEffectFiles(IoService& io, const SoundInput& input, std::span<const PreparedSource> sources,
                           std::unordered_map<StableSound, LoadState, StableSoundHash>& states,
                           std::vector<pistoris::SoundFile>& out) {
  for (const PreparedSource& source : sources) {
    if (source.sound.kind != pistoris::SoundKind::kEffect) continue;
    const std::string lookup = "sfx/" + source.canonical_path + ".wav";
    const AudioLookupMode mode = input.use_format_sources ? AudioLookupMode::kFormatPriority : AudioLookupMode::kExact;
    media::PreparedAudio prepared;
    const AudioCandidateResult result = loadCandidate(io, input, lookup, mode, prepared);
    LoadState& state = states[source.sound];
    if (result == AudioCandidateResult::kFailed) {
      state.failed = true;
      continue;
    }
    if (result != AudioCandidateResult::kLoaded) continue;
    std::string path = encodedAudioPath("sfx/" + source.canonical_path, prepared);
    if (path == "sfx/" + source.canonical_path) {
      state.failed = true;
      continue;
    }
    out.push_back({source.sound.index, std::move(path), std::move(prepared.encoded)});
    state.loaded = true;
  }
}

bool loadNativeSpeechFiles(IoService& io, const SoundInput& input, std::span<const PreparedSource> sources,
                           std::unordered_map<StableSound, LoadState, StableSoundHash>& states,
                           std::vector<pistoris::SoundFile>& out) {
  bool enumerated = true;
  std::vector<NativeSpeechCandidate> candidates;
  if (!discoverNativeSpeech(io, input, sources, enumerated, candidates)) return false;
  if (!enumerated) {
    for (auto& [sound, state] : states) {
      if (sound.kind == pistoris::SoundKind::kSpeech) state.failed = true;
    }
    return true;
  }

  std::unordered_set<std::string> assigned;
  for (const NativeSpeechCandidate& candidate : candidates) {
    const std::string assignment =
        std::to_string(static_cast<unsigned>(candidate.sound.index)) + ':' + languageKey(candidate.language);
    if (assigned.contains(assignment)) continue;
    media::PreparedAudio prepared;
    const AudioCandidateResult loaded =
        loadAudioCandidate(io, candidate.location, candidate.relative_path, "Cinematic", prepared);
    LoadState& state = states[candidate.sound];
    if (loaded == AudioCandidateResult::kFailed) {
      state.failed = true;
      continue;
    }
    if (loaded != AudioCandidateResult::kLoaded) continue;
    std::optional<std::size_t> ignored;
    const std::string stem = stripKnownExtension(candidate.relative_path, ignored);
    std::string path = encodedAudioPath("speech/" + stem, prepared);
    if (path == "speech/" + stem) {
      state.failed = true;
      continue;
    }
    out.push_back({candidate.sound.index, std::move(path), std::move(prepared.encoded)});
    assigned.insert(assignment);
    state.loaded = true;
  }
  return true;
}

}  // namespace

bool prepareCinematicSounds(pistoris::Cinematic& cinematic, IoService& io, const SoundInput& input,
                            std::span<pistoris::CinematicSoundSourceReference> sources,
                            CinematicSoundSourceFormat format, bool load_files) {
  std::vector<PreparedSource> prepared_sources;
  const bool prepared = format == CinematicSoundSourceFormat::kGlb
                            ? canonicalizeGlbSources(cinematic, sources, prepared_sources)
                            : nativeSources(cinematic, sources, prepared_sources);
  if (!prepared || !load_files) return prepared;

  std::unordered_map<StableSound, LoadState, StableSoundHash> states;
  states.reserve(prepared_sources.size());
  for (const PreparedSource& source : prepared_sources) states.try_emplace(source.sound);
  if (!loadEffects(cinematic, io, input, format, prepared_sources, states)) return false;

  std::unordered_map<std::string, pistoris::LanguageId> languages;
  if (!copyLanguages(cinematic, languages)) {
    diagnostic(DiagnosticCode::kCinematicInputFailed, "Cinematic language projection failed");
    return false;
  }
  if (format == CinematicSoundSourceFormat::kCin) {
    if (!loadNativeSpeech(cinematic, io, input, prepared_sources, states, languages)) return false;
  } else if (!loadGlbSpeech(cinematic, io, input, prepared_sources, states, languages)) {
    return false;
  }

  std::size_t loaded = 0;
  for (const auto& [sound, state] : states) {
    if (state.loaded) {
      ++loaded;
    } else if (!state.failed) {
      const auto source = std::ranges::find_if(
          prepared_sources, [&](const PreparedSource& candidate) { return candidate.sound == sound; });
      if (source != prepared_sources.end()) warnMissing(source->canonical_path);
    }
  }
  if (loaded != 0) log(ARX_LOG_INFO, "loaded audio for %zu Cinematic sound(s)", loaded);
  return true;
}

void loadNativeCinematicSoundFiles(const pistoris::cin::Data& cinematic, pistoris::NativeTextMode text_mode,
                                   IoService& io, const SoundInput& input, std::vector<pistoris::SoundFile>& out) {
  std::vector<PreparedSource> sources;
  nativeCarrierSources(cinematic, text_mode, sources);
  std::unordered_map<StableSound, LoadState, StableSoundHash> states;
  states.reserve(sources.size());
  for (const PreparedSource& source : sources) states.try_emplace(source.sound);

  out.clear();
  out.reserve(sources.size());
  loadNativeEffectFiles(io, input, sources, states, out);
  if (!loadNativeSpeechFiles(io, input, sources, states, out)) {
    for (auto& [sound, state] : states) {
      if (sound.kind == pistoris::SoundKind::kSpeech) state.failed = true;
    }
  }

  std::size_t loaded = 0;
  for (const auto& [sound, state] : states) {
    if (state.loaded) {
      ++loaded;
    } else if (!state.failed) {
      const auto source =
          std::ranges::find_if(sources, [&](const PreparedSource& candidate) { return candidate.sound == sound; });
      if (source != sources.end()) warnMissing(source->canonical_path);
    }
  }
  if (loaded != 0) log(ARX_LOG_INFO, "loaded %zu native Cinematic sound sidecar(s)", loaded);
}

}  // namespace cli
