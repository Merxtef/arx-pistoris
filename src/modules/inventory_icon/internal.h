// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#pragma once

#include <cstdint>

namespace pistoris::inventory_icon {

void deriveFootprint(std::uint32_t width_pixels, std::uint32_t height_pixels, std::uint8_t& out_width_slots,
                     std::uint8_t& out_height_slots) noexcept;
void resolveFootprint(std::uint32_t width_pixels, std::uint32_t height_pixels, std::int8_t width_slots,
                      std::int8_t height_slots, std::uint8_t& out_width_slots, std::uint8_t& out_height_slots) noexcept;

}  // namespace pistoris::inventory_icon
