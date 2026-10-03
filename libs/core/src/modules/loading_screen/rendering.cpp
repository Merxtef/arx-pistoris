// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#include "modules/loading_screen.h"
#include "utils/encoded_image.h"

#include <cstdint>
#include <span>
#include <utility>
#include <vector>

namespace pistoris::loading_screen {
namespace {

Error imageError(image::Error error) noexcept {
  if (error == image::Error::kNone) return Error::kNone;
  return error == image::Error::kOutOfMemory ? Error::kOutOfMemory : Error::kBadImage;
}

}  // namespace

Error render(const LoadingScreenData& loading_screen, Layout layout, image::Format format,
             std::vector<std::uint8_t>& out) {
  return render(loading_screen.encoded_image, layout, format, out);
}

Error render(std::span<const std::uint8_t> encoded, Layout layout, image::Format format,
             std::vector<std::uint8_t>& out) {
  if (encoded.empty()) {
    out.clear();
    return Error::kNone;
  }
  if (format != image::Format::kPng && format != image::Format::kBmp && format != image::Format::kTga)
    return Error::kInvalidOptions;
  if (layout == Layout::kOriginal) return imageError(image::transcode(encoded, format, out));
  std::vector<std::uint8_t> png;
  image::Error image_error = image::Error::kMalformed;
  switch (layout) {
    case Layout::kOriginal:
      break;
    case Layout::kNormal:
      image_error = image::fitToPng(encoded, kWidth, kHeight, image::FitMode::kStretch, png, image::BmpColorKey::kNone);
      break;
    case Layout::kFullscreen:
      image_error = image::fitToPng(
          encoded, kFullscreenWidth, kFullscreenHeight, image::FitMode::kStretch, png, image::BmpColorKey::kNone);
      break;
  }
  if (image_error != image::Error::kNone) return imageError(image_error);
  if (format == image::Format::kPng) {
    out = std::move(png);
    return Error::kNone;
  }
  return imageError(image::transcode(png, format, out));
}

}  // namespace pistoris::loading_screen
