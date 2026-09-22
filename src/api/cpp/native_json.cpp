// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#include "arx_pistoris/base/status.h"
#include "arx_pistoris/native.hpp"
#include "arx_pistoris/native/text.hpp"

#include "api/status_boundary.h"
#include "external/json.h"

#include <source_location>
#include <string>
#include <string_view>
#include <utility>

namespace pistoris {

using api_detail::statusBoundary;

namespace {

template <typename Export>
ArxReturnCode exportJsonImpl(std::string& out, Export&& export_json,
                             std::source_location where = std::source_location::current()) noexcept {
  return statusBoundary(
      [&] {
        std::string result;
        ArxReturnCode rc = export_json(result);
        if (rc == ARX_OK) out = std::move(result);
        return rc;
      },
      where);
}

template <typename Native, typename Import>
ArxReturnCode importJsonImpl(Native& out, Import&& import_json,
                             std::source_location where = std::source_location::current()) noexcept {
  return statusBoundary(
      [&] {
        Native result;
        ArxReturnCode rc = import_json(&result);
        if (rc == ARX_OK) out = std::move(result);
        return rc;
      },
      where);
}

}  // namespace

ArxReturnCode toJson(const Amb& value, std::string& out, bool pretty, NativeTextMode text_mode) noexcept {
  return exportJsonImpl(out, [&](std::string& result) { return exportAmbToJson(value, pretty, text_mode, result); });
}

ArxReturnCode fromJson(std::string_view json, Amb& out, NativeTextMode text_mode) noexcept {
  return importJsonImpl(out, [&](Amb* result) { return importJsonToAmb(json, text_mode, result); });
}

ArxReturnCode toJson(const Dlf& value, std::string& out, bool pretty, std::string_view signer,
                     NativeTextMode text_mode) noexcept {
  return exportJsonImpl(out,
                        [&](std::string& result) { return exportDlfToJson(value, pretty, signer, text_mode, result); });
}

ArxReturnCode fromJson(std::string_view json, Dlf& out, NativeTextMode text_mode) noexcept {
  return importJsonImpl(out, [&](Dlf* result) { return importJsonToDlf(json, text_mode, result); });
}

ArxReturnCode toJson(const Ftl& value, std::string& out, bool pretty, NativeTextMode text_mode) noexcept {
  return exportJsonImpl(out, [&](std::string& result) { return exportFtlToJson(value, pretty, text_mode, result); });
}

ArxReturnCode fromJson(std::string_view json, Ftl& out, NativeTextMode text_mode) noexcept {
  return importJsonImpl(out, [&](Ftl* result) { return importJsonToFtl(json, text_mode, result); });
}

ArxReturnCode toJson(const Fts& value, std::string& out, bool pretty, NativeTextMode text_mode) noexcept {
  return exportJsonImpl(out, [&](std::string& result) { return exportFtsToJson(value, pretty, text_mode, result); });
}

ArxReturnCode fromJson(std::string_view json, Fts& out, NativeTextMode text_mode) noexcept {
  return importJsonImpl(out, [&](Fts* result) { return importJsonToFts(json, text_mode, result); });
}

ArxReturnCode toJson(const Llf& value, std::string& out, bool pretty, std::string_view signer) noexcept {
  return exportJsonImpl(out, [&](std::string& result) { return exportLlfToJson(value, pretty, signer, result); });
}

ArxReturnCode fromJson(std::string_view json, Llf& out) noexcept {
  return importJsonImpl(out, [&](Llf* result) { return importJsonToLlf(json, result); });
}

ArxReturnCode toJson(const Tea& value, std::string& out, bool pretty, NativeTextMode text_mode) noexcept {
  return exportJsonImpl(out, [&](std::string& result) { return exportTeaToJson(value, pretty, text_mode, result); });
}

ArxReturnCode fromJson(std::string_view json, Tea& out, NativeTextMode text_mode) noexcept {
  return importJsonImpl(out, [&](Tea* result) { return importJsonToTea(json, text_mode, result); });
}

}  // namespace pistoris
