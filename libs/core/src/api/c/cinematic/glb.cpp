// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#include "arx_pistoris/cinematic/glb.hpp"

#include "arx_pistoris/base/error.h"
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
                                                ArxCinematicSoundSourceReferences** out_sound_sources,
                                                ArxError* error) noexcept {
  if (!data || !out_cinematic) return pistoris::c_api::publishCode(ARX_INVALID_DATA_POINTER, error);
  *out_cinematic = nullptr;
  if (out_sound_sources) *out_sound_sources = nullptr;
  return pistoris::c_api::guard(error, [&]() -> ArxReturnCode {
    auto result = std::make_unique<ArxCinematic>();
    std::unique_ptr<ArxCinematicSoundSourceReferences> sources;
    if (out_sound_sources) sources = std::make_unique<ArxCinematicSoundSourceReferences>();
    auto imported =
        pistoris::Cinematic::importGlb(std::span<const std::uint8_t>(data, size), sources ? &sources->value : nullptr);
    if (!imported) return pistoris::c_api::publish(imported, error);
    result->value = std::move(*imported);
    *out_cinematic = result.release();
    if (out_sound_sources) *out_sound_sources = sources.release();
    return pistoris::c_api::publishCode(ARX_OK, error);
  });
}

ArxReturnCode arx_pistoris_cinematic_export_glb(const ArxCinematic* cinematic, uint8_t** out_data, size_t* out_size,
                                                ArxCinematicSoundFiles** out_sounds, ArxError* error) noexcept {
  if (!cinematic) return pistoris::c_api::publishCode(ARX_INVALID_HANDLE, error);
  if (!out_data || !out_size) return pistoris::c_api::publishCode(ARX_INVALID_DATA_POINTER, error);
  *out_data = nullptr;
  *out_size = 0;
  if (out_sounds) *out_sounds = nullptr;
  return pistoris::c_api::guard(error, [&]() -> ArxReturnCode {
    if (out_sounds) {
      auto exported = cinematic->value.exportGlbBundle();
      if (!exported) return pistoris::c_api::publish(exported, error);
      pistoris::CinematicGlbBundle bundle = std::move(*exported);
      auto sounds = std::make_unique<ArxCinematicSoundFiles>();
      sounds->value = std::move(bundle.sound_files);
      const ArxReturnCode publish_rc = pistoris::c_api::publishBytes(std::move(bundle.glb), out_data, out_size);
      if (publish_rc != ARX_OK) return publish_rc;
      *out_sounds = sounds.release();
      return pistoris::c_api::publishCode(ARX_OK, error);
    }

    auto exported = cinematic->value.exportGlb();
    if (!exported) return pistoris::c_api::publish(exported, error);
    return pistoris::c_api::publishBytes(std::move(*exported), out_data, out_size);
  });
}

// NOLINTEND(readability-identifier-naming)
