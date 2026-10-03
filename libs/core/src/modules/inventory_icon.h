// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#pragma once

#include <cstdint>
#include <optional>
#include <span>
#include <vector>

namespace pistoris {

struct InventoryIconData {
  std::vector<std::uint8_t> encoded_image;
  std::uint8_t width_slots = 0;
  std::uint8_t height_slots = 0;
};

namespace image {
enum class Format : std::uint8_t;
}

namespace inventory_icon {

inline constexpr std::uint8_t kSlotPixels = 32;
inline constexpr std::uint8_t kMaxSlots = 3;

enum class Error : std::uint8_t {
  kNone,
  kInvalidOptions,
  kUnsupportedFormat,
  kBadImage,
  kOutOfMemory,
};

struct SetOptions {
  std::optional<std::uint8_t> width_slots;
  std::optional<std::uint8_t> height_slots;
};

enum class Layout : std::uint8_t {
  kCenter = 0,
  kTopLeft,
  kTopRight,
  kBottomLeft,
  kBottomRight,
  kStretch,
};

struct RenderOptions {
  std::optional<std::uint8_t> width_slots;
  std::optional<std::uint8_t> height_slots;
  Layout layout = Layout::kCenter;
};

// --- Validation ---

Error validate(const InventoryIconData& icon) noexcept;
Error validateImage(std::span<const std::uint8_t> encoded) noexcept;
Error validateSetOptions(const SetOptions& options) noexcept;
Error validateRenderOptions(const RenderOptions& options) noexcept;

// --- Queries ---

Error resolveImageFootprint(std::span<const std::uint8_t> encoded, const SetOptions& options,
                            std::uint8_t& out_width_slots, std::uint8_t& out_height_slots) noexcept;

// --- Mutation ---

void setImage(InventoryIconData& icon, std::vector<std::uint8_t> encoded, std::uint8_t width_slots,
              std::uint8_t height_slots) noexcept;
void clear(InventoryIconData& icon) noexcept;

// --- Generation ---

Error render(const InventoryIconData& icon, const RenderOptions& options, image::Format format,
             std::vector<std::uint8_t>& out);

}  // namespace inventory_icon
}  // namespace pistoris
