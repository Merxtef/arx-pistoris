// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#include "arx_pistoris/ambiance.h"
#include "arx_pistoris/ambiance.hpp"
#include "arx_pistoris/ambiance/bake.hpp"
#include "arx_pistoris/base/error.h"
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
                                               ArxSoundSourceReferences** out_sound_sources, ArxError* error) noexcept {
  if (!data || !out_ambiance) return pistoris::c_api::publishCode(ARX_INVALID_DATA_POINTER, error);
  *out_ambiance = nullptr;
  if (out_sound_sources) *out_sound_sources = nullptr;
  return pistoris::c_api::guard(error, [&]() -> ArxReturnCode {
    auto result = std::make_unique<ArxAmbiance>();
    std::unique_ptr<ArxSoundSourceReferences> sources;
    if (out_sound_sources) sources = std::make_unique<ArxSoundSourceReferences>();
    pistoris::Ambiance::GlbImportOptions cpp_options;
    if (options) cpp_options.arx_units_per_glb_unit = options->arx_units_per_glb_unit;
    auto imported = pistoris::Ambiance::importGlb(
        std::span<const std::uint8_t>(data, size), cpp_options, sources ? &sources->value : nullptr);
    if (!imported) return pistoris::c_api::publish(imported, error);
    result->value = std::move(*imported);
    *out_ambiance = result.release();
    if (out_sound_sources) *out_sound_sources = sources.release();
    return pistoris::c_api::publishCode(ARX_OK, error);
  });
}

ArxReturnCode arx_pistoris_ambiance_export_glb(const ArxAmbiance* ambiance, const ArxAmbianceGlbExportOptions* options,
                                               const ArxModel* reference_model, uint8_t** out_data, size_t* out_size,
                                               ArxSoundFiles** out_sounds, ArxError* error) noexcept {
  if (!ambiance) return pistoris::c_api::publishCode(ARX_INVALID_HANDLE, error);
  if (!out_data || !out_size) return pistoris::c_api::publishCode(ARX_INVALID_DATA_POINTER, error);
  *out_data = nullptr;
  *out_size = 0;
  if (out_sounds) *out_sounds = nullptr;
  return pistoris::c_api::guard(error, [&]() -> ArxReturnCode {
    pistoris::Ambiance::GlbExportOptions cpp_options;
    if (options) cpp_options.arx_units_per_glb_unit = options->arx_units_per_glb_unit;
    const pistoris::Model* cpp_reference = reference_model != nullptr ? &reference_model->value : nullptr;
    if (out_sounds) {
      auto baked = ambiance->value.exportGlbBundle(cpp_options, cpp_reference);
      if (!baked) return pistoris::c_api::publish(baked, error);
      pistoris::AmbianceGlbBundle bundle = std::move(*baked);
      auto sounds = std::make_unique<ArxSoundFiles>();
      sounds->value = std::move(bundle.sound_files);
      const ArxReturnCode publish_rc = pistoris::c_api::publishBytes(std::move(bundle.glb), out_data, out_size);
      if (publish_rc != ARX_OK) return publish_rc;
      *out_sounds = sounds.release();
      return pistoris::c_api::publishCode(ARX_OK, error);
    }

    auto result = ambiance->value.exportGlb(cpp_options, cpp_reference);
    if (!result) return pistoris::c_api::publish(result, error);
    return pistoris::c_api::publishBytes(std::move(*result), out_data, out_size);
  });
}

// NOLINTEND(readability-identifier-naming)
