// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#pragma once

#include "arx_pistoris/base/status.h"

#include "cgltf/cgltf.h"
#include "utils/encoded_image.h"

#include <cstdint>
#include <span>
#include <string_view>
#include <vector>

namespace pistoris::glb {

std::string_view imageMimeType(image::Format format) noexcept;
bool isDataUri(std::string_view uri) noexcept;
ArxReturnCode readEmbeddedImage(const cgltf_image& source, std::vector<std::uint8_t>& out, image::Format& format);

}  // namespace pistoris::glb
