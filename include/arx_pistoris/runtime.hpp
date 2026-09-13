// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#pragma once

#include "arx_pistoris/base/status.h"
#include "arx_pistoris/runtime/types.h"

namespace pistoris {

[[nodiscard]] const char* version() noexcept;
[[nodiscard]] const char* buildTime() noexcept;
[[nodiscard]] const char* errorString(ArxReturnCode rc) noexcept;

void setLogCallback(ArxLogFn fn, void* userdata) noexcept;

}  // namespace pistoris
