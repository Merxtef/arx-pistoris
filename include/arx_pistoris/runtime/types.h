// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#ifndef ARX_PISTORIS_RUNTIME_TYPES_H
#define ARX_PISTORIS_RUNTIME_TYPES_H

#include <stdint.h>

// NOLINTBEGIN(readability-identifier-naming, performance-enum-size)

typedef int32_t ArxLogLevel;
enum { ARX_LOG_DEBUG = 0, ARX_LOG_INFO = 1, ARX_LOG_WARN = 2, ARX_LOG_ERROR = 3 };

// fn = NULL disables logging (default)
typedef void (*ArxLogFn)(ArxLogLevel level, const char* msg, void* userdata);

// NOLINTEND(readability-identifier-naming, performance-enum-size)

#endif /* ARX_PISTORIS_RUNTIME_TYPES_H */
