// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#include "arx_pistoris/base/error.h"
#include "arx_pistoris/base/math.h"
#include "arx_pistoris/base/status.h"
#include "arx_pistoris/level.h"
#include "arx_pistoris/level.hpp"
#include "arx_pistoris/level/bake.hpp"
#include "arx_pistoris/level/types.h"
#include "arx_pistoris/model.h"
#include "arx_pistoris/native.h"
#include "arx_pistoris/native/text.h"
#include "arx_pistoris/texture.h"

#include "api/c/internal.h"
#include "api/c/level/internal.h"
#include "api/c/model/internal.h"  // IWYU pragma: keep
#include "api/c/native/internal.h"
#include "api/c/native/text_internal.h"
#include "api/c/texture/internal.h"

#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <span>
#include <utility>
#include <vector>

// NOLINTBEGIN(readability-identifier-naming)

ArxReturnCode arx_pistoris_level_create(ArxLevel** out_level, ArxError* error) noexcept {
  if (!out_level) return pistoris::c_api::publishCode(ARX_INVALID_DATA_POINTER, error);
  *out_level = nullptr;
  return pistoris::c_api::guard(error, [&]() -> ArxReturnCode {
    auto level = std::make_unique<ArxLevel>();
    *out_level = level.release();
    return pistoris::c_api::publishCode(ARX_OK, error);
  });
}

ArxReturnCode arx_pistoris_level_clone(const ArxLevel* level, ArxLevel** out_level, ArxError* error) noexcept {
  if (!level) return pistoris::c_api::publishCode(ARX_INVALID_HANDLE, error);
  if (!out_level) return pistoris::c_api::publishCode(ARX_INVALID_DATA_POINTER, error);
  *out_level = nullptr;
  return pistoris::c_api::guard(error, [&]() -> ArxReturnCode {
    auto result = std::make_unique<ArxLevel>();
    result->value = level->value;
    *out_level = result.release();
    return pistoris::c_api::publishCode(ARX_OK, error);
  });
}

void arx_pistoris_level_destroy(ArxLevel* level) noexcept { delete level; }

ArxReturnCode arx_pistoris_level_reset(ArxLevel* level, ArxError* error) noexcept {
  if (!level) return pistoris::c_api::publishCode(ARX_INVALID_HANDLE, error);
  return pistoris::c_api::guard(
      error, [&]() -> ArxReturnCode { return pistoris::c_api::publish(level->value.reset(), error); });
}

ArxReturnCode arx_pistoris_level_import_native(const ArxFts* fts, const ArxLlf* llf, const ArxDlf* dlf,
                                               ArxLevel** out_level, ArxTextureSourcePaths** out_texture_source_paths,
                                               ArxNativeTextMode text_mode, ArxError* error) noexcept {
  if (!fts) return pistoris::c_api::publishCode(ARX_INVALID_HANDLE, error);
  if (!pistoris::c_api::validNativeTextMode(text_mode)) return pistoris::c_api::publishCode(ARX_INVALID_OPTIONS, error);
  if (!out_level) return pistoris::c_api::publishCode(ARX_INVALID_DATA_POINTER, error);
  *out_level = nullptr;
  if (out_texture_source_paths) *out_texture_source_paths = nullptr;

  return pistoris::c_api::guard(error, [&]() -> ArxReturnCode {
    auto result = std::make_unique<ArxLevel>();
    std::unique_ptr<ArxTextureSourcePaths> paths;
    if (out_texture_source_paths) paths = std::make_unique<ArxTextureSourcePaths>();
    const auto* lptr = llf ? &llf->value : nullptr;
    const auto* dptr = dlf ? &dlf->value : nullptr;
    auto imported = pistoris::Level::importNative(
        fts->value, lptr, dptr, paths ? &paths->value : nullptr, pistoris::c_api::nativeTextMode(text_mode));
    if (!imported) return pistoris::c_api::publish(imported, error);
    result->value = std::move(*imported);
    *out_level = result.release();
    if (out_texture_source_paths) *out_texture_source_paths = paths.release();
    return pistoris::c_api::publishCode(ARX_OK, error);
  });
}

