// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#include "arx_pistoris/runtime.h"

#include "arx_pistoris/runtime/types.h"

#include "layout_hash_gen.h"
#include "utils/log.h"
#include "version.h"

namespace pistoris {
ArxLogFn log_fn = nullptr;
void* log_ud = nullptr;

const char* generatedBuildTime();
}  // namespace pistoris

// NOLINTBEGIN(readability-identifier-naming)

const char* arx_pistoris_version(void) noexcept { return pistoris::kVersion; }

const char* arx_pistoris_build_time(void) noexcept { return pistoris::generatedBuildTime(); }

const char* arx_pistoris_layout_hash(void) noexcept { return ARX_PISTORIS_LAYOUT_HASH; }

void arx_pistoris_set_log_callback(ArxLogFn fn, void* userdata) noexcept {
  pistoris::log_fn = fn;
  pistoris::log_ud = userdata;
}

// NOLINTEND(readability-identifier-naming)
