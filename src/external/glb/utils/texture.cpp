// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#include "texture.h"

#include "arx_pistoris/base/indices.h"
#include "arx_pistoris/base/status.h"
#include "arx_pistoris/runtime/types.h"

#include "external/glb/utils/image.h"
#include "external/glb/writer.h"
#include "modules/textures.h"
#include "utils/encoded_image.h"
#include "utils/log.h"
#include "utils/path.h"

#include <cassert>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace pistoris::glb {
namespace {

void logPathRepairs(const textures::PathRepairInfo& info, std::string_view log_prefix) {
  for (const textures::PathRepairInfo::Repair& repair : info.repairs)
    log(ARX_LOG_WARN, "{}: texture path '{}' normalized to '{}'", log_prefix, repair.original, repair.repaired);
}

}  // namespace

TextureBindingError decodeTextureBinding(const cgltf_texture_view& view, TextureBinding& out) noexcept {
  TextureBinding result;
  result.image = view.texture != nullptr ? view.texture->image : nullptr;
  result.texcoord = view.texcoord;
  if (view.texture != nullptr && (view.texture->extensions_count != 0 ||
                                  (view.texture->image != nullptr && view.texture->image->extensions_count != 0)))
    return TextureBindingError::kUnsupportedFeature;

  if (view.has_transform) {
    result.transformed = true;
    result.offset = {view.transform.offset[0], view.transform.offset[1]};
    result.scale = {view.transform.scale[0], view.transform.scale[1]};
    result.rotation = view.transform.rotation;
    if (view.transform.has_texcoord) result.texcoord = view.transform.texcoord;
    if (!std::isfinite(result.offset.x) || !std::isfinite(result.offset.y) || !std::isfinite(result.scale.x) ||
        !std::isfinite(result.scale.y) || !std::isfinite(result.rotation))
      return TextureBindingError::kBadBinding;
  }
  if (result.texcoord < 0) return TextureBindingError::kBadBinding;
  out = result;
  return TextureBindingError::kNone;
}

Vec2 transformTexcoord(const TextureBinding& binding, Vec2 value) noexcept {
  if (!binding.transformed) return value;
  value.x *= binding.scale.x;
  value.y *= binding.scale.y;
  const float sine = std::sin(binding.rotation);
  const float cosine = std::cos(binding.rotation);
  return {cosine * value.x - sine * value.y + binding.offset.x, sine * value.x + cosine * value.y + binding.offset.y};
}

TextureImporter::TextureImporter(TexturesData& textures, std::vector<std::string>* source_paths,
                                 std::string_view log_prefix)
    : textures_(textures), source_paths_(source_paths), log_prefix_(log_prefix) {}

TextureIndex TextureImporter::find(const TextureImportRequest& request) const noexcept {
  if (request.image != nullptr) {
    if (request.image->uri != nullptr && !isDataUri(request.image->uri)) {
      const auto found = external_images_.find(request.image->uri);
      return found == external_images_.end() ? kNoTexture : found->second;
    }
    const auto found = embedded_images_.find(request.image);
    return found == embedded_images_.end() ? kNoTexture : found->second;
  }
  const auto found = fallbacks_.find(request.fallback_path);
  return found == fallbacks_.end() ? kNoTexture : found->second;
}

void TextureImporter::insert(const TextureImportRequest& request, TextureIndex texture) {
  if (request.image != nullptr) {
    if (request.image->uri != nullptr && !isDataUri(request.image->uri))
      external_images_.emplace(request.image->uri, texture);
    else
      embedded_images_.emplace(request.image, texture);
  } else {
    fallbacks_.emplace(request.fallback_path, texture);
  }
}

void TextureImporter::appendSourcePath(const TextureImportRequest& request) {
  if (source_paths_ == nullptr) return;
  if (request.image != nullptr && request.image->uri != nullptr && !isDataUri(request.image->uri))
    source_paths_->emplace_back(request.image->uri);
  else
    source_paths_->emplace_back();
}

TextureImportError TextureImporter::import(const TextureImportRequest& request, TextureIndex& out) {
  const TextureIndex existing = find(request);
  if (existing != kNoTexture) {
    out = existing;
    return TextureImportError::kNone;
  }

  Texture texture;
  if (request.image != nullptr && request.image->uri != nullptr && !isDataUri(request.image->uri)) {
    texture = textures::fromImagePath(request.image->uri);
  } else if (request.image != nullptr) {
    std::vector<std::uint8_t> encoded;
    image::Format format = image::Format::kUnknown;
    const ArxReturnCode rc = readEmbeddedImage(*request.image, encoded, format);
    if (rc == ARX_BAD_ALLOC) return TextureImportError::kOutOfMemory;
    if (rc != ARX_OK) return TextureImportError::kBadImage;
    texture = textures::fromImagePath(embeddedTexturePath(*request.image, request.fallback_path, format));
    texture.external_image_extension.clear();
    texture.encoded_image = std::move(encoded);
  } else {
    texture = Texture(request.fallback_path);
  }

  textures::PathRepairInfo repair_info;
  const textures::Error repair_error = textures::repairPaths(std::span<Texture>(&texture, 1), &repair_info);
  if (repair_error == textures::Error::kOutOfMemory) return TextureImportError::kOutOfMemory;
  if (repair_error != textures::Error::kNone) return TextureImportError::kBadPath;
  logPathRepairs(repair_info, log_prefix_);
  if (textures_.textures.size() >= static_cast<std::size_t>(kNoTexture)) return TextureImportError::kTooManyTextures;

  out = static_cast<TextureIndex>(textures_.textures.size());
  textures_.textures.push_back(std::move(texture));
  insert(request, out);
  appendSourcePath(request);
  return TextureImportError::kNone;
}

void exportTexture(Builder& builder, const Texture& texture, const textures::PreparedImage* prepared,
                   ExportedTexture& out) {
  ExportedTexture result;
  if (texture.encoded_image.empty()) {
    std::string uri = texture.path;
    if (texture.external_image_extension.empty()) {
      uri += ".png";
      result.assumed_png = true;
    } else {
      uri += texture.external_image_extension;
    }
    std::string name(pathFilename(uri));
    result.index = builder.addExternalTexture(std::move(name), std::move(uri));
    out = result;
    return;
  }

  assert(prepared != nullptr);
  result.alpha = image::hasAlpha(prepared->info) ? TextureAlpha::kPresent : TextureAlpha::kAbsent;
  std::string name = texture.path;
  name += image::extension(prepared->info.format);
  result.index = builder.addEmbeddedTexture(
      std::move(name), std::string(imageMimeType(prepared->info.format)), prepared->bytes.data());
  out = result;
}

std::string embeddedTexturePath(const cgltf_image& image, std::string_view fallback, pistoris::image::Format format) {
  if (image.name != nullptr) {
    const std::string_view filename = pathFilename(image.name);
    if (!filename.empty()) {
      const Texture texture = textures::fromImagePath(image.name);
      if (!texture.path.empty()) return texture.path + std::string(image::extension(format));
    }
  }
  return std::string(fallback) + std::string(image::extension(format));
}

bool makeTexturePathsUnique(std::span<Texture> textures, std::string_view log_prefix) {
  textures::PathRepairInfo info;
  if (textures::repairPaths(textures, &info) != textures::Error::kNone) return false;
  logPathRepairs(info, log_prefix);
  return true;
}

}  // namespace pistoris::glb
