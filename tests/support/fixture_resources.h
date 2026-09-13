// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#pragma once

#include "doctest/doctest.h"

#include "arx_pistoris/animation.hpp"
#include "arx_pistoris/base/audio.h"
#include "arx_pistoris/base/image.h"
#include "arx_pistoris/binary.hpp"
#include "arx_pistoris/model/obj.hpp"
#include "arx_pistoris/sound.h"
#include "arx_pistoris/sound.hpp"
#include "arx_pistoris/texture.h"
#include "arx_pistoris/texture.hpp"

#include "fixture_catalog.h"

#include <algorithm>
#include <cctype>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <ios>
#include <iterator>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace test_support {

struct HydrationResult {
  std::size_t hydrated = 0;
};

enum class SoundSourceLayout : std::uint8_t { kLoose, kNativeAmbiance, kNativeAnimation };

inline std::vector<std::uint8_t> readBytes(const std::filesystem::path& path) {
  std::ifstream file(path, std::ios::binary);
  return {std::istreambuf_iterator<char>(file), {}};
}

inline std::string readText(const std::filesystem::path& path) {
  std::ifstream file(path);
  return {std::istreambuf_iterator<char>(file), {}};
}

inline std::string resourceKey(std::string_view path) {
  std::string key(path);
  std::transform(key.begin(), key.end(), key.begin(), [](unsigned char value) {
    if (value == '\\') return '/';
    return static_cast<char>(std::tolower(value));
  });
  return key;
}

inline std::filesystem::path resourceFile(const std::filesystem::path& owner, std::string_view relative) {
  std::string portable(relative);
  std::replace(portable.begin(), portable.end(), '\\', '/');
  const std::filesystem::path path(portable);
  CHECK_FALSE(path.is_absolute());
  return owner / path;
}

inline std::filesystem::path soundFile(const std::filesystem::path& owner, std::string_view path,
                                       SoundSourceLayout layout) {
  if (layout == SoundSourceLayout::kLoose) return resourceFile(owner, path);
  std::string native_path(path);
  std::replace(native_path.begin(), native_path.end(), '\\', '/');
  if (layout == SoundSourceLayout::kNativeAmbiance) return resourceFile(owner, native_path);
  if (native_path.starts_with("sfx/")) native_path.erase(0, 4);
  const std::size_t dot = native_path.find_last_of('.');
  const std::size_t separator = native_path.find_last_of("/\\");
  if (dot != std::string::npos && (separator == std::string::npos || dot > separator)) native_path.resize(dot);
  native_path += ".wav";
  return resourceFile(owner / "sfx", native_path);
}

inline std::string nativeSoundKey(std::string_view path) {
  std::string key = resourceKey(path);
  if (key.starts_with("sfx/")) key.erase(0, 4);
  const std::size_t separator = key.find_last_of('/');
  const std::size_t dot = key.find_last_of('.');
  if (dot != std::string::npos && (separator == std::string::npos || dot > separator)) key.resize(dot);
  return key;
}

inline bool isCommittedFixture(const std::filesystem::path& path) {
  const std::filesystem::path relative = path.lexically_relative(kFixtureMount);
  return !relative.empty() && *relative.begin() != "..";
}

inline std::filesystem::path nativeMount(const std::filesystem::path& path) {
  return isCommittedFixture(path) ? kFixtureMount : "data/arx";
}

inline std::filesystem::path textureFile(const std::filesystem::path& owner, std::string_view path,
                                         bool game_priority) {
  if (!game_priority) return resourceFile(owner, path);
  std::string base(path);
  const std::size_t dot = base.find_last_of('.');
  const std::size_t separator = base.find_last_of("/\\");
  if (dot != std::string::npos && (separator == std::string::npos || dot > separator)) base.resize(dot);
  static constexpr std::string_view kExtensions[] = {".png", ".jpg", ".jpeg", ".bmp", ".tga"};
  for (std::string_view extension : kExtensions) {
    const std::filesystem::path candidate = resourceFile(owner, base + std::string(extension));
    if (std::filesystem::is_regular_file(candidate)) return candidate;
  }
  return resourceFile(owner, base);
}

