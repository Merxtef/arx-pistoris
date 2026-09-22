// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#include "modules/textures.h"

#include "arx_pistoris/base/indices.h"
#include "arx_pistoris/base/status.h"
#include "arx_pistoris/level.hpp"
#include "arx_pistoris/native/fts.hpp"
#include "arx_pistoris/texture.hpp"

#include "level/native/internal.h"
#include "utils/encoded_image.h"
#include "utils/unique_value.h"

#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#include <utility>
#include <vector>

namespace pistoris::level_native {

ArxReturnCode projectNativeTextures(const TexturesData& texture_data, const Level::NativeBakeOptions& options,
                                    NativeTextureResources& out, NativeBakeWarnings& warnings) {
  if (texture_data.textures.size() > kFtsMaxTextures) return ARX_FTS_BAD_TEXTURE_COUNT;

  std::vector<textures::ImagePreparationRequest> requests;
  if (options.include_texture_files) {
    requests.reserve(texture_data.textures.size());
    for (std::size_t index = 0; index < texture_data.textures.size(); ++index) {
      if (texture_data.textures[index].encoded_image.empty()) continue;
      requests.push_back({static_cast<TextureIndex>(index),
                          {.accepted_formats = image::kFormatsAll,
                           .fallback_format = image::Format::kPng,
                           .require_power_of_two = true}});
    }
  }
  std::vector<textures::PreparedImage> prepared;
  const textures::Error preparation_error = textures::prepareImages(texture_data, requests, prepared);
  if (preparation_error == textures::Error::kInvalidOptions) return ARX_INVALID_OPTIONS;
  if (preparation_error == textures::Error::kOutOfMemory) return ARX_BAD_ALLOC;
  if (preparation_error != textures::Error::kNone) return ARX_LEVEL_BAD_TEXTURE_IMAGE;

  NativeTextureResources resources;
  resources.include_texture_files = options.include_texture_files;
  resources.next_fts_id = static_cast<std::int32_t>(texture_data.textures.size()) + 1;
  resources.families.reserve(texture_data.textures.size());
  resources.unavailable.reserve(texture_data.textures.size() * 4U);
  std::size_t prepared_index = 0;
  for (std::size_t index = 0; index < texture_data.textures.size(); ++index) {
    const Texture& texture = texture_data.textures[index];
    std::string resource_path = textures::normalizePath(texture.path);
    if (resource_path.empty() || !textures::validExternalPath(resource_path)) return ARX_FTS_BAD_TEXTURE_PATH;

    NativeTextureFamily family;
    resources.unavailable.insert(resource_path);
    family.shards.push_back({static_cast<std::int32_t>(index) + 1, std::move(resource_path)});
    if (options.include_texture_files && !texture.encoded_image.empty()) {
      textures::PreparedImage& image = prepared[prepared_index++];
      family.image_extension = pistoris::image::extension(image.info.format);
      if (family.image_extension.empty()) return ARX_LEVEL_BAD_TEXTURE_IMAGE;
      if (image.rescaled) ++warnings.rescaled_texture_images;
      if (image.bytes.converted.empty()) {
        family.encoded_image.assign(image.bytes.borrowed.begin(), image.bytes.borrowed.end());
      } else {
        family.encoded_image = std::move(image.bytes.converted);
      }
    }
    resources.families.push_back(std::move(family));
  }
  out = std::move(resources);
  return ARX_OK;
}

ArxReturnCode addNativeTextureShard(NativeTextureResources& textures, TextureIndex source_texture,
                                    std::size_t& out_shard) {
  if (static_cast<std::size_t>(source_texture) >= textures.families.size()) return ARX_FTS_BAD_TEXTURE_ID;
  if (textures.next_fts_id > static_cast<std::int32_t>(kFtsMaxTextures)) return ARX_FTS_BAD_TEXTURE_COUNT;

  NativeTextureFamily& family = textures.families[static_cast<std::size_t>(source_texture)];
  const std::string& base = family.shards.front().resource_path;
  std::string resource_path = makeUniqueName(base, textures.unavailable);

  textures.unavailable.insert(resource_path);
  out_shard = family.shards.size();
  family.shards.push_back({textures.next_fts_id++, std::move(resource_path)});
  return ARX_OK;
}

void buildNativeTextureFiles(NativeTextureResources& textures, std::vector<NativeTextureFile>& out) {
  out.clear();
  if (!textures.include_texture_files) return;

  std::size_t file_count = 0;
  for (const NativeTextureFamily& family : textures.families)
    if (!family.encoded_image.empty()) file_count += family.shards.size();
  out.reserve(file_count);

  for (std::size_t index = 0; index < textures.families.size(); ++index) {
    NativeTextureFamily& family = textures.families[index];
    if (family.encoded_image.empty()) continue;
    for (std::size_t shard = 0; shard < family.shards.size(); ++shard) {
      NativeTextureFile file;
      file.source_texture = static_cast<TextureIndex>(index);
      file.resource_path = family.shards[shard].resource_path + family.image_extension;
      if (shard + 1U == family.shards.size())
        file.encoded_image = std::move(family.encoded_image);
      else
        file.encoded_image = family.encoded_image;
      out.push_back(std::move(file));
    }
  }
}

}  // namespace pistoris::level_native
