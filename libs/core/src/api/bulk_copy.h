// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#pragma once

#include "arx_pistoris/base/status.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <optional>
#include <span>

namespace pistoris::api_detail {

class BulkCopyOutputs {
 public:
  template <class T>
  ArxReturnCode add(std::optional<std::span<T>> output, std::size_t expected) noexcept {
    if (!output || error_ != ARX_OK) return error_;
    ++requested_;
    const auto values = *output;
    if (!values.empty() && values.data() == nullptr) return error_ = ARX_INVALID_DATA_POINTER;
    if (values.size() < expected) return error_ = ARX_BUFFER_TOO_SMALL;
    if (values.size() != expected) return error_ = ARX_INVALID_OPTIONS;
    if (values.empty()) return ARX_OK;
    const auto begin = reinterpret_cast<std::uintptr_t>(values.data());
    if (begin % alignof(T) != 0) return error_ = ARX_INVALID_DATA_POINTER;
    if (values.size() > std::numeric_limits<std::uintptr_t>::max() / sizeof(T))
      return error_ = ARX_INVALID_DATA_POINTER;
    const auto bytes = values.size() * sizeof(T);
    if (begin > std::numeric_limits<std::uintptr_t>::max() - bytes) return error_ = ARX_INVALID_DATA_POINTER;
    const auto end = begin + bytes;
    for (std::size_t i = 0; i < range_count_; ++i)
      if (begin < ranges_[i].end && ranges_[i].begin < end) return error_ = ARX_INVALID_OPTIONS;
    if (range_count_ == ranges_.size()) return error_ = ARX_INVALID_OPTIONS;
    ranges_[range_count_++] = {begin, end};
    return ARX_OK;
  }

  template <class T>
  ArxReturnCode add(std::span<T> output, std::size_t expected) noexcept {
    return add(std::optional{output}, expected);
  }

  [[nodiscard]] ArxReturnCode finish() const noexcept {
    return error_ != ARX_OK ? error_ : requested_ == 0 ? ARX_INVALID_OPTIONS : ARX_OK;
  }

 private:
  struct Range {
    std::uintptr_t begin;
    std::uintptr_t end;
  };
  std::array<Range, 8> ranges_{};
  std::size_t range_count_ = 0;
  std::size_t requested_ = 0;
  ArxReturnCode error_ = ARX_OK;
};

}  // namespace pistoris::api_detail
