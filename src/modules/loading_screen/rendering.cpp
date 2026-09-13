// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#include "modules/loading_screen.h"
#include "utils/encoded_image.h"

#include <cstdint>
#include <span>
#include <vector>

namespace pistoris::loading_screen {

Error renderPng(const LoadingScreenData& loading_screen, bool fullscreen, std::vector<std::uint8_t>& out) {
  return renderPng(loading_screen.encoded_image, fullscreen, out);
}

Error renderPng(std::span<const std::uint8_t> encoded, bool fullscreen, std::vector<std::uint8_t>& out) {
  if (encoded.empty()) {
    out.clear();
    return Error::kNone;
  }
  const std::uint32_t width = fullscreen ? kFullscreenWidth : kWidth;
  const std::uint32_t height = fullscreen ? kFullscreenHeight : kHeight;
  const image::Error image_error =
      image::fitToPng(encoded, width, height, image::FitMode::kStretch, out, image::BmpColorKey::kNone);
  if (image_error == image::Error::kOutOfMemory) return Error::kOutOfMemory;
  return image_error == image::Error::kNone ? Error::kNone : Error::kBadImage;
}

Error transcodePng(const LoadingScreenData& loading_screen, std::vector<std::uint8_t>& out) {
  return transcodePng(loading_screen.encoded_image, out);
}

Error transcodePng(std::span<const std::uint8_t> encoded, std::vector<std::uint8_t>& out) {
  if (encoded.empty()) {
    out.clear();
    return Error::kNone;
  }
  const image::Error image_error = image::transcodeToPng(encoded, out);
  if (image_error == image::Error::kOutOfMemory) return Error::kOutOfMemory;
  return image_error == image::Error::kNone ? Error::kNone : Error::kBadImage;
}

}  // namespace pistoris::loading_screen
