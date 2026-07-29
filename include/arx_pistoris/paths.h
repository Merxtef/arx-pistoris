// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#ifndef ARX_PISTORIS_PATHS_H
#define ARX_PISTORIS_PATHS_H

#include "arx_pistoris/api.h"
#include "arx_pistoris/paths/types.h"
#include "arx_pistoris/pistoris_types.h"

// NOLINTBEGIN(readability-identifier-naming)

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
ARX_API ArxReturnCode arx_pistoris_path_resource_shorthand_kind(ArxStringView shorthand,
                                                                ArxResourceKind* out_kind) ARX_NOEXCEPT;
ARX_API ArxReturnCode arx_pistoris_path_is_portable_filename(ArxStringView filename,
                                                             uint32_t* out_portable) ARX_NOEXCEPT;
ARX_API ArxReturnCode arx_pistoris_path_sanitize_portable_filename(ArxStringView filename, char* out, size_t capacity,
                                                                   size_t* out_size) ARX_NOEXCEPT;
ARX_API ArxReturnCode arx_pistoris_path_texture_from_game(ArxStringView path, char* out, size_t capacity,
                                                          size_t* out_size) ARX_NOEXCEPT;
ARX_API ArxReturnCode arx_pistoris_path_texture_to_game(ArxStringView path, char* out, size_t capacity,
                                                        size_t* out_size) ARX_NOEXCEPT;
ARX_API ArxReturnCode arx_pistoris_path_normalize_zone_ambiance(ArxStringView path, char* out, size_t capacity,
                                                                size_t* out_size) ARX_NOEXCEPT;
ARX_API ArxReturnCode arx_pistoris_path_zone_ambiance_file(ArxStringView ambiance, char* out, size_t capacity,
                                                           size_t* out_size) ARX_NOEXCEPT;

ARX_API ArxReturnCode arx_pistoris_path_level_dlf(uint32_t level, char* out, size_t capacity,
                                                  size_t* out_size) ARX_NOEXCEPT;
ARX_API ArxReturnCode arx_pistoris_path_level_llf(uint32_t level, char* out, size_t capacity,
                                                  size_t* out_size) ARX_NOEXCEPT;
ARX_API ArxReturnCode arx_pistoris_path_level_fts(uint32_t level, char* out, size_t capacity,
                                                  size_t* out_size) ARX_NOEXCEPT;
ARX_API ArxReturnCode arx_pistoris_path_level_from_dlf(ArxStringView path, uint32_t* out_level) ARX_NOEXCEPT;
ARX_API ArxReturnCode arx_pistoris_path_level_from_llf(ArxStringView path, uint32_t* out_level) ARX_NOEXCEPT;
ARX_API ArxReturnCode arx_pistoris_path_level_from_fts(ArxStringView path, uint32_t* out_level) ARX_NOEXCEPT;
ARX_API ArxReturnCode arx_pistoris_path_dlf_scene_from_level_name(ArxStringView name, char* out, size_t capacity,
                                                                  size_t* out_size) ARX_NOEXCEPT;
ARX_API ArxReturnCode arx_pistoris_path_level_shorthand(uint32_t level, char* out, size_t capacity,
                                                        size_t* out_size) ARX_NOEXCEPT;
ARX_API ArxReturnCode arx_pistoris_path_level_from_shorthand(ArxStringView shorthand, uint32_t* out_level) ARX_NOEXCEPT;

ARX_API size_t arx_pistoris_path_model_type_count(void) ARX_NOEXCEPT;
ARX_API ArxReturnCode arx_pistoris_path_model_type(size_t index, ArxStringView* out_type) ARX_NOEXCEPT;
ARX_API ArxReturnCode arx_pistoris_path_model_ftl(ArxModelPathView model, char* out, size_t capacity,
                                                  size_t* out_size) ARX_NOEXCEPT;
ARX_API ArxReturnCode arx_pistoris_path_model_from_ftl(ArxStringView path, ArxModelPathView* out_model) ARX_NOEXCEPT;
ARX_API ArxReturnCode arx_pistoris_path_entity_class_from_model(ArxModelPathView model, char* out, size_t capacity,
                                                                size_t* out_size) ARX_NOEXCEPT;
ARX_API ArxReturnCode arx_pistoris_path_model_from_entity_class(ArxStringView path,
                                                                ArxModelPathView* out_model) ARX_NOEXCEPT;
