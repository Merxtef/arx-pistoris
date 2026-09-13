// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#include "arx_pistoris/base/math.h"
#include "arx_pistoris/base/status.h"
#include "arx_pistoris/level.h"
#include "arx_pistoris/level.hpp"
#include "arx_pistoris/level/bake.hpp"
#include "arx_pistoris/level/types.h"
#include "arx_pistoris/model.h"
#include "arx_pistoris/native.h"
#include "arx_pistoris/texture.h"
#include "arx_pistoris/texture.hpp"

#include "api/c/internal.h"
#include "api/c/level/internal.h"
#include "api/c/model/internal.h"  // IWYU pragma: keep
#include "api/c/native/internal.h"
#include "api/c/texture/internal.h"

#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <span>
#include <utility>
#include <vector>

// NOLINTBEGIN(readability-identifier-naming)

ArxReturnCode arx_pistoris_level_create(ArxLevel** out_level) noexcept {
  if (!out_level) return ARX_INVALID_DATA_POINTER;
  *out_level = nullptr;
  return pistoris::c_api::guard([&]() -> ArxReturnCode {
    auto level = std::make_unique<ArxLevel>();
    *out_level = level.release();
    return ARX_OK;
  });
}

ArxReturnCode arx_pistoris_level_clone(const ArxLevel* level, ArxLevel** out_level) noexcept {
  if (!level) return ARX_INVALID_HANDLE;
  if (!out_level) return ARX_INVALID_DATA_POINTER;
  *out_level = nullptr;
  return pistoris::c_api::guard([&]() -> ArxReturnCode {
    auto result = std::make_unique<ArxLevel>();
    result->value = level->value;
    *out_level = result.release();
    return ARX_OK;
  });
}

void arx_pistoris_level_destroy(ArxLevel* level) noexcept { delete level; }

ArxReturnCode arx_pistoris_level_reset(ArxLevel* level) noexcept {
  if (!level) return ARX_INVALID_HANDLE;
  return pistoris::c_api::guard([&]() -> ArxReturnCode {
    level->value.reset();
    return ARX_OK;
  });
}

ArxReturnCode arx_pistoris_level_import_native(const ArxFts* fts, const ArxLlf* llf, const ArxDlf* dlf,
                                               ArxLevel** out_level,
                                               ArxTextureSourcePaths** out_texture_source_paths) noexcept {
  if (!fts) return ARX_INVALID_HANDLE;
  if (!out_level) return ARX_INVALID_DATA_POINTER;
  *out_level = nullptr;
  if (out_texture_source_paths) *out_texture_source_paths = nullptr;

  return pistoris::c_api::guard([&]() -> ArxReturnCode {
    auto result = std::make_unique<ArxLevel>();
    std::unique_ptr<ArxTextureSourcePaths> paths;
    if (out_texture_source_paths) paths = std::make_unique<ArxTextureSourcePaths>();
    const auto* lptr = llf ? &llf->value : nullptr;
    const auto* dptr = dlf ? &dlf->value : nullptr;
    const ArxReturnCode rc =
        pistoris::Level::importNative(result->value, fts->value, lptr, dptr, paths ? &paths->value : nullptr);
    if (rc != ARX_OK) return rc;
    *out_level = result.release();
    if (out_texture_source_paths) *out_texture_source_paths = paths.release();
    return ARX_OK;
  });
}

ArxReturnCode arx_pistoris_level_import_glb(const uint8_t* data, size_t size, const ArxLevelGlbImportOptions* options,
                                            ArxLevel** out_level, ArxLevelGlbImportInfo* out_info,
                                            ArxTextureSourcePaths** out_texture_source_paths) noexcept {
  if (!data) return ARX_INVALID_DATA_POINTER;
  if (!out_level) return ARX_INVALID_DATA_POINTER;
  *out_level = nullptr;
  if (out_info) *out_info = {};
  if (out_texture_source_paths) *out_texture_source_paths = nullptr;

  return pistoris::c_api::guard([&]() -> ArxReturnCode {
    auto result = std::make_unique<ArxLevel>();
    std::unique_ptr<ArxTextureSourcePaths> paths;
    if (out_texture_source_paths) paths = std::make_unique<ArxTextureSourcePaths>();
    ArxLevelGlbImportInfo info{};
    pistoris::Level::GlbImportOptions cpp_options;
    if (options) {
      cpp_options.arx_units_per_glb_unit = options->arx_units_per_glb_unit;
      if (options->has_arx_offset) cpp_options.arx_offset = options->arx_offset;
    }
    const ArxReturnCode rc = pistoris::Level::importGlb(
        result->value, std::span<const std::uint8_t>(data, size), cpp_options, &info, paths ? &paths->value : nullptr);
    if (rc != ARX_OK) return rc;
    if (out_info) *out_info = info;
    *out_level = result.release();
    if (out_texture_source_paths) *out_texture_source_paths = paths.release();
    return ARX_OK;
  });
}

