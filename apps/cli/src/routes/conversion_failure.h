// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#pragma once

#include "arx_pistoris/error.hpp"
#include "arx_pistoris/resource_io/location.hpp"
#include "arx_pistoris/runtime.hpp"

#include "console/diagnostics.h"

#include <string>
#include <string_view>

namespace cli {

template <class T>
std::string conversionFailureDescription(const pistoris::resource_io::ResourceIoResult<T>& result) {
  if (const auto* error = result.error()) return pistoris::resource_io::describeError(*error);
  return pistoris::errorString(result.code());
}

inline bool conversionStageFailure(DiagnosticCode code, std::string_view stage) {
  diagnostic(code, "Stage: %.*s", static_cast<int>(stage.size()), stage.data());
  return false;
}

template <class Result>
std::string conversionFailureDescription(const Result& result) {
  if (const auto* error = result.error()) return pistoris::describeError(*error);
  return pistoris::errorString(result.code());
}

template <class Result>
bool conversionStageFailure(DiagnosticCode code, std::string_view stage, const Result& result) {
  const std::string description = conversionFailureDescription(result);
  diagnostic(code, "Stage: %.*s: %s", static_cast<int>(stage.size()), stage.data(), description.c_str());
  return false;
}

inline bool conversionInputFailure(DiagnosticCode code, std::string_view source_kind, std::string_view path) {
  if (path.empty()) {
    diagnostic(code, "%.*s source", static_cast<int>(source_kind.size()), source_kind.data());
    return false;
  }
  diagnostic(code,
             "%.*s source: %.*s",
             static_cast<int>(source_kind.size()),
             source_kind.data(),
             static_cast<int>(path.size()),
             path.data());
  return false;
}

inline bool conversionInputFailure(DiagnosticCode code, std::string_view source_kind, std::string_view path,
                                   std::string_view description) {
  if (path.empty()) {
    diagnostic(code,
               "%.*s source: %.*s",
               static_cast<int>(source_kind.size()),
               source_kind.data(),
               static_cast<int>(description.size()),
               description.data());
    return false;
  }
  diagnostic(code,
             "%.*s source: %.*s: %.*s",
             static_cast<int>(source_kind.size()),
             source_kind.data(),
             static_cast<int>(path.size()),
             path.data(),
             static_cast<int>(description.size()),
             description.data());
  return false;
}

template <class T>
bool conversionInputFailure(DiagnosticCode code, std::string_view source_kind, std::string_view path,
                            const pistoris::resource_io::ResourceIoResult<T>& result) {
  switch (result.code()) {
    case ARX_RESOURCE_IO_NOT_FOUND:
      code = DiagnosticCode::kResourceNotFound;
      break;
    case ARX_RESOURCE_IO_INVALID_PATH:
      code = DiagnosticCode::kResourcePathInvalid;
      break;
    case ARX_RESOURCE_IO_STAT_FAILED:
    case ARX_RESOURCE_IO_OPEN_FAILED:
    case ARX_RESOURCE_IO_READ_FAILED:
      code = DiagnosticCode::kResourceReadFailed;
      break;
    default:
      break;
  }
  const std::string description = conversionFailureDescription(result);
  return conversionInputFailure(code, source_kind, path, std::string_view(description));
}

template <class Result>
bool conversionInputFailure(DiagnosticCode code, std::string_view source_kind, std::string_view path,
                            const Result& result) {
  const std::string description = conversionFailureDescription(result);
  if (path.empty()) {
    diagnostic(code, "%.*s source: %s", static_cast<int>(source_kind.size()), source_kind.data(), description.c_str());
    return false;
  }
  diagnostic(code,
             "%.*s source: %.*s: %s",
             static_cast<int>(source_kind.size()),
             source_kind.data(),
             static_cast<int>(path.size()),
             path.data(),
             description.c_str());
  return false;
}

inline bool conversionOutputFailure(DiagnosticCode code, std::string_view stage) {
  diagnostic(code, "Output stage: %.*s", static_cast<int>(stage.size()), stage.data());
  return false;
}

inline bool conversionOutputFailure(DiagnosticCode code, std::string_view stage, ArxReturnCode failure) {
  const std::string_view description = pistoris::errorString(failure);
  diagnostic(code,
             "Output stage: %.*s: %.*s",
             static_cast<int>(stage.size()),
             stage.data(),
             static_cast<int>(description.size()),
             description.data());
  return false;
}

template <class Result>
bool conversionOutputFailure(DiagnosticCode code, std::string_view stage, const Result& result) {
  const std::string description = conversionFailureDescription(result);
  diagnostic(code, "Output stage: %.*s: %s", static_cast<int>(stage.size()), stage.data(), description.c_str());
  return false;
}

}  // namespace cli
