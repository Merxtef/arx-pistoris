// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#include "arx_pistoris/base/indices.h"

#include "modules/textures.h"
#include "utils/encoded_image.h"

#include <cassert>
#include <cstddef>
#include <cstdint>
#include <new>
#include <span>
#include <utility>
#include <vector>

namespace pistoris::textures {
void setTexture(TexturesData& textures, TextureIndex index, Texture texture) noexcept {
  assert(static_cast<std::size_t>(index) < textures.textures.size());
  assert(validPath(texture.path));
  textures.textures[index] = std::move(texture);
}

TextureIndex addTexture(TexturesData& textures, Texture texture) {
  assert(textures.textures.size() < static_cast<std::size_t>(kNoTexture));
  assert(validPath(texture.path));
  const TextureIndex index = static_cast<TextureIndex>(textures.textures.size());
  textures.textures.push_back(std::move(texture));
  return index;
}

void setEncodedImage(TexturesData& textures, TextureIndex index, std::vector<std::uint8_t> encoded_image) noexcept {
  assert(static_cast<std::size_t>(index) < textures.textures.size());
  assert(!encoded_image.empty());
  Texture& texture = textures.textures[index];
  texture.encoded_image = std::move(encoded_image);
  texture.external_image_extension.clear();
}

void clearEncodedImage(TexturesData& textures, TextureIndex index) {
  assert(static_cast<std::size_t>(index) < textures.textures.size());
  Texture& texture = textures.textures[index];
  if (!texture.encoded_image.empty())
    texture.external_image_extension = image::extension(image::detectFormat(texture.encoded_image));
  texture.encoded_image.clear();
}

void removeTexture(TexturesData& textures, TextureIndex index) noexcept {
  assert(static_cast<std::size_t>(index) < textures.textures.size());
  textures.textures.erase(textures.textures.begin() + static_cast<std::ptrdiff_t>(index));
}

void replaceTextures(TexturesData& textures, std::vector<Texture>&& replacement) noexcept {
  textures.textures = std::move(replacement);
}

void clear(TexturesData& textures) noexcept { textures.textures.clear(); }

Error compact(TexturesData& textures, std::span<const std::uint8_t> used, std::vector<TextureIndex>& out_remap,
              std::size_t& out_removed) {
  if (used.size() != textures.textures.size()) return Error::kInvalidOptions;
  std::vector<TextureIndex> remap;
  try {
    remap.assign(used.size(), kNoTexture);
  } catch (const std::bad_alloc&) {
    return Error::kOutOfMemory;
  }
  TextureIndex next = 0;
  for (std::size_t index = 0; index < used.size(); ++index) {
    if (used[index] == 0) continue;
    remap[index] = next++;
  }

  const std::size_t original_size = textures.textures.size();
  std::size_t destination = 0;
  for (std::size_t source = 0; source < original_size; ++source) {
    if (used[source] == 0) continue;
    if (source != destination) textures.textures[destination] = std::move(textures.textures[source]);
    ++destination;
  }
  textures.textures.resize(destination);
  out_removed = original_size - destination;
  out_remap = std::move(remap);
  return Error::kNone;
}

}  // namespace pistoris::textures
