// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#pragma once

#include "arx_pistoris/pistoris_types.h"

#include "cgltf/cgltf.h"
#include "modules/geometry.h"

#include <cstdint>
#include <map>
#include <span>
#include <string_view>
#include <vector>

namespace pistoris::glb {

struct PreparedImage {
  std::vector<std::uint8_t> encoded;
  geometry::ImageInfo info;
};

struct EmbeddedImage {
  std::vector<std::uint8_t> encoded;
  geometry::ImageFormat format = geometry::ImageFormat::kUnknown;
};

using EmbeddedImageCache = std::map<const cgltf_image*, EmbeddedImage>;

geometry::ImageError prepareImage(std::span<const std::uint8_t> source, PreparedImage& out);
std::string_view imageMimeType(geometry::ImageFormat format) noexcept;
bool isDataUri(std::string_view uri) noexcept;
ArxReturnCode readEmbeddedImage(const cgltf_image& source, EmbeddedImageCache& cache,
                                const std::vector<std::uint8_t>*& out, geometry::ImageFormat& format);

}  // namespace pistoris::glb
