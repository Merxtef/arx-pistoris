// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#include "routes/level/mounted_textures.h"

#include "arx_pistoris/api.h"
#include "arx_pistoris/indices.h"
#include "arx_pistoris/level.hpp"
#include "arx_pistoris/level/types.h"
#include "arx_pistoris/paths.hpp"
#include "arx_pistoris/pistoris_types.h"

#include "console/logging.h"
#include "io/path_location.h"
#include "io/service.h"
#include "routes/level/invocation.h"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>

namespace cli::level {
namespace {

inline constexpr std::array<std::string_view, 5> kTextureExtensions = {
    ".png",
    ".jpg",
    ".jpeg",
    ".bmp",
    ".tga",
};
inline constexpr std::size_t kDetailedWarningLimit = 8;

struct TextureMountWarnings {
  std::vector<std::string> missing;
  std::vector<std::string> invalid_paths;
  std::vector<std::string> unreadable;
  std::vector<std::string> invalid_images;
  std::vector<std::string> ambiguous;
};

struct TextureLookup {
  std::string path;
  bool has_encoded_image = false;
};

std::string_view stringView(ArxStringView value) { return {value.data, value.size}; }

std::vector<TextureLookup> copyTextureLookups(const pistoris::Level& level) {
  std::vector<ArxLevelTextureView> projected(level.textureCount());
  if (level.copyTextureViews(0, projected.size(), projected.data()) != ARX_OK) return {};

  std::vector<TextureLookup> textures;
  textures.reserve(projected.size());
  for (const ArxLevelTextureView& texture : projected) {
    textures.push_back({std::string(stringView(texture.path)), texture.encoded_image.size != 0});
  }
  return textures;
}

std::vector<ArxLevelFace> copyFaces(const pistoris::Level& level) {
  std::vector<ArxLevelFace> faces(level.faceCount());
  if (level.copyFaces(0, faces.size(), faces.data()) != ARX_OK) return {};
  return faces;
}

std::string textureStem(std::string_view path) {
  std::string stem(path);
  std::size_t separator = stem.find_last_of("/\\");
  std::size_t extension = stem.find_last_of('.');
  if (extension != std::string::npos && (separator == std::string::npos || extension > separator)) {
    stem.erase(extension);
  }
  return stem;
}

std::string textureFilenameStem(std::string_view path) {
  const std::size_t separator = path.find_last_of("/\\");
  if (separator != std::string_view::npos) path.remove_prefix(separator + 1);
  return textureStem(path);
}

std::string warningIdentity(std::string_view path) {
  std::string identity;
  identity.reserve(path.size());
  for (char c : path) {
    if (c == '\\') c = '/';
    if (c >= 'A' && c <= 'Z') c = static_cast<char>(c - 'A' + 'a');
    identity.push_back(c);
  }
  return identity;
}

void appendUnique(std::vector<std::string>& paths, std::string path) {
  const std::string identity = warningIdentity(path);
  auto existing = std::find_if(
      paths.begin(), paths.end(), [&](const std::string& candidate) { return warningIdentity(candidate) == identity; });
  if (existing == paths.end()) paths.push_back(std::move(path));
}

void logPathWarnings(const char* singular, const char* plural, const std::vector<std::string>& paths) {
  if (paths.empty()) return;
  std::string listed;
  const std::size_t count = std::min(paths.size(), kDetailedWarningLimit);
  for (std::size_t i = 0; i < count; ++i) {
    if (i != 0) listed += ", ";
    listed += paths[i];
  }
  if (count != paths.size()) listed += ", ... (+" + std::to_string(paths.size() - count) + ')';

  if (paths.size() == 1)
    cli::log(ARX_LOG_WARN, "%s: %s", singular, listed.c_str());
  else
    cli::log(ARX_LOG_WARN, "%zu %s: %s", paths.size(), plural, listed.c_str());
  if (count != paths.size())
    for (const std::string& path : paths) cli::log(ARX_LOG_DEBUG, "%s: %s", singular, path.c_str());
}

void logWarnings(const TextureMountWarnings& warnings, bool game_resources, bool has_read_mounts) {
  if (game_resources) {
    if (!warnings.missing.empty() && !has_read_mounts) {
      logPathWarnings("Referenced Level texture image was not found because there are no readable mount folders",
                      "referenced Level texture images were not found because there are no readable mount folders",
                      warnings.missing);
    } else {
      logPathWarnings(
          "Level texture image not found in mounts", "Level texture images were not found in mounts", warnings.missing);
    }
  } else {
    logPathWarnings("Level texture image was not found in the flat input folder",
                    "Level texture images were not found in the flat input folder",
                    warnings.missing);
  }
  if (game_resources) {
    logPathWarnings("Level texture path cannot be resolved through mounts",
                    "Level texture paths cannot be resolved through mounts",
                    warnings.invalid_paths);
  } else {
    logPathWarnings("Level texture path cannot be resolved in the flat input folder",
                    "Level texture paths cannot be resolved in the flat input folder",
                    warnings.invalid_paths);
  }
  logPathWarnings(
      "Level texture image could not be read", "Level texture images could not be read", warnings.unreadable);
  logPathWarnings("Level texture image is invalid", "Level texture images are invalid", warnings.invalid_images);
  logPathWarnings("Level texture filename is ambiguous in the flat input folder",
                  "Level texture filenames are ambiguous in the flat input folder",
                  warnings.ambiguous);
}

std::vector<bool> ambiguousFlatTextures(std::span<const TextureLookup> textures, const std::vector<bool>& referenced) {
  std::vector<bool> ambiguous(textures.size(), false);
  std::unordered_map<std::string, std::pair<std::size_t, std::string>> first_by_stem;
  for (std::size_t index = 0; index < textures.size(); ++index) {
    if (!referenced[index] || textures[index].has_encoded_image) continue;
    const std::string logical_key = warningIdentity(textures[index].path);
    const std::string stem_key =
        warningIdentity(textureFilenameStem(pistoris::paths::textureToGame(textures[index].path)));
    auto [entry, inserted] = first_by_stem.emplace(stem_key, std::pair{index, logical_key});
    if (inserted || entry->second.second == logical_key) continue;
    ambiguous[index] = true;
    ambiguous[entry->second.first] = true;
  }
  return ambiguous;
}

}  // namespace

bool hasMissingReferencedTextureImages(const pistoris::Level& level) {
  const std::vector<TextureLookup> textures = copyTextureLookups(level);
  for (const ArxLevelFace& face : copyFaces(level)) {
    if (face.texture != pistoris::kNoTexture && static_cast<std::size_t>(face.texture) < textures.size() &&
        !textures[static_cast<std::size_t>(face.texture)].has_encoded_image) {
      return true;
    }
  }
  return false;
}

void loadMountedTextureImages(pistoris::Level& level, cli::IoService& io) { loadMountedTextureImages(level, io, {}); }

void loadMountedTextureImages(pistoris::Level& level, cli::IoService& io, const TextureInput& input) {
  const std::vector<TextureLookup> textures = copyTextureLookups(level);
  std::vector<bool> referenced(textures.size(), false);
  for (const ArxLevelFace& face : copyFaces(level)) {
    if (face.texture != pistoris::kNoTexture && static_cast<std::size_t>(face.texture) < referenced.size()) {
      referenced[static_cast<std::size_t>(face.texture)] = true;
    }
  }

  TextureMountWarnings warnings;
  const std::vector<bool> ambiguous = input.mode == TextureLookupMode::kFlatFolder
                                          ? ambiguousFlatTextures(textures, referenced)
                                          : std::vector<bool>(textures.size(), false);
  std::size_t loaded = 0;
  for (std::size_t index = 0; index < textures.size(); ++index) {
    if (!referenced[index] || textures[index].has_encoded_image) continue;

    const std::string& logical_path = textures[index].path;
    if (ambiguous[index]) {
      appendUnique(warnings.ambiguous, logical_path);
      continue;
    }
    const std::string game_path = pistoris::paths::textureToGame(logical_path);
    const std::string stem =
        input.mode == TextureLookupMode::kFlatFolder ? textureFilenameStem(game_path) : textureStem(game_path);
    bool selected = false;
    for (std::string_view extension : kTextureExtensions) {
      std::string candidate = stem;
      candidate.append(extension);

      std::vector<std::uint8_t> encoded;
      std::string resolved_path;
      ResourceReadResult result = ResourceReadResult::kInvalidPath;
      if (input.mode == TextureLookupMode::kFlatFolder) {
        PathLocation location;
        std::string error;
        if (io.appendPathLocation(input.folder, candidate, location, error)) {
          result = io.readPath(location, encoded, &resolved_path);
        }
      } else {
        result = io.readResource(candidate, encoded, &resolved_path);
      }
      switch (result) {
        case ResourceReadResult::kSuccess: {
          selected = true;
          ArxReturnCode rc =
              level.setTextureImage(static_cast<pistoris::TextureIndex>(index), {encoded.data(), encoded.size()});
          if (rc == ARX_OK) {
            ++loaded;
          } else {
            std::string warning = logical_path;
            warning.append(" -> ").append(resolved_path);
            appendUnique(warnings.invalid_images, std::move(warning));
          }
          break;
        }
        case ResourceReadResult::kReadFailed: {
          selected = true;
          std::string warning = logical_path;
          warning.append(" -> ").append(resolved_path);
          appendUnique(warnings.unreadable, std::move(warning));
          break;
        }
        case ResourceReadResult::kInvalidPath:
          selected = true;
          appendUnique(warnings.invalid_paths, logical_path);
          break;
        case ResourceReadResult::kNotFound:
          break;
      }
      if (selected) break;
    }
    if (!selected) appendUnique(warnings.missing, logical_path);
  }

  if (loaded != 0) cli::log(ARX_LOG_INFO, "loaded %zu mounted Level texture image(s)", loaded);
  logWarnings(warnings, input.mode == TextureLookupMode::kGameResources, io.hasReadMounts());
}

}  // namespace cli::level
