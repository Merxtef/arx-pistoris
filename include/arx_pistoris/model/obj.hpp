// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#pragma once

#include "arx_pistoris/base/indices.h"
#include "arx_pistoris/base/status.h"

#include <cstdint>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace pistoris {

struct ObjMaterialLibraryView {
  std::string_view path;
  std::string_view text;
};

struct ObjTextureFile {
  TextureIndex source_texture = kNoTexture;
  std::string path;
  std::vector<std::uint8_t> encoded_image;
};

struct ObjExportOptions {
  // Include encoded sidecars in output bundle
  bool include_files = true;
};

struct ObjBundle {
  std::string text;
  std::string mtl;
  std::vector<ObjTextureFile> texture_files;
};

[[nodiscard]] ArxReturnCode objMaterialLibraryPaths(std::string_view obj, std::vector<std::string>& out) noexcept;

}  // namespace pistoris