template <class Asset>
std::optional<HydrationResult> hydrateTextures(Asset& asset, const std::filesystem::path& owner,
                                               std::span<const std::string> source_paths, bool game_priority = false) {
  HydrationResult result;
  if (source_paths.size() != asset.textureCount()) {
    CHECK(source_paths.size() == asset.textureCount());
    return std::nullopt;
  }
  std::vector<bool> loaded(asset.textureCount(), false);
  std::vector<ArxTextureView> views(asset.textureCount());
  if (!views.empty()) {
    const ArxReturnCode status = asset.copyTextureViews(0, views.size(), views.data());
    CHECK(status == ARX_OK);
    if (status != ARX_OK) return std::nullopt;
  }
  for (std::size_t index = 0; index < views.size(); ++index) loaded[index] = views[index].encoded_image.size != 0;

  for (std::size_t texture = 0; texture < source_paths.size(); ++texture) {
    if (loaded[texture] || source_paths[texture].empty()) continue;
    const std::filesystem::path path = textureFile(owner, source_paths[texture], game_priority);
    if (!std::filesystem::is_regular_file(path)) continue;
    const std::vector<std::uint8_t> bytes = readBytes(path);
    const ArxReturnCode validation = pistoris::binary::validateEncodedImage(bytes);
    CHECK(validation == ARX_OK);
    if (validation != ARX_OK) return std::nullopt;
    const ArxReturnCode status = asset.setTextureImage(texture, {bytes.data(), bytes.size()});
    CHECK(status == ARX_OK);
    if (status != ARX_OK) return std::nullopt;
    loaded[texture] = true;
    ++result.hydrated;
  }

  return result;
}

template <class Asset>
std::optional<HydrationResult> hydrateSounds(Asset& asset, const std::filesystem::path& owner,
                                             std::span<const pistoris::SoundSourceReference> references,
                                             SoundSourceLayout layout = SoundSourceLayout::kLoose) {
  HydrationResult result;
  std::vector<bool> loaded(asset.soundCount(), false);
  std::vector<ArxSoundView> views(asset.soundCount());
  if (!views.empty()) {
    const ArxReturnCode status = asset.copySoundViews(0, views.size(), views.data());
    CHECK(status == ARX_OK);
    if (status != ARX_OK) return std::nullopt;
  }
  for (std::size_t index = 0; index < views.size(); ++index) loaded[index] = views[index].encoded_audio.size != 0;

  for (const pistoris::SoundSourceReference& reference : references) {
    if (reference.sound >= asset.soundCount()) {
      CHECK(reference.sound < asset.soundCount());
      return std::nullopt;
    }
    if (loaded[reference.sound]) continue;
    const std::filesystem::path path = soundFile(owner, reference.path, layout);
    if (!std::filesystem::is_regular_file(path)) continue;
    const std::vector<std::uint8_t> bytes = readBytes(path);
    const ArxReturnCode validation = pistoris::binary::validateEncodedAudio(bytes);
    CHECK(validation == ARX_OK);
    if (validation != ARX_OK) return std::nullopt;
    const ArxReturnCode status = asset.setSoundData(reference.sound, {bytes.data(), bytes.size()});
    CHECK(status == ARX_OK);
    if (status != ARX_OK) return std::nullopt;
    loaded[reference.sound] = true;
    ++result.hydrated;
  }

  return result;
}

inline std::optional<HydrationResult> hydrateAnimationSounds(
    std::span<std::unique_ptr<pistoris::Animation>> animations, const std::filesystem::path& owner,
    std::span<const pistoris::AnimationSoundSourceReference> references,
    SoundSourceLayout layout = SoundSourceLayout::kLoose) {
  HydrationResult result;
  for (const pistoris::AnimationSoundSourceReference& source : references) {
    if (source.animation_index >= animations.size()) {
      CHECK(source.animation_index < animations.size());
      return std::nullopt;
    }
    if (animations[source.animation_index] == nullptr) {
      CHECK(animations[source.animation_index] != nullptr);
      return std::nullopt;
    }
    pistoris::Animation& animation = *animations[source.animation_index];
    if (source.reference.sound >= animation.soundCount()) {
      CHECK(source.reference.sound < animation.soundCount());
      return std::nullopt;
    }

    ArxSoundView sound{};
    const ArxReturnCode copy_status = animation.copySoundViews(source.reference.sound, 1, &sound);
    CHECK(copy_status == ARX_OK);
    if (copy_status != ARX_OK) return std::nullopt;
    if (sound.encoded_audio.size != 0) continue;

    const std::filesystem::path path = soundFile(owner, source.reference.path, layout);
    if (!std::filesystem::is_regular_file(path)) continue;
    const std::vector<std::uint8_t> bytes = readBytes(path);
    const ArxReturnCode validation = pistoris::binary::validateEncodedAudio(bytes);
    CHECK(validation == ARX_OK);
    if (validation != ARX_OK) return std::nullopt;
    const ArxReturnCode set_status = animation.setSoundData(source.reference.sound, {bytes.data(), bytes.size()});
    CHECK(set_status == ARX_OK);
    if (set_status != ARX_OK) return std::nullopt;
    ++result.hydrated;
  }
  return result;
}

