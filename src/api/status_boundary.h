// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#pragma once

#include "arx_pistoris/base/status.h"

#include "utils/log.h"

#include <exception>
#include <new>
#include <source_location>
#include <utility>

namespace pistoris::api_detail {

template <class Fn>
ArxReturnCode statusBoundary(Fn&& fn, std::source_location where = std::source_location::current()) noexcept {
  try {
    return std::forward<Fn>(fn)();
  } catch (const std::bad_alloc&) {
    return ARX_BAD_ALLOC;
  } catch (const std::exception& exception) {
    log(ARX_LOG_ERROR, "Unexpected exception in {}: {}", where.function_name(), exception.what());
    return ARX_INTERNAL_ERROR;
  } catch (...) {
    log(ARX_LOG_ERROR, "Unexpected exception in {}", where.function_name());
    return ARX_INTERNAL_ERROR;
  }
}

}  // namespace pistoris::api_detail