ArxReturnCode arx_pistoris_level_import_glb(const uint8_t* data, size_t size, const ArxLevelGlbImportOptions* options,
                                            ArxLevel** out_level, ArxLevelGlbImportInfo* out_info,
                                            ArxTextureSourcePaths** out_texture_source_paths,
                                            ArxError* error) noexcept {
  if (!data) return pistoris::c_api::publishCode(ARX_INVALID_DATA_POINTER, error);
  if (!out_level) return pistoris::c_api::publishCode(ARX_INVALID_DATA_POINTER, error);
  *out_level = nullptr;
  if (out_info) *out_info = {};
  if (out_texture_source_paths) *out_texture_source_paths = nullptr;

  return pistoris::c_api::guard(error, [&]() -> ArxReturnCode {
    auto result = std::make_unique<ArxLevel>();
    std::unique_ptr<ArxTextureSourcePaths> paths;
    if (out_texture_source_paths) paths = std::make_unique<ArxTextureSourcePaths>();
    ArxLevelGlbImportInfo info{};
    pistoris::Level::GlbImportOptions cpp_options;
    if (options) {
      cpp_options.arx_units_per_glb_unit = options->arx_units_per_glb_unit;
      if (options->has_arx_offset) cpp_options.arx_offset = options->arx_offset;
    }
    auto imported = pistoris::Level::importGlb(
        std::span<const std::uint8_t>(data, size), cpp_options, &info, paths ? &paths->value : nullptr);
    if (!imported) return pistoris::c_api::publish(imported, error);
    result->value = std::move(*imported);
    if (out_info) *out_info = info;
    *out_level = result.release();
    if (out_texture_source_paths) *out_texture_source_paths = paths.release();
    return pistoris::c_api::publishCode(ARX_OK, error);
  });
}

ArxReturnCode arx_pistoris_level_export_glb(const ArxLevel* level, const ArxModel* const* models, size_t model_count,
                                            const ArxLevelGlbExportOptions* options, ArxLevelModelPreviewReport* report,
                                            uint8_t** out_data, size_t* out_size, ArxError* error) noexcept {
  if (!level) return pistoris::c_api::publishCode(ARX_INVALID_HANDLE, error);
  if (!pistoris::c_api::valid(models, model_count) || !out_data || !out_size)
    return pistoris::c_api::publishCode(ARX_INVALID_DATA_POINTER, error);
  for (std::size_t index = 0; index < model_count; ++index)
    if (!models[index]) return pistoris::c_api::publishCode(ARX_INVALID_HANDLE, error);
  *out_data = nullptr;
  *out_size = 0;
  if (report) *report = {};

  return pistoris::c_api::guard(error, [&]() -> ArxReturnCode {
    std::vector<const pistoris::Model*> model_views;
    model_views.reserve(model_count);
    for (std::size_t index = 0; index < model_count; ++index) model_views.push_back(&models[index]->value);
    pistoris::Level::GlbExportOptions cpp_options;
    if (options) {
      cpp_options.arx_units_per_glb_unit = options->arx_units_per_glb_unit;
      cpp_options.arx_offset = options->arx_offset;
    }
    ArxLevelModelPreviewReport local_report{};
    auto result = level->value.exportGlb(model_views, cpp_options, report ? &local_report : nullptr);
    if (!result) return pistoris::c_api::publish(result, error);
    const ArxReturnCode publish_rc = pistoris::c_api::publishBytes(std::move(*result), out_data, out_size);
    if (publish_rc != ARX_OK) return publish_rc;
    if (report) *report = local_report;
    return pistoris::c_api::publishCode(ARX_OK, error);
  });
}

