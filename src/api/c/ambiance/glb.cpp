// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#include "arx_pistoris/ambiance.h"
#include "arx_pistoris/ambiance.hpp"
#include "arx_pistoris/ambiance/bake.hpp"
#include "arx_pistoris/base/status.h"
#include "arx_pistoris/sound.h"

#include "api/c/ambiance/internal.h"
#include "api/c/internal.h"
#include "api/c/model/internal.h"  // IWYU pragma: keep
#include "api/c/sound/internal.h"

#include <cstddef>
#include <cstdint>
#include <memory>
#include <span>
#include <utility>
#include <vector>

// NOLINTBEGIN(readability-identifier-naming)

ArxReturnCode arx_pistoris_ambiance_import_glb(const uint8_t* data, size_t size,
                                               const ArxAmbianceGlbImportOptions* options, ArxAmbiance** out_ambiance,
                                               ArxSoundSourceReferences** out_sound_sources) noexcept {
  if (!data || !out_ambiance) return ARX_INVALID_DATA_POINTER;
  *out_ambiance = nullptr;
  if (out_sound_sources) *out_sound_sources = nullptr;
  return pistoris::c_api::guard([&]() -> ArxReturnCode {
    auto result = std::make_unique<ArxAmbiance>();
    std::unique_ptr<ArxSoundSourceReferences> sources;
    if (out_sound_sources) sources = std::make_unique<ArxSoundSourceReferences>();
    pistoris::Ambiance::GlbImportOptions cpp_options;
    if (options) cpp_options.arx_units_per_glb_unit = options->arx_units_per_glb_unit;
    const ArxReturnCode rc = pistoris::Ambiance::importGlb(
        result->value, std::span<const std::uint8_t>(data, size), cpp_options, sources ? &sources->value : nullptr);
    if (rc != ARX_OK) return rc;
    *out_ambiance = result.release();
    if (out_sound_sources) *out_sound_sources = sources.release();
    return ARX_OK;
  });
}

ArxReturnCode arx_pistoris_ambiance_export_glb(const ArxAmbiance* ambiance, const ArxAmbianceGlbExportOptions* options,
                                               const ArxModel* reference_model, uint8_t** out_data, size_t* out_size,
                                               ArxSoundFiles** out_sounds) noexcept {
  if (!ambiance) return ARX_INVALID_HANDLE;
  if (!out_data || !out_size) return ARX_INVALID_DATA_POINTER;
  *out_data = nullptr;
  *out_size = 0;
  if (out_sounds) *out_sounds = nullptr;
  return pistoris::c_api::guard([&]() -> ArxReturnCode {
    pistoris::Ambiance::GlbExportOptions cpp_options;
    if (options) cpp_options.arx_units_per_glb_unit = options->arx_units_per_glb_unit;
    const pistoris::Model* cpp_reference = reference_model != nullptr ? &reference_model->value : nullptr;
    if (out_sounds) {
      pistoris::AmbianceGlbBundle bundle;
      const ArxReturnCode rc = ambiance->value.exportGlbBundle(cpp_options, cpp_reference, bundle);
      if (rc != ARX_OK) return rc;
      auto sounds = std::make_unique<ArxSoundFiles>();
      sounds->value = std::move(bundle.sound_files);
      const ArxReturnCode publish_rc = pistoris::c_api::publishBytes(std::move(bundle.glb), out_data, out_size);
      if (publish_rc != ARX_OK) return publish_rc;
      *out_sounds = sounds.release();
      return ARX_OK;
    }

    std::vector<std::uint8_t> result;
    const ArxReturnCode rc = ambiance->value.exportGlb(result, cpp_options, cpp_reference);
    if (rc != ARX_OK) return rc;
    return pistoris::c_api::publishBytes(std::move(result), out_data, out_size);
  });
}

// NOLINTEND(readability-identifier-naming)
