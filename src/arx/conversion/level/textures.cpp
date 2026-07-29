// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#include "arx_pistoris/indices.h"
#include "arx_pistoris/level.hpp"
#include "arx_pistoris/level/bake.hpp"
#include "arx_pistoris/native/fts.hpp"
#include "arx_pistoris/paths.hpp"
#include "arx_pistoris/pistoris_types.h"

#include "arx/conversion/level/internal.h"
#include "modules/geometry.h"
#include "utils/log.h"
#include "utils/unique_name.h"

#include <cstddef>
#include <cstdint>
#include <format>
#include <span>
#include <string>
#include <string_view>
#include <unordered_set>
#include <utility>
#include <vector>

namespace pistoris::arx_level_conversion {
namespace {

char lowerAscii(char value) {
  if (value >= 'A' && value <= 'Z') return static_cast<char>(value - 'A' + 'a');
  return value;
}

bool validTexturePathComponent(std::string_view component) {
  return !component.empty() && component != "." && component != "..";
}

bool validUnresolvedTexturePathComponent(std::string_view component) {
  if (!validTexturePathComponent(component) || component.back() == '.' || component.back() == ' ' ||
      isPortableReservedName(component)) {
    return false;
  }
  for (unsigned char value : component) {
    if (value < 32) return false;
    switch (value) {
      case '<':
      case '>':
      case ':':
      case '"':
      case '|':
      case '?':
      case '*':
        return false;
      default:
        break;
    }
  }
  return true;
}

bool normalizedTexturePath(std::string_view path, bool sanitize_components, std::string& out,
                           bool* out_sanitized = nullptr) {
  out.clear();
  if (out_sanitized) *out_sanitized = false;
  const bool drive_qualified =
      path.size() >= 2 && ((path[0] >= 'A' && path[0] <= 'Z') || (path[0] >= 'a' && path[0] <= 'z')) && path[1] == ':';
  if (path.empty() || drive_qualified || path.front() == '/' || path.front() == '\\' || path.back() == '/' ||
      path.back() == '\\')
    return false;

  std::string component;
  component.reserve(path.size());
  for (char value : path) {
    if (value == '\0') return false;
    if (value != '/' && value != '\\') {
      component.push_back(value);
      continue;
    }
    if (!validTexturePathComponent(component)) return false;
    if (sanitize_components) {
      std::string sanitized = makeUniquePortableName(component);
      if (out_sanitized && sanitized != component) *out_sanitized = true;
      component = std::move(sanitized);
    } else if (!validUnresolvedTexturePathComponent(component)) {
      return false;
    }
    if (!out.empty()) out.push_back('/');
    out += component;
    component.clear();
  }
  if (!validTexturePathComponent(component)) return false;
  if (sanitize_components) {
    std::string sanitized = makeUniquePortableName(component);
    if (out_sanitized && sanitized != component) *out_sanitized = true;
    component = std::move(sanitized);
  } else if (!validUnresolvedTexturePathComponent(component)) {
    return false;
  }
  if (!out.empty()) out.push_back('/');
  out += component;
  return true;
}

bool normalizedTextureFolder(std::string_view folder, std::string& out) {
  if (folder.empty()) {
    out.clear();
    return true;
  }
  if (!normalizedTexturePath(folder, false, out)) return false;
  for (char& value : out) value = lowerAscii(value);
  out.push_back('/');
  return true;
}

std::string_view filename(std::string_view path) {
  const std::size_t separator = path.find_last_of('/');
  return separator == std::string_view::npos ? path : path.substr(separator + 1);
}

std::string withoutExtension(std::string_view path) {
  const std::size_t separator = path.find_last_of('/');
  const std::size_t dot = path.find_last_of('.');
  if (dot == std::string_view::npos || (separator != std::string_view::npos && dot < separator))
    return std::string(path);
  return std::string(path.substr(0, dot));
}

std::string lowerTexturePath(std::string_view path) {
  std::string out(path);
  for (char& value : out) value = lowerAscii(value);
  return out;
}

void reserveTextureResource(NativeTextureResources& resources, std::string_view path) {
  resources.unavailable.insert(lowerTexturePath(path));
  resources.unavailable.insert(lowerTexturePath(paths::textureFromGame(path)));
}

}  // namespace

ArxReturnCode projectNativeTextures(std::span<const Texture> textures, const Level::NativeBakeOptions& options,
                                    NativeTextureResources& out, NativeBakeWarnings& warnings) {
  if (options.texture_path_mode == NativeTexturePathMode::kPreserve && !options.texture_folder.empty())
    return ARX_FTS_BAD_TEXTURE_PATH;
  if (textures.size() > kFtsMaxTextures) return ARX_FTS_BAD_TEXTURE_COUNT;

  std::string texture_folder;
  if (options.texture_path_mode == NativeTexturePathMode::kRebase &&
      !normalizedTextureFolder(options.texture_folder, texture_folder)) {
    return ARX_FTS_BAD_TEXTURE_PATH;
  }

  std::vector<std::string> references;
  references.reserve(textures.size());
  std::size_t sanitized_paths = 0;
  std::string first_sanitized;
  for (const Texture& texture : textures) {
    std::string normalized;
    const bool may_sanitize = !texture.encoded_image.empty();
    bool sanitized = false;
    if (!normalizedTexturePath(texture.path, may_sanitize, normalized, &sanitized)) return ARX_FTS_BAD_TEXTURE_PATH;
    if (sanitized) {
      ++sanitized_paths;
      if (first_sanitized.empty()) first_sanitized = texture.path + " -> " + normalized;
    }
    std::string reference;
    if (options.texture_path_mode == NativeTexturePathMode::kRebase) {
      reference.reserve(texture_folder.size() + filename(normalized).size());
      reference = texture_folder;
      reference += filename(normalized);
    } else {
      reference = std::move(normalized);
    }
    reference = withoutExtension(reference);
    if (reference.empty()) return ARX_FTS_BAD_TEXTURE_PATH;
    reference = paths::textureToGame(reference);
    if (reference.size() >= sizeof(fts::Texture::fic)) return ARX_FTS_BAD_TEXTURE_PATH;
    references.push_back(std::move(reference));
  }
  if (sanitized_paths != 0) {
    log(ARX_LOG_WARN,
        std::format("Level native bake: sanitized {} native texture path(s) for portable output (first: {})",
                    sanitized_paths,
                    first_sanitized));
  }

  std::unordered_set<std::string> unavailable;
  unavailable.reserve(references.size() * 2U);
  for (const std::string& reference : references) unavailable.insert(lowerTexturePath(reference));

  std::unordered_set<std::string> assigned;
  assigned.reserve(references.size());
  for (std::string& reference : references) {
    const std::string key = lowerTexturePath(reference);
    if (assigned.insert(key).second) continue;
    const std::string unique = makeUniqueName(key, unavailable);
    reference.append(unique.substr(key.size()));
    unavailable.insert(unique);
    assigned.insert(unique);
  }

  NativeTextureResources resources;
  resources.include_texture_files = options.include_texture_files;
  resources.next_fts_id = static_cast<std::int32_t>(textures.size()) + 1;
  resources.families.reserve(textures.size());
  resources.unavailable.reserve(textures.size() * 4U);
  for (std::size_t index = 0; index < textures.size(); ++index) {
    NativeTextureFamily family;
    family.alias_base = paths::textureFromGame(references[index]);
    family.shards.push_back({static_cast<std::int32_t>(index) + 1, references[index]});
    reserveTextureResource(resources, references[index]);

    if (options.include_texture_files && !textures[index].encoded_image.empty()) {
      geometry::ImageInfo info;
      bool rescaled = false;
      const geometry::ImageError image_error =
          geometry::normalizeImageToPowerOfTwo(textures[index].encoded_image, family.encoded_image, &info, &rescaled);
      if (image_error != geometry::ImageError::kNone) {
        return image_error == geometry::ImageError::kOutOfMemory ? ARX_BAD_ALLOC : ARX_LEVEL_BAD_TEXTURE_IMAGE;
      }
      family.image_extension = geometry::imageExtension(info.format);
      if (family.image_extension.empty()) return ARX_LEVEL_BAD_TEXTURE_IMAGE;
      if (rescaled) ++warnings.rescaled_texture_images;
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
  const std::string base_key = lowerTexturePath(family.alias_base);
  std::string resource_path;
  for (;;) {
    const std::string unique_key = makeUniqueName(base_key, textures.unavailable);
    std::string candidate = family.alias_base;
    candidate.append(unique_key.substr(base_key.size()));
    resource_path = paths::textureToGame(candidate);
    const std::string resource_key = lowerTexturePath(resource_path);
    const std::string logical_key = lowerTexturePath(paths::textureFromGame(resource_path));
    if (!textures.unavailable.contains(resource_key) && !textures.unavailable.contains(logical_key)) break;
    textures.unavailable.insert(unique_key);
  }

  if (resource_path.size() >= sizeof(fts::Texture::fic)) return ARX_FTS_BAD_TEXTURE_PATH;
  reserveTextureResource(textures, resource_path);
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

}  // namespace pistoris::arx_level_conversion