ArxReturnCode arx_pistoris_level_bake_native(const ArxLevel* level, const ArxLevelNativeBakeOptions* options,
                                             ArxFts** out_fts, ArxLlf** out_llf, ArxDlf** out_dlf,
                                             ArxNativeTextureFiles** out_textures, ArxError* error) noexcept {
  if (!level) return pistoris::c_api::publishCode(ARX_INVALID_HANDLE, error);
  if (!options) return pistoris::c_api::publishCode(ARX_INVALID_OPTIONS, error);
  if (!out_fts || !out_llf || !out_dlf) return pistoris::c_api::publishCode(ARX_INVALID_DATA_POINTER, error);
  if (!pistoris::c_api::valid(options->level_name) || !pistoris::c_api::valid(options->dlf_scene_path))
    return pistoris::c_api::publishCode(ARX_INVALID_DATA_POINTER, error);
  if (!pistoris::c_api::validNativeTextMode(options->text_mode))
    return pistoris::c_api::publishCode(ARX_INVALID_OPTIONS, error);
  *out_fts = nullptr;
  *out_llf = nullptr;
  *out_dlf = nullptr;
  if (out_textures) *out_textures = nullptr;

  return pistoris::c_api::guard(error, [&]() -> ArxReturnCode {
    pistoris::Level::NativeBakeOptions cpp_options;
    cpp_options.level_name = pistoris::c_api::stringView(options->level_name);
    cpp_options.include_texture_files = out_textures && options->include_texture_files != 0;
    cpp_options.text_mode = pistoris::c_api::nativeTextMode(options->text_mode);
    cpp_options.reconstruct_quads = options->reconstruct_quads != 0;
    cpp_options.dlf_scene_path = pistoris::c_api::stringView(options->dlf_scene_path);

    auto baked = level->value.bakeNativeBundle(cpp_options);
    if (!baked) return pistoris::c_api::publish(baked, error);
    pistoris::NativeLevelBundle bundle = std::move(*baked);

    auto fts = std::make_unique<ArxFts>();
    auto llf = std::make_unique<ArxLlf>();
    auto dlf = std::make_unique<ArxDlf>();
    std::unique_ptr<ArxNativeTextureFiles> textures;
    fts->value = std::move(bundle.fts);
    llf->value = std::move(bundle.llf);
    dlf->value = std::move(bundle.dlf);
    if (out_textures) {
      textures = std::make_unique<ArxNativeTextureFiles>();
      textures->value = std::move(bundle.texture_files);
    }

    *out_fts = fts.release();
    *out_llf = llf.release();
    *out_dlf = dlf.release();
    if (out_textures) *out_textures = textures.release();
    return pistoris::c_api::publishCode(ARX_OK, error);
  });
}

ArxReturnCode arx_pistoris_level_bake_dlf(const ArxLevel* level, const ArxLevelDlfBakeOptions* options,
                                          ArxDlf** out_dlf, ArxError* error) noexcept {
  if (!level) return pistoris::c_api::publishCode(ARX_INVALID_HANDLE, error);
  if (!options) return pistoris::c_api::publishCode(ARX_INVALID_OPTIONS, error);
  if (!out_dlf) return pistoris::c_api::publishCode(ARX_INVALID_DATA_POINTER, error);
  if (!pistoris::c_api::valid(options->level_name) || !pistoris::c_api::valid(options->dlf_scene_path))
    return pistoris::c_api::publishCode(ARX_INVALID_DATA_POINTER, error);
  if (!pistoris::c_api::validNativeTextMode(options->text_mode))
    return pistoris::c_api::publishCode(ARX_INVALID_OPTIONS, error);
  *out_dlf = nullptr;

  return pistoris::c_api::guard(error, [&]() -> ArxReturnCode {
    const pistoris::Level::DlfBakeOptions cpp_options{pistoris::c_api::stringView(options->level_name),
                                                      options->target_fts_offset,
                                                      pistoris::c_api::stringView(options->dlf_scene_path),
                                                      pistoris::c_api::nativeTextMode(options->text_mode)};
    auto baked = level->value.bakeDlf(cpp_options);
    if (!baked) return pistoris::c_api::publish(baked, error);
    auto result = std::make_unique<ArxDlf>();
    result->value = std::move(*baked);
    *out_dlf = result.release();
    return pistoris::c_api::publishCode(ARX_OK, error);
  });
}

