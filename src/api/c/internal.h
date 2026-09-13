// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#pragma once

#include "arx_pistoris/base/audio.h"
#include "arx_pistoris/base/image.h"
#include "arx_pistoris/base/status.h"
#include "arx_pistoris/base/string_view.h"

#include "api/status_boundary.h"

#include <cstdint>
#include <cstring>
#include <memory>
#include <source_location>
#include <span>
#include <string_view>
#include <utility>
#include <vector>

namespace pistoris::c_api {

template <class Fn>
ArxReturnCode guard(Fn&& fn, std::source_location where = std::source_location::current()) noexcept {
  return api_detail::statusBoundary(std::forward<Fn>(fn), where);
}

inline bool valid(ArxStringView value) noexcept { return value.data || value.size == 0; }

inline bool valid(ArxEncodedImageView value) noexcept { return value.data || value.size == 0; }

inline bool valid(ArxEncodedAudioView value) noexcept { return value.data || value.size == 0; }

template <class T>
bool valid(const T* data, std::size_t size) noexcept {
  return data || size == 0;
}

inline std::string_view stringView(ArxStringView value) noexcept { return {value.data ? value.data : "", value.size}; }

inline ArxStringView view(std::string_view value) noexcept { return {value.data(), value.size()}; }

inline ArxEncodedImageView view(std::span<const std::uint8_t> value) noexcept {
  if (value.empty()) return {};
  return {value.data(), value.size()};
}

inline ArxEncodedAudioView audioView(std::span<const std::uint8_t> value) noexcept {
  if (value.empty()) return {};
  return {value.data(), value.size()};
}

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
