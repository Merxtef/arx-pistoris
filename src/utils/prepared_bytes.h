// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#pragma once

#include <cstdint>
#include <span>
#include <vector>

namespace pistoris {

struct PreparedBytes {
  std::span<const std::uint8_t> borrowed;
  std::vector<std::uint8_t> converted;

  [[nodiscard]] std::span<const std::uint8_t> data() const noexcept {
    return converted.empty() ? borrowed : std::span<const std::uint8_t>(converted);
  }
};

}  // namespace pistoris
