// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#include "arx_pistoris/ambiance.h"
#include "arx_pistoris/ambiance/types.h"
#include "arx_pistoris/base/error.h"
#include "arx_pistoris/base/indices.h"
#include "arx_pistoris/base/status.h"
#include "arx_pistoris/base/string_view.h"
#include "arx_pistoris/sound.h"

#include "api/c/ambiance/internal.h"
#include "api/c/internal.h"

#include <cstddef>

// NOLINTBEGIN(readability-identifier-naming)

ArxReturnCode arx_pistoris_ambiance_set_resource_path(ArxAmbiance* ambiance, ArxStringView path,
                                                      ArxError* error) noexcept {
  if (!ambiance) return pistoris::c_api::publishCode(ARX_INVALID_HANDLE, error);
  if (!pistoris::c_api::valid(path)) return pistoris::c_api::publishCode(ARX_INVALID_DATA_POINTER, error);
  return pistoris::c_api::publish(ambiance->value.setResourcePath(pistoris::c_api::stringView(path)), error);
}

ArxReturnCode arx_pistoris_ambiance_set_panned_track(ArxAmbiance* ambiance, ArxAmbianceTrackIndex index,
                                                     const ArxAmbiancePannedTrackInput* track,
                                                     ArxError* error) noexcept {
  if (!ambiance) return pistoris::c_api::publishCode(ARX_INVALID_HANDLE, error);
  if (!track || !pistoris::c_api::valid(*track)) return pistoris::c_api::publishCode(ARX_INVALID_DATA_POINTER, error);
  return pistoris::c_api::publish(ambiance->value.setPannedTrack(index, *track), error);
}

ArxReturnCode arx_pistoris_ambiance_add_panned_track(ArxAmbiance* ambiance, const ArxAmbiancePannedTrackInput* track,
                                                     ArxAmbianceTrackIndex* out_index, ArxError* error) noexcept {
  if (!ambiance) return pistoris::c_api::publishCode(ARX_INVALID_HANDLE, error);
  if (!track || !out_index || !pistoris::c_api::valid(*track))
    return pistoris::c_api::publishCode(ARX_INVALID_DATA_POINTER, error);
  *out_index = ARX_INVALID_INDEX;
  auto result = ambiance->value.addPannedTrack(*track);
  if (!result) return pistoris::c_api::publish(result, error);
  *out_index = *result;
  return pistoris::c_api::publishCode(ARX_OK, error);
}

ArxReturnCode arx_pistoris_ambiance_set_positioned_track(ArxAmbiance* ambiance, ArxAmbianceTrackIndex index,
                                                         const ArxAmbiancePositionedTrackInput* track,
                                                         ArxError* error) noexcept {
  if (!ambiance) return pistoris::c_api::publishCode(ARX_INVALID_HANDLE, error);
  if (!track || !pistoris::c_api::valid(*track)) return pistoris::c_api::publishCode(ARX_INVALID_DATA_POINTER, error);
  return pistoris::c_api::publish(ambiance->value.setPositionedTrack(index, *track), error);
}

ArxReturnCode arx_pistoris_ambiance_add_positioned_track(ArxAmbiance* ambiance,
                                                         const ArxAmbiancePositionedTrackInput* track,
                                                         ArxAmbianceTrackIndex* out_index, ArxError* error) noexcept {
  if (!ambiance) return pistoris::c_api::publishCode(ARX_INVALID_HANDLE, error);
  if (!track || !out_index || !pistoris::c_api::valid(*track))
    return pistoris::c_api::publishCode(ARX_INVALID_DATA_POINTER, error);
  *out_index = ARX_INVALID_INDEX;
  auto result = ambiance->value.addPositionedTrack(*track);
  if (!result) return pistoris::c_api::publish(result, error);
  *out_index = *result;
  return pistoris::c_api::publishCode(ARX_OK, error);
}

ArxReturnCode arx_pistoris_ambiance_remove_track(ArxAmbiance* ambiance, ArxAmbianceTrackIndex index,
                                                 ArxError* error) noexcept {
  if (!ambiance) return pistoris::c_api::publishCode(ARX_INVALID_HANDLE, error);
  return pistoris::c_api::publish(ambiance->value.removeTrack(index), error);
}

ArxReturnCode arx_pistoris_ambiance_clear_tracks(ArxAmbiance* ambiance, ArxError* error) noexcept {
  if (!ambiance) return pistoris::c_api::publishCode(ARX_INVALID_HANDLE, error);
  ambiance->value.clearTracks();
  return pistoris::c_api::publishCode(ARX_OK, error);
}

ArxReturnCode arx_pistoris_ambiance_set_master_track(ArxAmbiance* ambiance, ArxAmbianceTrackIndex index,
                                                     ArxError* error) noexcept {
  if (!ambiance) return pistoris::c_api::publishCode(ARX_INVALID_HANDLE, error);
  return pistoris::c_api::publish(ambiance->value.setMasterTrack(index), error);
}

