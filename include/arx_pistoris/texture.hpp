// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#pragma once

#include "arx_pistoris/base/indices.h"

#include <cstdint>
#include <string>
#include <vector>

namespace pistoris {

struct NativeTextureBakeOptions {
  // Include encoded sidecars in output bundle
  bool include_files = true;
};

struct NativeTextureFile {
  TextureIndex source_texture = kNoTexture;
  std::string resource_path;
  std::vector<std::uint8_t> encoded_image;
};

}  // namespace pistoris
