// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#pragma once

#include "arx_pistoris/indices.h"
#include "arx_pistoris/native/dlf.hpp"
#include "arx_pistoris/native/fts.hpp"
#include "arx_pistoris/native/llf.hpp"

#include <cstdint>
#include <string>
#include <vector>

namespace pistoris {

struct NativeTextureFile {
  TextureIndex source_texture = kNoTexture;
  std::string resource_path;
  std::vector<std::uint8_t> encoded_image;
};

// NOLINTNEXTLINE(bugprone-exception-escape): MSVC debug STL misreports a throwing container move
struct NativeLevelBundle {
  fts::Data fts;
  llf::Data llf;
  dlf::Data dlf;
  std::vector<NativeTextureFile> texture_files;
};

}  // namespace pistoris
