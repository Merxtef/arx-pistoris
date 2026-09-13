// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#include "arx_pistoris/base/status.h"
#include "arx_pistoris/native.hpp"

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

ArxReturnCode toJson(const Amb& value, std::string& out, bool pretty) noexcept {
  return exportJsonImpl(out, [&](std::string& result) { return exportAmbToJson(value, pretty, result); });
}

ArxReturnCode fromJson(std::string_view json, Amb& out) noexcept {
  return importJsonImpl(out, [&](Amb* result) { return importJsonToAmb(json, result); });
}

ArxReturnCode toJson(const Dlf& value, std::string& out, bool pretty, std::string_view signer) noexcept {
  return exportJsonImpl(out, [&](std::string& result) { return exportDlfToJson(value, pretty, signer, result); });
}

ArxReturnCode fromJson(std::string_view json, Dlf& out) noexcept {
  return importJsonImpl(out, [&](Dlf* result) { return importJsonToDlf(json, result); });
}

ArxReturnCode toJson(const Ftl& value, std::string& out, bool pretty) noexcept {
  return exportJsonImpl(out, [&](std::string& result) { return exportFtlToJson(value, pretty, result); });
}

ArxReturnCode fromJson(std::string_view json, Ftl& out) noexcept {
  return importJsonImpl(out, [&](Ftl* result) { return importJsonToFtl(json, result); });
}

ArxReturnCode toJson(const Fts& value, std::string& out, bool pretty) noexcept {
  return exportJsonImpl(out, [&](std::string& result) { return exportFtsToJson(value, pretty, result); });
}

ArxReturnCode fromJson(std::string_view json, Fts& out) noexcept {
  return importJsonImpl(out, [&](Fts* result) { return importJsonToFts(json, result); });
}

ArxReturnCode toJson(const Llf& value, std::string& out, bool pretty, std::string_view signer) noexcept {
  return exportJsonImpl(out, [&](std::string& result) { return exportLlfToJson(value, pretty, signer, result); });
}

ArxReturnCode fromJson(std::string_view json, Llf& out) noexcept {
  return importJsonImpl(out, [&](Llf* result) { return importJsonToLlf(json, result); });
}

ArxReturnCode toJson(const Tea& value, std::string& out, bool pretty) noexcept {
  return exportJsonImpl(out, [&](std::string& result) { return exportTeaToJson(value, pretty, result); });
}

ArxReturnCode fromJson(std::string_view json, Tea& out) noexcept {
  return importJsonImpl(out, [&](Tea* result) { return importJsonToTea(json, result); });
}

}  // namespace pistoris
