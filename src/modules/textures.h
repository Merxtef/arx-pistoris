// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#pragma once

#include "arx_pistoris/base/indices.h"

#include "utils/encoded_image.h"
#include "utils/prepared_bytes.h"

#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace pistoris {

struct Texture {
  std::string path;
  std::vector<std::uint8_t> encoded_image;
  std::string external_image_extension;

  Texture() = default;
  Texture(std::string value) : path(std::move(value)) {}
  Texture(const char* value) : path(value) {}
  Texture(std::string_view value) : path(value) {}

  bool operator==(const Texture&) const = default;
  bool operator==(std::string_view value) const { return path == value; }
};

inline bool operator==(std::string_view value, const Texture& texture) { return texture == value; }

struct TexturesData {
  std::vector<Texture> textures;
};

namespace textures {

inline constexpr std::size_t kPathMax = 1023;

enum class Error : std::uint8_t {
  kNone,
  kInvalidOptions,
  kBadIndex,
  kTooManyTextures,
  kBadTexture,
  kDuplicateTexture,
  kBadImage,
  kOutOfMemory,
};

struct ImagePreparationOptions {
  image::FormatFlags accepted_formats = image::kFormatsAll;
  image::Format fallback_format = image::Format::kPng;
  bool require_power_of_two = false;
};

struct ImagePreparationRequest {
  TextureIndex texture = kNoTexture;
  ImagePreparationOptions options;
};

struct PreparedImage {
  PreparedBytes bytes;
  image::Info info;
  bool rescaled = false;
};

struct PathRepairInfo {
  struct Repair {
    std::string original;
    std::string repaired;
  };

  std::vector<Repair> repairs;
};

using PathRebaseInfo = PathRepairInfo;

// --- Validation ---

Error validateTextureCount(std::size_t count) noexcept;
Error validateTexture(const Texture& texture) noexcept;
Error validateEncodedImage(std::span<const std::uint8_t> encoded) noexcept;
Error validate(std::span<const Texture> textures) noexcept;
bool validPath(std::string_view path) noexcept;
bool validExternalPath(std::string_view path) noexcept;

// --- Queries ---

Texture fromImagePath(std::string_view path);
std::string normalizePath(std::string_view path);

// --- Mutation ---

void setTexture(TexturesData& textures, TextureIndex index, Texture texture) noexcept;
TextureIndex addTexture(TexturesData& textures, Texture texture);
void setEncodedImage(TexturesData& textures, TextureIndex index, std::vector<std::uint8_t> encoded_image) noexcept;
void clearEncodedImage(TexturesData& textures, TextureIndex index);
void removeTexture(TexturesData& textures, TextureIndex index) noexcept;
void replaceTextures(TexturesData& textures, std::vector<Texture>&& replacement) noexcept;
void clear(TexturesData& textures) noexcept;

// --- Repair ---

Error repairPath(const TexturesData& textures, Texture& texture, TextureIndex ignored,
                 PathRepairInfo* out_info = nullptr);
Error repairPaths(const TexturesData& textures, std::span<Texture> candidates, PathRepairInfo* out_info = nullptr);
Error repairPaths(std::span<Texture> textures, PathRepairInfo* out_info = nullptr);
Error compact(TexturesData& textures, std::span<const std::uint8_t> used, std::vector<TextureIndex>& out_remap,
              std::size_t& out_removed);

// --- Transformation ---

Error rebasePaths(TexturesData& textures, std::string_view directory, PathRebaseInfo* out_info = nullptr);

// --- Generation ---

Error prepareImages(const TexturesData& textures, std::span<const ImagePreparationRequest> requests,
                    std::vector<PreparedImage>& out);

}  // namespace textures
}  // namespace pistoris
