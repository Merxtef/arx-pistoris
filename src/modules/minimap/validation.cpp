// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#include "arx_pistoris/base/math.h"

#include "modules/minimap.h"
#include "utils/encoded_image.h"
#include "utils/math/finite.h"

#include <cstdint>
#include <span>

namespace pistoris::minimap {
namespace {

bool validColor(const ArxColor3& color) noexcept {
  return math::finite(color) && color.r >= 0.0f && color.r <= 1.0f && color.g >= 0.0f && color.g <= 1.0f &&
         color.b >= 0.0f && color.b <= 1.0f;
}

}  // namespace

Error validateImage(std::span<const std::uint8_t> encoded) noexcept {
  if (encoded.empty()) return Error::kBadImage;
  const image::Error error = image::inspect(encoded);
  if (error == image::Error::kOutOfMemory) return Error::kOutOfMemory;
  return error == image::Error::kNone ? Error::kNone : Error::kBadImage;
}

Error validateBounds(const ArxRect& bounds) noexcept {
  if (!math::finite(bounds.min) || !math::finite(bounds.max) || bounds.min.x >= bounds.max.x ||
      bounds.min.y >= bounds.max.y) {
    return Error::kBadBounds;
  }
  return Error::kNone;
}

Error validateRenderOptions(const RenderOptions& options) noexcept {
  if (!math::finite(options.projection_offset) || !validColor(options.fill_color) ||
      (options.border_color && !validColor(*options.border_color))) {
    return Error::kInvalidOptions;
  }
  return Error::kNone;
}

Error validateGenerationOptions(const GenerationOptions& options) noexcept {
  if (!validColor(options.foreground.color) || !validColor(options.background.color) ||
      !validColor(options.water.color) || !validColor(options.lava.color) || !validColor(options.halo_color)) {
    return Error::kInvalidOptions;
  }
  return Error::kNone;
}

Error validate(const MinimapData& minimap) noexcept {
  if (minimap.encoded_image.empty()) {
    const ArxRect empty{};
    return minimap.world_xz_bounds.min.x == empty.min.x && minimap.world_xz_bounds.min.y == empty.min.y &&
                   minimap.world_xz_bounds.max.x == empty.max.x && minimap.world_xz_bounds.max.y == empty.max.y
               ? Error::kNone
               : Error::kBadBounds;
  }
  Error error = validateBounds(minimap.world_xz_bounds);
  return error == Error::kNone ? validateImage(minimap.encoded_image) : error;
}

}  // namespace pistoris::minimap
