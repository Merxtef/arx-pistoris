// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#include "arx_pistoris/ambiance.h"
#include "arx_pistoris/ambiance/types.h"
#include "arx_pistoris/base/indices.h"
#include "arx_pistoris/base/status.h"
#include "arx_pistoris/base/string_view.h"
#include "arx_pistoris/sound.h"

#include "api/c/ambiance/internal.h"
#include "api/c/internal.h"

#include <cstddef>

// NOLINTBEGIN(readability-identifier-naming)

ArxReturnCode arx_pistoris_ambiance_set_resource_path(ArxAmbiance* ambiance, ArxStringView path) noexcept {
  if (!ambiance) return ARX_INVALID_HANDLE;
  if (!pistoris::c_api::valid(path)) return ARX_INVALID_DATA_POINTER;
  return pistoris::c_api::guard([&] { return ambiance->value.setResourcePath(pistoris::c_api::stringView(path)); });
}

ArxReturnCode arx_pistoris_ambiance_set_panned_track(ArxAmbiance* ambiance, ArxAmbianceTrackIndex index,
                                                     const ArxAmbiancePannedTrackInput* track) noexcept {
  if (!ambiance) return ARX_INVALID_HANDLE;
  if (!track || !pistoris::c_api::valid(*track)) return ARX_INVALID_DATA_POINTER;
  return pistoris::c_api::guard([&] { return ambiance->value.setPannedTrack(index, *track); });
}

ArxReturnCode arx_pistoris_ambiance_add_panned_track(ArxAmbiance* ambiance, const ArxAmbiancePannedTrackInput* track,
                                                     ArxAmbianceTrackIndex* out_index) noexcept {
  if (!ambiance) return ARX_INVALID_HANDLE;
  if (!track || !out_index || !pistoris::c_api::valid(*track)) return ARX_INVALID_DATA_POINTER;
  *out_index = ARX_INVALID_INDEX;
  return pistoris::c_api::guard([&] { return ambiance->value.addPannedTrack(*track, *out_index); });
}

ArxReturnCode arx_pistoris_ambiance_set_positioned_track(ArxAmbiance* ambiance, ArxAmbianceTrackIndex index,
                                                         const ArxAmbiancePositionedTrackInput* track) noexcept {
  if (!ambiance) return ARX_INVALID_HANDLE;
  if (!track || !pistoris::c_api::valid(*track)) return ARX_INVALID_DATA_POINTER;
  return pistoris::c_api::guard([&] { return ambiance->value.setPositionedTrack(index, *track); });
}

ArxReturnCode arx_pistoris_ambiance_add_positioned_track(ArxAmbiance* ambiance,
                                                         const ArxAmbiancePositionedTrackInput* track,
                                                         ArxAmbianceTrackIndex* out_index) noexcept {
  if (!ambiance) return ARX_INVALID_HANDLE;
  if (!track || !out_index || !pistoris::c_api::valid(*track)) return ARX_INVALID_DATA_POINTER;
  *out_index = ARX_INVALID_INDEX;
  return pistoris::c_api::guard([&] { return ambiance->value.addPositionedTrack(*track, *out_index); });
}

ArxReturnCode arx_pistoris_ambiance_remove_track(ArxAmbiance* ambiance, ArxAmbianceTrackIndex index) noexcept {
  if (!ambiance) return ARX_INVALID_HANDLE;
  return pistoris::c_api::guard([&] { return ambiance->value.removeTrack(index); });
}

ArxReturnCode arx_pistoris_ambiance_clear_tracks(ArxAmbiance* ambiance) noexcept {
  if (!ambiance) return ARX_INVALID_HANDLE;
  ambiance->value.clearTracks();
  return ARX_OK;
}

ArxReturnCode arx_pistoris_ambiance_set_master_track(ArxAmbiance* ambiance, ArxAmbianceTrackIndex index) noexcept {
  if (!ambiance) return ARX_INVALID_HANDLE;
  return pistoris::c_api::guard([&] { return ambiance->value.setMasterTrack(index); });
}

ArxReturnCode arx_pistoris_ambiance_trim_tracks_to_master(ArxAmbiance* ambiance, size_t* out_trimmed_tracks) noexcept {
  if (!ambiance) return ARX_INVALID_HANDLE;
  return pistoris::c_api::guard([&] { return ambiance->value.trimTracksToMaster(out_trimmed_tracks); });
}

ArxReturnCode arx_pistoris_ambiance_compact_sounds(ArxAmbiance* ambiance, size_t* out_removed) noexcept {
  if (!ambiance) return ARX_INVALID_HANDLE;
  return pistoris::c_api::guard([&] { return ambiance->value.compactSounds(out_removed); });
}

ArxReturnCode arx_pistoris_ambiance_rebase_sound_paths(ArxAmbiance* ambiance, ArxStringView directory) noexcept {
  if (!ambiance) return ARX_INVALID_HANDLE;
  if (!pistoris::c_api::valid(directory)) return ARX_INVALID_DATA_POINTER;
  return pistoris::c_api::guard(
      [&] { return ambiance->value.rebaseSoundPaths(pistoris::c_api::stringView(directory)); });
}

ArxReturnCode arx_pistoris_ambiance_set_sound(ArxAmbiance* ambiance, ArxSoundIndex index,
                                              const ArxSoundView* sound) noexcept {
  if (!ambiance) return ARX_INVALID_HANDLE;
  if (!sound || !pistoris::c_api::valid(sound->path) || !pistoris::c_api::valid(sound->encoded_audio))
    return ARX_INVALID_DATA_POINTER;
  return pistoris::c_api::guard([&] { return ambiance->value.setSound(index, *sound); });
}

ArxReturnCode arx_pistoris_ambiance_add_sound(ArxAmbiance* ambiance, const ArxSoundView* sound,
                                              ArxSoundIndex* out_index) noexcept {
  if (!ambiance) return ARX_INVALID_HANDLE;
  if (!sound || !out_index || !pistoris::c_api::valid(sound->path) || !pistoris::c_api::valid(sound->encoded_audio))
    return ARX_INVALID_DATA_POINTER;
  *out_index = ARX_NO_SOUND;
  return pistoris::c_api::guard([&] { return ambiance->value.addSound(*sound, *out_index); });
}

ArxReturnCode arx_pistoris_ambiance_set_sound_data(ArxAmbiance* ambiance, ArxSoundIndex index,
                                                   ArxEncodedAudioView encoded_audio) noexcept {
  if (!ambiance) return ARX_INVALID_HANDLE;
  if (!pistoris::c_api::valid(encoded_audio)) return ARX_INVALID_DATA_POINTER;
  return pistoris::c_api::guard([&] { return ambiance->value.setSoundData(index, encoded_audio); });
}

ArxReturnCode arx_pistoris_ambiance_clear_sound_data(ArxAmbiance* ambiance, ArxSoundIndex index) noexcept {
  if (!ambiance) return ARX_INVALID_HANDLE;
  return ambiance->value.clearSoundData(index);
}

ArxReturnCode arx_pistoris_ambiance_remove_sound(ArxAmbiance* ambiance, ArxSoundIndex index) noexcept {
  if (!ambiance) return ARX_INVALID_HANDLE;
  return ambiance->value.removeSound(index);
}

// NOLINTEND(readability-identifier-naming)