ArxReturnCode arx_pistoris_level_export_glb(const ArxLevel* level, const ArxModel* const* models, size_t model_count,
                                            const ArxLevelGlbExportOptions* options, ArxLevelModelPreviewReport* report,
                                            uint8_t** out_data, size_t* out_size) noexcept {
  if (!level) return ARX_INVALID_HANDLE;
  if (!pistoris::c_api::valid(models, model_count) || !out_data || !out_size) return ARX_INVALID_DATA_POINTER;
  for (std::size_t index = 0; index < model_count; ++index)
    if (!models[index]) return ARX_INVALID_HANDLE;
  *out_data = nullptr;
  *out_size = 0;
  if (report) *report = {};

  return pistoris::c_api::guard([&]() -> ArxReturnCode {
    std::vector<const pistoris::Model*> model_views;
    model_views.reserve(model_count);
    for (std::size_t index = 0; index < model_count; ++index) model_views.push_back(&models[index]->value);
    pistoris::Level::GlbExportOptions cpp_options;
    if (options) {
      cpp_options.arx_units_per_glb_unit = options->arx_units_per_glb_unit;
      cpp_options.arx_offset = options->arx_offset;
    }
    std::vector<std::uint8_t> result;
    ArxLevelModelPreviewReport local_report{};
    const ArxReturnCode rc = level->value.exportGlb(result, model_views, cpp_options, report ? &local_report : nullptr);
    if (rc != ARX_OK) return rc;
    const ArxReturnCode publish_rc = pistoris::c_api::publishBytes(std::move(result), out_data, out_size);
    if (publish_rc != ARX_OK) return publish_rc;
    if (report) *report = local_report;
    return ARX_OK;
  });
}

ArxReturnCode arx_pistoris_level_bake_native(const ArxLevel* level, const ArxLevelNativeBakeOptions* options,
                                             ArxFts** out_fts, ArxLlf** out_llf, ArxDlf** out_dlf,
                                             ArxNativeTextureFiles** out_textures) noexcept {
  if (!level) return ARX_INVALID_HANDLE;
  if (!options) return ARX_INVALID_OPTIONS;
  if (!out_fts || !out_llf || !out_dlf) return ARX_INVALID_DATA_POINTER;
  if (!pistoris::c_api::valid(options->level_name) || !pistoris::c_api::valid(options->dlf_scene_path))
    return ARX_INVALID_DATA_POINTER;
  *out_fts = nullptr;
  *out_llf = nullptr;
  *out_dlf = nullptr;
  if (out_textures) *out_textures = nullptr;

  return pistoris::c_api::guard([&]() -> ArxReturnCode {
    pistoris::Level::NativeBakeOptions cpp_options;
    cpp_options.level_name = pistoris::c_api::stringView(options->level_name);
    cpp_options.textures.include_files = out_textures && options->textures.include_files != 0;
    cpp_options.reconstruct_quads = options->reconstruct_quads != 0;
    cpp_options.dlf_scene_path = pistoris::c_api::stringView(options->dlf_scene_path);

    pistoris::NativeLevelBundle bundle;
    ArxReturnCode rc = level->value.bakeNativeBundle(cpp_options, bundle);
    if (rc != ARX_OK) return rc;

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
    return ARX_OK;
  });
}

ArxReturnCode arx_pistoris_level_bake_dlf(const ArxLevel* level, const ArxLevelDlfBakeOptions* options,
                                          ArxDlf** out_dlf) noexcept {
  if (!level) return ARX_INVALID_HANDLE;
  if (!options) return ARX_INVALID_OPTIONS;
  if (!out_dlf) return ARX_INVALID_DATA_POINTER;
  if (!pistoris::c_api::valid(options->level_name) || !pistoris::c_api::valid(options->dlf_scene_path))
    return ARX_INVALID_DATA_POINTER;
  *out_dlf = nullptr;

  return pistoris::c_api::guard([&]() -> ArxReturnCode {
    const pistoris::Level::DlfBakeOptions cpp_options{pistoris::c_api::stringView(options->level_name),
                                                      options->target_fts_offset,
                                                      pistoris::c_api::stringView(options->dlf_scene_path)};
    auto result = std::make_unique<ArxDlf>();
    ArxReturnCode rc = level->value.bakeDlf(cpp_options, result->value);
    if (rc != ARX_OK) return rc;
    *out_dlf = result.release();
    return ARX_OK;
  });
}

#define ARX_LEVEL_VALIDATE_C(name, method)                                           \
  ArxReturnCode arx_pistoris_level_validate_##name(const ArxLevel* level) noexcept { \
    if (!level) return ARX_INVALID_HANDLE;                                           \
    return pistoris::c_api::guard([&] { return level->value.method(); });            \
  }

ArxReturnCode arx_pistoris_level_validate(const ArxLevel* level) noexcept {
  if (!level) return ARX_INVALID_HANDLE;
  return pistoris::c_api::guard([&] { return level->value.validate(); });
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

ArxReturnCode arx_pistoris_level_bounds(const ArxLevel* level, ArxAabb* out_bounds) noexcept {
  if (!level) return ARX_INVALID_HANDLE;
  if (!out_bounds) return ARX_INVALID_DATA_POINTER;
  *out_bounds = {};
  return pistoris::c_api::guard([&]() -> ArxReturnCode {
    ArxReturnCode rc = level->value.validateVertices();
    if (rc != ARX_OK) return rc;
    const std::optional<ArxAabb> bounds = level->value.bounds();
    if (!bounds.has_value()) return ARX_INTERNAL_ERROR;
    *out_bounds = *bounds;
    return ARX_OK;
  });
}

ArxReturnCode arx_pistoris_level_referenced_bounds(const ArxLevel* level, ArxAabb* out_bounds) noexcept {
  if (!level) return ARX_INVALID_HANDLE;
  if (!out_bounds) return ARX_INVALID_DATA_POINTER;
  *out_bounds = {};
  return pistoris::c_api::guard([&]() -> ArxReturnCode {
    ArxReturnCode rc = level->value.validateFaces();
    if (rc != ARX_OK) return rc;
    const std::optional<ArxAabb> bounds = level->value.referencedBounds();
    if (!bounds.has_value()) return ARX_INTERNAL_ERROR;
    *out_bounds = *bounds;
    return ARX_OK;
  });
}

// NOLINTEND(readability-identifier-naming)