#define ARX_LEVEL_VALIDATE_C(name, method)                                                            \
  ArxReturnCode arx_pistoris_level_validate_##name(const ArxLevel* level, ArxError* error) noexcept { \
    if (!level) return pistoris::c_api::publishCode(ARX_INVALID_HANDLE, error);                       \
    return pistoris::c_api::guard(error, [&] {                                                        \
      auto result = level->value.method();                                                            \
      return pistoris::c_api::publish(result, error);                                                 \
    });                                                                                               \
  }

ArxReturnCode arx_pistoris_level_validate(const ArxLevel* level, ArxError* error) noexcept {
  if (!level) return pistoris::c_api::publishCode(ARX_INVALID_HANDLE, error);
  return pistoris::c_api::guard(error, [&] {
    auto result = level->value.validate();
    return pistoris::c_api::publish(result, error);
  });
}

ARX_LEVEL_VALIDATE_C(mesh, validateMesh)
ARX_LEVEL_VALIDATE_C(vertices, validateVertices)
ARX_LEVEL_VALIDATE_C(textures, validateTextures)
ARX_LEVEL_VALIDATE_C(faces, validateFaces)
ARX_LEVEL_VALIDATE_C(face_rooms, validateFaceRooms)
ARX_LEVEL_VALIDATE_C(corner_colors, validateCornerColors)
ARX_LEVEL_VALIDATE_C(rooms, validateRooms)
ARX_LEVEL_VALIDATE_C(portals, validatePortals)
ARX_LEVEL_VALIDATE_C(room_distances, validateRoomDistances)
ARX_LEVEL_VALIDATE_C(nav_surface, validateNavSurface)
ARX_LEVEL_VALIDATE_C(anchors, validateAnchors)
ARX_LEVEL_VALIDATE_C(anchor_connections, validateAnchorConnections)
ARX_LEVEL_VALIDATE_C(lights, validateLights)
ARX_LEVEL_VALIDATE_C(player_spawn, validatePlayerSpawn)
ARX_LEVEL_VALIDATE_C(entities, validateEntities)
ARX_LEVEL_VALIDATE_C(fogs, validateFogs)
ARX_LEVEL_VALIDATE_C(zones, validateZones)
ARX_LEVEL_VALIDATE_C(paths, validatePaths)
ARX_LEVEL_VALIDATE_C(minimap, validateMinimap)
ARX_LEVEL_VALIDATE_C(loading_screen, validateLoadingScreen)

#undef ARX_LEVEL_VALIDATE_C

ArxReturnCode arx_pistoris_level_bounds(const ArxLevel* level, ArxAabb* out_bounds, ArxError* error) noexcept {
  if (!level) return pistoris::c_api::publishCode(ARX_INVALID_HANDLE, error);
  if (!out_bounds) return pistoris::c_api::publishCode(ARX_INVALID_DATA_POINTER, error);
  *out_bounds = {};
  return pistoris::c_api::guard(error, [&]() -> ArxReturnCode {
    auto validation = level->value.validateVertices();
    if (!validation) return pistoris::c_api::publish(validation, error);
    const std::optional<ArxAabb> bounds = level->value.bounds();
    if (!bounds.has_value()) return pistoris::c_api::publishCode(ARX_INTERNAL_ERROR, error);
    *out_bounds = *bounds;
    return pistoris::c_api::publishCode(ARX_OK, error);
  });
}

ArxReturnCode arx_pistoris_level_referenced_bounds(const ArxLevel* level, ArxAabb* out_bounds,
                                                   ArxError* error) noexcept {
  if (!level) return pistoris::c_api::publishCode(ARX_INVALID_HANDLE, error);
  if (!out_bounds) return pistoris::c_api::publishCode(ARX_INVALID_DATA_POINTER, error);
  *out_bounds = {};
  return pistoris::c_api::guard(error, [&]() -> ArxReturnCode {
    auto validation = level->value.validateFaces();
    if (!validation) return pistoris::c_api::publish(validation, error);
    const std::optional<ArxAabb> bounds = level->value.referencedBounds();
    if (!bounds.has_value()) return pistoris::c_api::publishCode(ARX_INTERNAL_ERROR, error);
    *out_bounds = *bounds;
    return pistoris::c_api::publishCode(ARX_OK, error);
  });
}

// NOLINTEND(readability-identifier-naming)
