// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#ifndef ARX_PISTORIS_PATHS_H
#define ARX_PISTORIS_PATHS_H

#include "arx_pistoris/base/abi.h"
#include "arx_pistoris/base/status.h"
#include "arx_pistoris/base/string_view.h"
#include "arx_pistoris/paths/types.h"

#include <stddef.h>
#include <stdint.h>

// NOLINTBEGIN(readability-identifier-naming)

// Inverse path helper fields borrow from the input path and remain valid only while it remains unchanged
typedef struct ArxModelPathView {
  ArxStringView type;
  ArxStringView name;
  ArxStringView tweak;
} ArxModelPathView;

typedef struct ArxAnimationPathView {
  ArxStringView type;
  ArxStringView name;
} ArxAnimationPathView;

typedef struct ArxCinematicPathView {
  ArxStringView name;
} ArxCinematicPathView;

typedef struct ArxAmbiancePathView {
  ArxStringView name;
} ArxAmbiancePathView;

typedef struct ArxResourceSearchLocation {
  ArxStringView base_path;
  uint32_t max_discovery_depth;
} ArxResourceSearchLocation;

ARX_EXTERN_C_BEGIN

// String builders write a trailing NUL and report the required length excluding it
// {NULL, 0} queries the required length without writing

// --- Common ---

ARX_API ArxReturnCode arx_pistoris_path_resource_selector_kind(ArxStringView selector,
                                                               ArxResourceKind* out_kind) ARX_NOEXCEPT;
ARX_API ArxReturnCode arx_pistoris_path_is_portable_filename(ArxStringView filename,
                                                             uint32_t* out_portable) ARX_NOEXCEPT;
ARX_API ArxReturnCode arx_pistoris_path_sanitize_portable_filename(ArxStringView filename, char* out, size_t capacity,
                                                                   size_t* out_size) ARX_NOEXCEPT;
ARX_API ArxReturnCode arx_pistoris_path_is_portable_resource_path_component(ArxStringView component,
                                                                            uint32_t* out_portable) ARX_NOEXCEPT;
ARX_API ArxReturnCode arx_pistoris_path_texture_directory(ArxStringView* out_directory) ARX_NOEXCEPT;
ARX_API ArxReturnCode arx_pistoris_path_sound_directory(ArxStringView* out_directory) ARX_NOEXCEPT;
ARX_API ArxReturnCode arx_pistoris_path_ambiance_sound_directory(ArxStringView* out_directory) ARX_NOEXCEPT;
ARX_API ArxReturnCode arx_pistoris_path_normalize_zone_ambiance(ArxStringView path, char* out, size_t capacity,
                                                                size_t* out_size) ARX_NOEXCEPT;
ARX_API ArxReturnCode arx_pistoris_path_amb_from_zone_ambiance(ArxStringView ambiance, char* out, size_t capacity,
                                                               size_t* out_size) ARX_NOEXCEPT;

// --- Level ---

ARX_API ArxReturnCode arx_pistoris_path_level_dlf(uint32_t level, char* out, size_t capacity,
                                                  size_t* out_size) ARX_NOEXCEPT;
ARX_API ArxReturnCode arx_pistoris_path_level_llf(uint32_t level, char* out, size_t capacity,
                                                  size_t* out_size) ARX_NOEXCEPT;
ARX_API ArxReturnCode arx_pistoris_path_level_fts(uint32_t level, char* out, size_t capacity,
                                                  size_t* out_size) ARX_NOEXCEPT;
ARX_API ArxReturnCode arx_pistoris_path_minimap_resource_level(uint32_t level,
                                                               uint32_t* out_resource_level) ARX_NOEXCEPT;
ARX_API ArxReturnCode arx_pistoris_path_level_minimap(uint32_t level, char* out, size_t capacity,
                                                      size_t* out_size) ARX_NOEXCEPT;
ARX_API ArxReturnCode arx_pistoris_path_level_loading_screen(uint32_t level, char* out, size_t capacity,
                                                             size_t* out_size) ARX_NOEXCEPT;
