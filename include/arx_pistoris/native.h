// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#ifndef ARX_PISTORIS_NATIVE_H
#define ARX_PISTORIS_NATIVE_H

#include "arx_pistoris/base/abi.h"
#include "arx_pistoris/base/buffer.h"
#include "arx_pistoris/base/status.h"
#include "arx_pistoris/base/string_view.h"
#include "arx_pistoris/native/text.h"

#include <stddef.h>
#include <stdint.h>

// Public C ABI naming
// NOLINTBEGIN(readability-identifier-naming)

typedef struct arx_pistoris_amb ArxAmb;
typedef struct arx_pistoris_cin ArxCin;
typedef struct arx_pistoris_dlf ArxDlf;
typedef struct arx_pistoris_ftl ArxFtl;
typedef struct arx_pistoris_fts ArxFts;
typedef struct arx_pistoris_llf ArxLlf;
typedef struct arx_pistoris_tea ArxTea;

typedef struct ArxDlfWriteOptions {
  const ArxLlf* embedded_llf;
  /* Optional printable-ASCII arx-pistoris/<signer> suffix, truncated to field capacity */
  ArxStringView signer;
} ArxDlfWriteOptions;

#define ARX_DLF_WRITE_OPTIONS_INIT {NULL, {NULL, 0}}

typedef struct ArxLlfWriteOptions {
  /* Optional printable-ASCII arx-pistoris/<signer> suffix, truncated to field capacity */
  ArxStringView signer;
} ArxLlfWriteOptions;

#define ARX_LLF_WRITE_OPTIONS_INIT {{NULL, 0}}

ARX_EXTERN_C_BEGIN

// --- Binary I/O ---

/* Readers do not retain input bytes */
/* Successful outputs are independently owned handles */
/* DLF embedded lighting is returned separately when out_embedded_llf is non-NULL */
ARX_API ArxReturnCode arx_pistoris_amb_read(const uint8_t* data, size_t size, ArxAmb** out_amb) ARX_NOEXCEPT;
ARX_API ArxReturnCode arx_pistoris_cin_read(const uint8_t* data, size_t size, ArxCin** out_cin) ARX_NOEXCEPT;
ARX_API ArxReturnCode arx_pistoris_dlf_read(const uint8_t* data, size_t size, ArxDlf** out_dlf,
                                            ArxLlf** out_embedded_llf) ARX_NOEXCEPT;
ARX_API ArxReturnCode arx_pistoris_ftl_read(const uint8_t* data, size_t size, ArxFtl** out_ftl) ARX_NOEXCEPT;
ARX_API ArxReturnCode arx_pistoris_fts_read(const uint8_t* data, size_t size, ArxFts** out_fts) ARX_NOEXCEPT;
ARX_API ArxReturnCode arx_pistoris_llf_read(const uint8_t* data, size_t size, ArxLlf** out_llf) ARX_NOEXCEPT;
ARX_API ArxReturnCode arx_pistoris_tea_read(const uint8_t* data, size_t size, ArxTea** out_tea) ARX_NOEXCEPT;

/* Successful byte buffers are owned by the caller and freed with arx_pistoris_free_bytes */
ARX_API ArxReturnCode arx_pistoris_amb_write(const ArxAmb* amb, uint8_t** out_data, size_t* out_size) ARX_NOEXCEPT;
ARX_API ArxReturnCode arx_pistoris_cin_write(const ArxCin* cin, uint8_t** out_data, size_t* out_size) ARX_NOEXCEPT;
/* compress == 0 writes raw storage; every other value enables native compression */
ARX_API ArxReturnCode arx_pistoris_dlf_write(const ArxDlf* dlf, const ArxDlfWriteOptions* options, uint32_t compress,
                                             uint8_t** out_data, size_t* out_size) ARX_NOEXCEPT;
ARX_API ArxReturnCode arx_pistoris_ftl_write(const ArxFtl* ftl, uint32_t compress, uint8_t** out_data,
                                             size_t* out_size) ARX_NOEXCEPT;
ARX_API ArxReturnCode arx_pistoris_fts_write(const ArxFts* fts, uint32_t compress, uint8_t** out_data,
                                             size_t* out_size) ARX_NOEXCEPT;
ARX_API ArxReturnCode arx_pistoris_llf_write(const ArxLlf* llf, const ArxLlfWriteOptions* options, uint32_t compress,
                                             uint8_t** out_data, size_t* out_size) ARX_NOEXCEPT;
ARX_API ArxReturnCode arx_pistoris_tea_write(const ArxTea* tea, uint8_t** out_data, size_t* out_size) ARX_NOEXCEPT;

// --- JSON conversion ---

