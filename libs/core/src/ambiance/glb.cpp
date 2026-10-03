// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#include "arx_pistoris/ambiance.hpp"
#include "arx_pistoris/ambiance/bake.hpp"
#include "arx_pistoris/ambiance/location.hpp"
#include "arx_pistoris/base/indices.h"
#include "arx_pistoris/base/status.h"
#include "arx_pistoris/glb/location.hpp"
#include "arx_pistoris/model.hpp"
#include "arx_pistoris/model/location.hpp"
#include "arx_pistoris/runtime/types.h"
#include "arx_pistoris/sound.hpp"

#include "ambiance/data.h"
#include "api/result_failure.h"
#include "api/status_boundary.h"
#include "external/glb/ambiance/api.h"
#include "external/glb/failure.h"
#include "model/data.h"
#include "modules/ambiance.h"
#include "modules/sounds.h"
#include "utils/log.h"

#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace pistoris {
namespace {

void logGlbStart(std::string_view direction) { log(ARX_LOG_INFO, "=== {} conversion started ===", direction); }

ArxReturnCode exportGlbInternal(const AmbianceModules& modules, const Ambiance::GlbExportOptions& options,
                                const ModelModules* reference_modules, std::vector<std::uint8_t>& out,
                                std::vector<SoundFile>* sound_files) {
  constexpr std::string_view kDirection = "Ambiance -> GLB";
  logGlbStart(kDirection);
  std::vector<std::uint8_t> encoded;
  ArxReturnCode rc = exportAmbianceToGlb(modules, options, reference_modules, encoded);
  if (rc != ARX_OK) return rc;

  std::vector<SoundFile> files;
  if (sound_files != nullptr) {
    const std::size_t sound_count = sounds::count(modules.sounds, SoundKind::kEffect);
    std::vector<std::uint8_t> used(sound_count, 0);
    for (const AmbianceTrack& track : modules.ambiance.tracks) {
      SoundIndex sound = kNoSound;
      if (!sounds::effectIndex(track.sound, sound) || sound >= used.size()) return ARX_AMBIANCE_BAD_TRACK_SOUND;
      used[sound] = 1;
    }
    files.reserve(sound_count);
    for (std::size_t index = 0; index < sound_count; ++index) {
      const SoundHandle handle = sounds::effectHandle(static_cast<SoundIndex>(index));
      const std::span<const std::uint8_t> encoded_audio = sounds::encodedAudio(modules.sounds, handle);
      if (used[index] != 0 && !encoded_audio.empty())
        files.push_back({static_cast<SoundIndex>(index),
                         std::string(sounds::path(modules.sounds, handle)),
                         {encoded_audio.begin(), encoded_audio.end()}});
    }
  }

  log(ARX_LOG_INFO, "=== {} conversion completed: {} track(s) ===", kDirection, modules.ambiance.tracks.size());
  out = std::move(encoded);
  if (sound_files != nullptr) *sound_files = std::move(files);
  return ARX_OK;
}

}  // namespace

GlbResult<Ambiance> Ambiance::importGlb(std::span<const std::uint8_t> data) noexcept {
  return importGlb(data, GlbImportOptions{}, nullptr);
}

GlbResult<Ambiance> Ambiance::importGlb(std::span<const std::uint8_t> data, const GlbImportOptions& options,
                                        std::vector<SoundSourceReference>* sound_sources) noexcept {
  constexpr std::string_view kOperation = "GLB -> Ambiance conversion";
  return api_detail::glbBoundary(
      [&]() -> GlbResult<Ambiance> {
        constexpr std::string_view kDirection = "GLB -> Ambiance";
        logGlbStart(kDirection);
        Ambiance result;
        std::vector<SoundSourceReference> sources;
        glb::Failure failure;
        ArxReturnCode rc = importAmbianceFromGlb(
            data, options, static_cast<AmbianceModules&>(*result.data_), sound_sources ? &sources : nullptr, &failure);
        if (rc != ARX_OK) {
          return api_detail::glbFailure<Ambiance>(
              rc, std::move(failure.location), std::move(failure.detail), kOperation);
        }
        log(ARX_LOG_INFO,
            "=== {} conversion completed: {} track(s) ===",
            kDirection,
            result.data_->ambiance.tracks.size());
        if (sound_sources) *sound_sources = std::move(sources);
        return GlbResult<Ambiance>::success(std::move(result));
      },
      kOperation);
}

