// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#include "modules/loading_screen.h"
#include "utils/encoded_image.h"

#include <cstdint>
#include <span>

namespace pistoris::loading_screen {

Error validateImage(std::span<const std::uint8_t> encoded) noexcept {
  if (encoded.empty()) return Error::kBadImage;
  const image::Error error = image::inspect(encoded);
  if (error == image::Error::kOutOfMemory) return Error::kOutOfMemory;
  return error == image::Error::kNone ? Error::kNone : Error::kBadImage;
}

Error validate(const LoadingScreenData& loading_screen) noexcept {
  return loading_screen.encoded_image.empty() ? Error::kNone : validateImage(loading_screen.encoded_image);
}

}  // namespace pistoris::loading_screen
