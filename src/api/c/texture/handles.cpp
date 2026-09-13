// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#include "arx_pistoris/base/status.h"
#include "arx_pistoris/base/string_view.h"
#include "arx_pistoris/texture.h"
#include "arx_pistoris/texture.hpp"

#include "api/c/internal.h"
#include "api/c/texture/internal.h"  // IWYU pragma: keep

#include <cstddef>

// NOLINTBEGIN(readability-identifier-naming)

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

ArxReturnCode arx_pistoris_texture_source_paths_count(const ArxTextureSourcePaths* paths, size_t* out_count) noexcept {
  if (!paths) return ARX_INVALID_HANDLE;
  if (!out_count) return ARX_INVALID_DATA_POINTER;
  *out_count = paths->value.size();
  return ARX_OK;
}

ArxReturnCode arx_pistoris_texture_source_paths_get(const ArxTextureSourcePaths* paths, size_t texture,
                                                    ArxStringView* out_path) noexcept {
  if (!paths) return ARX_INVALID_HANDLE;
  if (!out_path) return ARX_INVALID_DATA_POINTER;
  *out_path = {};
  if (texture >= paths->value.size()) return ARX_INDEX_OUT_OF_RANGE;
  *out_path = pistoris::c_api::view(paths->value[texture]);
  return ARX_OK;
}

void arx_pistoris_texture_source_paths_destroy(ArxTextureSourcePaths* paths) noexcept { delete paths; }

// NOLINTEND(readability-identifier-naming)
