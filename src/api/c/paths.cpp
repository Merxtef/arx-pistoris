// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#include "arx_pistoris/paths.h"

#include "arx_pistoris/paths.hpp"
#include "arx_pistoris/paths/types.h"
#include "arx_pistoris/pistoris_types.h"

#include "api/c_api_internal.h"

#include <cstdint>
#include <cstring>
#include <span>
#include <string>
#include <string_view>

// NOLINTBEGIN(readability-identifier-naming)

namespace {

bool validOutput(char* out, std::size_t capacity, std::size_t* out_size) noexcept {
  return out_size && (out || capacity == 0);
}

ArxReturnCode publishPath(std::string_view value, char* out, std::size_t capacity, std::size_t* out_size) noexcept {
  *out_size = value.size();
  if (!out) return ARX_OK;
  if (capacity <= value.size()) return ARX_BUFFER_TOO_SMALL;
  if (!value.empty()) std::memcpy(out, value.data(), value.size());
  out[value.size()] = '\0';
  return ARX_OK;
}

bool valid(ArxModelPathView value) noexcept {
  return pistoris::c_api::valid(value.type) && pistoris::c_api::valid(value.name) &&
         pistoris::c_api::valid(value.tweak);
}

bool valid(ArxAnimationPathView value) noexcept {
  return pistoris::c_api::valid(value.type) && pistoris::c_api::valid(value.name);
}

bool valid(ArxCinematicPathView value) noexcept { return pistoris::c_api::valid(value.name); }

bool valid(ArxAmbiancePathView value) noexcept { return pistoris::c_api::valid(value.name); }

pistoris::paths::ModelPathView modelView(ArxModelPathView value) noexcept {
  return {
      pistoris::c_api::stringView(value.type),
      pistoris::c_api::stringView(value.name),
      pistoris::c_api::stringView(value.tweak),
  };
}

pistoris::paths::AnimationPathView animationView(ArxAnimationPathView value) noexcept {
  return {pistoris::c_api::stringView(value.type), pistoris::c_api::stringView(value.name)};
}

ArxModelPathView cView(pistoris::paths::ModelPathView value) noexcept {
  return {
      pistoris::c_api::view(value.type),
      pistoris::c_api::view(value.name),
      pistoris::c_api::view(value.tweak),
  };
}

ArxAnimationPathView cView(pistoris::paths::AnimationPathView value) noexcept {
  return {pistoris::c_api::view(value.type), pistoris::c_api::view(value.name)};
}

ArxCinematicPathView cView(pistoris::paths::CinematicPathView value) noexcept {
  return {pistoris::c_api::view(value.name)};
}

ArxAmbiancePathView cView(pistoris::paths::AmbiancePathView value) noexcept {
  return {pistoris::c_api::view(value.name)};
}

ArxResourceSearchLocation cView(pistoris::paths::ResourceSearchLocation value) noexcept {
  return {pistoris::c_api::view(value.base_path), value.max_discovery_depth};
}

ArxReturnCode typeAt(std::span<const std::string_view> types, std::size_t index, ArxStringView* out_type) noexcept {
  if (!out_type) return ARX_INVALID_DATA_POINTER;
  *out_type = {};
  if (index >= types.size()) return ARX_INDEX_OUT_OF_RANGE;
  *out_type = pistoris::c_api::view(types[index]);
  return ARX_OK;
}

template <class Builder>
ArxReturnCode buildPath(char* out, std::size_t capacity, std::size_t* out_size, Builder&& builder) noexcept {
  if (!validOutput(out, capacity, out_size)) return ARX_INVALID_DATA_POINTER;
  *out_size = 0;
  return pistoris::c_api::guard([&]() -> ArxReturnCode {
    std::string result;
    if (!builder(result)) return ARX_INVALID_IDENTIFIER;
    return publishPath(result, out, capacity, out_size);
  });
}

template <class Parser>
ArxReturnCode parseLevel(ArxStringView path, std::uint32_t* out_level, Parser&& parser) noexcept {
  if (!pistoris::c_api::valid(path) || !out_level) return ARX_INVALID_DATA_POINTER;
  *out_level = 0;
  return pistoris::c_api::guard([&]() -> ArxReturnCode {
    std::uint32_t level = 0;
    if (!parser(pistoris::c_api::stringView(path), level)) return ARX_INVALID_IDENTIFIER;
    *out_level = level;
    return ARX_OK;
  });
}

}  // namespace

