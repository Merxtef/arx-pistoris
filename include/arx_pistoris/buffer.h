// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#ifndef ARX_PISTORIS_BUFFER_H
#define ARX_PISTORIS_BUFFER_H

#include "arx_pistoris/api.h"

ARX_EXTERN_C_BEGIN

// Public C API
// NOLINTBEGIN(readability-identifier-naming)

ARX_API void arx_pistoris_free_bytes(uint8_t* data) ARX_NOEXCEPT;
ARX_API void arx_pistoris_free_string(char* data) ARX_NOEXCEPT;

// NOLINTEND(readability-identifier-naming)

ARX_EXTERN_C_END

#endif /* ARX_PISTORIS_BUFFER_H */
