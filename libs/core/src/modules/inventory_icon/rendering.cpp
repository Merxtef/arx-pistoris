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
  if (!options.width_slots && !options.height_slots) {
    width = icon.width_slots;
    height = icon.height_slots;
    return;
  }
  inventory_icon::resolveFootprint(info.width, info.height, options.width_slots, options.height_slots, width, height);
}

}  // namespace

Error render(const InventoryIconData& icon, const RenderOptions& options, image::Format format,
             std::vector<std::uint8_t>& out) {
  const Error options_error = validateRenderOptions(options);
  if (options_error != Error::kNone) return options_error;
  switch (format) {
    case image::Format::kPng:
    case image::Format::kBmp:
    case image::Format::kTga:
      break;
    case image::Format::kUnknown:
    case image::Format::kJpeg:
      return Error::kUnsupportedFormat;
  }
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
      options.width_slots.value_or(0),
      options.height_slots.value_or(0),
      width,
      height,
      static_cast<std::uint32_t>(width) * kSlotPixels,
      static_cast<std::uint32_t>(height) * kSlotPixels,
      layoutName(options.layout));
  if (format == image::Format::kPng) {
    out = std::move(rendered);
    return Error::kNone;
  }
  image_error =
      format == image::Format::kBmp ? image::transcodeToBmp(rendered, out) : image::transcodeToTga(rendered, out);
  if (image_error == image::Error::kOutOfMemory) return Error::kOutOfMemory;
  return image_error == image::Error::kNone ? Error::kNone : Error::kBadImage;
}

}  // namespace pistoris::inventory_icon
