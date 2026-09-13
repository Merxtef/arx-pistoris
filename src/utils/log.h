// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#pragma once

#include "arx_pistoris/runtime/types.h"

#include <format>
#include <new>
#include <string>
#include <utility>

namespace pistoris {

extern ArxLogFn log_fn;
extern void* log_ud;

inline void log(ArxLogLevel level, const char* msg) noexcept {
  try {
    if (log_fn) log_fn(level, msg, log_ud);
  } catch (...) {
    // Callback exceptions cannot cross public ABI
    return;
  }
}

inline void log(ArxLogLevel level, const std::string& msg) noexcept { log(level, msg.c_str()); }

template <class... Args>
void log(ArxLogLevel level, std::format_string<Args...> format, Args&&... args) noexcept {
  if (!log_fn) return;
  try {
    log(level, std::format(format, std::forward<Args>(args)...));
  } catch (const std::bad_alloc&) {
    log(level, "Log message omitted: allocation failed");
  } catch (...) {
    log(level, "Log message omitted: formatting failed");
  }
}

template <class Builder>
void logLazy(ArxLogLevel level, Builder&& builder) noexcept {
  if (!log_fn) return;
  try {
    log(level, std::forward<Builder>(builder)());
  } catch (const std::bad_alloc&) {
    log(level, "Log message omitted: allocation failed");
  } catch (...) {
    log(level, "Log message omitted: formatting failed");
  }
}

}  // namespace pistoris
