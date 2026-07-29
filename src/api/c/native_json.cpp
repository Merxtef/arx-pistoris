// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#include "arx_pistoris/api.h"
#include "arx_pistoris/native.h"
#include "arx_pistoris/pistoris_types.h"

#include "api/c_api_internal.h"
#include "external/json.h"

#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <string_view>

namespace {

template <typename Handle, typename Export>
ArxReturnCode exportJson(const Handle* handle, std::uint32_t pretty, char** out_json, Export&& export_json) noexcept {
  if (!handle) return ARX_INVALID_HANDLE;
  if (!out_json) return ARX_INVALID_DATA_POINTER;
  *out_json = nullptr;

  return pistoris::c_api::guard([&]() -> ArxReturnCode {
    std::string result;
    ArxReturnCode rc = export_json(handle->value, pretty != 0, result);
    if (rc != ARX_OK) return rc;
    return pistoris::c_api::publishString(result, out_json);
  });
}

template <typename Handle, typename Export>
ArxReturnCode exportJson(const Handle* handle, std::uint32_t pretty, ArxStringView signer, char** out_json,
                         Export&& export_json) noexcept {
  if (!handle) return ARX_INVALID_HANDLE;
  if (!pistoris::c_api::valid(signer) || !out_json) return ARX_INVALID_DATA_POINTER;
  *out_json = nullptr;

  return pistoris::c_api::guard([&]() -> ArxReturnCode {
    std::string result;
    ArxReturnCode rc = export_json(handle->value, pretty != 0, pistoris::c_api::stringView(signer), result);
    if (rc != ARX_OK) return rc;
    return pistoris::c_api::publishString(result, out_json);
  });
}

template <typename Handle, typename Import>
ArxReturnCode importJson(const std::uint8_t* data, std::size_t size, Handle** out_handle,
                         Import&& import_json) noexcept {
  if (!data) return ARX_INVALID_DATA_POINTER;
  if (!out_handle) return ARX_INVALID_DATA_POINTER;
  *out_handle = nullptr;

  return pistoris::c_api::guard([&]() -> ArxReturnCode {
    auto result = std::make_unique<Handle>();
    const std::string_view json(reinterpret_cast<const char*>(data), size);
    ArxReturnCode rc = import_json(json, &result->value);
    if (rc != ARX_OK) return rc;
    *out_handle = result.release();
    return ARX_OK;
  });
}

}  // namespace

// NOLINTBEGIN(readability-identifier-naming)

ArxReturnCode arx_pistoris_dlf_to_json(const ArxDlf* dlf, std::uint32_t pretty, ArxStringView signer,
                                       char** out_json) noexcept {
  return exportJson(dlf, pretty, signer, out_json, pistoris::exportDlfToJson);
}

ArxReturnCode arx_pistoris_dlf_from_json(const std::uint8_t* data, std::size_t size, ArxDlf** out_dlf) noexcept {
  return importJson(data, size, out_dlf, pistoris::importJsonToDlf);
}

ArxReturnCode arx_pistoris_fts_to_json(const ArxFts* fts, std::uint32_t pretty, char** out_json) noexcept {
  return exportJson(fts, pretty, out_json, pistoris::exportFtsToJson);
}

ArxReturnCode arx_pistoris_fts_from_json(const std::uint8_t* data, std::size_t size, ArxFts** out_fts) noexcept {
  return importJson(data, size, out_fts, pistoris::importJsonToFts);
}

ArxReturnCode arx_pistoris_llf_to_json(const ArxLlf* llf, std::uint32_t pretty, ArxStringView signer,
                                       char** out_json) noexcept {
  return exportJson(llf, pretty, signer, out_json, pistoris::exportLlfToJson);
}

ArxReturnCode arx_pistoris_llf_from_json(const std::uint8_t* data, std::size_t size, ArxLlf** out_llf) noexcept {
  return importJson(data, size, out_llf, pistoris::importJsonToLlf);
}

// NOLINTEND(readability-identifier-naming)
