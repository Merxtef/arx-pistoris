// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#include "arx_pistoris/cinematic/glb.hpp"

#include "arx_pistoris/base/status.h"
#include "arx_pistoris/cinematic.h"
#include "arx_pistoris/cinematic.hpp"

#include "api/c/cinematic/internal.h"
#include "api/c/internal.h"

#include <cstddef>
#include <cstdint>
#include <memory>
#include <span>
#include <utility>
#include <vector>

// NOLINTBEGIN(readability-identifier-naming)

ArxReturnCode arx_pistoris_cinematic_import_glb(const uint8_t* data, size_t size, ArxCinematic** out_cinematic,
                                                ArxCinematicSoundSourceReferences** out_sound_sources) noexcept {
  if (!data || !out_cinematic) return ARX_INVALID_DATA_POINTER;
  *out_cinematic = nullptr;
  if (out_sound_sources) *out_sound_sources = nullptr;
  return pistoris::c_api::guard([&]() -> ArxReturnCode {
    auto result = std::make_unique<ArxCinematic>();
    std::unique_ptr<ArxCinematicSoundSourceReferences> sources;
    if (out_sound_sources) sources = std::make_unique<ArxCinematicSoundSourceReferences>();
    const ArxReturnCode rc = pistoris::Cinematic::importGlb(
        result->value, std::span<const std::uint8_t>(data, size), sources ? &sources->value : nullptr);
    if (rc != ARX_OK) return rc;
    *out_cinematic = result.release();
    if (out_sound_sources) *out_sound_sources = sources.release();
    return ARX_OK;
  });
}

ArxReturnCode arx_pistoris_cinematic_export_glb(const ArxCinematic* cinematic, uint8_t** out_data, size_t* out_size,
                                                ArxCinematicSoundFiles** out_sounds) noexcept {
  if (!cinematic) return ARX_INVALID_HANDLE;
  if (!out_data || !out_size) return ARX_INVALID_DATA_POINTER;
  *out_data = nullptr;
  *out_size = 0;
  if (out_sounds) *out_sounds = nullptr;
  return pistoris::c_api::guard([&]() -> ArxReturnCode {
    if (out_sounds) {
      pistoris::CinematicGlbBundle bundle;
      const ArxReturnCode rc = cinematic->value.exportGlbBundle(bundle);
      if (rc != ARX_OK) return rc;
      auto sounds = std::make_unique<ArxCinematicSoundFiles>();
      sounds->value = std::move(bundle.sound_files);
      const ArxReturnCode publish_rc = pistoris::c_api::publishBytes(std::move(bundle.glb), out_data, out_size);
      if (publish_rc != ARX_OK) return publish_rc;
      *out_sounds = sounds.release();
      return ARX_OK;
    }

    std::vector<std::uint8_t> result;
    const ArxReturnCode rc = cinematic->value.exportGlb(result);
    if (rc != ARX_OK) return rc;
    return pistoris::c_api::publishBytes(std::move(result), out_data, out_size);
  });
}

// NOLINTEND(readability-identifier-naming)
