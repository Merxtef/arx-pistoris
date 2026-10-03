// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#include "arx_pistoris/base/indices.h"
#include "arx_pistoris/runtime/types.h"

#include "modules/textures.h"
#include "utils/encoded_image.h"
#include "utils/log.h"
#include "utils/resource_path.h"

#include <cstddef>
#include <cstdint>
#include <new>
#include <span>
#include <string_view>
#include <unordered_set>

namespace pistoris::textures {
namespace {

bool validImageExtension(std::string_view extension) noexcept {
  if (extension.empty()) return true;
  if (extension.size() < 2 || extension.front() != '.') return false;
  for (std::size_t index = 1; index < extension.size(); ++index) {
    const unsigned char value = static_cast<unsigned char>(extension[index]);
    if ((value < '0' || value > '9') && (value < 'a' || value > 'z')) return false;
  }
  return true;
}

}  // namespace

Error validateTexture(const Texture& texture) noexcept {
  if (!validPath(texture.path)) return Error::kBadTexture;
  if (!validImageExtension(texture.external_image_extension) ||
      (!texture.encoded_image.empty() && !texture.external_image_extension.empty()))
    return Error::kBadImage;
  if (texture.encoded_image.empty()) return Error::kNone;
  const image::Error error = image::inspect(texture.encoded_image);
  if (error == image::Error::kOutOfMemory) return Error::kOutOfMemory;
  return error == image::Error::kNone ? Error::kNone : Error::kBadImage;
}

Error validateTextureCount(std::size_t count) noexcept {
  return count > static_cast<std::size_t>(kNoTexture) ? Error::kTooManyTextures : Error::kNone;
}

Error validateEncodedImage(std::span<const std::uint8_t> encoded) noexcept {
  if (encoded.empty()) return Error::kBadImage;
  const image::Error error = image::inspect(encoded);
  if (error == image::Error::kOutOfMemory) return Error::kOutOfMemory;
  return error == image::Error::kNone ? Error::kNone : Error::kBadImage;
}

Error validate(std::span<const Texture> texture_list) noexcept {
  const Error count_error = validateTextureCount(texture_list.size());
  if (count_error != Error::kNone) {
    log(ARX_LOG_DEBUG,
        "Texture validation: count {} exceeds limit {}",
        texture_list.size(),
        static_cast<std::size_t>(kNoTexture));
    return count_error;
  }
  try {
    std::unordered_set<std::string_view, ResourcePathIdentityHash, ResourcePathIdentityEqual> identities;
    identities.reserve(texture_list.size());
    for (std::size_t index = 0; index < texture_list.size(); ++index) {
      const Texture& texture = texture_list[index];
      const Error error = validateTexture(texture);
      if (error != Error::kNone) {
        log(ARX_LOG_DEBUG,
            "Texture validation: texture {} '{}' is invalid: extension '{}', {} encoded bytes, error {}",
            index,
            texture.path,
            texture.external_image_extension,
            texture.encoded_image.size(),
            static_cast<int>(error));
        return error;
      }
      if (!identities.insert(texture.path).second) {
        log(ARX_LOG_DEBUG, "Texture validation: texture {} duplicates path '{}'", index, texture.path);
        return Error::kDuplicateTexture;
      }
    }
  } catch (const std::bad_alloc&) {
    return Error::kOutOfMemory;
  }
  return Error::kNone;
}

}  // namespace pistoris::textures
