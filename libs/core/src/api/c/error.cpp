// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#include "arx_pistoris/base/error.h"

#include "api/c/error_internal.h"

// NOLINTBEGIN(readability-identifier-naming)

void arx_pistoris_error_clear(ArxError* error) noexcept { pistoris::c_api::clearError(error); }

// NOLINTEND(readability-identifier-naming)
