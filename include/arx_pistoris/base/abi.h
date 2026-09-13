// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#ifndef ARX_PISTORIS_BASE_ABI_H
#define ARX_PISTORIS_BASE_ABI_H

// Public C ABI naming
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

// NOLINTEND(readability-identifier-naming)

#endif /* ARX_PISTORIS_BASE_ABI_H */
