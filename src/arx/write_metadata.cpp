// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#include "arx/write_metadata.h"

#include <chrono>
#include <cstdint>
#include <cstring>
#include <limits>
#include <string_view>

namespace pistoris {
namespace {

constexpr std::string_view kWriter = "arx-pistoris";

bool utf8Continuation(char value) noexcept { return (static_cast<unsigned char>(value) & 0xc0U) == 0x80U; }

std::size_t truncatedUtf8Size(std::string_view value, std::size_t capacity) noexcept {
  if (value.size() <= capacity) return value.size();
  std::size_t size = capacity;
  while (size > 0 && utf8Continuation(value[size])) --size;
  return size;
}

std::int32_t currentUnixTime() noexcept {
  const auto seconds =
      std::chrono::duration_cast<std::chrono::seconds>(std::chrono::system_clock::now().time_since_epoch()).count();
  if (seconds <= 0) return 0;
  constexpr std::int32_t kMax = std::numeric_limits<std::int32_t>::max();
  if (seconds >= kMax) return kMax;
  return static_cast<std::int32_t>(seconds);
}

}  // namespace

NativeWriteMetadata nativeWriteMetadata(std::string_view signer) noexcept {
  NativeWriteMetadata result;
  std::memcpy(result.last_user.data(), kWriter.data(), kWriter.size());
  result.last_user_size = kWriter.size();

  const std::size_t nul = signer.find('\0');
  if (nul != std::string_view::npos) signer = signer.substr(0, nul);
  if (!signer.empty()) {
    const std::size_t capacity = result.last_user.size() - result.last_user_size - 2;
    const std::size_t size = truncatedUtf8Size(signer, capacity);
    if (size != 0) {
      result.last_user[result.last_user_size++] = '/';
      std::memcpy(result.last_user.data() + result.last_user_size, signer.data(), size);
      result.last_user_size += size;
    }
  }
  result.modified_at = currentUnixTime();
  return result;
}

}  // namespace pistoris
