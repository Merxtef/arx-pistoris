// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#ifndef ARX_PISTORIS_AMBIANCE_H
#define ARX_PISTORIS_AMBIANCE_H

#include "arx_pistoris/ambiance/types.h"
#include "arx_pistoris/base/abi.h"
#include "arx_pistoris/base/audio.h"
#include "arx_pistoris/base/status.h"
#include "arx_pistoris/base/string_view.h"

#include <stddef.h>
#include <stdint.h>

// Public C ABI naming
// NOLINTBEGIN(readability-identifier-naming)

typedef struct arx_pistoris_amb ArxAmb;
typedef struct arx_pistoris_ambiance ArxAmbiance;
typedef struct arx_pistoris_model ArxModel;
typedef struct arx_pistoris_sound_files ArxSoundFiles;
typedef struct arx_pistoris_sound_source_references ArxSoundSourceReferences;
typedef struct ArxNativeSoundBakeOptions ArxNativeSoundBakeOptions;
typedef struct ArxSoundView ArxSoundView;

typedef struct ArxAmbianceGlbImportOptions {
  // Range [1, 1000]
  float arx_units_per_glb_unit;
} ArxAmbianceGlbImportOptions;
#define ARX_AMBIANCE_GLB_IMPORT_OPTIONS_INIT {10.0f}

typedef struct ArxAmbianceGlbExportOptions {
  // Range [1, 1000]
  float arx_units_per_glb_unit;
} ArxAmbianceGlbExportOptions;
#define ARX_AMBIANCE_GLB_EXPORT_OPTIONS_INIT {10.0f}

/*
 * Input string, encoded-audio, and key-array storage required only for call duration
 * Functions taking ArxAmbiance* invalidate Sound and track indices and borrowed views
 */

ARX_EXTERN_C_BEGIN

// --- Lifetime ---

ARX_API ArxReturnCode arx_pistoris_ambiance_create(ArxAmbiance** out_ambiance) ARX_NOEXCEPT;
ARX_API ArxReturnCode arx_pistoris_ambiance_clone(const ArxAmbiance* ambiance, ArxAmbiance** out_ambiance) ARX_NOEXCEPT;
ARX_API void arx_pistoris_ambiance_destroy(ArxAmbiance* ambiance) ARX_NOEXCEPT;
ARX_API ArxReturnCode arx_pistoris_ambiance_reset(ArxAmbiance* ambiance) ARX_NOEXCEPT;

// --- Conversion ---

ARX_API ArxReturnCode arx_pistoris_ambiance_import_native(const ArxAmb* native, ArxAmbiance** out_ambiance,
                                                          ArxSoundSourceReferences** out_sound_sources) ARX_NOEXCEPT;
ARX_API ArxReturnCode arx_pistoris_ambiance_import_glb(const uint8_t* data, size_t size,
                                                       const ArxAmbianceGlbImportOptions* options,
                                                       ArxAmbiance** out_ambiance,
                                                       ArxSoundSourceReferences** out_sound_sources) ARX_NOEXCEPT;
ARX_API ArxReturnCode arx_pistoris_ambiance_bake_native(const ArxAmbiance* ambiance,
                                                        const ArxNativeSoundBakeOptions* options, ArxAmb** out_native,
                                                        ArxSoundFiles** out_sounds) ARX_NOEXCEPT;
ARX_API ArxReturnCode arx_pistoris_ambiance_export_glb(const ArxAmbiance* ambiance,
                                                       const ArxAmbianceGlbExportOptions* options,
                                                       const ArxModel* reference_model, uint8_t** out_data,
                                                       size_t* out_size, ArxSoundFiles** out_sounds) ARX_NOEXCEPT;

// --- Validation ---

ARX_API ArxReturnCode arx_pistoris_ambiance_validate(const ArxAmbiance* ambiance) ARX_NOEXCEPT;

// --- Resource data ---

ARX_API ArxReturnCode arx_pistoris_ambiance_resource_path(const ArxAmbiance* ambiance,
                                                          ArxStringView* out_path) ARX_NOEXCEPT;
ARX_API ArxReturnCode arx_pistoris_ambiance_set_resource_path(ArxAmbiance* ambiance, ArxStringView path) ARX_NOEXCEPT;

// --- Inspection ---

ARX_API ArxReturnCode arx_pistoris_ambiance_track_count(const ArxAmbiance* ambiance, size_t* out_count) ARX_NOEXCEPT;
ARX_API ArxReturnCode arx_pistoris_ambiance_sound_count(const ArxAmbiance* ambiance, size_t* out_count) ARX_NOEXCEPT;
ARX_API ArxReturnCode arx_pistoris_ambiance_master_track(const ArxAmbiance* ambiance,
                                                         ArxAmbianceTrackIndex* out_track) ARX_NOEXCEPT;
