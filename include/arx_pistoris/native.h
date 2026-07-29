// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#ifndef ARX_PISTORIS_NATIVE_H
#define ARX_PISTORIS_NATIVE_H

#include "arx_pistoris/buffer.h"
#include "arx_pistoris/pistoris_types.h"

// Public C API: ABI-stable ARX_* / arx_pistoris_* naming
// NOLINTBEGIN(readability-identifier-naming)

typedef struct arx_pistoris_dlf ArxDlf;
typedef struct arx_pistoris_fts ArxFts;
typedef struct arx_pistoris_llf ArxLlf;

typedef struct ArxDlfWriteOptions {
  const ArxLlf* embedded_llf;
  ArxStringView signer;
} ArxDlfWriteOptions;

#define ARX_DLF_WRITE_OPTIONS_INIT {NULL, {NULL, 0}}

typedef struct ArxLlfWriteOptions {
  ArxStringView signer;
} ArxLlfWriteOptions;

#define ARX_LLF_WRITE_OPTIONS_INIT {{NULL, 0}}

ARX_EXTERN_C_BEGIN

/* Parsers copy input bytes; successful outputs are independently owned handles */
/* DLF embedded lighting is returned separately; *out_embedded_llf is NULL when absent */
ARX_API ArxReturnCode arx_pistoris_dlf_parse(const uint8_t* data, size_t size, ArxDlf** out_dlf,
                                             ArxLlf** out_embedded_llf) ARX_NOEXCEPT;
ARX_API ArxReturnCode arx_pistoris_fts_parse(const uint8_t* data, size_t size, ArxFts** out_fts) ARX_NOEXCEPT;
ARX_API ArxReturnCode arx_pistoris_llf_parse(const uint8_t* data, size_t size, ArxLlf** out_llf) ARX_NOEXCEPT;

/* JSON strings are NUL-terminated, caller-owned, and freed with arx_pistoris_free_string */
/* pretty == 0 writes compact JSON; every other value enables two-space indentation */
ARX_API ArxReturnCode arx_pistoris_dlf_to_json(const ArxDlf* dlf, uint32_t pretty, ArxStringView signer,
                                               char** out_json) ARX_NOEXCEPT;
ARX_API ArxReturnCode arx_pistoris_dlf_from_json(const uint8_t* data, size_t size, ArxDlf** out_dlf) ARX_NOEXCEPT;
ARX_API ArxReturnCode arx_pistoris_fts_to_json(const ArxFts* fts, uint32_t pretty, char** out_json) ARX_NOEXCEPT;
ARX_API ArxReturnCode arx_pistoris_fts_from_json(const uint8_t* data, size_t size, ArxFts** out_fts) ARX_NOEXCEPT;
ARX_API ArxReturnCode arx_pistoris_llf_to_json(const ArxLlf* llf, uint32_t pretty, ArxStringView signer,
                                               char** out_json) ARX_NOEXCEPT;
ARX_API ArxReturnCode arx_pistoris_llf_from_json(const uint8_t* data, size_t size, ArxLlf** out_llf) ARX_NOEXCEPT;

/* compress == 0 writes raw storage; every other value enables native compression */
/* Successful byte buffers are owned by the caller and freed with arx_pistoris_free_bytes */
ARX_API ArxReturnCode arx_pistoris_dlf_write(const ArxDlf* dlf, const ArxDlfWriteOptions* options, uint32_t compress,
                                             uint8_t** out_data, size_t* out_size) ARX_NOEXCEPT;
ARX_API ArxReturnCode arx_pistoris_fts_write(const ArxFts* fts, uint32_t compress, uint8_t** out_data,
                                             size_t* out_size) ARX_NOEXCEPT;
ARX_API ArxReturnCode arx_pistoris_llf_write(const ArxLlf* llf, const ArxLlfWriteOptions* options, uint32_t compress,
                                             uint8_t** out_data, size_t* out_size) ARX_NOEXCEPT;

ARX_API ArxReturnCode arx_pistoris_dlf_validate(const ArxDlf* dlf) ARX_NOEXCEPT;
ARX_API ArxReturnCode arx_pistoris_fts_validate(const ArxFts* fts) ARX_NOEXCEPT;
ARX_API ArxReturnCode arx_pistoris_llf_validate(const ArxLlf* llf) ARX_NOEXCEPT;

ARX_API void arx_pistoris_dlf_destroy(ArxDlf* dlf) ARX_NOEXCEPT;
ARX_API void arx_pistoris_fts_destroy(ArxFts* fts) ARX_NOEXCEPT;
ARX_API void arx_pistoris_llf_destroy(ArxLlf* llf) ARX_NOEXCEPT;

ARX_EXTERN_C_END

// NOLINTEND(readability-identifier-naming)

#endif /* ARX_PISTORIS_NATIVE_H */
