// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#pragma once

#include "arx_pistoris/level.h"
#include "arx_pistoris/level.hpp"
#include "arx_pistoris/level/bake.hpp"
#include "arx_pistoris/native/dlf.hpp"
#include "arx_pistoris/native/fts.hpp"
#include "arx_pistoris/native/llf.hpp"

#include "utils/log.h"

#include <cstring>
#include <memory>
#include <new>
#include <span>
#include <string_view>
#include <utility>
#include <vector>

struct arx_pistoris_dlf {
  pistoris::dlf::Data value;
};

struct arx_pistoris_fts {
  pistoris::fts::Data value;
};

struct arx_pistoris_llf {
  pistoris::llf::Data value;
};

struct arx_pistoris_level {
  pistoris::Level value;
};

struct arx_pistoris_native_texture_files {
  std::vector<pistoris::NativeTextureFile> value;
};

namespace pistoris::c_api {

template <class Fn>
ArxReturnCode guard(const char* where, Fn&& fn) noexcept {
  try {
    return std::forward<Fn>(fn)();
  } catch (const std::bad_alloc&) {
    return ARX_BAD_ALLOC;
  } catch (...) {
    log(ARX_LOG_ERROR, where ? where : "C API: unexpected exception");
    return ARX_INTERNAL_ERROR;
  }
}

template <class Fn>
ArxReturnCode guard(Fn&& fn) noexcept {
  return guard("C API: unexpected exception", std::forward<Fn>(fn));
}

inline bool valid(ArxStringView value) noexcept { return value.data || value.size == 0; }

inline bool valid(ArxEncodedImageView value) noexcept { return value.data || value.size == 0; }

template <class T>
bool valid(const T* data, std::size_t size) noexcept {
  return data || size == 0;
}

inline std::string_view stringView(ArxStringView value) noexcept { return {value.data ? value.data : "", value.size}; }

inline ArxStringView view(std::string_view value) noexcept { return {value.data(), value.size()}; }

inline ArxEncodedImageView view(std::span<const std::uint8_t> value) noexcept { return {value.data(), value.size()}; }

inline ArxReturnCode publishBytes(std::vector<std::uint8_t> data, std::uint8_t** out_data, std::size_t* out_size) {
  std::unique_ptr<std::uint8_t[]> result;
  if (!data.empty()) {
    result = std::make_unique<std::uint8_t[]>(data.size());
    std::memcpy(result.get(), data.data(), data.size());
  }
  *out_size = data.size();
  *out_data = result.release();
  return ARX_OK;
}

inline ArxReturnCode publishString(std::string_view data, char** out_data) {
  auto result = std::make_unique<char[]>(data.size() + 1);
  if (!data.empty()) std::memcpy(result.get(), data.data(), data.size());
  result[data.size()] = '\0';
  *out_data = result.release();
  return ARX_OK;
}

}  // namespace pistoris::c_api
