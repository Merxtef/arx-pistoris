// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#ifndef ARX_PISTORIS_TEXTURE_H
#define ARX_PISTORIS_TEXTURE_H

#include "arx_pistoris/base/abi.h"
#include "arx_pistoris/base/image.h"
#include "arx_pistoris/base/indices.h"
#include "arx_pistoris/base/status.h"
#include "arx_pistoris/base/string_view.h"

#include <stddef.h>
#include <stdint.h>

// NOLINTBEGIN(readability-identifier-naming, performance-enum-size)

typedef struct arx_pistoris_native_texture_files ArxNativeTextureFiles;
typedef struct arx_pistoris_texture_source_paths ArxTextureSourcePaths;

typedef struct ArxTextureView {
  ArxStringView path;
  ArxEncodedImageView encoded_image;
#ifdef __cplusplus
  ArxStringView external_image_extension = {};
#else
  ArxStringView external_image_extension;
#endif
} ArxTextureView;

typedef struct ArxNativeTextureFile {
  ArxTextureIndex source_texture;
  ArxStringView resource_path;
  ArxEncodedImageView encoded_image;
} ArxNativeTextureFile;

/* Returned views remain valid until their owning texture-files or source-paths handle is destroyed */

ARX_EXTERN_C_BEGIN

ARX_API ArxReturnCode arx_pistoris_native_texture_files_count(const ArxNativeTextureFiles* files,
                                                              size_t* out_count) ARX_NOEXCEPT;
ARX_API ArxReturnCode arx_pistoris_native_texture_files_get(const ArxNativeTextureFiles* files, size_t index,
                                                            ArxNativeTextureFile* out_file) ARX_NOEXCEPT;
ARX_API void arx_pistoris_native_texture_files_destroy(ArxNativeTextureFiles* files) ARX_NOEXCEPT;
ARX_API ArxReturnCode arx_pistoris_texture_source_paths_count(const ArxTextureSourcePaths* paths,
                                                              size_t* out_count) ARX_NOEXCEPT;
ARX_API ArxReturnCode arx_pistoris_texture_source_paths_get(const ArxTextureSourcePaths* paths, size_t texture,
                                                            ArxStringView* out_path) ARX_NOEXCEPT;
ARX_API void arx_pistoris_texture_source_paths_destroy(ArxTextureSourcePaths* paths) ARX_NOEXCEPT;

ARX_EXTERN_C_END

// NOLINTEND(readability-identifier-naming, performance-enum-size)

#endif /* ARX_PISTORIS_TEXTURE_H */
