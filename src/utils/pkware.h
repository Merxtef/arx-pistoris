// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#pragma once

#include "arx_pistoris/pistoris_types.h"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <vector>

namespace pistoris::pkware {

constexpr std::size_t kMaxDecodedBytes = 256ULL * 1024ULL * 1024ULL;

bool looksLikeDcl(std::span<const std::uint8_t> data) noexcept;
ArxReturnCode compress(std::span<const std::uint8_t> input, std::vector<std::uint8_t>& out);
ArxReturnCode decompress(std::span<const std::uint8_t> input, std::vector<std::uint8_t>& out,
                         std::size_t max_output = kMaxDecodedBytes,
                         std::optional<std::size_t> expected_size = std::nullopt);

}  // namespace pistoris::pkware