ARX_API ArxReturnCode arx_pistoris_path_minimap_offsets_file(ArxStringView* out_path) ARX_NOEXCEPT;
ARX_API ArxReturnCode arx_pistoris_path_level_from_dlf(ArxStringView path, uint32_t* out_level) ARX_NOEXCEPT;
ARX_API ArxReturnCode arx_pistoris_path_level_from_llf(ArxStringView path, uint32_t* out_level) ARX_NOEXCEPT;
ARX_API ArxReturnCode arx_pistoris_path_level_from_fts(ArxStringView path, uint32_t* out_level) ARX_NOEXCEPT;
ARX_API ArxReturnCode arx_pistoris_path_dlf_scene_from_level_name(ArxStringView name, char* out, size_t capacity,
                                                                  size_t* out_size) ARX_NOEXCEPT;
ARX_API ArxReturnCode arx_pistoris_path_level_selector(uint32_t level, char* out, size_t capacity,
                                                       size_t* out_size) ARX_NOEXCEPT;
ARX_API ArxReturnCode arx_pistoris_path_level_from_selector(ArxStringView selector, uint32_t* out_level) ARX_NOEXCEPT;
ARX_API ArxReturnCode arx_pistoris_path_fts_from_dlf_scene(ArxStringView scene_path, char* out, size_t capacity,
                                                           size_t* out_size) ARX_NOEXCEPT;

// --- Model ---

ARX_API size_t arx_pistoris_path_model_selector_type_count(void) ARX_NOEXCEPT;
ARX_API ArxReturnCode arx_pistoris_path_model_selector_type(size_t index, ArxStringView* out_type) ARX_NOEXCEPT;
ARX_API ArxReturnCode arx_pistoris_path_model_ftl(ArxModelPathView model, char* out, size_t capacity,
                                                  size_t* out_size) ARX_NOEXCEPT;
ARX_API ArxReturnCode arx_pistoris_path_model_from_ftl(ArxStringView path, ArxModelPathView* out_model) ARX_NOEXCEPT;
ARX_API ArxReturnCode arx_pistoris_path_entity_class_from_ftl(ArxStringView path, char* out, size_t capacity,
                                                              size_t* out_size) ARX_NOEXCEPT;
ARX_API ArxReturnCode arx_pistoris_path_ftl_from_entity_class(ArxStringView path, char* out, size_t capacity,
                                                              size_t* out_size) ARX_NOEXCEPT;
ARX_API ArxReturnCode arx_pistoris_path_entity_class_kind(ArxStringView path,
                                                          ArxEntityClassKind* out_kind) ARX_NOEXCEPT;
ARX_API ArxReturnCode arx_pistoris_path_item_icon_from_entity_class(ArxStringView path, char* out, size_t capacity,
                                                                    size_t* out_size) ARX_NOEXCEPT;
ARX_API ArxReturnCode arx_pistoris_path_entity_class_from_model(ArxModelPathView model, char* out, size_t capacity,
                                                                size_t* out_size) ARX_NOEXCEPT;
ARX_API ArxReturnCode arx_pistoris_path_base_entity_class_from_model(ArxModelPathView model, char* out, size_t capacity,
                                                                     size_t* out_size) ARX_NOEXCEPT;
ARX_API ArxReturnCode arx_pistoris_path_model_from_entity_class(ArxStringView path,
                                                                ArxModelPathView* out_model) ARX_NOEXCEPT;
ARX_API ArxReturnCode arx_pistoris_path_model_selector(ArxModelPathView model, char* out, size_t capacity,
                                                       size_t* out_size) ARX_NOEXCEPT;
ARX_API ArxReturnCode arx_pistoris_path_model_from_selector(ArxStringView selector,
                                                            ArxModelPathView* out_model) ARX_NOEXCEPT;

// --- Animation ---

