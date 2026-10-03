// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#include "arx_pistoris/ambiance.h"
#include "arx_pistoris/ambiance/types.h"
#include "arx_pistoris/base/error.h"
#include "arx_pistoris/base/indices.h"
#include "arx_pistoris/base/status.h"
#include "arx_pistoris/base/string_view.h"
#include "arx_pistoris/sound.h"

#include "api/c/ambiance/internal.h"  // IWYU pragma: keep
#include "api/c/internal.h"

#include <cstddef>

// NOLINTBEGIN(readability-identifier-naming)

ArxReturnCode arx_pistoris_ambiance_resource_path(const ArxAmbiance* ambiance, ArxStringView* out_path,
                                                  ArxError* error) noexcept {
  if (!ambiance) return pistoris::c_api::publishCode(ARX_INVALID_HANDLE, error);
  if (!out_path) return pistoris::c_api::publishCode(ARX_INVALID_DATA_POINTER, error);
  *out_path = pistoris::c_api::view(ambiance->value.resourcePath());
  return pistoris::c_api::publishCode(ARX_OK, error);
}

ArxReturnCode arx_pistoris_ambiance_track_count(const ArxAmbiance* ambiance, size_t* out_count,
                                                ArxError* error) noexcept {
  if (!ambiance) return pistoris::c_api::publishCode(ARX_INVALID_HANDLE, error);
  if (!out_count) return pistoris::c_api::publishCode(ARX_INVALID_DATA_POINTER, error);
  *out_count = ambiance->value.trackCount();
  return pistoris::c_api::publishCode(ARX_OK, error);
}

ArxReturnCode arx_pistoris_ambiance_sound_count(const ArxAmbiance* ambiance, size_t* out_count,
                                                ArxError* error) noexcept {
  if (!ambiance) return pistoris::c_api::publishCode(ARX_INVALID_HANDLE, error);
  if (!out_count) return pistoris::c_api::publishCode(ARX_INVALID_DATA_POINTER, error);
  *out_count = ambiance->value.soundCount();
  return pistoris::c_api::publishCode(ARX_OK, error);
}

ArxReturnCode arx_pistoris_ambiance_master_track(const ArxAmbiance* ambiance, ArxAmbianceTrackIndex* out_track,
                                                 ArxError* error) noexcept {
  if (!ambiance) return pistoris::c_api::publishCode(ARX_INVALID_HANDLE, error);
  if (!out_track) return pistoris::c_api::publishCode(ARX_INVALID_DATA_POINTER, error);
  *out_track = ambiance->value.masterTrack();
  return pistoris::c_api::publishCode(ARX_OK, error);
}

ArxReturnCode arx_pistoris_ambiance_copy_tracks(const ArxAmbiance* ambiance, size_t offset, size_t count,
                                                ArxAmbianceTrack* out_tracks, ArxError* error) noexcept {
  if (!ambiance) return pistoris::c_api::publishCode(ARX_INVALID_HANDLE, error);
  const pistoris::Ambiance::TracksView values = ambiance->value.tracks();
  if (offset > values.size() || count > values.size() - offset)
    return pistoris::c_api::publishCode(ARX_INDEX_OUT_OF_RANGE, error);
  if (count != 0 && !out_tracks) return pistoris::c_api::publishCode(ARX_INVALID_DATA_POINTER, error);
  for (std::size_t index = 0; index < count; ++index) out_tracks[index] = values[offset + index];
  return pistoris::c_api::publishCode(ARX_OK, error);
}

ArxReturnCode arx_pistoris_ambiance_copy_sound_views(const ArxAmbiance* ambiance, size_t offset, size_t count,
                                                     ArxSoundView* out_sounds, ArxError* error) noexcept {
  if (!ambiance) return pistoris::c_api::publishCode(ARX_INVALID_HANDLE, error);
  const pistoris::Ambiance::SoundsView values = ambiance->value.sounds();
  if (offset > values.size() || count > values.size() - offset)
    return pistoris::c_api::publishCode(ARX_INDEX_OUT_OF_RANGE, error);
  if (count != 0 && !out_sounds) return pistoris::c_api::publishCode(ARX_INVALID_DATA_POINTER, error);
  for (std::size_t index = 0; index < count; ++index) out_sounds[index] = values[offset + index];
  return pistoris::c_api::publishCode(ARX_OK, error);
}

ArxReturnCode arx_pistoris_ambiance_copy_panned_keys(const ArxAmbiance* ambiance, ArxAmbianceTrackIndex track,
                                                     size_t offset, size_t count, ArxAmbiancePannedKey* out_keys,
                                                     ArxError* error) noexcept {
  if (!ambiance) return pistoris::c_api::publishCode(ARX_INVALID_HANDLE, error);
  auto result = ambiance->value.pannedKeys(track);
  if (!result) return pistoris::c_api::publish(result, error);
  if (offset > result->size() || count > result->size() - offset)
    return pistoris::c_api::publishCode(ARX_INDEX_OUT_OF_RANGE, error);
  if (count != 0 && !out_keys) return pistoris::c_api::publishCode(ARX_INVALID_DATA_POINTER, error);
  for (std::size_t index = 0; index < count; ++index) out_keys[index] = (*result)[offset + index];
  return pistoris::c_api::publishCode(ARX_OK, error);
}

ArxReturnCode arx_pistoris_ambiance_copy_positioned_keys(const ArxAmbiance* ambiance, ArxAmbianceTrackIndex track,
                                                         size_t offset, size_t count,
                                                         ArxAmbiancePositionedKey* out_keys, ArxError* error) noexcept {
  if (!ambiance) return pistoris::c_api::publishCode(ARX_INVALID_HANDLE, error);
  auto result = ambiance->value.positionedKeys(track);
  if (!result) return pistoris::c_api::publish(result, error);
  if (offset > result->size() || count > result->size() - offset)
    return pistoris::c_api::publishCode(ARX_INDEX_OUT_OF_RANGE, error);
  if (count != 0 && !out_keys) return pistoris::c_api::publishCode(ARX_INVALID_DATA_POINTER, error);
  for (std::size_t index = 0; index < count; ++index) out_keys[index] = (*result)[offset + index];
  return pistoris::c_api::publishCode(ARX_OK, error);
}

// NOLINTEND(readability-identifier-naming)
