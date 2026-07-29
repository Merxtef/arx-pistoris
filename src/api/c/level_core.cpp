// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#include "arx_pistoris/arx_math.h"
#include "arx_pistoris/level.h"
#include "arx_pistoris/level.hpp"
#include "arx_pistoris/level/bake.hpp"
#include "arx_pistoris/native.h"
#include "arx_pistoris/pistoris_types.h"

#include "api/c_api_internal.h"

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

ArxReturnCode arx_pistoris_level_from_native(const ArxFts* fts, const ArxLlf* llf, const ArxDlf* dlf,
                                             ArxLevel** out_level) noexcept {
  if (!fts) return ARX_INVALID_HANDLE;
  if (!out_level) return ARX_INVALID_DATA_POINTER;
  *out_level = nullptr;

  return pistoris::c_api::guard([&]() -> ArxReturnCode {
    auto result = std::make_unique<ArxLevel>();
    const auto* lptr = llf ? &llf->value : nullptr;
    const auto* dptr = dlf ? &dlf->value : nullptr;
    ArxReturnCode rc = pistoris::Level::fromNative(result->value, fts->value, lptr, dptr);
    if (rc != ARX_OK) return rc;
    *out_level = result.release();
    return ARX_OK;
  });
}

ArxReturnCode arx_pistoris_level_from_glb(const uint8_t* data, size_t size, const ArxLevelGlbImportOptions* options,
                                          ArxLevelGlbImportInfo* out_info, ArxLevel** out_level) noexcept {
  if (!data) return ARX_INVALID_DATA_POINTER;
  if (!out_level) return ARX_INVALID_DATA_POINTER;
  *out_level = nullptr;
  if (out_info) *out_info = {};

  return pistoris::c_api::guard([&]() -> ArxReturnCode {
    auto result = std::make_unique<ArxLevel>();
    pistoris::Level::GlbImportInfo info;
    pistoris::Level::GlbImportOptions cpp_options;
    if (options) {
      cpp_options.arx_units_per_glb_unit = options->arx_units_per_glb_unit;
      if (options->has_arx_offset) cpp_options.arx_offset = options->arx_offset;
    }
    ArxReturnCode rc =
        pistoris::Level::fromGlb(result->value, std::span<const std::uint8_t>(data, size), cpp_options, &info);
    if (rc != ARX_OK) return rc;
    if (out_info) out_info->applied_arx_offset = info.applied_arx_offset;
    *out_level = result.release();
    return ARX_OK;
  });
}

ArxReturnCode arx_pistoris_level_export_glb(const ArxLevel* level, const ArxLevelGlbExportOptions* options,
                                            uint8_t** out_data, size_t* out_size) noexcept {
  if (!level) return ARX_INVALID_HANDLE;
  if (!out_data || !out_size) return ARX_INVALID_DATA_POINTER;
  *out_data = nullptr;
  *out_size = 0;

  return pistoris::c_api::guard([&]() -> ArxReturnCode {
    std::vector<std::uint8_t> result;
    ArxReturnCode rc;
    if (options) {
      pistoris::Level::GlbExportOptions cpp_options;
      cpp_options.arx_units_per_glb_unit = options->arx_units_per_glb_unit;
      cpp_options.arx_offset = options->arx_offset;
      rc = level->value.exportGlb(result, cpp_options);
    } else {
      rc = level->value.exportGlb(result);
    }
    if (rc != ARX_OK) return rc;
    return pistoris::c_api::publishBytes(std::move(result), out_data, out_size);
  });
}

