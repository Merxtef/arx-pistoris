// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#pragma once

#include "utils/encoded_image.h"

#include <cstdint>
#include <span>
#include <vector>

namespace pistoris::image {

struct ImageVariants {
  Info source;
  Info png_info;
  Info power_of_two_info;
  std::vector<std::uint8_t> png;
  std::vector<std::uint8_t> power_of_two;
  bool power_of_two_rescaled = false;
};

Error prepareVariants(std::span<const std::uint8_t> encoded, bool make_png, bool make_power_of_two,
                      BmpColorKey bmp_color_key, ImageVariants& out);

}  // namespace pistoris::image