ArxReturnCode arx_pistoris_ambiance_trim_tracks_to_master(ArxAmbiance* ambiance, size_t* out_trimmed_tracks,
                                                          ArxError* error) noexcept {
  if (!ambiance) return pistoris::c_api::publishCode(ARX_INVALID_HANDLE, error);
  auto result = ambiance->value.trimTracksToMaster();
  if (!result) return pistoris::c_api::publish(result, error);
  if (out_trimmed_tracks) *out_trimmed_tracks = *result;
  return pistoris::c_api::publishCode(ARX_OK, error);
}

ArxReturnCode arx_pistoris_ambiance_compact_sounds(ArxAmbiance* ambiance, size_t* out_removed,
                                                   ArxError* error) noexcept {
  if (!ambiance) return pistoris::c_api::publishCode(ARX_INVALID_HANDLE, error);
  auto result = ambiance->value.compactSounds();
  if (!result) return pistoris::c_api::publish(result, error);
  if (out_removed) *out_removed = *result;
  return pistoris::c_api::publishCode(ARX_OK, error);
}

ArxReturnCode arx_pistoris_ambiance_rebase_sound_paths(ArxAmbiance* ambiance, ArxStringView directory,
                                                       ArxError* error) noexcept {
  if (!ambiance) return pistoris::c_api::publishCode(ARX_INVALID_HANDLE, error);
  if (!pistoris::c_api::valid(directory)) return pistoris::c_api::publishCode(ARX_INVALID_DATA_POINTER, error);
  return pistoris::c_api::publish(ambiance->value.rebaseSoundPaths(pistoris::c_api::stringView(directory)), error);
}

ArxReturnCode arx_pistoris_ambiance_set_sound(ArxAmbiance* ambiance, ArxSoundIndex index, const ArxSoundView* sound,
                                              ArxError* error) noexcept {
  if (!ambiance) return pistoris::c_api::publishCode(ARX_INVALID_HANDLE, error);
  if (!sound || !pistoris::c_api::valid(sound->path) || !pistoris::c_api::valid(sound->encoded_audio))
    return pistoris::c_api::publishCode(ARX_INVALID_DATA_POINTER, error);
  return pistoris::c_api::publish(ambiance->value.setSound(index, *sound), error);
}

ArxReturnCode arx_pistoris_ambiance_add_sound(ArxAmbiance* ambiance, const ArxSoundView* sound,
                                              ArxSoundIndex* out_index, ArxError* error) noexcept {
  if (!ambiance) return pistoris::c_api::publishCode(ARX_INVALID_HANDLE, error);
  if (!sound || !out_index || !pistoris::c_api::valid(sound->path) || !pistoris::c_api::valid(sound->encoded_audio))
    return pistoris::c_api::publishCode(ARX_INVALID_DATA_POINTER, error);
  *out_index = ARX_NO_SOUND;
  auto result = ambiance->value.addSound(*sound);
  if (!result) return pistoris::c_api::publish(result, error);
  *out_index = *result;
  return pistoris::c_api::publishCode(ARX_OK, error);
}

ArxReturnCode arx_pistoris_ambiance_set_sound_path(ArxAmbiance* ambiance, ArxSoundIndex index, ArxStringView path,
                                                   ArxError* error) noexcept {
  if (!ambiance) return pistoris::c_api::publishCode(ARX_INVALID_HANDLE, error);
  if (!pistoris::c_api::valid(path)) return pistoris::c_api::publishCode(ARX_INVALID_DATA_POINTER, error);
  return pistoris::c_api::publish(ambiance->value.setSoundPath(index, pistoris::c_api::stringView(path)), error);
}

ArxReturnCode arx_pistoris_ambiance_set_sound_data(ArxAmbiance* ambiance, ArxSoundIndex index,
                                                   ArxEncodedAudioView encoded_audio, ArxError* error) noexcept {
  if (!ambiance) return pistoris::c_api::publishCode(ARX_INVALID_HANDLE, error);
  if (!pistoris::c_api::valid(encoded_audio)) return pistoris::c_api::publishCode(ARX_INVALID_DATA_POINTER, error);
  return pistoris::c_api::publish(ambiance->value.setSoundData(index, encoded_audio), error);
}

ArxReturnCode arx_pistoris_ambiance_clear_sound_data(ArxAmbiance* ambiance, ArxSoundIndex index,
                                                     ArxError* error) noexcept {
  if (!ambiance) return pistoris::c_api::publishCode(ARX_INVALID_HANDLE, error);
  return pistoris::c_api::publish(ambiance->value.clearSoundData(index), error);
}

ArxReturnCode arx_pistoris_ambiance_remove_sound(ArxAmbiance* ambiance, ArxSoundIndex index, ArxError* error) noexcept {
  if (!ambiance) return pistoris::c_api::publishCode(ARX_INVALID_HANDLE, error);
  return pistoris::c_api::publish(ambiance->value.removeSound(index), error);
}

// NOLINTEND(readability-identifier-naming)
