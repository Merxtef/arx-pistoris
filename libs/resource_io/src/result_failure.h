// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#pragma once

#include "arx_pistoris/base/status.h"
#include "arx_pistoris/resource_io/resource_mounts.hpp"
#include "arx_pistoris/runtime.hpp"

#include "utils/log.h"

#include <filesystem>
#include <format>
#include <new>
#include <source_location>
#include <string>
#include <string_view>
#include <utility>

namespace pistoris::resource_io::detail {

inline std::string_view operationName(ResourceIoOperation operation) noexcept {
  switch (operation) {
    case ResourceIoOperation::kOpenMount:
      return "mount opening";
    case ResourceIoOperation::kRead:
      return "resource reading";
    case ResourceIoOperation::kWrite:
      return "resource writing";
    case ResourceIoOperation::kListDirectory:
      return "resource listing";
    case ResourceIoOperation::kScanCatalog:
      return "catalog scanning";
    case ResourceIoOperation::kClassify:
      return "resource classification";
  }
  return "resource I/O";
}

inline void logFailureFallback(ResourceIoOperation operation, ArxReturnCode code) noexcept {
  log(ARX_LOG_DEBUG,
      "Resource I/O source failure during {}: {} (code {})",
      operationName(operation),
      errorString(code),
      code);
}

template <class T>
[[nodiscard]] ResourceIoResult<T> resourceIoFailure(
    ArxReturnCode code, ResourceIoOperation operation, std::string_view resource_path,
    const std::filesystem::path& native_path, ResourceMountMask mounts, std::string_view detail = {},
    std::source_location where = std::source_location::current()) noexcept {
  if (code == ARX_OK) {
    log(ARX_LOG_DEBUG,
        "ARX_OK passed to Resource I/O failure construction at {}:{} in {}; using ARX_INTERNAL_ERROR",
        where.file_name(),
        where.line(),
        where.function_name());
    code = ARX_INTERNAL_ERROR;
  }
  try {
    auto result = ResourceIoResult<T>::failure(
        code,
        ResourceIoLocation{operation, std::string(resource_path), native_path, mounts, std::nullopt},
        std::string(detail));
    const auto* error = result.error();
    if (!error) {
      logFailureFallback(operation, ARX_INTERNAL_ERROR);
      return ResourceIoResult<T>::failure(ARX_INTERNAL_ERROR, std::nullopt);
    }
    logLazy(ARX_LOG_DEBUG, [&] {
      return std::format("Resource I/O source failure during {}: {} (code {}) at {}:{} in {}",
                         operationName(operation),
                         describeError(*error),
                         error->code(),
                         where.file_name(),
                         where.line(),
                         where.function_name());
    });
    return result;
  } catch (const std::bad_alloc&) {
    logFailureFallback(operation, ARX_BAD_ALLOC);
    return ResourceIoResult<T>::failure(ARX_BAD_ALLOC, std::nullopt);
  } catch (...) {
    logFailureFallback(operation, ARX_INTERNAL_ERROR);
    return ResourceIoResult<T>::failure(ARX_INTERNAL_ERROR, std::nullopt);
  }
}

}  // namespace pistoris::resource_io::detail
