// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#pragma once

#include <cstdint>
#include <span>
#include <string_view>

namespace cli {

inline std::string_view byteStringView(std::span<const std::uint8_t> data) noexcept {
  return {reinterpret_cast<const char*>(data.data()), data.size()};
}

}  // namespace cli
