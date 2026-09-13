// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#include "geometry_material.h"

#include "arx_pistoris/base/flags.h"

#include "external/glb/utils/texture.h"
#include "external/material_name.h"

#include <cmath>
#include <optional>
#include <string_view>

namespace pistoris::glb {

namespace {

constexpr std::string_view kNoTexture = "no_tex";

}  // namespace

GeometryMaterialError decodeGeometryMaterial(const cgltf_material* source, GeometryMaterial& out,
                                             GeometryMaterialInfo* out_info) noexcept {
  out = {};
  GeometryMaterialInfo info;
  if (source == nullptr) {
    if (out_info) *out_info = info;
    return GeometryMaterialError::kNone;
  }
  if (source->name == nullptr) return GeometryMaterialError::kBadMaterial;
  if (source->extensions_count != 0) return GeometryMaterialError::kUnsupportedFeature;

  material_names::Decoded decoded;
  material_names::DecodeInfo decode_info;
  if (material_names::decode(source->name, false, decoded, &decode_info) != material_names::DecodeError::kNone)
    return GeometryMaterialError::kBadMaterial;
  out.fallback_stem = decoded.fallback_stem;
  out.flags = decoded.flags;
  info.duplicate_flags = decode_info.duplicate_flags;
  if ((out.flags & kFaceBitQuad) != 0) {
    out.flags &= ~kFaceBitQuad;
    info.stripped_quad = true;
  }
  if (source->double_sided) out.flags |= kFaceBitDoublesided;

  const bool masked = source->alpha_mode == cgltf_alpha_mode_mask;
  const bool blended = source->alpha_mode == cgltf_alpha_mode_blend;
  if (source->alpha_mode != cgltf_alpha_mode_opaque && !masked && !blended) return GeometryMaterialError::kBadMaterial;
  const float alpha = source->has_pbr_metallic_roughness ? source->pbr_metallic_roughness.base_color_factor[3] : 1.0f;
  if ((masked || blended || (out.flags & kFaceBitTrans) != 0) &&
      (!std::isfinite(alpha) || alpha < 0.0f || alpha > 1.0f))
    return GeometryMaterialError::kBadAlpha;
  if (masked) {
    if (!std::isfinite(source->alpha_cutoff)) return GeometryMaterialError::kBadAlpha;
    info.normalized_mask = alpha != 1.0f || source->alpha_cutoff != 0.5f;
  }

  if (source->has_pbr_metallic_roughness) {
    const cgltf_texture_view& view = source->pbr_metallic_roughness.base_color_texture;
    if (view.has_transform) return GeometryMaterialError::kUnsupportedFeature;
    if (view.texcoord < 0) return GeometryMaterialError::kBadMaterial;
    out.uv_set = view.texcoord;
    if (view.texture != nullptr && view.texture->image != nullptr) {
      if (view.texture->extensions_count != 0 || view.texture->image->extensions_count != 0)
        return GeometryMaterialError::kUnsupportedFeature;
      info.no_tex_with_image = out.fallback_stem == kNoTexture;
      out.texture = TextureImportRequest{view.texture->image,
                                         info.no_tex_with_image ? std::string_view("texture") : out.fallback_stem};
    }
  }
  if (!out.texture.has_value() && out.fallback_stem != kNoTexture)
    out.texture = TextureImportRequest{nullptr, out.fallback_stem};

  if (blended && (decoded.transval.has_value() || alpha < 1.0f)) out.flags |= kFaceBitTrans;
  if ((out.flags & kFaceBitTrans) != 0) {
    out.transval = decoded.transval.value_or(1.0f - alpha);
  } else if (decoded.transval.has_value()) {
    return GeometryMaterialError::kBadMaterial;
  } else if (blended) {
    info.normalized_blend = true;
  }

  if (out_info) *out_info = info;
  return GeometryMaterialError::kNone;
}

bool isStandardGeometryTransparency(FaceType flags, float transval) noexcept {
  return (flags & kFaceBitTrans) != 0 && std::isfinite(transval) && transval > 0.0f && transval < 1.0f;
}

ExportedGeometryMaterial exportGeometryMaterial(std::string_view fallback_stem, FaceType flags, float transval,
                                                TextureAlpha texture_alpha) {
  ExportedGeometryMaterial result;
  result.name = material_names::encode(fallback_stem, flags, transval);
  const bool transparent = (flags & kFaceBitTrans) != 0;
  const bool standard_transparency = isStandardGeometryTransparency(flags, transval);
  result.alpha = standard_transparency ? 1.0f - transval : 1.0f;
  result.alpha_cutout = !transparent && texture_alpha == TextureAlpha::kPresent;
  result.unknown_alpha = !transparent && texture_alpha == TextureAlpha::kUnknown;
  result.nonstandard_transval = transparent && !standard_transparency;
  return result;
}

}  // namespace pistoris::glb