inline std::string_view textureFilePath(const pistoris::NativeTextureFile& file) { return file.resource_path; }
inline std::string_view textureFilePath(const pistoris::ObjTextureFile& file) { return file.path; }

inline std::string nativeTextureKey(std::string_view path) {
  std::string key = resourceKey(path);
  const std::size_t separator = key.find_last_of('/');
  const std::size_t dot = key.find_last_of('.');
  if (dot != std::string::npos && (separator == std::string::npos || dot > separator)) key.resize(dot);
  return key;
}

inline bool texturePathsMatch(std::string_view reference, const pistoris::NativeTextureFile& file) {
  return nativeTextureKey(reference) == nativeTextureKey(file.resource_path);
}

inline bool texturePathsMatch(std::string_view reference, const pistoris::ObjTextureFile& file) {
  return resourceKey(reference) == resourceKey(file.path);
}

template <class Asset, class File>
bool validateTextureFiles(const Asset& asset, std::span<const File> files) {
  for (const File& file : files) {
    CAPTURE(textureFilePath(file));
    if (file.source_texture >= asset.textureCount()) {
      CHECK(file.source_texture < asset.textureCount());
      return false;
    }
    if (textureFilePath(file).empty()) {
      CHECK_FALSE(textureFilePath(file).empty());
      return false;
    }
    if (std::filesystem::path(textureFilePath(file)).is_absolute()) {
      CHECK_FALSE(std::filesystem::path(textureFilePath(file)).is_absolute());
      return false;
    }
    if (file.encoded_image.empty()) {
      CHECK_FALSE(file.encoded_image.empty());
      return false;
    }
    const ArxReturnCode validation = pistoris::binary::validateEncodedImage(file.encoded_image);
    CHECK(validation == ARX_OK);
    if (validation != ARX_OK) return false;
  }
  return true;
}

template <class Asset>
bool validateSoundFiles(const Asset& asset, std::span<const pistoris::SoundFile> files) {
  for (const pistoris::SoundFile& file : files) {
    CAPTURE(file.path);
    if (file.source_sound >= asset.soundCount()) {
      CHECK(file.source_sound < asset.soundCount());
      return false;
    }
    if (file.path.empty()) {
      CHECK_FALSE(file.path.empty());
      return false;
    }
    if (std::filesystem::path(file.path).is_absolute()) {
      CHECK_FALSE(std::filesystem::path(file.path).is_absolute());
      return false;
    }
    if (file.encoded_audio.empty()) {
      CHECK_FALSE(file.encoded_audio.empty());
      return false;
    }
    const ArxReturnCode validation = pistoris::binary::validateEncodedAudio(file.encoded_audio);
    CHECK(validation == ARX_OK);
    if (validation != ARX_OK) return false;
  }
  return true;
}

inline bool validateAnimationSoundFiles(std::span<const pistoris::Animation* const> animations,
                                        std::span<const pistoris::AnimationSoundFile> files) {
  for (const pistoris::AnimationSoundFile& file : files) {
    if (file.animation_index >= animations.size()) {
      CHECK(file.animation_index < animations.size());
      return false;
    }
    if (animations[file.animation_index] == nullptr) {
      CHECK(animations[file.animation_index] != nullptr);
      return false;
    }
    if (!validateSoundFiles(*animations[file.animation_index], std::span<const pistoris::SoundFile>(&file.file, 1)))
      return false;
  }
  return true;
}

