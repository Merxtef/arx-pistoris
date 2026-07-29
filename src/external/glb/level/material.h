// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#pragma once

#include "arx_pistoris/flags.h"
#include "arx_pistoris/level.hpp"
#include "arx_pistoris/pistoris_types.h"

#include "cgltf/cgltf.h"
#include "external/glb/utils/image.h"

#include <cstddef>
#include <cstdint>
#include <limits>
#include <map>
#include <string>
#include <string_view>
#include <vector>

namespace pistoris::glb_level {

struct LevelRenderKey {
  std::size_t texture_group = std::numeric_limits<std::size_t>::max();
  FaceType flags = 0;
  float transval = 0.0f;

  bool operator<(const LevelRenderKey& other) const;
};

struct ImportMaterial {
  std::string stem;
  std::string path;
  const cgltf_image* image_source = nullptr;
  const std::vector<std::uint8_t>* encoded_image = nullptr;
  FaceType flags = 0;
  float transval = 0.0f;
  std::int32_t uv_set = 0;
  bool external_image = false;
  bool portal_fallback = false;
};

using ImportImageCache = glb::EmbeddedImageCache;

ArxReturnCode parseImportMaterial(const cgltf_material* material, ImportImageCache& image_cache, ImportMaterial& out,
                                  std::uint64_t& skipped_quad_flags);
std::string materialName(std::string_view stem, FaceType flags, float transval);
std::string sanitizeMaterialStem(std::string_view path);
std::string normalizeTexturePath(std::string_view path);

}  // namespace pistoris::glb_level