ARX_API ArxReturnCode arx_pistoris_ambiance_copy_tracks(const ArxAmbiance* ambiance, size_t offset, size_t count,
                                                        ArxAmbianceTrack* out_tracks) ARX_NOEXCEPT;
ARX_API ArxReturnCode arx_pistoris_ambiance_copy_sound_views(const ArxAmbiance* ambiance, size_t offset, size_t count,
                                                             ArxSoundView* out_sounds) ARX_NOEXCEPT;
ARX_API ArxReturnCode arx_pistoris_ambiance_copy_panned_keys(const ArxAmbiance* ambiance, ArxAmbianceTrackIndex track,
                                                             size_t offset, size_t count,
                                                             ArxAmbiancePannedKey* out_keys) ARX_NOEXCEPT;
ARX_API ArxReturnCode arx_pistoris_ambiance_copy_positioned_keys(const ArxAmbiance* ambiance,
                                                                 ArxAmbianceTrackIndex track, size_t offset,
                                                                 size_t count,
                                                                 ArxAmbiancePositionedKey* out_keys) ARX_NOEXCEPT;

// --- Tracks ---

ARX_API ArxReturnCode arx_pistoris_ambiance_set_panned_track(ArxAmbiance* ambiance, ArxAmbianceTrackIndex index,
                                                             const ArxAmbiancePannedTrackInput* track) ARX_NOEXCEPT;
ARX_API ArxReturnCode arx_pistoris_ambiance_add_panned_track(ArxAmbiance* ambiance,
                                                             const ArxAmbiancePannedTrackInput* track,
                                                             ArxAmbianceTrackIndex* out_index) ARX_NOEXCEPT;
ARX_API ArxReturnCode arx_pistoris_ambiance_set_positioned_track(
    ArxAmbiance* ambiance, ArxAmbianceTrackIndex index, const ArxAmbiancePositionedTrackInput* track) ARX_NOEXCEPT;
ARX_API ArxReturnCode arx_pistoris_ambiance_add_positioned_track(ArxAmbiance* ambiance,
                                                                 const ArxAmbiancePositionedTrackInput* track,
                                                                 ArxAmbianceTrackIndex* out_index) ARX_NOEXCEPT;
ARX_API ArxReturnCode arx_pistoris_ambiance_remove_track(ArxAmbiance* ambiance,
                                                         ArxAmbianceTrackIndex index) ARX_NOEXCEPT;
ARX_API ArxReturnCode arx_pistoris_ambiance_clear_tracks(ArxAmbiance* ambiance) ARX_NOEXCEPT;
ARX_API ArxReturnCode arx_pistoris_ambiance_set_master_track(ArxAmbiance* ambiance,
                                                             ArxAmbianceTrackIndex index) ARX_NOEXCEPT;
ARX_API ArxReturnCode arx_pistoris_ambiance_trim_tracks_to_master(ArxAmbiance* ambiance,
                                                                  size_t* out_trimmed_tracks) ARX_NOEXCEPT;

// --- Sounds ---

ARX_API ArxReturnCode arx_pistoris_ambiance_compact_sounds(ArxAmbiance* ambiance, size_t* out_removed) ARX_NOEXCEPT;
ARX_API ArxReturnCode arx_pistoris_ambiance_rebase_sound_paths(ArxAmbiance* ambiance,
                                                               ArxStringView directory) ARX_NOEXCEPT;
ARX_API ArxReturnCode arx_pistoris_ambiance_set_sound(ArxAmbiance* ambiance, ArxSoundIndex index,
                                                      const ArxSoundView* sound) ARX_NOEXCEPT;
ARX_API ArxReturnCode arx_pistoris_ambiance_add_sound(ArxAmbiance* ambiance, const ArxSoundView* sound,
                                                      ArxSoundIndex* out_index) ARX_NOEXCEPT;
ARX_API ArxReturnCode arx_pistoris_ambiance_set_sound_data(ArxAmbiance* ambiance, ArxSoundIndex index,
                                                           ArxEncodedAudioView encoded_audio) ARX_NOEXCEPT;
ARX_API ArxReturnCode arx_pistoris_ambiance_clear_sound_data(ArxAmbiance* ambiance, ArxSoundIndex index) ARX_NOEXCEPT;
ARX_API ArxReturnCode arx_pistoris_ambiance_remove_sound(ArxAmbiance* ambiance, ArxSoundIndex index) ARX_NOEXCEPT;

ARX_EXTERN_C_END

// NOLINTEND(readability-identifier-naming)

#endif /* ARX_PISTORIS_AMBIANCE_H */