ArxReturnCode arx_pistoris_path_resource_shorthand_kind(ArxStringView shorthand, ArxResourceKind* out_kind) noexcept {
  if (!pistoris::c_api::valid(shorthand) || !out_kind) return ARX_INVALID_DATA_POINTER;
  *out_kind = pistoris::paths::resourceShorthandKind(pistoris::c_api::stringView(shorthand));
  return ARX_OK;
}

ArxReturnCode arx_pistoris_path_is_portable_filename(ArxStringView filename, uint32_t* out_portable) noexcept {
  if (!pistoris::c_api::valid(filename) || !out_portable) return ARX_INVALID_DATA_POINTER;
  *out_portable = 0;
  return pistoris::c_api::guard([&]() -> ArxReturnCode {
    *out_portable = pistoris::paths::isPortableFilename(pistoris::c_api::stringView(filename)) ? 1U : 0U;
    return ARX_OK;
  });
}

ArxReturnCode arx_pistoris_path_sanitize_portable_filename(ArxStringView filename, char* out, size_t capacity,
                                                           size_t* out_size) noexcept {
  if (!pistoris::c_api::valid(filename)) return ARX_INVALID_DATA_POINTER;
  return buildPath(out, capacity, out_size, [&](std::string& result) {
    result = pistoris::paths::sanitizePortableFilename(pistoris::c_api::stringView(filename));
    return true;
  });
}

ArxReturnCode arx_pistoris_path_texture_from_game(ArxStringView path, char* out, size_t capacity,
                                                  size_t* out_size) noexcept {
  if (!pistoris::c_api::valid(path)) return ARX_INVALID_DATA_POINTER;
  return buildPath(out, capacity, out_size, [&](std::string& result) {
    result = pistoris::paths::textureFromGame(pistoris::c_api::stringView(path));
    return true;
  });
}

ArxReturnCode arx_pistoris_path_texture_to_game(ArxStringView path, char* out, size_t capacity,
                                                size_t* out_size) noexcept {
  if (!pistoris::c_api::valid(path)) return ARX_INVALID_DATA_POINTER;
  return buildPath(out, capacity, out_size, [&](std::string& result) {
    result = pistoris::paths::textureToGame(pistoris::c_api::stringView(path));
    return true;
  });
}

ArxReturnCode arx_pistoris_path_normalize_zone_ambiance(ArxStringView path, char* out, size_t capacity,
                                                        size_t* out_size) noexcept {
  if (!pistoris::c_api::valid(path)) return ARX_INVALID_DATA_POINTER;
  return buildPath(out, capacity, out_size, [&](std::string& result) {
    return pistoris::paths::normalizeZoneAmbiance(pistoris::c_api::stringView(path), result);
  });
}

ArxReturnCode arx_pistoris_path_zone_ambiance_file(ArxStringView ambiance, char* out, size_t capacity,
                                                   size_t* out_size) noexcept {
  if (!pistoris::c_api::valid(ambiance)) return ARX_INVALID_DATA_POINTER;
  return buildPath(out, capacity, out_size, [&](std::string& result) {
    return pistoris::paths::zoneAmbianceFile(pistoris::c_api::stringView(ambiance), result);
  });
}

