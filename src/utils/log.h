// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#pragma once

#include "arx_pistoris/pistoris_types.h"

#include <string>

namespace pistoris {

extern ArxLogFn log_fn;
extern void* log_ud;

inline void log(ArxLogLevel level, const char* msg) noexcept {
  try {
    if (log_fn) log_fn(level, msg, log_ud);
  } catch (...) {
    // User callbacks cannot propagate through the public ABI
    return;
  }
}

inline void log(ArxLogLevel level, const std::string& msg) noexcept { log(level, msg.c_str()); }

}  // namespace pistoris