template <class Asset, class File>
std::optional<HydrationResult> hydrateTexturesFromFiles(Asset& asset, std::span<const std::string> source_paths,
                                                        std::span<const File> files) {
  HydrationResult result;
  if (source_paths.size() != asset.textureCount()) {
    CHECK(source_paths.size() == asset.textureCount());
    return std::nullopt;
  }
  std::vector<bool> loaded(asset.textureCount(), false);
  std::vector<ArxTextureView> views(asset.textureCount());
  if (!views.empty()) {
    const ArxReturnCode status = asset.copyTextureViews(0, views.size(), views.data());
    CHECK(status == ARX_OK);
    if (status != ARX_OK) return std::nullopt;
  }
  for (std::size_t index = 0; index < views.size(); ++index) loaded[index] = views[index].encoded_image.size != 0;

  for (std::size_t texture = 0; texture < source_paths.size(); ++texture) {
    if (loaded[texture] || source_paths[texture].empty()) continue;
    const auto found =
        std::ranges::find_if(files, [&](const File& file) { return texturePathsMatch(source_paths[texture], file); });
    if (found == files.end()) continue;
    const ArxReturnCode status =
        asset.setTextureImage(texture, {found->encoded_image.data(), found->encoded_image.size()});
    CHECK(status == ARX_OK);
    if (status != ARX_OK) return std::nullopt;
    loaded[texture] = true;
    ++result.hydrated;
  }
  return result;
}

template <class Asset>
std::optional<HydrationResult> hydrateSoundsFromFiles(Asset& asset,
                                                      std::span<const pistoris::SoundSourceReference> references,
                                                      std::span<const pistoris::SoundFile> files,
                                                      SoundSourceLayout layout = SoundSourceLayout::kLoose) {
  HydrationResult result;
  std::vector<bool> loaded(asset.soundCount(), false);
  std::vector<ArxSoundView> views(asset.soundCount());
  if (!views.empty()) {
    const ArxReturnCode status = asset.copySoundViews(0, views.size(), views.data());
    CHECK(status == ARX_OK);
    if (status != ARX_OK) return std::nullopt;
  }
  for (std::size_t index = 0; index < views.size(); ++index) loaded[index] = views[index].encoded_audio.size != 0;

  for (const pistoris::SoundSourceReference& reference : references) {
    if (reference.sound >= asset.soundCount()) {
      CHECK(reference.sound < asset.soundCount());
      return std::nullopt;
    }
    if (loaded[reference.sound]) continue;
    const auto found = std::ranges::find_if(files, [&](const pistoris::SoundFile& file) {
      if (layout == SoundSourceLayout::kNativeAnimation)
        return nativeSoundKey(file.path) == nativeSoundKey(reference.path);
      return resourceKey(file.path) == resourceKey(reference.path);
    });
    if (found == files.end()) continue;
    const ArxReturnCode status =
        asset.setSoundData(reference.sound, {found->encoded_audio.data(), found->encoded_audio.size()});
    CHECK(status == ARX_OK);
    if (status != ARX_OK) return std::nullopt;
    loaded[reference.sound] = true;
    ++result.hydrated;
  }
  return result;
}

struct ObjFixtureInput {
  std::string obj;
  std::vector<std::string> library_paths;
  std::vector<std::string> library_texts;
};

inline std::optional<ObjFixtureInput> readObjFixture(const std::filesystem::path& path) {
  ObjFixtureInput input;
  input.obj = readText(path);
  const ArxReturnCode status = pistoris::objMaterialLibraryPaths(input.obj, input.library_paths);
  CHECK(status == ARX_OK);
  if (status != ARX_OK) return std::nullopt;
  input.library_texts.reserve(input.library_paths.size());
  for (const std::string& library_path : input.library_paths) {
    const std::filesystem::path file = resourceFile(path.parent_path(), library_path);
    if (!std::filesystem::is_regular_file(file)) {
      CHECK(std::filesystem::is_regular_file(file));
      return std::nullopt;
    }
    input.library_texts.push_back(readText(file));
  }
  return input;
}

inline std::vector<pistoris::ObjMaterialLibraryView> objMaterialLibraryViews(const ObjFixtureInput& input) {
  std::vector<pistoris::ObjMaterialLibraryView> views;
  views.reserve(input.library_paths.size());
  for (std::size_t index = 0; index < input.library_paths.size(); ++index)
    views.push_back({input.library_paths[index], input.library_texts[index]});
  return views;
}

}  // namespace test_support
