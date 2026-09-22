// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#pragma once

#include "arx_pistoris/base/status.h"
#include "arx_pistoris/base/string_view.h"
#include "arx_pistoris/native/text.hpp"

#include "api/c/native/internal.h"

#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <string_view>

namespace pistoris::c_api {

template <typename Handle, typename Export>
ArxReturnCode toJson(const Handle* handle, std::uint32_t pretty, char** out_json, Export&& export_json) noexcept {
  if (!handle) return ARX_INVALID_HANDLE;
  if (!out_json) return ARX_INVALID_DATA_POINTER;
  *out_json = nullptr;

  return guard([&]() -> ArxReturnCode {
    std::string result;
    ArxReturnCode rc = export_json(handle->value, pretty != 0, result);
    if (rc != ARX_OK) return rc;
    return publishString(result, out_json);
  });
}

template <typename Handle, typename Export>
ArxReturnCode toJson(const Handle* handle, std::uint32_t pretty, NativeTextMode text_mode, char** out_json,
                     Export&& export_json) noexcept {
  if (!handle) return ARX_INVALID_HANDLE;
  if (!out_json) return ARX_INVALID_DATA_POINTER;
  *out_json = nullptr;

  return guard([&]() -> ArxReturnCode {
    std::string result;
    ArxReturnCode rc = export_json(handle->value, pretty != 0, text_mode, result);
    if (rc != ARX_OK) return rc;
    return publishString(result, out_json);
  });
}

template <typename Handle, typename Export>
ArxReturnCode toJson(const Handle* handle, std::uint32_t pretty, ArxStringView signer, char** out_json,
                     Export&& export_json) noexcept {
  if (!handle) return ARX_INVALID_HANDLE;
  if (!valid(signer) || !out_json) return ARX_INVALID_DATA_POINTER;
  *out_json = nullptr;

  return guard([&]() -> ArxReturnCode {
    std::string result;
    ArxReturnCode rc = export_json(handle->value, pretty != 0, stringView(signer), result);
    if (rc != ARX_OK) return rc;
    return publishString(result, out_json);
  });
}

template <typename Handle, typename Export>
ArxReturnCode toJson(const Handle* handle, std::uint32_t pretty, ArxStringView signer, NativeTextMode text_mode,
                     char** out_json, Export&& export_json) noexcept {
  if (!handle) return ARX_INVALID_HANDLE;
  if (!valid(signer) || !out_json) return ARX_INVALID_DATA_POINTER;
  *out_json = nullptr;

  return guard([&]() -> ArxReturnCode {
    std::string result;
    ArxReturnCode rc = export_json(handle->value, pretty != 0, stringView(signer), text_mode, result);
    if (rc != ARX_OK) return rc;
    return publishString(result, out_json);
  });
}

template <typename Handle, typename Import>
ArxReturnCode fromJson(const std::uint8_t* data, std::size_t size, Handle** out_handle, Import&& import_json) noexcept {
  if (!data || !out_handle) return ARX_INVALID_DATA_POINTER;
  *out_handle = nullptr;

  return guard([&]() -> ArxReturnCode {
    auto result = std::make_unique<Handle>();
    const std::string_view json(reinterpret_cast<const char*>(data), size);
    ArxReturnCode rc = import_json(json, &result->value);
    if (rc != ARX_OK) return rc;
    *out_handle = result.release();
    return ARX_OK;
  });
}

template <typename Handle, typename Import>
ArxReturnCode fromJson(const std::uint8_t* data, std::size_t size, NativeTextMode text_mode, Handle** out_handle,
                       Import&& import_json) noexcept {
  if (!data || !out_handle) return ARX_INVALID_DATA_POINTER;
  *out_handle = nullptr;

  return guard([&]() -> ArxReturnCode {
    auto result = std::make_unique<Handle>();
    const std::string_view json(reinterpret_cast<const char*>(data), size);
    ArxReturnCode rc = import_json(json, text_mode, &result->value);
    if (rc != ARX_OK) return rc;
    *out_handle = result.release();
    return ARX_OK;
  });
}

}  // namespace pistoris::c_api
