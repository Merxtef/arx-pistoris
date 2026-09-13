// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#include "arx_pistoris/ambiance.hpp"
#include "arx_pistoris/ambiance/bake.hpp"
#include "arx_pistoris/base/indices.h"
#include "arx_pistoris/base/status.h"
#include "arx_pistoris/model.hpp"
#include "arx_pistoris/runtime/types.h"
#include "arx_pistoris/sound.hpp"

#include "ambiance/data.h"
#include "api/status_boundary.h"
#include "external/glb/ambiance/api.h"
#include "model/data.h"
#include "modules/ambiance.h"
#include "modules/sounds.h"
#include "utils/log.h"

#include <cstddef>
#include <cstdint>
#include <span>
#include <string_view>
#include <utility>
#include <vector>

namespace pistoris {
namespace {

void logGlbStart(std::string_view direction) { log(ARX_LOG_INFO, "=== {} conversion started ===", direction); }

ArxReturnCode logGlbFailure(std::string_view direction, ArxReturnCode rc) {
  log(ARX_LOG_INFO, "=== {} conversion failed with code {} ===", direction, rc);
  return rc;
}

ArxReturnCode exportGlbInternal(const Ambiance& ambiance, const AmbianceModules& modules,
                                const Ambiance::GlbExportOptions& options, const Model* reference_model,
                                const ModelModules* reference_modules, std::vector<std::uint8_t>& out,
                                std::vector<SoundFile>* sound_files) {
  constexpr std::string_view kDirection = "Ambiance -> GLB";
  logGlbStart(kDirection);
  ArxReturnCode rc = ambiance.validate();
  if (rc != ARX_OK) return logGlbFailure(kDirection, rc);
  if (reference_model != nullptr) {
    rc = reference_model->validate();
    if (rc != ARX_OK) return logGlbFailure(kDirection, rc);
  }

  std::vector<std::uint8_t> encoded;
  rc = exportAmbianceToGlb(modules, options, reference_modules, encoded);
  if (rc != ARX_OK) return logGlbFailure(kDirection, rc);

  std::vector<SoundFile> files;
  if (sound_files != nullptr) {
    std::vector<std::uint8_t> used(modules.sounds.sounds.size(), 0);
    for (const AmbianceTrack& track : modules.ambiance.tracks) used[track.sound] = 1;
    files.reserve(modules.sounds.sounds.size());
    for (std::size_t index = 0; index < modules.sounds.sounds.size(); ++index) {
      const Sound& sound = modules.sounds.sounds[index];
      if (used[index] != 0 && !sound.encoded_audio.empty())
        files.push_back({static_cast<SoundIndex>(index), sound.path, sound.encoded_audio});
    }
  }

  log(ARX_LOG_INFO, "=== {} conversion completed: {} track(s) ===", kDirection, modules.ambiance.tracks.size());
  out = std::move(encoded);
  if (sound_files != nullptr) *sound_files = std::move(files);
  return ARX_OK;
}

}  // namespace

ArxReturnCode Ambiance::importGlb(Ambiance& out, std::span<const std::uint8_t> data) noexcept {
  return importGlb(out, data, GlbImportOptions{}, nullptr);
}

ArxReturnCode Ambiance::importGlb(Ambiance& out, std::span<const std::uint8_t> data, const GlbImportOptions& options,
                                  std::vector<SoundSourceReference>* sound_sources) noexcept {
  return api_detail::statusBoundary([&]() -> ArxReturnCode {
    constexpr std::string_view kDirection = "GLB -> Ambiance";
    logGlbStart(kDirection);
    Ambiance result;
    std::vector<SoundSourceReference> sources;
    ArxReturnCode rc = importAmbianceFromGlb(
        data, options, static_cast<AmbianceModules&>(*result.data_), sound_sources ? &sources : nullptr);
    if (rc != ARX_OK) return logGlbFailure(kDirection, rc);
    log(ARX_LOG_INFO, "=== {} conversion completed: {} track(s) ===", kDirection, result.data_->ambiance.tracks.size());
    out.swap(result);
    if (sound_sources) *sound_sources = std::move(sources);
    return ARX_OK;
  });
}

ArxReturnCode Ambiance::exportGlb(std::vector<std::uint8_t>& out) const noexcept {
  return exportGlb(out, GlbExportOptions{});
}

ArxReturnCode Ambiance::exportGlb(std::vector<std::uint8_t>& out, const GlbExportOptions& options) const noexcept {
  return exportGlb(out, options, nullptr);
}

ArxReturnCode Ambiance::exportGlb(std::vector<std::uint8_t>& out, const GlbExportOptions& options,
                                  const Model* reference_model) const noexcept {
  return api_detail::statusBoundary([&]() -> ArxReturnCode {
    const ModelModules* reference_modules =
        reference_model != nullptr ? &static_cast<const ModelModules&>(*reference_model->data_) : nullptr;
    return exportGlbInternal(
        *this, static_cast<const AmbianceModules&>(*data_), options, reference_model, reference_modules, out, nullptr);
  });
}

ArxReturnCode Ambiance::exportGlbBundle(const GlbExportOptions& options, const Model* reference_model,
                                        AmbianceGlbBundle& out) const noexcept {
  return api_detail::statusBoundary([&]() -> ArxReturnCode {
    AmbianceGlbBundle result;
    const ModelModules* reference_modules =
        reference_model != nullptr ? &static_cast<const ModelModules&>(*reference_model->data_) : nullptr;
    const ArxReturnCode rc = exportGlbInternal(*this,
                                               static_cast<const AmbianceModules&>(*data_),
                                               options,
                                               reference_model,
                                               reference_modules,
                                               result.glb,
                                               &result.sound_files);
    if (rc != ARX_OK) return rc;
    out = std::move(result);
    return ARX_OK;
  });
}

}  // namespace pistoris
