// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#ifndef ARX_PISTORIS_API_H
#define ARX_PISTORIS_API_H

#include <stddef.h>
#include <stdint.h>

// Public C API: ABI-stable ARX_* / arx_pistoris_* naming
// NOLINTBEGIN(readability-identifier-naming)

#ifdef _WIN32
#if defined(ARX_PISTORIS_EXPORTS)
#define ARX_API __declspec(dllexport)
#else
#define ARX_API __declspec(dllimport)
#endif
#else
#define ARX_API __attribute__((visibility("default")))
#endif

#ifdef __cplusplus
#define ARX_NOEXCEPT noexcept
#define ARX_EXTERN_C_BEGIN extern "C" {
#define ARX_EXTERN_C_END }
#else
#define ARX_NOEXCEPT
#define ARX_EXTERN_C_BEGIN
#define ARX_EXTERN_C_END
#endif

typedef struct ArxStringView {
  const char* data;
  size_t size;
} ArxStringView;

#define ARX_INVALID_INDEX UINT32_MAX

/* String views are not null-terminated and never own their data; {NULL, 0} is an empty view */

// NOLINTEND(readability-identifier-naming)

#endif /* ARX_PISTORIS_API_H */