ARX_API ArxReturnCode arx_pistoris_path_model_shorthand(ArxModelPathView model, char* out, size_t capacity,
                                                        size_t* out_size) ARX_NOEXCEPT;
ARX_API ArxReturnCode arx_pistoris_path_model_from_shorthand(ArxStringView shorthand,
                                                             ArxModelPathView* out_model) ARX_NOEXCEPT;

ARX_API size_t arx_pistoris_path_animation_type_count(void) ARX_NOEXCEPT;
ARX_API ArxReturnCode arx_pistoris_path_animation_type(size_t index, ArxStringView* out_type) ARX_NOEXCEPT;
ARX_API ArxReturnCode arx_pistoris_path_animation_directory(ArxStringView interactive_type, char* out, size_t capacity,
                                                            size_t* out_size) ARX_NOEXCEPT;
ARX_API ArxReturnCode arx_pistoris_path_animation_tea(ArxAnimationPathView animation, char* out, size_t capacity,
                                                      size_t* out_size) ARX_NOEXCEPT;
ARX_API ArxReturnCode arx_pistoris_path_animation_from_tea(ArxStringView path,
                                                           ArxAnimationPathView* out_animation) ARX_NOEXCEPT;
ARX_API ArxReturnCode arx_pistoris_path_animation_shorthand(ArxAnimationPathView animation, char* out, size_t capacity,
                                                            size_t* out_size) ARX_NOEXCEPT;
ARX_API ArxReturnCode arx_pistoris_path_animation_from_shorthand(ArxStringView shorthand,
                                                                 ArxAnimationPathView* out_animation) ARX_NOEXCEPT;

ARX_API ArxReturnCode arx_pistoris_path_cinematic_file(ArxCinematicPathView cinematic, char* out, size_t capacity,
                                                       size_t* out_size) ARX_NOEXCEPT;
ARX_API ArxReturnCode arx_pistoris_path_cinematic_from_file(ArxStringView path,
                                                            ArxCinematicPathView* out_cinematic) ARX_NOEXCEPT;
ARX_API ArxReturnCode arx_pistoris_path_cinematic_shorthand(ArxCinematicPathView cinematic, char* out, size_t capacity,
                                                            size_t* out_size) ARX_NOEXCEPT;
ARX_API ArxReturnCode arx_pistoris_path_cinematic_from_shorthand(ArxStringView shorthand,
                                                                 ArxCinematicPathView* out_cinematic) ARX_NOEXCEPT;

ARX_API ArxReturnCode arx_pistoris_path_ambiance_file(ArxAmbiancePathView ambiance, char* out, size_t capacity,
                                                      size_t* out_size) ARX_NOEXCEPT;
ARX_API ArxReturnCode arx_pistoris_path_ambiance_from_file(ArxStringView path,
                                                           ArxAmbiancePathView* out_ambiance) ARX_NOEXCEPT;
ARX_API ArxReturnCode arx_pistoris_path_ambiance_shorthand(ArxAmbiancePathView ambiance, char* out, size_t capacity,
                                                           size_t* out_size) ARX_NOEXCEPT;
ARX_API ArxReturnCode arx_pistoris_path_ambiance_from_shorthand(ArxStringView shorthand,
                                                                ArxAmbiancePathView* out_ambiance) ARX_NOEXCEPT;

ARX_API ArxReturnCode arx_pistoris_path_model_search_location(ArxStringView type,
                                                              ArxResourceSearchLocation* out_location) ARX_NOEXCEPT;
ARX_API ArxReturnCode arx_pistoris_path_animation_search_location(ArxStringView type,
                                                                  ArxResourceSearchLocation* out_location) ARX_NOEXCEPT;
ARX_API ArxReturnCode arx_pistoris_path_level_search_location(ArxResourceSearchLocation* out_location) ARX_NOEXCEPT;
ARX_API ArxReturnCode arx_pistoris_path_cinematic_search_location(ArxResourceSearchLocation* out_location) ARX_NOEXCEPT;
ARX_API ArxReturnCode arx_pistoris_path_ambiance_search_location(ArxResourceSearchLocation* out_location) ARX_NOEXCEPT;

ARX_API ArxReturnCode arx_pistoris_path_fts_from_dlf_scene(ArxStringView scene_path, char* out, size_t capacity,
                                                           size_t* out_size) ARX_NOEXCEPT;

ARX_EXTERN_C_END

// NOLINTEND(readability-identifier-naming)

#endif /* ARX_PISTORIS_PATHS_H */
