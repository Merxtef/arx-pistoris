// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#include "arx_pistoris/runtime.hpp"

#include "arx_pistoris/base/status.h"
#include "arx_pistoris/runtime/types.h"

#include "api/strerror.h"
#include "utils/log.h"
#include "version.h"

namespace pistoris {

ArxLogFn log_fn = nullptr;
void* log_ud = nullptr;

const char* generatedBuildTime();

const char* version() noexcept { return kVersion; }

const char* buildTime() noexcept { return generatedBuildTime(); }

const char* errorString(ArxReturnCode rc) noexcept { return arx_pistoris_strerror(rc); }

void setLogCallback(ArxLogFn fn, void* userdata) noexcept {
  log_fn = fn;
  log_ud = userdata;
}

}  // namespace pistoris
