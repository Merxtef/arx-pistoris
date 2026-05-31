// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#ifndef ARX_PISTORIS_H
#define ARX_PISTORIS_H

// Public C API: ABI-stable ARX_* / arx_pistoris_* naming
// NOLINTBEGIN(readability-identifier-naming)

#ifdef ARX_PISTORIS_CPP_API
#error "arx_pistoris.h is the C ABI header; use pistoris.hpp for the C++ API target."
#endif

#include "arx_pistoris/pistoris_types.h"

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

#ifdef _WIN32
#if defined(ARX_PISTORIS_EXPORTS)
#define ARX_API __declspec(dllexport)
#else
#define ARX_API __declspec(dllimport)
#endif
#else
#define ARX_API __attribute__((visibility("default")))
#endif

// free returned buffers with the paired arx_pistoris_free_*, never free() or delete[]
// null native handle values/output slots return ARX_INVALID_HANDLE
// null byte/string/array/size storage pointers return ARX_INVALID_DATA_POINTER

typedef struct arx_ftl_data* ArxFtlHandle;
typedef struct arx_tea_data* ArxTeaHandle;

ARX_API const char* arx_pistoris_version(void);

ARX_API const char* arx_pistoris_build_time(void);

ARX_API const char* arx_pistoris_strerror(ArxReturnCode rc);

// public-header layout hash; detect ABI mismatch across hot reloads
ARX_API const char* arx_pistoris_get_layout_hash(void);

ARX_API void arx_pistoris_set_log_callback(ArxLogFn fn, void* userdata);

ARX_API ArxReturnCode arx_pistoris_ftl_parse(const uint8_t* data, size_t size, ArxFtlHandle* out);
ARX_API void arx_pistoris_ftl_free(ArxFtlHandle h);

// static geometry only; groups/actions/selections dropped
ARX_API ArxReturnCode arx_pistoris_ftl_to_obj(ArxFtlHandle h, const char* obj_stem, char** out);
ARX_API ArxReturnCode arx_pistoris_ftl_to_mtl(ArxFtlHandle h, char** out);
ARX_API void arx_pistoris_free_string(char* s);

// pretty != 0: 2-space indent
ARX_API ArxReturnCode arx_pistoris_ftl_to_json(ArxFtlHandle h, int pretty, char** out);
ARX_API ArxReturnCode arx_pistoris_ftl_from_json(const uint8_t* data, size_t size, ArxFtlHandle* out);
ARX_API ArxReturnCode arx_pistoris_tea_to_json(ArxTeaHandle h, int pretty, char** out);
ARX_API ArxReturnCode arx_pistoris_tea_from_json(const uint8_t* data, size_t size, ArxTeaHandle* out);

// dead fields from original game data are zero-padded
ARX_API ArxReturnCode arx_pistoris_ftl_write(ArxFtlHandle h, uint8_t** out, size_t* out_size);
ARX_API void arx_pistoris_free_bytes(uint8_t* p);

ARX_API ArxReturnCode arx_pistoris_tea_parse(const uint8_t* data, size_t size, ArxTeaHandle* out);
ARX_API void arx_pistoris_tea_free(ArxTeaHandle h);

// dead fields zero-padded; audio dropped, sample_size = 0
ARX_API ArxReturnCode arx_pistoris_tea_write(ArxTeaHandle h, uint8_t** out, size_t* out_size);

// pointer to anim_name (null-terminated, lifetime tied to h); NULL if h is null; do not free
ARX_API const char* arx_pistoris_tea_name(ArxTeaHandle h);

// teas/tea_count: NULL/0 = mesh+skeleton only
ARX_API ArxReturnCode arx_pistoris_to_glb(ArxFtlHandle ftl, const ArxTeaHandle* teas, size_t tea_count, uint8_t** out,
                                          size_t* out_size);

// out_teas: heap array of out_tea_count handles, free with arx_pistoris_free_tea_array
// glb_filename: sets header.name = "arx_pistoris\FILENAME"; may be null
// after argument validation succeeds, outputs are reset to null/0 before parsing
ARX_API ArxReturnCode arx_pistoris_from_glb(const uint8_t* data, size_t size, const char* glb_filename,
                                            ArxFtlHandle* out_ftl, ArxTeaHandle** out_teas, size_t* out_tea_count);
ARX_API void arx_pistoris_free_tea_array(ArxTeaHandle* teas, size_t count);

ARX_API ArxReturnCode arx_pistoris_ftl_validate(ArxFtlHandle ftl_h);
ARX_API ArxReturnCode arx_pistoris_tea_validate(ArxTeaHandle tea_h);

// debug aid: replace existing texture containers with one filename (truncated to buffer)
// empty FTL texture container list: no-op
ARX_API ArxReturnCode arx_pistoris_ftl_overwrite_texture_paths(ArxFtlHandle ftl_h, const char* path);

// in-place; euler XYZ intrinsic (deg), M = Rx*Ry*Rz*diag(sx,sy,sz)
// FTL positions: M*p + t; normals: M^-T*n; header.origin vertex skips translation
// TEA root motion: M*p + t; group deltas: M*d
ARX_API ArxReturnCode arx_pistoris_ftl_apply_xform(ArxFtlHandle ftl_h, float rx, float ry, float rz, float sx, float sy,
                                                   float sz, float tx, float ty, float tz);
ARX_API ArxReturnCode arx_pistoris_tea_apply_xform(ArxTeaHandle tea_h, float rx, float ry, float rz, float sx, float sy,
                                                   float sz, float tx, float ty, float tz);

// deterministic FTL repair helpers; target is mutated in place, reference is read-only
ARX_API ArxReturnCode arx_pistoris_ftl_snap_bone_origins_to_reference(ArxFtlHandle target, ArxFtlHandle reference);
ARX_API ArxReturnCode arx_pistoris_ftl_snap_action_points_to_reference(ArxFtlHandle target, ArxFtlHandle reference);
ARX_API ArxReturnCode arx_pistoris_ftl_copy_synthetic_selection_affiliations(ArxFtlHandle target,
                                                                             ArxFtlHandle reference);

// obj_filename sets header.name = "arx_pistoris\FILENAME"; may be null
// mtl_data/mtl_size: NULL/0 for no MTL; NULL/nonzero is invalid
ARX_API ArxReturnCode arx_pistoris_obj_parse(const uint8_t* obj_data, size_t obj_size, const uint8_t* mtl_data,
                                             size_t mtl_size, const char* obj_filename, ArxFtlHandle* out);

#ifdef __cplusplus
}
#endif

// NOLINTEND(readability-identifier-naming)

#endif /*ARX_PISTORIS_H*/
