// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#include "modules/inventory_icon.h"
#include "modules/inventory_icon/internal.h"
#include "utils/encoded_image.h"

#include <algorithm>
#include <cassert>
#include <cstdint>
#include <span>

namespace pistoris::inventory_icon {

Error validateImage(std::span<const std::uint8_t> encoded) noexcept {
  if (encoded.empty()) return Error::kBadImage;
  const image::Error error = image::inspect(encoded);
  if (error == image::Error::kOutOfMemory) return Error::kOutOfMemory;
  return error == image::Error::kNone ? Error::kNone : Error::kBadImage;
}

Error validate(const InventoryIconData& icon) noexcept {
  if (icon.encoded_image.empty())
    return icon.width_slots == 0 && icon.height_slots == 0 ? Error::kNone : Error::kBadImage;
  if (icon.width_slots == 0 || icon.width_slots > kMaxSlots || icon.height_slots == 0 || icon.height_slots > kMaxSlots)
    return Error::kBadImage;
  return validateImage(icon.encoded_image);
}

Error validateSetOptions(const SetOptions& options) noexcept {
  const auto valid = [](std::int8_t value) {
    return value == -1 || (value >= 1 && value <= static_cast<std::int8_t>(kMaxSlots));
  };
  if (!valid(options.width_slots) || !valid(options.height_slots)) return Error::kInvalidOptions;
  return Error::kNone;
}

Error validateRenderOptions(const RenderOptions& options) noexcept {
  if (options.width_slots < -1 || options.width_slots > static_cast<std::int8_t>(kMaxSlots) ||
      options.height_slots < -1 || options.height_slots > static_cast<std::int8_t>(kMaxSlots))
    return Error::kInvalidOptions;
  switch (options.layout) {
    case Layout::kCenter:
    case Layout::kTopLeft:
    case Layout::kTopRight:
    case Layout::kBottomLeft:
    case Layout::kBottomRight:
    case Layout::kStretch:
      return Error::kNone;
  }
  return Error::kInvalidOptions;
}

void deriveFootprint(std::uint32_t width_pixels, std::uint32_t height_pixels, std::uint8_t& out_width_slots,
                     std::uint8_t& out_height_slots) noexcept {
  assert(width_pixels != 0 && height_pixels != 0);
  const auto slots = [](std::uint64_t numerator, std::uint64_t denominator) {
    return static_cast<std::uint8_t>(
        std::clamp<std::uint64_t>((numerator + denominator - 1U) / denominator, 1U, kMaxSlots));
  };
  const std::uint32_t longest = std::max(width_pixels, height_pixels);
  if (longest <= static_cast<std::uint32_t>(kSlotPixels) * kMaxSlots) {
    out_width_slots = slots(width_pixels, kSlotPixels);
    out_height_slots = slots(height_pixels, kSlotPixels);
    return;
  }
  out_width_slots = slots(static_cast<std::uint64_t>(kMaxSlots) * width_pixels, longest);
  out_height_slots = slots(static_cast<std::uint64_t>(kMaxSlots) * height_pixels, longest);
}

void resolveFootprint(std::uint32_t width_pixels, std::uint32_t height_pixels, std::int8_t width_slots,
                      std::int8_t height_slots, std::uint8_t& out_width_slots,
                      std::uint8_t& out_height_slots) noexcept {
  assert(width_pixels != 0 && height_pixels != 0);
  assert(width_slots == -1 || (width_slots >= 1 && width_slots <= static_cast<std::int8_t>(kMaxSlots)));
  assert(height_slots == -1 || (height_slots >= 1 && height_slots <= static_cast<std::int8_t>(kMaxSlots)));
  if (width_slots == -1 && height_slots == -1) {
    deriveFootprint(width_pixels, height_pixels, out_width_slots, out_height_slots);
    return;
  }
  const auto slots = [](std::uint64_t numerator, std::uint64_t denominator) {
    return static_cast<std::uint8_t>(
        std::clamp<std::uint64_t>((numerator + denominator - 1U) / denominator, 1U, kMaxSlots));
  };
  if (width_slots == -1)
    width_slots =
        static_cast<std::int8_t>(slots(static_cast<std::uint64_t>(height_slots) * width_pixels, height_pixels));
  if (height_slots == -1)
    height_slots =
        static_cast<std::int8_t>(slots(static_cast<std::uint64_t>(width_slots) * height_pixels, width_pixels));
  out_width_slots = static_cast<std::uint8_t>(width_slots);
  out_height_slots = static_cast<std::uint8_t>(height_slots);
}

Error resolveImageFootprint(std::span<const std::uint8_t> encoded, const SetOptions& options,
                            std::uint8_t& out_width_slots, std::uint8_t& out_height_slots) noexcept {
  assert(validateSetOptions(options) == Error::kNone);
  image::Info info;
  const image::Error error = image::inspect(encoded, &info);
  if (error == image::Error::kOutOfMemory) return Error::kOutOfMemory;
  if (error != image::Error::kNone) return Error::kBadImage;
  resolveFootprint(
      info.width, info.height, options.width_slots, options.height_slots, out_width_slots, out_height_slots);
  return Error::kNone;
}

}  // namespace pistoris::inventory_icon
