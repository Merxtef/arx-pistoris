// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#ifndef ARX_PISTORIS_RUNTIME_H
#define ARX_PISTORIS_RUNTIME_H

#include "arx_pistoris/base/abi.h"
#include "arx_pistoris/base/status.h"
#include "arx_pistoris/runtime/types.h"

// Public C ABI naming
// NOLINTBEGIN(readability-identifier-naming)

ARX_EXTERN_C_BEGIN

ARX_API const char* arx_pistoris_version(void) ARX_NOEXCEPT;
ARX_API const char* arx_pistoris_build_time(void) ARX_NOEXCEPT;
ARX_API const char* arx_pistoris_strerror(ArxReturnCode rc) ARX_NOEXCEPT;
/* Public-header layout hash for ABI checks */
ARX_API const char* arx_pistoris_layout_hash(void) ARX_NOEXCEPT;
ARX_API void arx_pistoris_set_log_callback(ArxLogFn fn, void* userdata) ARX_NOEXCEPT;

ARX_EXTERN_C_END

// NOLINTEND(readability-identifier-naming)

#endif /* ARX_PISTORIS_RUNTIME_H */