ArxReturnCode arx_pistoris_level_bake_native(const ArxLevel* level, const ArxLevelNativeBakeOptions* options,
                                             ArxFts** out_fts, ArxLlf** out_llf, ArxDlf** out_dlf,
                                             ArxNativeTextureFiles** out_textures) noexcept {
  if (!level) return ARX_INVALID_HANDLE;
  if (!options) return ARX_INVALID_OPTIONS;
  if (!out_fts || !out_llf || !out_dlf || !out_textures) return ARX_INVALID_DATA_POINTER;
  if (!pistoris::c_api::valid(options->level_name) || !pistoris::c_api::valid(options->texture_folder) ||
      !pistoris::c_api::valid(options->dlf_scene_path))
    return ARX_INVALID_DATA_POINTER;
  if (options->texture_path_mode != ARX_NATIVE_TEXTURE_PATH_PRESERVE &&
      options->texture_path_mode != ARX_NATIVE_TEXTURE_PATH_REBASE)
    return ARX_INVALID_OPTIONS;
  *out_fts = nullptr;
  *out_llf = nullptr;
  *out_dlf = nullptr;
  *out_textures = nullptr;

  return pistoris::c_api::guard([&]() -> ArxReturnCode {
    pistoris::Level::NativeBakeOptions cpp_options;
    cpp_options.level_name = pistoris::c_api::stringView(options->level_name);
    cpp_options.texture_folder = pistoris::c_api::stringView(options->texture_folder);
    cpp_options.texture_path_mode = static_cast<pistoris::NativeTexturePathMode>(options->texture_path_mode);
    cpp_options.reconstruct_quads = options->reconstruct_quads != 0;
    cpp_options.include_texture_files = options->include_texture_files != 0;
    cpp_options.dlf_scene_path = pistoris::c_api::stringView(options->dlf_scene_path);

    pistoris::NativeLevelBundle bundle;
    ArxReturnCode rc = level->value.bakeNativeBundle(cpp_options, bundle);
    if (rc != ARX_OK) return rc;

    auto fts = std::make_unique<ArxFts>();
    auto llf = std::make_unique<ArxLlf>();
    auto dlf = std::make_unique<ArxDlf>();
    auto textures = std::make_unique<ArxNativeTextureFiles>();
    fts->value = std::move(bundle.fts);
    llf->value = std::move(bundle.llf);
    dlf->value = std::move(bundle.dlf);
    textures->value = std::move(bundle.texture_files);

    *out_fts = fts.release();
    *out_llf = llf.release();
    *out_dlf = dlf.release();
    *out_textures = textures.release();
    return ARX_OK;
  });
}

ArxReturnCode arx_pistoris_level_bake_dlf(const ArxLevel* level, const ArxLevelNativeDlfBakeOptions* options,
                                          ArxDlf** out_dlf) noexcept {
  if (!level) return ARX_INVALID_HANDLE;
  if (!options) return ARX_INVALID_OPTIONS;
  if (!out_dlf) return ARX_INVALID_DATA_POINTER;
  if (!pistoris::c_api::valid(options->level_name) || !pistoris::c_api::valid(options->dlf_scene_path))
    return ARX_INVALID_DATA_POINTER;
  *out_dlf = nullptr;

  return pistoris::c_api::guard([&]() -> ArxReturnCode {
    const pistoris::Level::NativeDlfBakeOptions cpp_options{pistoris::c_api::stringView(options->level_name),
                                                            options->target_fts_offset,
                                                            pistoris::c_api::stringView(options->dlf_scene_path)};
    auto result = std::make_unique<ArxDlf>();
    ArxReturnCode rc = level->value.bakeNativeDlf(cpp_options, result->value);
    if (rc != ARX_OK) return rc;
    *out_dlf = result.release();
    return ARX_OK;
  });
}

ArxReturnCode arx_pistoris_native_texture_files_count(const ArxNativeTextureFiles* files, size_t* out_count) noexcept {
  if (!files) return ARX_INVALID_HANDLE;
  if (!out_count) return ARX_INVALID_DATA_POINTER;
  *out_count = files->value.size();
  return ARX_OK;
}

ArxReturnCode arx_pistoris_native_texture_files_get(const ArxNativeTextureFiles* files, size_t index,
                                                    ArxNativeTextureFile* out_file) noexcept {
  if (!files) return ARX_INVALID_HANDLE;
  if (!out_file) return ARX_INVALID_DATA_POINTER;
  *out_file = {};
  if (index >= files->value.size()) return ARX_INDEX_OUT_OF_RANGE;
  const pistoris::NativeTextureFile& file = files->value[index];
  *out_file = {
      file.source_texture, pistoris::c_api::view(file.resource_path), pistoris::c_api::view(file.encoded_image)};
  return ARX_OK;
}

void arx_pistoris_native_texture_files_destroy(ArxNativeTextureFiles* files) noexcept { delete files; }

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
