// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#include "material.h"

#include "arx_pistoris/flags.h"
#include "arx_pistoris/pistoris_types.h"

#include "external/glb/utils/image.h"
#include "external/mat_name.h"
#include "modules/geometry.h"
#include "palette.h"
#include "utils/log.h"
#include "utils/name_tokens.h"

#include <array>
#include <cctype>
#include <charconv>
#include <cmath>
#include <cstdint>
#include <format>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <system_error>
#include <tuple>
#include <vector>

namespace pistoris::glb_level {

bool LevelRenderKey::operator<(const LevelRenderKey& other) const {
  return std::tie(texture_group, flags, transval) < std::tie(other.texture_group, other.flags, other.transval);
}

namespace {

constexpr std::string_view kTransvalPrefix = "TRANSVAL_";

std::string transvalText(float value) {
  if (value == 0.0f) value = 0.0f;
  std::array<char, 64> buffer{};
  auto [end, error] = std::to_chars(buffer.data(), buffer.data() + buffer.size(), value, std::chars_format::general);
  return error == std::errc{} ? std::string(buffer.data(), end) : std::string("0");
}

bool parseTransval(std::string_view text, float& out) {
  if (text.empty()) return false;
  float value = 0.0f;
  auto [end, error] = std::from_chars(text.data(), text.data() + text.size(), value, std::chars_format::general);
  if (error != std::errc{} || end != text.data() + text.size() || !std::isfinite(value)) return false;
  out = value == 0.0f ? 0.0f : value;
  return true;
}

std::string embeddedImagePath(const cgltf_image& image_source, std::string_view material_stem,
                              geometry::ImageFormat format) {
  if (image_source.name != nullptr) {
    std::string_view filename = pathFilename(image_source.name);
    std::string_view stem = pathStem(filename);
    if (!stem.empty() && !hasDoubleUnderscore(filename))
      return std::string(stem) + std::string(geometry::imageExtension(format));
  }
  return std::string(material_stem) + std::string(geometry::imageExtension(format));
}

ArxReturnCode decodeName(std::string_view name, std::string& stem, FaceType& flags, std::optional<float>& transval,
                         std::uint64_t& skipped_quad_flags) {
  if (name.empty() || name.starts_with("__")) return ARX_GLB_BAD_LEVEL_MATERIAL;
  std::vector<std::string_view> tokens;
  splitDoubleUnderscore(name, tokens);
  stem = std::string(tokens.front());
  if (stem.empty()) return ARX_GLB_BAD_LEVEL_MATERIAL;
  flags = 0;
  for (std::string_view token : std::span<const std::string_view>(tokens).subspan(1)) {
    if (token.empty()) return ARX_GLB_BAD_LEVEL_MATERIAL;
    if (token.starts_with(kTransvalPrefix)) {
      float value = 0.0f;
      if (transval.has_value() || !parseTransval(token.substr(kTransvalPrefix.size()), value))
        return ARX_GLB_BAD_LEVEL_MATERIAL;
      transval = value;
      continue;
    }
    bool matched = false;
    for (const FlagEntry& entry : kFlagNames) {
      if (token != entry.name) continue;
      matched = true;
      if (entry.bit == kFaceBitQuad)
        ++skipped_quad_flags;
      else
        flags |= entry.bit;
      break;
    }
    if (!matched) return ARX_GLB_BAD_LEVEL_MATERIAL;
  }
  return ARX_OK;
}

}  // namespace

std::string materialName(std::string_view stem, FaceType flags, float transval) {
  std::string name = matName(stem, flags);
  if ((flags & kFaceBitTrans) != 0) {
    name += "__";
    name += kTransvalPrefix;
    name += transvalText(transval);
  }
  return name;
}

ArxReturnCode parseImportMaterial(const cgltf_material* material, ImportImageCache& image_cache, ImportMaterial& out,
                                  std::uint64_t& skipped_quad_flags) {
  out = {};
  if (material == nullptr) {
    out.stem = "no_tex";
    return ARX_OK;
  }
  if (material->name == nullptr) return ARX_GLB_BAD_LEVEL_MATERIAL;
  std::optional<float> named_transval;
  ArxReturnCode rc = decodeName(material->name, out.stem, out.flags, named_transval, skipped_quad_flags);
  if (rc != ARX_OK) return rc;
  const bool masked = material->alpha_mode == cgltf_alpha_mode_mask;
  const bool blended = material->alpha_mode == cgltf_alpha_mode_blend;
  const bool alpha_relevant = masked || blended || (out.flags & kFaceBitTrans) != 0;
  const float alpha =
      material->has_pbr_metallic_roughness ? material->pbr_metallic_roughness.base_color_factor[3] : 1.0f;
  if (alpha_relevant && (!std::isfinite(alpha) || alpha < 0.0f || alpha > 1.0f)) return ARX_GLB_BAD_FORMAT;
  if (masked) {
    if (!std::isfinite(material->alpha_cutoff)) return ARX_GLB_BAD_FORMAT;
    if (alpha != 1.0f || material->alpha_cutoff != 0.5f) {
      log(ARX_LOG_WARN,
          std::format("GLB -> Level: material '{}' MASK base alpha {} and cutoff {} normalized to texture alpha "
                      "with cutoff 0.5",
                      material->name,
                      alpha,
                      material->alpha_cutoff));
    }
  }
  if (blended) out.flags |= kFaceBitTrans;
  if ((out.flags & kFaceBitTrans) != 0) {
    out.transval = named_transval.value_or(1.0f - alpha);
  } else if (named_transval.has_value())
    return ARX_GLB_BAD_LEVEL_MATERIAL;
  if (material->double_sided) out.flags |= kFaceBitDoublesided;

  if (out.stem == "arx_portal") {
    out.portal_fallback = true;
    return ARX_OK;
  }
  if (isReservedPaletteStem(out.stem)) return ARX_GLB_BAD_LEVEL_MATERIAL_RESERVED_STEM;
  if (out.stem == "no_tex") {
    if (material->has_pbr_metallic_roughness && material->pbr_metallic_roughness.base_color_texture.texture != nullptr)
      return ARX_GLB_BAD_LEVEL_MATERIAL;
    return ARX_OK;
  }

  if (!material->has_pbr_metallic_roughness) {
    out.path = out.stem;
    return ARX_OK;
  }
  const cgltf_texture_view& view = material->pbr_metallic_roughness.base_color_texture;
  if (view.has_transform) return ARX_GLB_UNSUPPORTED_FEATURE;
  out.uv_set = view.texcoord;
  if (view.texture == nullptr || view.texture->image == nullptr) {
    out.path = out.stem;
    return ARX_OK;
  }
  if (view.texture->extensions_count != 0 || view.texture->image->extensions_count != 0)
    return ARX_GLB_UNSUPPORTED_FEATURE;
  const cgltf_image& image = *view.texture->image;
  out.image_source = &image;
  if (image.uri != nullptr && !glb::isDataUri(image.uri)) {
    out.path = image.uri;
    out.external_image = true;
    if (out.path.empty()) return ARX_GLB_BAD_FORMAT;
    if (hasDoubleUnderscore(out.path)) return ARX_GLB_BAD_LEVEL_MATERIAL;
    return ARX_OK;
  }

  geometry::ImageFormat format = geometry::ImageFormat::kPng;
  rc = glb::readEmbeddedImage(image, image_cache, out.encoded_image, format);
  if (rc != ARX_OK) return rc;
  out.path = embeddedImagePath(image, out.stem, format);
  return ARX_OK;
}

std::string sanitizeMaterialStem(std::string_view path) {
  std::string_view stem = pathStem(path);
  std::string result;
  result.reserve(stem.size());
  bool previous_underscore = false;
  for (char ch : stem) {
    if (ch == '_') {
      if (!previous_underscore) result.push_back(ch);
      previous_underscore = true;
    } else {
      result.push_back(ch);
      previous_underscore = false;
    }
  }
  if (!result.empty() && result.back() == '_') result.pop_back();
  return result;
}

std::string normalizeTexturePath(std::string_view path) {
  std::string result(path);
  for (char& ch : result) {
    if (ch == '/') ch = '\\';
    ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
  }
  return result;
}

}  // namespace pistoris::glb_level