ArxReturnCode arx_pistoris_path_level_dlf(uint32_t level, char* out, size_t capacity, size_t* out_size) noexcept {
  return buildPath(out, capacity, out_size, [&](std::string& result) {
    result = pistoris::paths::levelDlf(level);
    return true;
  });
}

ArxReturnCode arx_pistoris_path_level_llf(uint32_t level, char* out, size_t capacity, size_t* out_size) noexcept {
  return buildPath(out, capacity, out_size, [&](std::string& result) {
    result = pistoris::paths::levelLlf(level);
    return true;
  });
}

ArxReturnCode arx_pistoris_path_level_fts(uint32_t level, char* out, size_t capacity, size_t* out_size) noexcept {
  return buildPath(out, capacity, out_size, [&](std::string& result) {
    result = pistoris::paths::levelFts(level);
    return true;
  });
}

ArxReturnCode arx_pistoris_path_level_from_dlf(ArxStringView path, uint32_t* out_level) noexcept {
  return parseLevel(path, out_level, pistoris::paths::levelFromDlf);
}

ArxReturnCode arx_pistoris_path_level_from_llf(ArxStringView path, uint32_t* out_level) noexcept {
  return parseLevel(path, out_level, pistoris::paths::levelFromLlf);
}

ArxReturnCode arx_pistoris_path_level_from_fts(ArxStringView path, uint32_t* out_level) noexcept {
  return parseLevel(path, out_level, pistoris::paths::levelFromFts);
}

ArxReturnCode arx_pistoris_path_dlf_scene_from_level_name(ArxStringView name, char* out, size_t capacity,
                                                          size_t* out_size) noexcept {
  if (!pistoris::c_api::valid(name)) return ARX_INVALID_DATA_POINTER;
  return buildPath(out, capacity, out_size, [&](std::string& result) {
    return pistoris::paths::dlfSceneFromLevelName(pistoris::c_api::stringView(name), result);
  });
}

ArxReturnCode arx_pistoris_path_level_shorthand(uint32_t level, char* out, size_t capacity, size_t* out_size) noexcept {
  return buildPath(out, capacity, out_size, [&](std::string& result) {
    result = pistoris::paths::levelShorthand(level);
    return true;
  });
}

ArxReturnCode arx_pistoris_path_level_from_shorthand(ArxStringView shorthand, uint32_t* out_level) noexcept {
  return parseLevel(shorthand, out_level, pistoris::paths::levelFromShorthand);
}

size_t arx_pistoris_path_model_type_count(void) noexcept { return pistoris::paths::modelTypes().size(); }

ArxReturnCode arx_pistoris_path_model_type(size_t index, ArxStringView* out_type) noexcept {
  return typeAt(pistoris::paths::modelTypes(), index, out_type);
}

ArxReturnCode arx_pistoris_path_model_ftl(ArxModelPathView model, char* out, size_t capacity,
                                          size_t* out_size) noexcept {
  if (!valid(model)) return ARX_INVALID_DATA_POINTER;
  return buildPath(out, capacity, out_size, [&](std::string& result) {
    return pistoris::paths::modelFtl(modelView(model), result);
  });
}

ArxReturnCode arx_pistoris_path_model_from_ftl(ArxStringView path, ArxModelPathView* out_model) noexcept {
  if (!pistoris::c_api::valid(path) || !out_model) return ARX_INVALID_DATA_POINTER;
  *out_model = {};
  return pistoris::c_api::guard([&]() -> ArxReturnCode {
    pistoris::paths::ModelPathView result;
    if (!pistoris::paths::modelFromFtl(pistoris::c_api::stringView(path), result)) return ARX_INVALID_IDENTIFIER;
    *out_model = cView(result);
    return ARX_OK;
  });
}

ArxReturnCode arx_pistoris_path_entity_class_from_model(ArxModelPathView model, char* out, size_t capacity,
                                                        size_t* out_size) noexcept {
  if (!valid(model)) return ARX_INVALID_DATA_POINTER;
  return buildPath(out, capacity, out_size, [&](std::string& result) {
    return pistoris::paths::entityClassFromModel(modelView(model), result);
  });
}

