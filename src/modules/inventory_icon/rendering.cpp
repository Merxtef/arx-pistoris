// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#include "arx_pistoris/runtime/types.h"

#include "modules/inventory_icon.h"
#include "modules/inventory_icon/internal.h"
#include "utils/encoded_image.h"
#include "utils/log.h"

#include <cstdint>
#include <utility>
#include <vector>

namespace pistoris::inventory_icon {
namespace {

const char* layoutName(Layout layout) noexcept {
  switch (layout) {
    case Layout::kCenter:
      return "center";
    case Layout::kTopLeft:
      return "top-left";
    case Layout::kTopRight:
      return "top-right";
    case Layout::kBottomLeft:
      return "bottom-left";
    case Layout::kBottomRight:
      return "bottom-right";
    case Layout::kStretch:
      return "stretch";
  }
  return "invalid";
}

bool imageFitMode(Layout layout, image::FitMode& out) noexcept {
  switch (layout) {
    case Layout::kCenter:
      out = image::FitMode::kCenter;
      return true;
    case Layout::kTopLeft:
      out = image::FitMode::kTopLeft;
      return true;
    case Layout::kTopRight:
      out = image::FitMode::kTopRight;
      return true;
    case Layout::kBottomLeft:
      out = image::FitMode::kBottomLeft;
      return true;
    case Layout::kBottomRight:
      out = image::FitMode::kBottomRight;
      return true;
    case Layout::kStretch:
      out = image::FitMode::kStretch;
      return true;
  }
  return false;
}

void resolveFootprint(const InventoryIconData& icon, const RenderOptions& options, const image::Info& info,
                      std::uint8_t& width, std::uint8_t& height) noexcept {
  const std::int8_t resolved_width =
      options.width_slots == 0 ? static_cast<std::int8_t>(icon.width_slots) : options.width_slots;
  const std::int8_t resolved_height =
      options.height_slots == 0 ? static_cast<std::int8_t>(icon.height_slots) : options.height_slots;
  inventory_icon::resolveFootprint(info.width, info.height, resolved_width, resolved_height, width, height);
}

}  // namespace

Error renderPng(const InventoryIconData& icon, const RenderOptions& options, std::vector<std::uint8_t>& out) {
  const Error options_error = validateRenderOptions(options);
  if (options_error != Error::kNone) return options_error;
  if (icon.encoded_image.empty()) {
    out.clear();
    return Error::kNone;
  }

  image::Info info;
  image::Error image_error = image::inspectMetadata(icon.encoded_image, info);
  if (image_error == image::Error::kOutOfMemory) return Error::kOutOfMemory;
  if (image_error != image::Error::kNone) return Error::kBadImage;

  std::uint8_t width = 0;
  std::uint8_t height = 0;
  resolveFootprint(icon, options, info, width, height);
  image::FitMode fit_mode = image::FitMode::kCenter;
  if (!imageFitMode(options.layout, fit_mode)) return Error::kInvalidOptions;
  std::vector<std::uint8_t> rendered;
  image_error = image::fitToPng(icon.encoded_image,
                                static_cast<std::uint32_t>(width) * kSlotPixels,
                                static_cast<std::uint32_t>(height) * kSlotPixels,
                                fit_mode,
                                rendered,
                                image::BmpColorKey::kAntialiased);
  if (image_error == image::Error::kOutOfMemory) return Error::kOutOfMemory;
  if (image_error != image::Error::kNone) return Error::kBadImage;
  log(ARX_LOG_DEBUG,
      "Inventory-icon render: source {}x{}, stored {}x{} slots, request {}x{}, output {}x{} slots ({}x{} px), "
      "layout {}",
      info.width,
      info.height,
      icon.width_slots,
      icon.height_slots,
      options.width_slots,
      options.height_slots,
      width,
      height,
      static_cast<std::uint32_t>(width) * kSlotPixels,
      static_cast<std::uint32_t>(height) * kSlotPixels,
      layoutName(options.layout));
  out = std::move(rendered);
  return Error::kNone;
}

Error renderBmp(const InventoryIconData& icon, const RenderOptions& options, std::vector<std::uint8_t>& out) {
  std::vector<std::uint8_t> rendered;
  const Error render_error = renderPng(icon, options, rendered);
  if (render_error != Error::kNone) return render_error;
  if (rendered.empty()) {
    out.clear();
    return Error::kNone;
  }
  const image::Error image_error = image::transcodeToBmp(rendered, out);
  if (image_error == image::Error::kOutOfMemory) return Error::kOutOfMemory;
  return image_error == image::Error::kNone ? Error::kNone : Error::kBadImage;
}

}  // namespace pistoris::inventory_icon
