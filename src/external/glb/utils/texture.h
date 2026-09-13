// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#pragma once

#include "arx_pistoris/base/indices.h"

#include "cgltf/cgltf.h"
#include "modules/textures.h"
#include "utils/encoded_image.h"
#include "utils/resource_path.h"

#include <cstdint>
#include <span>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace pistoris::glb {

class Builder;

enum class TextureAlpha : std::uint8_t {
  kUnknown,
  kAbsent,
  kPresent,
};

struct ExportedTexture {
  int index = -1;
  TextureAlpha alpha = TextureAlpha::kUnknown;
  bool assumed_png = false;
};

struct TextureImportRequest {
  const cgltf_image* image = nullptr;
  std::string_view fallback_path;
};

enum class TextureImportError : std::uint8_t {
  kNone,
  kBadImage,
  kBadPath,
  kTooManyTextures,
  kOutOfMemory,
};

class TextureImporter {
 public:
  TextureImporter(TexturesData& textures, std::vector<std::string>* source_paths, std::string_view log_prefix);

  TextureImportError import(const TextureImportRequest& request, TextureIndex& out);

 private:
  using PathMap = std::unordered_map<std::string, TextureIndex, ResourcePathIdentityHash, ResourcePathIdentityEqual>;

  TextureIndex find(const TextureImportRequest& request) const noexcept;
  void insert(const TextureImportRequest& request, TextureIndex texture);
  void appendSourcePath(const TextureImportRequest& request);

  TexturesData& textures_;
  std::vector<std::string>* source_paths_ = nullptr;
  std::string_view log_prefix_;
  PathMap external_images_;
  std::unordered_map<const cgltf_image*, TextureIndex> embedded_images_;
  PathMap fallbacks_;
};

void exportTexture(Builder& builder, const Texture& texture, const textures::PreparedImage* prepared,
                   ExportedTexture& out);
std::string embeddedTexturePath(const cgltf_image& image, std::string_view fallback, pistoris::image::Format format);
bool makeTexturePathsUnique(std::span<Texture> textures, std::string_view log_prefix);

}  // namespace pistoris::glb
