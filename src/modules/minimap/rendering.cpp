// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#include "arx_pistoris/base/math.h"

#include "modules/minimap.h"
#include "utils/encoded_image.h"

#include <array>
#include <cmath>
#include <cstdint>
#include <limits>
#include <optional>
#include <span>
#include <vector>

namespace pistoris::minimap {
namespace {

constexpr double kPixelSnapEpsilon = 1.0e-4;

double snapPixel(double value) noexcept {
  const double rounded = std::round(value);
  return std::abs(value - rounded) <= kPixelSnapEpsilon ? rounded : value;
}

bool pixelEdge(double value, bool upper, double& out) noexcept {
  value = snapPixel(value);
  if (!std::isfinite(value)) return false;
  out = upper ? std::ceil(value) : std::floor(value);
  return std::isfinite(out);
}

std::uint8_t colorByte(float value) noexcept {
  return static_cast<std::uint8_t>(std::lround(static_cast<double>(value) * 255.0));
}

Error imageError(image::Error error) noexcept {
  if (error == image::Error::kNone) return Error::kNone;
  return error == image::Error::kOutOfMemory ? Error::kOutOfMemory : Error::kBadImage;
}

Error renderPixelBounds(std::span<const std::uint8_t> encoded, double left_value, double top_value, double right_value,
                        double bottom_value, const ArxColor3& fill_color, const std::optional<ArxColor3>& border_color,
                        std::vector<std::uint8_t>& out, RenderInfo* info) {
  double left = 0.0;
  double top = 0.0;
  double right = 0.0;
  double bottom = 0.0;
  if (!pixelEdge(left_value, false, left) || !pixelEdge(top_value, false, top) ||
      !pixelEdge(right_value, true, right) || !pixelEdge(bottom_value, true, bottom) || right <= left ||
      bottom <= top) {
    return Error::kBadBounds;
  }
  RenderInfo rendered{
      .padded = left > 0 || top > 0,
      .cropped = left < 0 || top < 0,
      .invisible = right <= 0 || bottom <= 0,
  };
  if (rendered.invisible) {
    out.clear();
    if (info != nullptr) *info = rendered;
    return Error::kNone;
  }
  const double width_value = right - left;
  const double height_value = bottom - top;
  if (right > image::kMaxDimension || bottom > image::kMaxDimension || width_value > image::kMaxDimension ||
      height_value > image::kMaxDimension) {
    return Error::kBadBounds;
  }

  const auto left_pixel = static_cast<std::int64_t>(left);
  const auto top_pixel = static_cast<std::int64_t>(top);
  const auto right_pixel = static_cast<std::uint32_t>(right);
  const auto bottom_pixel = static_cast<std::uint32_t>(bottom);
  const auto width = static_cast<std::uint32_t>(width_value);
  const auto height = static_cast<std::uint32_t>(height_value);

  const std::array<std::uint8_t, 4> fill = {
      colorByte(fill_color.r), colorByte(fill_color.g), colorByte(fill_color.b), 0xff};
  std::optional<std::array<std::uint8_t, 4>> border;
  if (border_color)
    border = std::array<std::uint8_t, 4>{
        colorByte(border_color->r), colorByte(border_color->g), colorByte(border_color->b), 0xff};
  const image::Error image_error =
      image::placeToPng(encoded,
                        right_pixel,
                        bottom_pixel,
                        {.x = left_pixel, .y = top_pixel, .width = width, .height = height},
                        fill,
                        out,
                        border);
  if (image_error != image::Error::kNone) return imageError(image_error);
  if (info != nullptr) *info = rendered;
  return Error::kNone;
}

}  // namespace

Error projectedBounds(std::span<const std::uint8_t> encoded, const ArxAabb& referenced_bounds,
                      const ArxVector2& projection_offset, ArxRect& out) noexcept {
  if (!std::isfinite(referenced_bounds.min.x) || !std::isfinite(referenced_bounds.max.z) ||
      !std::isfinite(projection_offset.x) || !std::isfinite(projection_offset.y)) {
    return Error::kBadBounds;
  }
  image::Info info;
  const image::Error image_error = image::inspect(encoded, &info);
  if (image_error != image::Error::kNone) return imageError(image_error);

  const double left = static_cast<double>(referenced_bounds.min.x) - projection_offset.x;
  const double top = static_cast<double>(referenced_bounds.max.z) + projection_offset.y;
  const double right = left + static_cast<double>(info.width) * kArxUnitsPerPixel;
  const double bottom = top - static_cast<double>(info.height) * kArxUnitsPerPixel;
  constexpr double kLowest = std::numeric_limits<float>::lowest();
  constexpr double kHighest = std::numeric_limits<float>::max();
  if (!std::isfinite(left) || !std::isfinite(top) || !std::isfinite(right) || !std::isfinite(bottom) ||
      left < kLowest || left > kHighest || right < kLowest || right > kHighest || bottom < kLowest ||
      bottom > kHighest || top < kLowest || top > kHighest) {
    return Error::kBadBounds;
  }
  const ArxRect bounds{
      .min = {static_cast<float>(left), static_cast<float>(bottom)},
      .max = {static_cast<float>(right), static_cast<float>(top)},
  };
  const Error error = validateBounds(bounds);
  if (error != Error::kNone) return error;
  out = bounds;
  return Error::kNone;
}

Error compactProjectionOffset(const MinimapData& minimap, const ArxAabb& referenced_bounds, ArxVector2& out) noexcept {
  if (minimap.encoded_image.empty()) {
    out = {};
    return Error::kNone;
  }
  if (!std::isfinite(referenced_bounds.min.x) || !std::isfinite(referenced_bounds.max.z)) return Error::kBadBounds;
  const double x = static_cast<double>(referenced_bounds.min.x) - minimap.world_xz_bounds.min.x;
  const double y = static_cast<double>(minimap.world_xz_bounds.max.y) - referenced_bounds.max.z;
  constexpr double kLowest = std::numeric_limits<float>::lowest();
  constexpr double kHighest = std::numeric_limits<float>::max();
  if (!std::isfinite(x) || !std::isfinite(y) || x < kLowest || x > kHighest || y < kLowest || y > kHighest)
    return Error::kBadBounds;
  out = {static_cast<float>(x), static_cast<float>(y)};
  if (std::abs(static_cast<double>(out.x) / kArxUnitsPerPixel) <= kPixelSnapEpsilon) out.x = 0.0f;
  if (std::abs(static_cast<double>(out.y) / kArxUnitsPerPixel) <= kPixelSnapEpsilon) out.y = 0.0f;
  return Error::kNone;
}

Error renderPng(const MinimapData& minimap, const ArxAabb& referenced_bounds, const RenderOptions& options,
                std::vector<std::uint8_t>& out, RenderInfo* info) {
  if (info != nullptr) *info = {};
  Error error = validateRenderOptions(options);
  if (error != Error::kNone) return error;
  if (minimap.encoded_image.empty()) {
    out.clear();
    return Error::kNone;
  }

  const ArxRect& bounds = minimap.world_xz_bounds;
  const double left_value = (static_cast<double>(bounds.min.x) - referenced_bounds.min.x) / kArxUnitsPerPixel +
                            static_cast<double>(options.projection_offset.x) / kArxUnitsPerPixel;
  const double top_value = (static_cast<double>(referenced_bounds.max.z) - bounds.max.y) / kArxUnitsPerPixel +
                           static_cast<double>(options.projection_offset.y) / kArxUnitsPerPixel;
  const double right_value = (static_cast<double>(bounds.max.x) - referenced_bounds.min.x) / kArxUnitsPerPixel +
                             static_cast<double>(options.projection_offset.x) / kArxUnitsPerPixel;
  const double bottom_value = (static_cast<double>(referenced_bounds.max.z) - bounds.min.y) / kArxUnitsPerPixel +
                              static_cast<double>(options.projection_offset.y) / kArxUnitsPerPixel;
  return renderPixelBounds(minimap.encoded_image,
                           left_value,
                           top_value,
                           right_value,
                           bottom_value,
                           options.fill_color,
                           options.border_color,
                           out,
                           info);
}

Error reprojectPng(std::span<const std::uint8_t> encoded, const ArxVector2& source_projection_offset,
                   const RenderOptions& options, std::vector<std::uint8_t>& out, RenderInfo* info) {
  if (info != nullptr) *info = {};
  Error error = validateRenderOptions(options);
  if (error != Error::kNone || !std::isfinite(source_projection_offset.x) || !std::isfinite(source_projection_offset.y))
    return Error::kInvalidOptions;
  image::Info image_info;
  const image::Error inspect_error = image::inspectMetadata(encoded, image_info);
  if (inspect_error != image::Error::kNone) return imageError(inspect_error);

  const double left =
      (static_cast<double>(options.projection_offset.x) - source_projection_offset.x) / kArxUnitsPerPixel;
  const double top =
      (static_cast<double>(options.projection_offset.y) - source_projection_offset.y) / kArxUnitsPerPixel;
  RenderInfo rendered;
  error = renderPixelBounds(encoded,
                            left,
                            top,
                            left + image_info.width,
                            top + image_info.height,
                            options.fill_color,
                            options.border_color,
                            out,
                            &rendered);
  if (error == Error::kNone && rendered.invisible) {
    const image::Error validation_error = image::inspect(encoded);
    if (validation_error != image::Error::kNone) return imageError(validation_error);
  }
  if (info != nullptr) *info = rendered;
  return error;
}

}  // namespace pistoris::minimap
