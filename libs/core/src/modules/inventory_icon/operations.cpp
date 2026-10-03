// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#include "modules/inventory_icon.h"

#include <cassert>
#include <cstdint>
#include <utility>
#include <vector>

namespace pistoris::inventory_icon {

void setImage(InventoryIconData& icon, std::vector<std::uint8_t> encoded, std::uint8_t width_slots,
              std::uint8_t height_slots) noexcept {
  assert(!encoded.empty());
  assert(width_slots != 0 && width_slots <= kMaxSlots);
  assert(height_slots != 0 && height_slots <= kMaxSlots);
  icon.encoded_image = std::move(encoded);
  icon.width_slots = width_slots;
  icon.height_slots = height_slots;
}

void clear(InventoryIconData& icon) noexcept {
  icon.encoded_image.clear();
  icon.width_slots = 0;
  icon.height_slots = 0;
}

}  // namespace pistoris::inventory_icon
