// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#pragma once

#include "arx_pistoris/base/image.h"

#include <cstdint>

namespace pistoris {

enum class ImageFormat : std::uint8_t {
  kUnknown = ARX_IMAGE_FORMAT_UNKNOWN,
  kJpeg = ARX_IMAGE_FORMAT_JPEG,
  kPng = ARX_IMAGE_FORMAT_PNG,
  kBmp = ARX_IMAGE_FORMAT_BMP,
  kTga = ARX_IMAGE_FORMAT_TGA,
};

}  // namespace pistoris
