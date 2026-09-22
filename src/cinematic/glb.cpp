// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#include "arx_pistoris/cinematic/glb.hpp"

#include "arx_pistoris/base/indices.h"
#include "arx_pistoris/base/status.h"
#include "arx_pistoris/cinematic.hpp"
#include "arx_pistoris/cinematic/sound.hpp"
#include "arx_pistoris/runtime/types.h"
#include "arx_pistoris/sound.hpp"

#include "api/status_boundary.h"
#include "cinematic/data.h"
#include "cinematic/internal.h"
#include "external/glb/cinematic/api.h"
#include "modules/cinematic.h"
#include "modules/sounds.h"
#include "utils/log.h"
#include "utils/resource_path.h"

#include <cstdint>
#include <span>
#include <string>
#include <string_view>
#include <unordered_set>
#include <utility>
#include <vector>

namespace pistoris {
namespace {

std::string_view audioExtension(sounds::AudioFormat format) noexcept {
  switch (format) {
    case sounds::AudioFormat::kWav:
      return ".wav";
    case sounds::AudioFormat::kMp3:
      return ".mp3";
    case sounds::AudioFormat::kOggVorbis:
      return ".ogg";
    case sounds::AudioFormat::kUnknown:
      return {};
  }
  return {};
}

ArxReturnCode collectSoundFiles(const CinematicModules& modules, std::vector<CinematicSoundFile>& out) {
  std::unordered_set<SoundHandle> used;
  used.reserve(modules.cinematic.keyframes.size());
  for (const CinematicKeyframe& key : modules.cinematic.keyframes)
    if (key.sound != kNoSoundHandle) used.insert(key.sound);

  std::vector<CinematicSoundFile> files;
  files.reserve(modules.sounds.encodings.size());
  std::unordered_set<std::string, ResourcePathIdentityHash, ResourcePathIdentityEqual> output_paths;
  output_paths.reserve(modules.sounds.encodings.size());
  for (const SoundEncoding& encoding : modules.sounds.encodings) {
    if (!used.contains(encoding.sound)) continue;
    sounds::AudioInfo info;
    const sounds::Error inspect_error = sounds::inspectEncodedAudio(encoding.encoded_audio, info);
    if (inspect_error != sounds::Error::kNone) return cinematic_detail::soundError(inspect_error);
    const std::string_view extension = audioExtension(info.format);
    if (extension.empty()) return ARX_CINEMATIC_BAD_SOUND_DATA;

    SoundKind kind = SoundKind::kEffect;
    if (soundHandleKind(encoding.sound, kind) != ARX_OK) return ARX_CINEMATIC_BAD_SOUND_ENCODING;
    std::string path(sounds::path(modules.sounds, encoding.sound));
    if (kind == SoundKind::kSpeech) {
      const auto language = modules.sounds.languages.find(encoding.language);
      if (language == modules.sounds.languages.end()) return ARX_CINEMATIC_BAD_LANGUAGE;
      path += '[' + language->second + ']';
    }
    path += extension;
    if (!output_paths.insert(path).second) {
      log(ARX_LOG_DEBUG, "Cinematic -> GLB: multiple sound encodings map to sidecar path '{}'", path);
      return ARX_CINEMATIC_BAD_SOUND_ENCODING;
    }
    files.push_back({encoding.sound, encoding.language, std::move(path), encoding.encoded_audio});
  }
  out = std::move(files);
  return ARX_OK;
}

void logStart(std::string_view direction) { log(ARX_LOG_INFO, "=== {} conversion started ===", direction); }

ArxReturnCode logFailure(std::string_view direction, ArxReturnCode rc) {
  log(ARX_LOG_INFO, "=== {} conversion failed with code {} ===", direction, rc);
  return rc;
}

ArxReturnCode exportGlbInternal(const Cinematic& cinematic, const CinematicModules& modules,
                                std::vector<std::uint8_t>& out, std::vector<CinematicSoundFile>* sound_files) {
  constexpr std::string_view kDirection = "Cinematic -> GLB";
  logStart(kDirection);
  ArxReturnCode rc = cinematic.validate();
  if (rc != ARX_OK) return logFailure(kDirection, rc);

  std::vector<std::uint8_t> encoded;
  rc = exportCinematicToGlb(modules, encoded);
  if (rc != ARX_OK) return logFailure(kDirection, rc);
  std::vector<CinematicSoundFile> files;
  if (sound_files != nullptr) {
    rc = collectSoundFiles(modules, files);
    if (rc != ARX_OK) return logFailure(kDirection, rc);
  }
  log(ARX_LOG_INFO,
      "=== {} conversion completed: {} illustration(s), {} keyframe(s) ===",
      kDirection,
      modules.cinematic.illustrations.size(),
      modules.cinematic.keyframes.size());
  out = std::move(encoded);
  if (sound_files != nullptr) *sound_files = std::move(files);
  return ARX_OK;
}

}  // namespace

ArxReturnCode Cinematic::importGlb(Cinematic& out, std::span<const std::uint8_t> data,
                                   std::vector<CinematicSoundSourceReference>* sound_sources) noexcept {
  return api_detail::statusBoundary([&]() -> ArxReturnCode {
    constexpr std::string_view kDirection = "GLB -> Cinematic";
    logStart(kDirection);
    Cinematic result;
    std::vector<CinematicSoundSourceReference> sources;
    const ArxReturnCode rc = importCinematicFromGlb(
        data, static_cast<CinematicModules&>(*result.data_), sound_sources != nullptr ? &sources : nullptr);
    if (rc != ARX_OK) return logFailure(kDirection, rc);
    log(ARX_LOG_INFO,
        "=== {} conversion completed: {} illustration(s), {} keyframe(s) ===",
        kDirection,
        result.data_->cinematic.illustrations.size(),
        result.data_->cinematic.keyframes.size());
    out.swap(result);
    if (sound_sources != nullptr) *sound_sources = std::move(sources);
    return ARX_OK;
  });
}

ArxReturnCode Cinematic::exportGlb(std::vector<std::uint8_t>& out) const noexcept {
  return api_detail::statusBoundary([&]() -> ArxReturnCode {
    return exportGlbInternal(*this, static_cast<const CinematicModules&>(*data_), out, nullptr);
  });
}

ArxReturnCode Cinematic::exportGlbBundle(CinematicGlbBundle& out) const noexcept {
  return api_detail::statusBoundary([&]() -> ArxReturnCode {
    CinematicGlbBundle result;
    const ArxReturnCode rc =
        exportGlbInternal(*this, static_cast<const CinematicModules&>(*data_), result.glb, &result.sound_files);
    if (rc != ARX_OK) return rc;
    out = std::move(result);
    return ARX_OK;
  });
}

}  // namespace pistoris