/* JSON strings are NUL-terminated, caller-owned, and freed with arx_pistoris_free_string */
/* pretty == 0 writes compact JSON; every other value enables two-space indentation */
/* JSON is UTF-8. text_mode controls conversion to or from the native carrier fields. */
ARX_API ArxReturnCode arx_pistoris_amb_to_json(const ArxAmb* amb, uint32_t pretty, ArxNativeTextMode text_mode,
                                               char** out_json) ARX_NOEXCEPT;
ARX_API ArxReturnCode arx_pistoris_amb_from_json(const uint8_t* data, size_t size, ArxNativeTextMode text_mode,
                                                 ArxAmb** out_amb) ARX_NOEXCEPT;
ARX_API ArxReturnCode arx_pistoris_dlf_to_json(const ArxDlf* dlf, uint32_t pretty, ArxStringView signer,
                                               ArxNativeTextMode text_mode, char** out_json) ARX_NOEXCEPT;
ARX_API ArxReturnCode arx_pistoris_dlf_from_json(const uint8_t* data, size_t size, ArxNativeTextMode text_mode,
                                                 ArxDlf** out_dlf) ARX_NOEXCEPT;
ARX_API ArxReturnCode arx_pistoris_ftl_to_json(const ArxFtl* ftl, uint32_t pretty, ArxNativeTextMode text_mode,
                                               char** out_json) ARX_NOEXCEPT;
ARX_API ArxReturnCode arx_pistoris_ftl_from_json(const uint8_t* data, size_t size, ArxNativeTextMode text_mode,
                                                 ArxFtl** out_ftl) ARX_NOEXCEPT;
ARX_API ArxReturnCode arx_pistoris_fts_to_json(const ArxFts* fts, uint32_t pretty, ArxNativeTextMode text_mode,
                                               char** out_json) ARX_NOEXCEPT;
ARX_API ArxReturnCode arx_pistoris_fts_from_json(const uint8_t* data, size_t size, ArxNativeTextMode text_mode,
                                                 ArxFts** out_fts) ARX_NOEXCEPT;
ARX_API ArxReturnCode arx_pistoris_llf_to_json(const ArxLlf* llf, uint32_t pretty, ArxStringView signer,
                                               char** out_json) ARX_NOEXCEPT;
ARX_API ArxReturnCode arx_pistoris_llf_from_json(const uint8_t* data, size_t size, ArxLlf** out_llf) ARX_NOEXCEPT;
ARX_API ArxReturnCode arx_pistoris_tea_to_json(const ArxTea* tea, uint32_t pretty, ArxNativeTextMode text_mode,
                                               char** out_json) ARX_NOEXCEPT;
ARX_API ArxReturnCode arx_pistoris_tea_from_json(const uint8_t* data, size_t size, ArxNativeTextMode text_mode,
                                                 ArxTea** out_tea) ARX_NOEXCEPT;

// --- Validation ---

ARX_API ArxReturnCode arx_pistoris_amb_validate(const ArxAmb* amb) ARX_NOEXCEPT;
ARX_API ArxReturnCode arx_pistoris_cin_validate(const ArxCin* cin) ARX_NOEXCEPT;
ARX_API ArxReturnCode arx_pistoris_dlf_validate(const ArxDlf* dlf) ARX_NOEXCEPT;
ARX_API ArxReturnCode arx_pistoris_ftl_validate(const ArxFtl* ftl) ARX_NOEXCEPT;
ARX_API ArxReturnCode arx_pistoris_fts_validate(const ArxFts* fts) ARX_NOEXCEPT;
ARX_API ArxReturnCode arx_pistoris_llf_validate(const ArxLlf* llf) ARX_NOEXCEPT;
ARX_API ArxReturnCode arx_pistoris_tea_validate(const ArxTea* tea) ARX_NOEXCEPT;

// --- Destruction ---

ARX_API void arx_pistoris_amb_destroy(ArxAmb* amb) ARX_NOEXCEPT;
ARX_API void arx_pistoris_cin_destroy(ArxCin* cin) ARX_NOEXCEPT;
ARX_API void arx_pistoris_dlf_destroy(ArxDlf* dlf) ARX_NOEXCEPT;
ARX_API void arx_pistoris_ftl_destroy(ArxFtl* ftl) ARX_NOEXCEPT;
ARX_API void arx_pistoris_fts_destroy(ArxFts* fts) ARX_NOEXCEPT;
ARX_API void arx_pistoris_llf_destroy(ArxLlf* llf) ARX_NOEXCEPT;
ARX_API void arx_pistoris_tea_destroy(ArxTea* tea) ARX_NOEXCEPT;

ARX_EXTERN_C_END

// NOLINTEND(readability-identifier-naming)

#endif /* ARX_PISTORIS_NATIVE_H */
