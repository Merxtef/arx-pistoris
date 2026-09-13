// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#include "arx_pistoris/paths.hpp"

#include "arx_pistoris/base/indices.h"

#include "modules/textures.h"
#include "utils/path.h"
#include "utils/portable_filename.h"
#include "utils/resource_path.h"
#include "utils/unique_value.h"

#include <algorithm>
#include <cstddef>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <unordered_set>
#include <utility>
#include <vector>

namespace pistoris::textures {
namespace {

void normalizeExternalImageExtension(std::string& extension) noexcept {
  for (char& value : extension)
    if (value >= 'A' && value <= 'Z') value = static_cast<char>(value - 'A' + 'a');
}

bool equalIgnoringAsciiCase(std::string_view left, std::string_view right) noexcept {
  if (left.size() != right.size()) return false;
  for (std::size_t index = 0; index < left.size(); ++index) {
    const char left_value =
        left[index] >= 'A' && left[index] <= 'Z' ? static_cast<char>(left[index] - 'A' + 'a') : left[index];
    const char right_value =
        right[index] >= 'A' && right[index] <= 'Z' ? static_cast<char>(right[index] - 'A' + 'a') : right[index];
    if (left_value != right_value) return false;
  }
  return true;
}

bool validRebasedFilename(std::string_view name, std::size_t max_length) {
  return name.size() <= max_length && name.find('/') == std::string_view::npos && isResourcePath(name);
}

std::optional<std::string> normalizedRebasedName(std::string_view requested, std::size_t max_length) {
  ResourcePathNormalization normalized = normalizeResourcePath(requested);
  if (normalized.error != ResourcePathError::kNone || normalized.value.find('/') != std::string::npos)
    return std::nullopt;
  if (validRebasedFilename(normalized.value, max_length)) return std::move(normalized.value);

  std::string bounded = std::move(normalized.value);
  while (!bounded.empty()) {
    bounded.resize(std::min(bounded.size(), max_length));
    ResourcePathNormalization candidate = normalizeResourcePath(bounded);
    if (candidate.error == ResourcePathError::kNone && validRebasedFilename(candidate.value, max_length))
      return std::move(candidate.value);
    bounded.pop_back();
  }
  return std::nullopt;
}

std::optional<std::string> suffixedRebasedName(std::string_view base, std::size_t ordinal, std::size_t max_length) {
  const std::string suffix = "_" + std::to_string(ordinal);
  if (suffix.size() >= max_length) return std::nullopt;

  std::string prefix(base.substr(0, max_length - suffix.size()));
  if (prefix.empty()) prefix = "t";
  std::string candidate = prefix + suffix;
  if (validRebasedFilename(candidate, max_length)) return candidate;
  return std::nullopt;
}

std::optional<std::string> makeUniqueRebasedName(std::string_view base, std::size_t max_length,
                                                 const std::unordered_set<std::string>& unavailable,
                                                 const std::unordered_set<std::string>& assigned) {
  for (std::size_t ordinal = 1;; ++ordinal) {
    std::optional<std::string> candidate = suffixedRebasedName(base, ordinal, max_length);
    if (!candidate) return std::nullopt;
    const std::string key = resourcePathIdentityKey(*candidate);
    if (!unavailable.contains(key) && !assigned.contains(key)) return candidate;
  }
}

std::optional<std::string> suffixedTexturePath(std::string_view path, std::size_t ordinal) {
  const std::string suffix = "_" + std::to_string(ordinal);
  const std::size_t separator = path.find_last_of('/');
  const std::size_t component_begin = separator == std::string_view::npos ? 0 : separator + 1U;
  const std::size_t component_limit = std::min(kPortableNameMax, kPathMax - component_begin);
  const std::size_t prefix_limit = component_limit > suffix.size() ? component_limit - suffix.size() : 0;
  std::string result(path.substr(0, component_begin));
  result += path.substr(component_begin, prefix_limit);
  result += suffix;
  return result;
}

}  // namespace

Texture fromImagePath(std::string_view path) {
  std::string normalized(path);
  for (char& value : normalized)
    if (value == '\\') value = '/';
  const std::string_view identity = pathWithoutExtension(normalized);
  const std::string_view extension = pathExtension(normalized);
  Texture texture(identity);
  if (extension.size() > 1U) {
    texture.external_image_extension = extension;
    normalizeExternalImageExtension(texture.external_image_extension);
  }
  return texture;
}

std::string normalizePath(std::string_view path) {
  ResourcePathNormalization normalized = normalizeResourcePath(path);
  if (normalized.error != ResourcePathError::kNone || normalized.value.size() > kPathMax) return {};
  return std::move(normalized.value);
}

bool validPath(std::string_view path) noexcept { return path.size() <= kPathMax && isResourcePath(path); }

bool validExternalPath(std::string_view path) noexcept { return validPath(path); }

namespace {

Error repairPathsImpl(std::span<const Texture> existing, TextureIndex ignored, std::span<Texture> textures,
                      PathRepairInfo* out_info) {
  std::vector<std::string> candidates;
  candidates.reserve(textures.size());
  ValueUniquifier<ResourcePathIdentityHash, ResourcePathIdentityEqual> values;
  values.reserve(existing.size() + textures.size());
  for (std::size_t index = 0; index < existing.size(); ++index) {
    if (index == static_cast<std::size_t>(ignored)) continue;
    if (!validPath(existing[index].path)) return Error::kBadTexture;
    values.occupy(existing[index].path);
  }
  for (const Texture& texture : textures) {
    std::string normalized = normalizePath(texture.path);
    if (normalized.empty()) return Error::kBadTexture;
    candidates.push_back(std::move(normalized));
  }
  if (values.apply(std::span(candidates), suffixedTexturePath).error != ValueUniquifierError::kNone)
    return Error::kBadTexture;
  for (const std::string& candidate : candidates)
    if (!validPath(candidate)) return Error::kBadTexture;

  PathRepairInfo info;
  if (out_info) {
    info.repairs.reserve(textures.size());
    for (std::size_t index = 0; index < textures.size(); ++index)
      if (textures[index].path != candidates[index] && !equalIgnoringAsciiCase(textures[index].path, candidates[index]))
        info.repairs.push_back({textures[index].path, candidates[index]});
  }

  for (std::size_t index = 0; index < textures.size(); ++index) {
    textures[index].path = std::move(candidates[index]);
    normalizeExternalImageExtension(textures[index].external_image_extension);
  }
  if (out_info) *out_info = std::move(info);
  return Error::kNone;
}

}  // namespace

Error repairPath(const TexturesData& textures, Texture& texture, TextureIndex ignored, PathRepairInfo* out_info) {
  return repairPathsImpl(textures.textures, ignored, std::span<Texture>(&texture, 1), out_info);
}

Error repairPaths(const TexturesData& textures, std::span<Texture> candidates, PathRepairInfo* out_info) {
  return repairPathsImpl(textures.textures, kNoTexture, candidates, out_info);
}

Error repairPaths(std::span<Texture> textures, PathRepairInfo* out_info) {
  return repairPathsImpl({}, kNoTexture, textures, out_info);
}

Error rebasePaths(TexturesData& textures, std::string_view directory, PathRebaseInfo* out_info) {
  ResourcePathNormalization normalized_directory = normalizeResourceDirectory(directory);
  if (normalized_directory.error != ResourcePathError::kNone) return Error::kBadTexture;
  std::string folder = std::move(normalized_directory.value);
  if (!folder.empty()) folder.push_back('/');
  if (folder.size() + 1U > kPathMax) return Error::kBadTexture;
  const std::size_t name_limit = std::min(kPortableNameMax, kPathMax - folder.size());

  std::vector<std::string> rebased;
  rebased.reserve(textures.textures.size());

  std::vector<std::string> normalized_names;
  normalized_names.reserve(textures.textures.size());
  for (const Texture& texture : textures.textures) {
    std::optional<std::string> normalized = normalizedRebasedName(pathFilename(texture.path), name_limit);
    if (!normalized) return Error::kBadTexture;
    normalized_names.push_back(std::move(*normalized));
  }

  std::unordered_set<std::string> unavailable;
  unavailable.reserve(textures.textures.size() * 2U);
  for (const std::string& name : normalized_names) unavailable.insert(resourcePathIdentityKey(name));

  std::unordered_set<std::string> assigned;
  assigned.reserve(textures.textures.size());
  PathRebaseInfo info;
  if (out_info) {
    info.repairs.reserve(textures.textures.size() + 1U);
    if (hasStructuralResourcePathRepair(normalized_directory.repair))
      info.repairs.push_back({std::string(directory), folder.substr(0, folder.size() - 1U)});
  }
  for (std::size_t index = 0; index < textures.textures.size(); ++index) {
    const Texture& texture = textures.textures[index];
    const std::string_view original_name = pathFilename(texture.path);
    std::string name = normalized_names[index];
    std::string key = resourcePathIdentityKey(name);
    const bool collision = assigned.contains(key);
    if (collision) {
      std::optional<std::string> unique = makeUniqueRebasedName(name, name_limit, unavailable, assigned);
      if (!unique) return Error::kBadTexture;
      name = std::move(*unique);
      key = resourcePathIdentityKey(name);
    }
    std::string path = folder + name;
    if (!validRebasedFilename(name, name_limit) || !validPath(path)) return Error::kBadTexture;
    unavailable.insert(key);
    assigned.insert(key);
    if (out_info && name != original_name && !equalIgnoringAsciiCase(name, original_name))
      info.repairs.push_back({texture.path, path});
    rebased.push_back(std::move(path));
  }

  for (std::size_t index = 0; index < rebased.size(); ++index) textures.textures[index].path.swap(rebased[index]);
  if (out_info) *out_info = std::move(info);
  return Error::kNone;
}

}  // namespace pistoris::textures
