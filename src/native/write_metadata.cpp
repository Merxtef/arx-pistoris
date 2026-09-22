// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#include "native/write_metadata.h"

#include "arx_pistoris/base/status.h"

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <cstring>
#include <limits>
#include <string_view>

namespace pistoris {
namespace {

constexpr std::string_view kWriter = "arx-pistoris";

std::int32_t currentUnixTime() noexcept {
  const auto seconds =
      std::chrono::duration_cast<std::chrono::seconds>(std::chrono::system_clock::now().time_since_epoch()).count();
  if (seconds <= 0) return 0;
  constexpr std::int32_t kMax = std::numeric_limits<std::int32_t>::max();
  if (seconds >= kMax) return kMax;
  return static_cast<std::int32_t>(seconds);
}

}  // namespace

ArxReturnCode nativeWriteMetadata(std::string_view signer, NativeWriteMetadata& out) noexcept {
  for (const unsigned char value : signer)
    if (value < 0x20U || value > 0x7eU) return ARX_INVALID_OPTIONS;

  NativeWriteMetadata result;
  std::memcpy(result.last_user.data(), kWriter.data(), kWriter.size());
  result.last_user_size = kWriter.size();

  if (!signer.empty()) {
    const std::size_t capacity = result.last_user.size() - result.last_user_size - 2;
    const std::size_t size = std::min(signer.size(), capacity);
    if (size != 0) {
      result.last_user[result.last_user_size++] = '/';
      std::memcpy(result.last_user.data() + result.last_user_size, signer.data(), size);
      result.last_user_size += size;
    }
  }
  result.modified_at = currentUnixTime();
  out = result;
  return ARX_OK;
}

}  // namespace pistoris
