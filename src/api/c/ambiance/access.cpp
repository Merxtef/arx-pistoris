// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#include "arx_pistoris/ambiance.h"
#include "arx_pistoris/ambiance/types.h"
#include "arx_pistoris/base/indices.h"
#include "arx_pistoris/base/status.h"
#include "arx_pistoris/base/string_view.h"
#include "arx_pistoris/sound.h"

#include "api/c/ambiance/internal.h"  // IWYU pragma: keep
#include "api/c/internal.h"

#include <cstddef>

// NOLINTBEGIN(readability-identifier-naming)

ArxReturnCode arx_pistoris_ambiance_resource_path(const ArxAmbiance* ambiance, ArxStringView* out_path) noexcept {
  if (!ambiance) return ARX_INVALID_HANDLE;
  if (!out_path) return ARX_INVALID_DATA_POINTER;
  *out_path = pistoris::c_api::view(ambiance->value.resourcePath());
  return ARX_OK;
}

ArxReturnCode arx_pistoris_ambiance_track_count(const ArxAmbiance* ambiance, size_t* out_count) noexcept {
  if (!ambiance) return ARX_INVALID_HANDLE;
  if (!out_count) return ARX_INVALID_DATA_POINTER;
  *out_count = ambiance->value.trackCount();
  return ARX_OK;
}

ArxReturnCode arx_pistoris_ambiance_sound_count(const ArxAmbiance* ambiance, size_t* out_count) noexcept {
  if (!ambiance) return ARX_INVALID_HANDLE;
  if (!out_count) return ARX_INVALID_DATA_POINTER;
  *out_count = ambiance->value.soundCount();
  return ARX_OK;
}

ArxReturnCode arx_pistoris_ambiance_master_track(const ArxAmbiance* ambiance,
                                                 ArxAmbianceTrackIndex* out_track) noexcept {
  if (!ambiance) return ARX_INVALID_HANDLE;
  if (!out_track) return ARX_INVALID_DATA_POINTER;
  *out_track = ambiance->value.masterTrack();
  return ARX_OK;
}

ArxReturnCode arx_pistoris_ambiance_copy_tracks(const ArxAmbiance* ambiance, size_t offset, size_t count,
                                                ArxAmbianceTrack* out_tracks) noexcept {
  if (!ambiance) return ARX_INVALID_HANDLE;
  return pistoris::c_api::guard([&] { return ambiance->value.copyTracks(offset, count, out_tracks); });
}

ArxReturnCode arx_pistoris_ambiance_copy_sound_views(const ArxAmbiance* ambiance, size_t offset, size_t count,
                                                     ArxSoundView* out_sounds) noexcept {
  if (!ambiance) return ARX_INVALID_HANDLE;
  return pistoris::c_api::guard([&] { return ambiance->value.copySoundViews(offset, count, out_sounds); });
}

ArxReturnCode arx_pistoris_ambiance_copy_panned_keys(const ArxAmbiance* ambiance, ArxAmbianceTrackIndex track,
                                                     size_t offset, size_t count,
                                                     ArxAmbiancePannedKey* out_keys) noexcept {
  if (!ambiance) return ARX_INVALID_HANDLE;
  return pistoris::c_api::guard([&] { return ambiance->value.copyPannedKeys(track, offset, count, out_keys); });
}

ArxReturnCode arx_pistoris_ambiance_copy_positioned_keys(const ArxAmbiance* ambiance, ArxAmbianceTrackIndex track,
                                                         size_t offset, size_t count,
                                                         ArxAmbiancePositionedKey* out_keys) noexcept {
  if (!ambiance) return ARX_INVALID_HANDLE;
  return pistoris::c_api::guard([&] { return ambiance->value.copyPositionedKeys(track, offset, count, out_keys); });
}

// NOLINTEND(readability-identifier-naming)
