// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#pragma once

#include "arx_pistoris/native/text.hpp"

#include "utils/utf8.h"

#include <algorithm>
#include <cstddef>
#include <cstring>
#include <string>
#include <string_view>

namespace pistoris::native_text {

[[nodiscard]] bool decode(std::string_view raw, NativeTextMode mode, std::string& out);
[[nodiscard]] bool encode(std::string_view utf8, NativeTextMode mode, std::string& out);
[[nodiscard]] bool validMode(NativeTextMode mode) noexcept;
[[nodiscard]] std::string diagnostic(std::string_view raw);

template <std::size_t N>
[[nodiscard]] bool encodeFixed(std::string_view text, NativeTextMode mode, char (&out)[N]) {
  static_assert(N != 0);
  std::string encoded;
  if (!encode(text, mode, encoded) || encoded.size() >= N) return false;
  if (!encoded.empty()) std::memcpy(out, encoded.data(), encoded.size());
  std::memset(out + encoded.size(), 0, N - encoded.size());
  return true;
}

template <std::size_t N>
[[nodiscard]] bool encodeTruncated(std::string_view text, NativeTextMode mode, char (&out)[N]) {
  static_assert(N != 0);
  std::string encoded;
  if (!encode(text, mode, encoded)) return false;
  const std::size_t limit = N - 1;
  const std::size_t size =
      mode == NativeTextMode::kLatin1 ? std::min(encoded.size(), limit) : utf8::prefixSize(encoded, limit);
  if (size != 0) std::memcpy(out, encoded.data(), size);
  std::memset(out + size, 0, N - size);
  return true;
}

}  // namespace pistoris::native_text