ArxReturnCode arx_pistoris_path_model_from_entity_class(ArxStringView path, ArxModelPathView* out_model) noexcept {
  if (!pistoris::c_api::valid(path) || !out_model) return ARX_INVALID_DATA_POINTER;
  *out_model = {};
  return pistoris::c_api::guard([&]() -> ArxReturnCode {
    pistoris::paths::ModelPathView result;
    if (!pistoris::paths::modelFromEntityClass(pistoris::c_api::stringView(path), result))
      return ARX_INVALID_IDENTIFIER;
    *out_model = cView(result);
    return ARX_OK;
  });
}

ArxReturnCode arx_pistoris_path_model_shorthand(ArxModelPathView model, char* out, size_t capacity,
                                                size_t* out_size) noexcept {
  if (!valid(model)) return ARX_INVALID_DATA_POINTER;
  return buildPath(out, capacity, out_size, [&](std::string& result) {
    return pistoris::paths::modelShorthand(modelView(model), result);
  });
}

ArxReturnCode arx_pistoris_path_model_from_shorthand(ArxStringView shorthand, ArxModelPathView* out_model) noexcept {
  if (!pistoris::c_api::valid(shorthand) || !out_model) return ARX_INVALID_DATA_POINTER;
  *out_model = {};
  return pistoris::c_api::guard([&]() -> ArxReturnCode {
    pistoris::paths::ModelPathView result;
    if (!pistoris::paths::modelFromShorthand(pistoris::c_api::stringView(shorthand), result))
      return ARX_INVALID_IDENTIFIER;
    *out_model = cView(result);
    return ARX_OK;
  });
}

size_t arx_pistoris_path_animation_type_count(void) noexcept { return pistoris::paths::animationTypes().size(); }

ArxReturnCode arx_pistoris_path_animation_type(size_t index, ArxStringView* out_type) noexcept {
  return typeAt(pistoris::paths::animationTypes(), index, out_type);
}

ArxReturnCode arx_pistoris_path_animation_directory(ArxStringView interactive_type, char* out, size_t capacity,
                                                    size_t* out_size) noexcept {
  if (!pistoris::c_api::valid(interactive_type)) return ARX_INVALID_DATA_POINTER;
  return buildPath(out, capacity, out_size, [&](std::string& result) {
    return pistoris::paths::animationDirectory(pistoris::c_api::stringView(interactive_type), result);
  });
}

ArxReturnCode arx_pistoris_path_animation_tea(ArxAnimationPathView animation, char* out, size_t capacity,
                                              size_t* out_size) noexcept {
  if (!valid(animation)) return ARX_INVALID_DATA_POINTER;
  return buildPath(out, capacity, out_size, [&](std::string& result) {
    return pistoris::paths::animationTea(animationView(animation), result);
  });
}

ArxReturnCode arx_pistoris_path_animation_from_tea(ArxStringView path, ArxAnimationPathView* out_animation) noexcept {
  if (!pistoris::c_api::valid(path) || !out_animation) return ARX_INVALID_DATA_POINTER;
  *out_animation = {};
  return pistoris::c_api::guard([&]() -> ArxReturnCode {
    pistoris::paths::AnimationPathView result;
    if (!pistoris::paths::animationFromTea(pistoris::c_api::stringView(path), result)) return ARX_INVALID_IDENTIFIER;
    *out_animation = cView(result);
    return ARX_OK;
  });
}

ArxReturnCode arx_pistoris_path_animation_shorthand(ArxAnimationPathView animation, char* out, size_t capacity,
                                                    size_t* out_size) noexcept {
  if (!valid(animation)) return ARX_INVALID_DATA_POINTER;
  return buildPath(out, capacity, out_size, [&](std::string& result) {
    return pistoris::paths::animationShorthand(animationView(animation), result);
  });
}

