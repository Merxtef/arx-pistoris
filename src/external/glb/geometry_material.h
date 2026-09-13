// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#pragma once

#include "arx_pistoris/base/flags.h"

#include "cgltf/cgltf.h"
#include "external/glb/utils/texture.h"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

namespace pistoris::glb {

enum class GeometryMaterialError : std::uint8_t {
  kNone,
  kBadMaterial,
  kBadAlpha,
  kUnsupportedFeature,
};

struct GeometryMaterial {
  std::string_view fallback_stem = "no_tex";
  std::optional<TextureImportRequest> texture;
  FaceType flags = 0;
  float transval = 0.0f;
  std::int32_t uv_set = 0;
};

struct GeometryMaterialInfo {
  std::size_t duplicate_flags = 0;
  bool normalized_blend = false;
  bool stripped_quad = false;
  bool normalized_mask = false;
  bool no_tex_with_image = false;
};

struct ExportedGeometryMaterial {
  std::string name;
  float alpha = 1.0f;
  bool alpha_cutout = false;
  bool unknown_alpha = false;
  bool nonstandard_transval = false;
};

GeometryMaterialError decodeGeometryMaterial(const cgltf_material* source, GeometryMaterial& out,
                                             GeometryMaterialInfo* out_info = nullptr) noexcept;
bool isStandardGeometryTransparency(FaceType flags, float transval) noexcept;
ExportedGeometryMaterial exportGeometryMaterial(std::string_view fallback_stem, FaceType flags, float transval,
                                                TextureAlpha texture_alpha);

}  // namespace pistoris::glb
