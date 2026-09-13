// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#include "resources/texture_io.h"

#include "arx_pistoris/base/indices.h"
#include "arx_pistoris/base/status.h"
#include "arx_pistoris/base/string_view.h"
#include "arx_pistoris/level.hpp"
#include "arx_pistoris/model.hpp"
#include "arx_pistoris/native/ftl.hpp"
#include "arx_pistoris/native/fts.hpp"
#include "arx_pistoris/runtime.hpp"
#include "arx_pistoris/runtime/types.h"
#include "arx_pistoris/texture.h"
#include "arx_pistoris/texture.hpp"

#include "base/resource_path.h"
#include "console/diagnostics.h"
#include "console/logging.h"
#include "io/path_location.h"
#include "io/service.h"
#include "media/encoded.h"
#include "resources/layout.h"
#include "resources/selector.h"

#include <algorithm>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <span>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>

namespace cli {
namespace {

struct TextureLookup {
  std::string path;
  bool has_encoded_image = false;
};

enum class TextureLoadResult : std::uint8_t {
  kLoaded,
  kNotFound,
  kFailed,
};

std::string_view stringView(ArxStringView value) { return {value.data, value.size}; }

template <typename Asset>
bool copyTextureLookups(const Asset& asset, std::string_view owner, std::vector<TextureLookup>& out) {
  std::vector<ArxTextureView> projected(asset.textureCount());
  const ArxReturnCode rc = asset.copyTextureViews(0, projected.size(), projected.data());
  if (rc != ARX_OK) {
    diagnostic(DiagnosticCode::kResourceInputInvalid,
               "%.*s texture projection failed: %s (code %d)",
               static_cast<int>(owner.size()),
               owner.data(),
               pistoris::errorString(rc),
               static_cast<int>(rc));
    return false;
  }

  out.clear();
  out.reserve(projected.size());
  for (const ArxTextureView& texture : projected)
    out.push_back({std::string(stringView(texture.path)), texture.encoded_image.size != 0});
  return true;
}

std::string_view fixedStringView(const char* value, std::size_t capacity) {
  const void* end = std::memchr(value, '\0', capacity);
  const std::size_t size = end ? static_cast<std::size_t>(static_cast<const char*>(end) - value) : capacity;
  return {value, size};
}

std::string removeFinalExtension(std::string_view path) {
  std::string normalized = normalizeResourceSeparators(path);
  const std::size_t separator = normalized.find_last_of('/');
  const std::size_t dot = normalized.find_last_of('.');
  if (dot != std::string::npos && (separator == std::string::npos || dot > separator)) normalized.resize(dot);
  return normalized;
}

std::string nativeLogicalPath(std::string_view path) { return resourcePathKey(removeFinalExtension(path)); }

bool resolveTextureOutputLocation(IoService& io, const TextureOutput& output, std::string_view resource_path,
                                  DiagnosticCode failure_code, std::string_view owner, PathLocation& out) {
  if (output.layout == ResourceLayout::kGame) {
    out.path = resource_path;
    out.address = PathAddress::kMountRelative;
    return true;
  }
  std::string error;
  if (io.appendPathLocation(output.base, resource_path, out, error)) return true;
  diagnostic(failure_code,
             "Cannot resolve %.*s texture output '%.*s': %s",
             static_cast<int>(owner.size()),
             owner.data(),
             static_cast<int>(resource_path.size()),
             resource_path.data(),
             error.c_str());
  return false;
}

std::string textureIdentity(std::string_view path) { return resourcePathKey(path); }

std::string selectedExtension(std::string_view selected_path) {
  const std::size_t separator = selected_path.find_last_of('/');
  const std::size_t dot = selected_path.find_last_of('.');
  if (dot == std::string_view::npos || (separator != std::string_view::npos && dot < separator)) return {};
  return std::string(selected_path.substr(dot));
}

template <typename Apply>
TextureLoadResult tryLoadTexture(IoService& io, const PathLocation& base, std::string_view path, ImageLookupMode mode,
                                 std::string_view owner, std::string_view display, Apply&& apply) {
  std::vector<std::uint8_t> encoded;
  std::string selected_path;
  std::string resolved_path;
  const ResourceReadResult result = io.readImage(base, path, mode, encoded, &selected_path, &resolved_path);
  switch (result) {
    case ResourceReadResult::kSuccess:
      if (apply(std::move(encoded), selected_path)) return TextureLoadResult::kLoaded;
      log(ARX_LOG_WARN,
          "%.*s texture image is invalid: %.*s -> %s",
          static_cast<int>(owner.size()),
          owner.data(),
          static_cast<int>(display.size()),
          display.data(),
          resolved_path.c_str());
      return TextureLoadResult::kFailed;
    case ResourceReadResult::kReadFailed:
      log(ARX_LOG_WARN,
          "%.*s texture image could not be read: %.*s -> %s",
          static_cast<int>(owner.size()),
          owner.data(),
          static_cast<int>(display.size()),
          display.data(),
          resolved_path.c_str());
      return TextureLoadResult::kFailed;
    case ResourceReadResult::kInvalidPath:
      log(ARX_LOG_WARN,
          "%.*s texture path cannot be resolved: %.*s",
          static_cast<int>(owner.size()),
          owner.data(),
          static_cast<int>(display.size()),
          display.data());
      return TextureLoadResult::kFailed;
    case ResourceReadResult::kNotFound:
      return TextureLoadResult::kNotFound;
  }
  return TextureLoadResult::kFailed;
}

template <typename Apply>
bool loadTextures(std::span<const TextureLookup> textures, std::span<const std::string> source_paths, IoService& io,
                  const TextureInput& input, std::string_view owner, Apply&& apply) {
  if (!source_paths.empty() && source_paths.size() != textures.size()) {
    diagnostic(DiagnosticCode::kResourceInputInvalid,
               "%.*s texture source mapping has %zu entries for %zu textures",
               static_cast<int>(owner.size()),
               owner.data(),
               source_paths.size(),
               textures.size());
    return false;
  }
  std::vector<bool> loaded(textures.size(), false);
  for (std::size_t index = 0; index < textures.size(); ++index) loaded[index] = textures[index].has_encoded_image;
  std::size_t loaded_count = 0;

  if (input.use_format_sources) {
    for (std::size_t index = 0; index < source_paths.size(); ++index) {
      const std::string& source_path = source_paths[index];
      if (loaded[index] || source_path.empty()) continue;
      std::string path = input.source_lookup == ImageLookupMode::kGamePriority
                             ? removeFinalExtension(source_path)
                             : normalizeResourceSeparators(source_path);
      const TextureLoadResult result = tryLoadTexture(
          io,
          input.source_base,
          path,
          input.source_lookup,
          owner,
          source_path,
          [&](std::vector<std::uint8_t> encoded, std::string_view selected_path) {
            return apply(
                static_cast<pistoris::TextureIndex>(index), selectedExtension(selected_path), std::move(encoded));
          });
      if (result == TextureLoadResult::kLoaded) {
        loaded[index] = true;
        ++loaded_count;
      }
    }
  }

  const PathLocation mounts;
  for (std::size_t index = 0; index < textures.size(); ++index) {
    if (loaded[index] || textures[index].path.empty()) continue;
    const TextureLoadResult result = tryLoadTexture(
        io,
        mounts,
        textures[index].path,
        ImageLookupMode::kGamePriority,
        owner,
        textures[index].path,
        [&](std::vector<std::uint8_t> encoded, std::string_view selected_path) {
          return apply(
              static_cast<pistoris::TextureIndex>(index), selectedExtension(selected_path), std::move(encoded));
        });
    if (result == TextureLoadResult::kLoaded) {
      loaded[index] = true;
      ++loaded_count;
    } else if (result == TextureLoadResult::kNotFound) {
      if (io.hasReadMounts()) {
        log(ARX_LOG_WARN,
            "%.*s texture image not found: %s",
            static_cast<int>(owner.size()),
            owner.data(),
            textures[index].path.c_str());
      } else {
        log(ARX_LOG_WARN,
            "Referenced %.*s texture image was not found because there are no readable mount folders: %s",
            static_cast<int>(owner.size()),
            owner.data(),
            textures[index].path.c_str());
      }
    }
  }

  if (loaded_count != 0)
    log(ARX_LOG_INFO, "loaded %zu %.*s texture image(s)", loaded_count, static_cast<int>(owner.size()), owner.data());
  return true;
}

template <typename Asset>
bool loadAssetTextureImages(Asset& asset, IoService& io, const TextureInput& input,
                            std::span<const std::string> source_paths, std::string_view owner) {
  std::vector<TextureLookup> textures;
  if (!copyTextureLookups(asset, owner, textures)) return false;
  return loadTextures(textures,
                      source_paths,
                      io,
                      input,
                      owner,
                      [&](pistoris::TextureIndex index, std::string_view, std::vector<std::uint8_t> encoded) {
                        return asset.setTextureImage(index, {encoded.data(), encoded.size()}) == ARX_OK;
                      });
}

template <typename Range, typename PathOf>
void loadNativePaths(const Range& native_paths, PathOf&& path_of, IoService& io, const TextureInput& input,
                     std::string_view owner, std::vector<pistoris::NativeTextureFile>& out) {
  std::vector<TextureLookup> textures;
  std::vector<std::string> source_paths;
  textures.reserve(native_paths.size());
  source_paths.reserve(native_paths.size());
  std::unordered_map<std::string, pistoris::TextureIndex> textures_by_path;
  textures_by_path.reserve(native_paths.size());
  for (const auto& entry : native_paths) {
    const std::string_view native_path = path_of(entry);
    std::string logical_path = nativeLogicalPath(native_path);
    const std::string identity = textureIdentity(logical_path);
    const bool inserted =
        textures_by_path.emplace(identity, static_cast<pistoris::TextureIndex>(textures.size())).second;
    if (inserted) {
      textures.push_back({std::move(logical_path), false});
      source_paths.emplace_back(native_path);
    }
  }

  out.clear();
  out.reserve(textures.size());
  const bool valid =
      loadTextures(textures,
                   source_paths,
                   io,
                   input,
                   owner,
                   [&](pistoris::TextureIndex index, std::string_view extension, std::vector<std::uint8_t> encoded) {
                     media::PreparedImage prepared;
                     if (media::prepareImage(std::move(encoded), prepared) != ARX_OK) return false;
                     std::string resource_path = textures[index].path;
                     resource_path += extension;
                     out.push_back({index, std::move(resource_path), std::move(prepared.encoded)});
                     return true;
                   });
  assert(valid);
  (void)valid;
}

}  // namespace

bool loadTextureImages(pistoris::Level& level, IoService& io, const TextureInput& input,
                       std::span<const std::string> source_paths) {
  return loadAssetTextureImages(level, io, input, source_paths, "Level");
}

bool loadTextureImages(pistoris::Model& model, IoService& io, const TextureInput& input,
                       std::span<const std::string> source_paths) {
  return loadAssetTextureImages(model, io, input, source_paths, "Model");
}

void loadNativeTextureFiles(const pistoris::ftl::Data& ftl, IoService& io, const TextureInput& input,
                            std::vector<pistoris::NativeTextureFile>& out) {
  loadNativePaths(
      ftl.texture_containers,
      [](const pistoris::ftl::TextureContainer& container) {
        return fixedStringView(container.filename, sizeof(container.filename));
      },
      io,
      input,
      "Model",
      out);
}

void loadNativeTextureFiles(const pistoris::fts::Data& fts, IoService& io, const TextureInput& input,
                            std::vector<pistoris::NativeTextureFile>& out) {
  std::vector<std::pair<std::int32_t, std::string_view>> ordered;
  ordered.reserve(fts.textures.size());
  for (const auto& [id, texture] : fts.textures) ordered.emplace_back(id, texture.fic);
  std::sort(ordered.begin(), ordered.end(), [](const auto& lhs, const auto& rhs) { return lhs.first < rhs.first; });
  loadNativePaths(ordered, [](const auto& entry) { return entry.second; }, io, input, "Level", out);
}

bool addNativeTextureFileOutputs(ResourceOutputPlan& plan, IoService& io, const TextureOutput& output,
                                 std::span<const pistoris::NativeTextureFile> files, ResourceAssetId asset,
                                 DiagnosticCode failure_code, std::string_view owner) {
  for (const pistoris::NativeTextureFile& file : files) {
    PathLocation location;
    if (!resolveTextureOutputLocation(io, output, file.resource_path, failure_code, owner, location)) return false;
    plan.add(
        ResourceFileKind::kImage, std::move(location), file.encoded_image.data(), file.encoded_image.size(), asset);
  }
  return true;
}

bool addObjTextureFileOutputs(ResourceOutputPlan& plan, IoService& io, const TextureOutput& output,
                              std::span<const pistoris::ObjTextureFile> files, ResourceAssetId asset,
                              DiagnosticCode failure_code, std::string_view owner) {
  for (const pistoris::ObjTextureFile& file : files) {
    PathLocation location;
    if (!resolveTextureOutputLocation(io, output, file.path, failure_code, owner, location)) return false;
    plan.add(
        ResourceFileKind::kImage, std::move(location), file.encoded_image.data(), file.encoded_image.size(), asset);
  }
  return true;
}

}  // namespace cli