ArxReturnCode arx_pistoris_path_animation_from_shorthand(ArxStringView shorthand,
                                                         ArxAnimationPathView* out_animation) noexcept {
  if (!pistoris::c_api::valid(shorthand) || !out_animation) return ARX_INVALID_DATA_POINTER;
  *out_animation = {};
  return pistoris::c_api::guard([&]() -> ArxReturnCode {
    pistoris::paths::AnimationPathView result;
    if (!pistoris::paths::animationFromShorthand(pistoris::c_api::stringView(shorthand), result))
      return ARX_INVALID_IDENTIFIER;
    *out_animation = cView(result);
    return ARX_OK;
  });
}

ArxReturnCode arx_pistoris_path_cinematic_file(ArxCinematicPathView cinematic, char* out, size_t capacity,
                                               size_t* out_size) noexcept {
  if (!valid(cinematic)) return ARX_INVALID_DATA_POINTER;
  return buildPath(out, capacity, out_size, [&](std::string& result) {
    return pistoris::paths::cinematicFile({pistoris::c_api::stringView(cinematic.name)}, result);
  });
}

ArxReturnCode arx_pistoris_path_cinematic_from_file(ArxStringView path, ArxCinematicPathView* out_cinematic) noexcept {
  if (!pistoris::c_api::valid(path) || !out_cinematic) return ARX_INVALID_DATA_POINTER;
  *out_cinematic = {};
  return pistoris::c_api::guard([&]() -> ArxReturnCode {
    pistoris::paths::CinematicPathView result;
    if (!pistoris::paths::cinematicFromFile(pistoris::c_api::stringView(path), result)) return ARX_INVALID_IDENTIFIER;
    *out_cinematic = cView(result);
    return ARX_OK;
  });
}

ArxReturnCode arx_pistoris_path_cinematic_shorthand(ArxCinematicPathView cinematic, char* out, size_t capacity,
                                                    size_t* out_size) noexcept {
  if (!valid(cinematic)) return ARX_INVALID_DATA_POINTER;
  return buildPath(out, capacity, out_size, [&](std::string& result) {
    return pistoris::paths::cinematicShorthand({pistoris::c_api::stringView(cinematic.name)}, result);
  });
}

ArxReturnCode arx_pistoris_path_cinematic_from_shorthand(ArxStringView shorthand,
                                                         ArxCinematicPathView* out_cinematic) noexcept {
  if (!pistoris::c_api::valid(shorthand) || !out_cinematic) return ARX_INVALID_DATA_POINTER;
  *out_cinematic = {};
  return pistoris::c_api::guard([&]() -> ArxReturnCode {
    pistoris::paths::CinematicPathView result;
    if (!pistoris::paths::cinematicFromShorthand(pistoris::c_api::stringView(shorthand), result))
      return ARX_INVALID_IDENTIFIER;
    *out_cinematic = cView(result);
    return ARX_OK;
  });
}

ArxReturnCode arx_pistoris_path_ambiance_file(ArxAmbiancePathView ambiance, char* out, size_t capacity,
                                              size_t* out_size) noexcept {
  if (!valid(ambiance)) return ARX_INVALID_DATA_POINTER;
  return buildPath(out, capacity, out_size, [&](std::string& result) {
    return pistoris::paths::ambianceFile({pistoris::c_api::stringView(ambiance.name)}, result);
  });
}

ArxReturnCode arx_pistoris_path_ambiance_from_file(ArxStringView path, ArxAmbiancePathView* out_ambiance) noexcept {
  if (!pistoris::c_api::valid(path) || !out_ambiance) return ARX_INVALID_DATA_POINTER;
  *out_ambiance = {};
  return pistoris::c_api::guard([&]() -> ArxReturnCode {
    pistoris::paths::AmbiancePathView result;
    if (!pistoris::paths::ambianceFromFile(pistoris::c_api::stringView(path), result)) return ARX_INVALID_IDENTIFIER;
    *out_ambiance = cView(result);
    return ARX_OK;
  });
}

