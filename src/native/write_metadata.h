// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#pragma once

#include "arx_pistoris/base/status.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <string_view>

namespace pistoris {

struct NativeWriteMetadata {
  std::array<char, 256> last_user = {};
  std::size_t last_user_size = 0;
  std::int32_t modified_at = 0;

  std::string_view lastUser() const noexcept { return {last_user.data(), last_user_size}; }
};

[[nodiscard]] ArxReturnCode nativeWriteMetadata(std::string_view signer, NativeWriteMetadata& out) noexcept;

}  // namespace pistoris