AmbianceGlbExportResult<std::vector<std::uint8_t>> Ambiance::exportGlb() const noexcept {
  return exportGlb(GlbExportOptions{});
}

AmbianceGlbExportResult<std::vector<std::uint8_t>> Ambiance::exportGlb(const GlbExportOptions& options) const noexcept {
  return exportGlb(options, nullptr);
}

AmbianceGlbExportResult<std::vector<std::uint8_t>> Ambiance::exportGlb(const GlbExportOptions& options,
                                                                       const Model* reference_model) const noexcept {
  if (!data_)
    return api_detail::ambianceGlbExportFailure<std::vector<std::uint8_t>>(
        ARX_INVALID_STATE,
        AmbianceGlbExportLocation{api_detail::resourceLocation(resourcePath(), AmbianceElement::kResource)});
  return api_detail::ambianceGlbExportBoundary(
      AmbianceGlbExportLocation{api_detail::resourceLocation(resourcePath(), AmbianceElement::kResource)},
      [&]() -> AmbianceGlbExportResult<std::vector<std::uint8_t>> {
        const ModelModules* reference_modules = nullptr;
        if (reference_model != nullptr) {
          auto validation = reference_model->validate();
          if (!validation)
            return api_detail::remapFailure<std::vector<std::uint8_t>, AmbianceGlbExportLocation>(
                std::move(validation),
                [](const ModelLocation& location) { return AmbianceGlbExportLocation{location}; });
          reference_modules = &static_cast<const ModelModules&>(*reference_model->data_);
        }
        auto validation = validate();
        if (!validation)
          return api_detail::remapFailure<std::vector<std::uint8_t>, AmbianceGlbExportLocation>(
              std::move(validation),
              [](const AmbianceLocation& location) { return AmbianceGlbExportLocation{location}; });
        std::vector<std::uint8_t> out;
        const ArxReturnCode rc =
            exportGlbInternal(static_cast<const AmbianceModules&>(*data_), options, reference_modules, out, nullptr);
        if (rc != ARX_OK)
          return api_detail::ambianceGlbExportFailure<std::vector<std::uint8_t>>(
              rc, AmbianceGlbExportLocation{api_detail::resourceLocation(resourcePath(), AmbianceElement::kResource)});
        return AmbianceGlbExportResult<std::vector<std::uint8_t>>::success(std::move(out));
      });
}

AmbianceGlbExportResult<AmbianceGlbBundle> Ambiance::exportGlbBundle(const GlbExportOptions& options,
                                                                     const Model* reference_model) const noexcept {
  if (!data_)
    return api_detail::ambianceGlbExportFailure<AmbianceGlbBundle>(
        ARX_INVALID_STATE,
        AmbianceGlbExportLocation{api_detail::resourceLocation(resourcePath(), AmbianceElement::kResource)});
  return api_detail::ambianceGlbExportBoundary(
      AmbianceGlbExportLocation{api_detail::resourceLocation(resourcePath(), AmbianceElement::kResource)},
      [&]() -> AmbianceGlbExportResult<AmbianceGlbBundle> {
        AmbianceGlbBundle result;
        const ModelModules* reference_modules = nullptr;
        if (reference_model != nullptr) {
          auto validation = reference_model->validate();
          if (!validation)
            return api_detail::remapFailure<AmbianceGlbBundle, AmbianceGlbExportLocation>(
                std::move(validation),
                [](const ModelLocation& location) { return AmbianceGlbExportLocation{location}; });
          reference_modules = &static_cast<const ModelModules&>(*reference_model->data_);
        }
        auto validation = validate();
        if (!validation)
          return api_detail::remapFailure<AmbianceGlbBundle, AmbianceGlbExportLocation>(
              std::move(validation),
              [](const AmbianceLocation& location) { return AmbianceGlbExportLocation{location}; });
        const ArxReturnCode rc = exportGlbInternal(
            static_cast<const AmbianceModules&>(*data_), options, reference_modules, result.glb, &result.sound_files);
        if (rc != ARX_OK)
          return api_detail::ambianceGlbExportFailure<AmbianceGlbBundle>(
              rc, AmbianceGlbExportLocation{api_detail::resourceLocation(resourcePath(), AmbianceElement::kResource)});
        return AmbianceGlbExportResult<AmbianceGlbBundle>::success(std::move(result));
      });
}

}  // namespace pistoris