ARX_API size_t arx_pistoris_path_animation_selector_type_count(void) ARX_NOEXCEPT;
ARX_API ArxReturnCode arx_pistoris_path_animation_selector_type(size_t index, ArxStringView* out_type) ARX_NOEXCEPT;
ARX_API ArxReturnCode arx_pistoris_path_animation_directory(ArxStringView interactive_type, char* out, size_t capacity,
                                                            size_t* out_size) ARX_NOEXCEPT;
ARX_API ArxReturnCode arx_pistoris_path_animation_tea(ArxAnimationPathView animation, char* out, size_t capacity,
                                                      size_t* out_size) ARX_NOEXCEPT;
ARX_API ArxReturnCode arx_pistoris_path_animation_from_tea(ArxStringView path,
                                                           ArxAnimationPathView* out_animation) ARX_NOEXCEPT;
ARX_API ArxReturnCode arx_pistoris_path_animation_selector(ArxAnimationPathView animation, char* out, size_t capacity,
                                                           size_t* out_size) ARX_NOEXCEPT;
ARX_API ArxReturnCode arx_pistoris_path_animation_from_selector(ArxStringView selector,
                                                                ArxAnimationPathView* out_animation) ARX_NOEXCEPT;

// --- Cinematic ---

ARX_API ArxReturnCode arx_pistoris_path_cinematic_illustration_directory(ArxStringView* out_directory) ARX_NOEXCEPT;
ARX_API ArxReturnCode arx_pistoris_path_cinematic_cin(ArxCinematicPathView cinematic, char* out, size_t capacity,
                                                      size_t* out_size) ARX_NOEXCEPT;
ARX_API ArxReturnCode arx_pistoris_path_cinematic_from_cin(ArxStringView path,
                                                           ArxCinematicPathView* out_cinematic) ARX_NOEXCEPT;
ARX_API ArxReturnCode arx_pistoris_path_cinematic_selector(ArxCinematicPathView cinematic, char* out, size_t capacity,
                                                           size_t* out_size) ARX_NOEXCEPT;
ARX_API ArxReturnCode arx_pistoris_path_cinematic_from_selector(ArxStringView selector,
                                                                ArxCinematicPathView* out_cinematic) ARX_NOEXCEPT;

// --- Ambiance ---

ARX_API ArxReturnCode arx_pistoris_path_ambiance_amb(ArxAmbiancePathView ambiance, char* out, size_t capacity,
                                                     size_t* out_size) ARX_NOEXCEPT;
ARX_API ArxReturnCode arx_pistoris_path_ambiance_from_amb(ArxStringView path,
                                                          ArxAmbiancePathView* out_ambiance) ARX_NOEXCEPT;
ARX_API ArxReturnCode arx_pistoris_path_ambiance_selector(ArxAmbiancePathView ambiance, char* out, size_t capacity,
                                                          size_t* out_size) ARX_NOEXCEPT;
ARX_API ArxReturnCode arx_pistoris_path_ambiance_from_selector(ArxStringView selector,
                                                               ArxAmbiancePathView* out_ambiance) ARX_NOEXCEPT;

// --- Discovery ---

ARX_API ArxReturnCode arx_pistoris_path_model_search_location(ArxStringView type,
                                                              ArxResourceSearchLocation* out_location) ARX_NOEXCEPT;
ARX_API ArxReturnCode arx_pistoris_path_animation_search_location(ArxStringView type,
                                                                  ArxResourceSearchLocation* out_location) ARX_NOEXCEPT;
ARX_API ArxReturnCode arx_pistoris_path_level_search_location(ArxResourceSearchLocation* out_location) ARX_NOEXCEPT;
ARX_API ArxReturnCode arx_pistoris_path_cinematic_search_location(ArxResourceSearchLocation* out_location) ARX_NOEXCEPT;
ARX_API ArxReturnCode arx_pistoris_path_ambiance_search_location(ArxResourceSearchLocation* out_location) ARX_NOEXCEPT;

ARX_EXTERN_C_END

// NOLINTEND(readability-identifier-naming)

#endif /* ARX_PISTORIS_PATHS_H */
