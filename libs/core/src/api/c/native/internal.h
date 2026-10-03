// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#pragma once

#include "arx_pistoris/native.h"
#include "arx_pistoris/native/amb.hpp"
#include "arx_pistoris/native/cin.hpp"
#include "arx_pistoris/native/dlf.hpp"
#include "arx_pistoris/native/ftl.hpp"
#include "arx_pistoris/native/fts.hpp"
#include "arx_pistoris/native/llf.hpp"
#include "arx_pistoris/native/tea.hpp"

#include "api/c/internal.h"

#include <cstddef>
#include <cstdint>
#include <memory>
#include <span>
#include <string_view>
#include <utility>

struct arx_pistoris_amb {
  pistoris::amb::Data value;
};

struct arx_pistoris_cin {
  pistoris::cin::Data value;
};

struct arx_pistoris_dlf {
  pistoris::dlf::Data value;
};

struct arx_pistoris_ftl {
  pistoris::ftl::Data value;
};

struct arx_pistoris_fts {
  pistoris::fts::Data value;
};

struct arx_pistoris_llf {
  pistoris::llf::Data value;
};

struct arx_pistoris_tea {
  pistoris::tea::Data value;
};

namespace pistoris::c_api {

template <class Handle, class Read>
ArxReturnCode readNative(const std::uint8_t* data, std::size_t size, Handle** out_handle, ArxError* error,
                         Read&& read) noexcept {
  if (!data || !out_handle) return publishCode(ARX_INVALID_DATA_POINTER, error);
  *out_handle = nullptr;
  return guard(error, [&]() -> ArxReturnCode {
    auto loaded = std::forward<Read>(read)(std::span<const std::uint8_t>(data, size));
    if (!loaded) return publish(loaded, error);
    auto result = std::make_unique<Handle>();
    result->value = std::move(*loaded);
    *out_handle = result.release();
    return publish(loaded, error);
  });
}

template <class Handle, class Write>
ArxReturnCode writeNative(const Handle* handle, std::uint8_t** out_data, std::size_t* out_size, ArxError* error,
                          Write&& write) noexcept {
  if (!handle) return publishCode(ARX_INVALID_HANDLE, error);
  if (!out_data || !out_size) return publishCode(ARX_INVALID_DATA_POINTER, error);
  *out_data = nullptr;
  *out_size = 0;
  return guard(error, [&]() -> ArxReturnCode {
    auto written = std::forward<Write>(write)(handle->value);
    if (!written) return publish(written, error);
    const ArxReturnCode code = publishBytes(std::move(*written), out_data, out_size);
    if (code != ARX_OK) return code;
    return publish(written, error);
  });
}

template <class Handle, class Validate>
ArxReturnCode validateNative(const Handle* handle, ArxError* error, Validate&& validate) noexcept {
  if (!handle) return publishCode(ARX_INVALID_HANDLE, error);
  return guard(error, [&] {
    auto result = std::forward<Validate>(validate)(handle->value);
    return publish(result, error);
  });
}

template <class Handle, class Export>
ArxReturnCode nativeToJson(const Handle* handle, char** out_json, ArxError* error, Export&& export_json) noexcept {
  if (!handle) return publishCode(ARX_INVALID_HANDLE, error);
  if (!out_json) return publishCode(ARX_INVALID_DATA_POINTER, error);
  *out_json = nullptr;
  return guard(error, [&]() -> ArxReturnCode {
    auto json = std::forward<Export>(export_json)(handle->value);
    if (!json) return publish(json, error);
    const ArxReturnCode code = publishString(*json, out_json);
    if (code != ARX_OK) return code;
    return publish(json, error);
  });
}

template <class Handle, class Import>
ArxReturnCode nativeFromJson(const std::uint8_t* data, std::size_t size, Handle** out_handle, ArxError* error,
                             Import&& import_json) noexcept {
  if (!data || !out_handle) return publishCode(ARX_INVALID_DATA_POINTER, error);
  *out_handle = nullptr;
  return guard(error, [&]() -> ArxReturnCode {
    const std::string_view json(reinterpret_cast<const char*>(data), size);
    auto imported = std::forward<Import>(import_json)(json);
    if (!imported) return publish(imported, error);
    auto result = std::make_unique<Handle>();
    result->value = std::move(*imported);
    *out_handle = result.release();
    return publish(imported, error);
  });
}

}  // namespace pistoris::c_api