ArxReturnCode arx_pistoris_path_ambiance_shorthand(ArxAmbiancePathView ambiance, char* out, size_t capacity,
                                                   size_t* out_size) noexcept {
  if (!valid(ambiance)) return ARX_INVALID_DATA_POINTER;
  return buildPath(out, capacity, out_size, [&](std::string& result) {
    return pistoris::paths::ambianceShorthand({pistoris::c_api::stringView(ambiance.name)}, result);
  });
}

ArxReturnCode arx_pistoris_path_ambiance_from_shorthand(ArxStringView shorthand,
                                                        ArxAmbiancePathView* out_ambiance) noexcept {
  if (!pistoris::c_api::valid(shorthand) || !out_ambiance) return ARX_INVALID_DATA_POINTER;
  *out_ambiance = {};
  return pistoris::c_api::guard([&]() -> ArxReturnCode {
    pistoris::paths::AmbiancePathView result;
    if (!pistoris::paths::ambianceFromShorthand(pistoris::c_api::stringView(shorthand), result))
      return ARX_INVALID_IDENTIFIER;
    *out_ambiance = cView(result);
    return ARX_OK;
  });
}

ArxReturnCode arx_pistoris_path_model_search_location(ArxStringView type,
                                                      ArxResourceSearchLocation* out_location) noexcept {
  if (!pistoris::c_api::valid(type) || !out_location) return ARX_INVALID_DATA_POINTER;
  *out_location = {};
  pistoris::paths::ResourceSearchLocation result;
  if (!pistoris::paths::modelSearchLocation(pistoris::c_api::stringView(type), result)) return ARX_INVALID_IDENTIFIER;
  *out_location = cView(result);
  return ARX_OK;
}

ArxReturnCode arx_pistoris_path_animation_search_location(ArxStringView type,
                                                          ArxResourceSearchLocation* out_location) noexcept {
  if (!pistoris::c_api::valid(type) || !out_location) return ARX_INVALID_DATA_POINTER;
  *out_location = {};
  pistoris::paths::ResourceSearchLocation result;
  if (!pistoris::paths::animationSearchLocation(pistoris::c_api::stringView(type), result))
    return ARX_INVALID_IDENTIFIER;
  *out_location = cView(result);
  return ARX_OK;
}

ArxReturnCode arx_pistoris_path_level_search_location(ArxResourceSearchLocation* out_location) noexcept {
  if (!out_location) return ARX_INVALID_DATA_POINTER;
  *out_location = cView(pistoris::paths::levelSearchLocation());
  return ARX_OK;
}

ArxReturnCode arx_pistoris_path_cinematic_search_location(ArxResourceSearchLocation* out_location) noexcept {
  if (!out_location) return ARX_INVALID_DATA_POINTER;
  *out_location = cView(pistoris::paths::cinematicSearchLocation());
  return ARX_OK;
}

ArxReturnCode arx_pistoris_path_ambiance_search_location(ArxResourceSearchLocation* out_location) noexcept {
  if (!out_location) return ARX_INVALID_DATA_POINTER;
  *out_location = cView(pistoris::paths::ambianceSearchLocation());
  return ARX_OK;
}

ArxReturnCode arx_pistoris_path_fts_from_dlf_scene(ArxStringView scene_path, char* out, size_t capacity,
                                                   size_t* out_size) noexcept {
  if (!pistoris::c_api::valid(scene_path)) return ARX_INVALID_DATA_POINTER;
  return buildPath(out, capacity, out_size, [&](std::string& result) {
    return pistoris::paths::ftsFromDlfScene(pistoris::c_api::stringView(scene_path), result);
  });
}

// NOLINTEND(readability-identifier-naming)
