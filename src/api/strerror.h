// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#pragma once

#include "arx_pistoris/pistoris_types.h"

#ifdef _WIN32
#if defined(ARX_PISTORIS_EXPORTS)
#define ARX_STRERROR_API __declspec(dllexport)
#else
#define ARX_STRERROR_API
#endif
#else
#define ARX_STRERROR_API __attribute__((visibility("default")))
#endif

#ifdef __cplusplus
extern "C" {
#endif

// NOLINTBEGIN(readability-identifier-naming)

ARX_STRERROR_API const char* arx_pistoris_strerror(ArxReturnCode rc);

// NOLINTEND(readability-identifier-naming)

#ifdef __cplusplus
}
#endif
