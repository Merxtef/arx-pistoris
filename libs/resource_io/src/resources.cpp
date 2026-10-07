// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#include "arx_pistoris/resource_io/resources.hpp"

#include "arx_pistoris/ambiance.hpp"
#include "arx_pistoris/ambiance/bake.hpp"
#include "arx_pistoris/animation.hpp"
#include "arx_pistoris/animation/bake.hpp"
#include "arx_pistoris/base/image.h"
#include "arx_pistoris/base/image.hpp"
#include "arx_pistoris/base/indices.h"
#include "arx_pistoris/base/math.h"
#include "arx_pistoris/base/result.hpp"
#include "arx_pistoris/base/status.h"
#include "arx_pistoris/binary.hpp"
#include "arx_pistoris/cinematic.hpp"
#include "arx_pistoris/cinematic/bake.hpp"
#include "arx_pistoris/cinematic/glb.hpp"
#include "arx_pistoris/cinematic/sound.hpp"
#include "arx_pistoris/error.hpp"
#include "arx_pistoris/level.hpp"
#include "arx_pistoris/level/bake.hpp"
#include "arx_pistoris/level/images.hpp"
#include "arx_pistoris/model.hpp"
#include "arx_pistoris/model/bake.hpp"
#include "arx_pistoris/model/location.hpp"
#include "arx_pistoris/model/obj.hpp"
#include "arx_pistoris/native.hpp"
#include "arx_pistoris/paths.hpp"
#include "arx_pistoris/paths/types.h"
#include "arx_pistoris/resource_io/document.hpp"
#include "arx_pistoris/resource_io/location.hpp"
#include "arx_pistoris/resource_io/native_bundle.hpp"
#include "arx_pistoris/resource_io/output.hpp"
#include "arx_pistoris/resource_io/resource_mounts.hpp"
#include "arx_pistoris/resource_io/status.h"
#include "arx_pistoris/runtime/types.h"
#include "arx_pistoris/sound.h"
#include "arx_pistoris/sound.hpp"
#include "arx_pistoris/texture.h"

#include "result_failure.h"
#include "utils/log.h"
#include "utils/number_text.h"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <ios>
#include <iosfwd>
#include <limits>
#include <locale>
#include <new>
#include <optional>
#include <span>
#include <sstream>
#include <string>
#include <string_view>
#include <system_error>
#include <type_traits>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <variant>
#include <vector>

namespace {

using pistoris::resource_io::ResourceIoLocation;
using pistoris::resource_io::ResourceIoOperation;
using pistoris::resource_io::ResourceIoResult;
using pistoris::resource_io::ResourceMountMask;
using pistoris::resource_io::ResourceMounts;
using pistoris::resource_io::ResourceOutputKind;
using pistoris::resource_io::ResourceRead;

pistoris::resource_io::ResourceLayout documentLayout(pistoris::resource_io::ResourceClassification classification,
                                                     pistoris::resource_io::ResourceAddress address) noexcept {
  if (address == pistoris::resource_io::ResourceAddress::kNative) return pistoris::resource_io::ResourceLayout::kLoose;
  switch (classification.format) {
    case pistoris::resource_io::ResourceFormat::kFtl:
    case pistoris::resource_io::ResourceFormat::kTea:
    case pistoris::resource_io::ResourceFormat::kDlf:
    case pistoris::resource_io::ResourceFormat::kAmb:
    case pistoris::resource_io::ResourceFormat::kCin:
      return pistoris::resource_io::ResourceLayout::kGame;
    case pistoris::resource_io::ResourceFormat::kJson:
    default:
      return pistoris::resource_io::ResourceLayout::kLoose;
  }
}

pistoris::resource_io::ResourceIoResult<pistoris::resource_io::ResourceDocument> documentFromRead(
    ResourceRead read, std::string requested_path, std::string logical_path,
    pistoris::resource_io::ResourceAddress address, ArxResourceKind selector_kind, ResourceMountMask mount_mask,
    pistoris::resource_io::ResourceIoFlags flags) {
  pistoris::resource_io::ResourceDocument result;
  result.requested_path = std::move(requested_path);
  result.logical_path = std::move(logical_path);
  result.native_path = std::move(read.native_path);
  result.data = std::move(read.data);
  result.address = address;
  result.mount_mask = mount_mask;
  result.mount_id = read.mount_id;
  result.flags = flags;
  result.selector_kind = selector_kind;
  const std::string_view classification_path =
      result.logical_path.empty() ? std::string_view(result.requested_path) : std::string_view(result.logical_path);
  auto classification = pistoris::resource_io::classifyResource(result.data, classification_path);
  if (!classification) {
    const auto* error = classification.error();
    return ResourceIoResult<pistoris::resource_io::ResourceDocument>::failure(
        classification.code(),
        ResourceIoLocation{
            ResourceIoOperation::kClassify, result.logical_path, result.native_path, mount_mask, std::nullopt},
        error ? std::string(error->detail()) : std::string{});
  }
  result.classification = *classification;
  result.layout = result.classification.format == pistoris::resource_io::ResourceFormat::kJson
                      ? pistoris::resource_io::ResourceLayout::kLoose
                  : selector_kind == ARX_RESOURCE_KIND_NONE ? documentLayout(result.classification, address)
                                                            : pistoris::resource_io::ResourceLayout::kGame;
  return ResourceIoResult<pistoris::resource_io::ResourceDocument>::success(std::move(result));
}

struct InputContext {
  enum class Layout : std::uint8_t {
    kMountedGame,
    kLoose,
  };

  std::string logical_path;
  std::filesystem::path native_path;
  ResourceMountMask mount = 0;
  Layout layout = Layout::kLoose;
};

using PendingOutput = pistoris::resource_io::ResourceOutput;

template <class T>
pistoris::resource_io::ResourceIoResult<T> loadFailure(ArxReturnCode code, std::string_view resource_path,
                                                       const std::filesystem::path& native_path,
                                                       ResourceMountMask mounts, std::string_view detail = {}) {
  return pistoris::resource_io::detail::resourceIoFailure<T>(
      code, ResourceIoOperation::kRead, resource_path, native_path, mounts, detail);
}

template <class T>
pistoris::resource_io::ResourceIoResult<T> writeFailure(ArxReturnCode code, std::string_view resource_path,
                                                        const std::filesystem::path& native_path,
                                                        std::string_view detail = {}) {
  return pistoris::resource_io::detail::resourceIoFailure<T>(
      code, ResourceIoOperation::kWrite, resource_path, native_path, 0, detail);
}

template <class T, class Result>
pistoris::resource_io::ResourceIoResult<T> conversionFailure(const Result& result, std::string_view resource_path,
                                                             const std::filesystem::path& native_path,
                                                             ResourceMountMask mounts) {
  const auto* error = result.error();
  ResourceIoLocation location{
      ResourceIoOperation::kRead, std::string(resource_path), native_path, mounts, std::nullopt};
  if (error && error->location()) {
    const auto assign = [&]<class Location>(const Location& inner) { location.content_location = inner; };
    if constexpr (requires { typename std::variant_size<typename Result::LocationType>::type; })
      std::visit(assign, *error->location());
    else
      assign(*error->location());
  }
  return ResourceIoResult<T>::failure(
      result.code(), std::move(location), error ? std::string(error->detail()) : std::string{});
}

template <class T, class Result>
pistoris::resource_io::ResourceIoResult<T> writeConversionFailure(const Result& result, std::string_view resource_path,
                                                                  const std::filesystem::path& native_path) {
  const auto* error = result.error();
  ResourceIoLocation location{ResourceIoOperation::kWrite, std::string(resource_path), native_path, 0, std::nullopt};
  if (error && error->location()) {
    const auto assign = [&]<class Location>(const Location& inner) { location.content_location = inner; };
    if constexpr (requires { typename std::variant_size<typename Result::LocationType>::type; })
      std::visit(assign, *error->location());
    else
      assign(*error->location());
  }
  return ResourceIoResult<T>::failure(
      result.code(), std::move(location), error ? std::string(error->detail()) : std::string{});
}

template <class T, class Function>
pistoris::resource_io::ResourceIoResult<T> guardedLoad(Function&& function) noexcept {
  try {
    return std::forward<Function>(function)();
  } catch (const std::bad_alloc&) {
    return pistoris::resource_io::detail::resourceIoFailure<T>(ARX_BAD_ALLOC, ResourceIoOperation::kRead, {}, {}, 0);
  } catch (...) {
    return pistoris::resource_io::detail::resourceIoFailure<T>(
        ARX_INTERNAL_ERROR, ResourceIoOperation::kRead, {}, {}, 0);
  }
}

template <class T, class Function>
pistoris::resource_io::ResourceIoResult<T> guardedWrite(Function&& function) noexcept {
  try {
    return std::forward<Function>(function)();
  } catch (const std::bad_alloc&) {
    return writeFailure<T>(ARX_BAD_ALLOC, {}, {});
  } catch (...) {
    return writeFailure<T>(ARX_INTERNAL_ERROR, {}, {});
  }
}

template <class T, class Function>
pistoris::resource_io::ResourceIoResult<T> guardedWrite(const pistoris::resource_io::ResourceOutputOptions& options,
                                                        Function&& function) noexcept {
  return guardedWrite<T>([&]() -> pistoris::resource_io::ResourceIoResult<T> {
    if ((options.outputs &
         ~static_cast<pistoris::resource_io::ResourceOutputParts>(pistoris::resource_io::kResourceOutputAll)) != 0)
      return writeFailure<T>(ARX_INVALID_OPTIONS, {}, {}, "unsupported Resource output parts");
    if ((options.io_flags &
         ~static_cast<pistoris::resource_io::ResourceIoFlags>(pistoris::resource_io::kResourceIoFlagsAll)) != 0)
      return writeFailure<T>(ARX_INVALID_OPTIONS, {}, {}, "unsupported Resource I/O flags");
    return std::forward<Function>(function)();
  });
}

std::string normalizedPath(std::string_view path) {
  std::string result(path);
  std::ranges::replace(result, '\\', '/');
  return result;
}

std::string asciiLower(std::string_view value) {
  std::string result(value);
  for (char& character : result) {
    if (character >= 'A' && character <= 'Z') character = static_cast<char>(character + ('a' - 'A'));
  }
  return result;
}

std::string pathToUtf8(const std::filesystem::path& path) {
  const std::u8string encoded = path.u8string();
  return {reinterpret_cast<const char*>(encoded.data()), encoded.size()};
}

std::string_view extension(std::string_view path) {
  const std::size_t separator = path.find_last_of('/');
  const std::size_t dot = path.find_last_of('.');
  if (dot == std::string_view::npos || (separator != std::string_view::npos && dot < separator)) return {};
  return path.substr(dot);
}

std::string lowerExtension(std::string_view path) { return asciiLower(extension(path)); }

bool hasAsciiSuffix(std::string_view path, std::string_view suffix) {
  return path.size() >= suffix.size() && asciiLower(path.substr(path.size() - suffix.size())) == suffix;
}

bool isLevelCompanionJson(std::string_view path) {
  return hasAsciiSuffix(path, ".llf.json") || hasAsciiSuffix(path, ".dlf.json");
}

std::string withDefaultExtension(std::string_view path, std::string_view default_extension) {
  std::string result = normalizedPath(path);
  if (extension(result).empty()) result += default_extension;
  return result;
}

std::string parentLogicalPath(std::string_view path) {
  const std::size_t separator = path.find_last_of('/');
  return separator == std::string_view::npos ? std::string{} : std::string(path.substr(0, separator));
}

std::string appendLogicalPath(std::string_view base, std::string_view path) {
  if (base.empty()) return normalizedPath(path);
  std::string result(base);
  result.push_back('/');
  result += normalizedPath(path);
  return result;
}

std::string looseLevelBase(std::string_view path) {
  std::string result = normalizedPath(path);
  if (asciiLower(result).ends_with(".json")) result.resize(result.size() - 5U);
  if (asciiLower(result).ends_with(".fts")) result.resize(result.size() - 4U);
  return result;
}

std::filesystem::path looseLevelBase(const std::filesystem::path& path) {
  std::filesystem::path result = path;
  if (asciiLower(pathToUtf8(result.extension())) == ".json") result.replace_extension();
  if (asciiLower(pathToUtf8(result.extension())) == ".fts") result.replace_extension();
  return result;
}

std::string derivedLevelCompanion(std::string_view primary, std::string_view role, bool json) {
  std::string result = looseLevelBase(primary);
  result += role;
  if (json) result += ".json";
  return result;
}

ResourceIoResult<std::optional<std::filesystem::path>> levelCompanionPath(
    const std::filesystem::path& primary, const std::optional<std::filesystem::path>& explicit_path,
    std::string_view role, bool discover) {
  if (explicit_path) return ResourceIoResult<std::optional<std::filesystem::path>>::success(explicit_path);
  if (!discover) return ResourceIoResult<std::optional<std::filesystem::path>>::success(std::nullopt);

  const std::filesystem::path base = looseLevelBase(primary);
  std::filesystem::path binary = base;
  binary += role;
  std::filesystem::path json = binary;
  json += ".json";
  const auto present = [](const std::filesystem::path& candidate, std::error_code& error) {
    const bool regular = std::filesystem::is_regular_file(candidate, error);
    if (error == std::errc::no_such_file_or_directory || error == std::errc::not_a_directory) error.clear();
    return regular;
  };
  std::error_code binary_error;
  const bool has_binary = present(binary, binary_error);
  if (binary_error)
    return loadFailure<std::optional<std::filesystem::path>>(
        ARX_RESOURCE_IO_STAT_FAILED, {}, binary, 0, binary_error.message());
  std::error_code json_error;
  const bool has_json = present(json, json_error);
  if (json_error)
    return loadFailure<std::optional<std::filesystem::path>>(
        ARX_RESOURCE_IO_STAT_FAILED, {}, json, 0, json_error.message());
  if (has_binary && has_json)
    return loadFailure<std::optional<std::filesystem::path>>(
        ARX_RESOURCE_IO_AMBIGUOUS_PATH, {}, primary, 0, "both binary and JSON Level companions are present");
  return ResourceIoResult<std::optional<std::filesystem::path>>::success(
      has_binary ? std::optional<std::filesystem::path>(std::move(binary))
      : has_json ? std::optional<std::filesystem::path>(std::move(json))
                 : std::nullopt);
}

ResourceIoResult<ResourceRead> readNativeFile(const std::filesystem::path& path, ResourceMountMask mount) {
  std::error_code absolute_error;
  const std::filesystem::path absolute = std::filesystem::absolute(path, absolute_error).lexically_normal();
  if (absolute_error) return loadFailure<ResourceRead>(ARX_RESOURCE_IO_STAT_FAILED, {}, path.lexically_normal(), mount);
  std::error_code status_error;
  if (!std::filesystem::is_regular_file(absolute, status_error)) {
    const bool missing = !status_error || status_error == std::errc::no_such_file_or_directory;
    return loadFailure<ResourceRead>(
        missing ? ARX_RESOURCE_IO_NOT_FOUND : ARX_RESOURCE_IO_STAT_FAILED, {}, absolute, mount);
  }
  std::ifstream input(absolute, std::ios::binary | std::ios::ate);
  if (!input) return loadFailure<ResourceRead>(ARX_RESOURCE_IO_OPEN_FAILED, {}, absolute, mount);
  const std::streampos end = input.tellg();
  if (end < 0 ||
      static_cast<std::uintmax_t>(static_cast<std::streamoff>(end)) > std::numeric_limits<std::size_t>::max())
    return loadFailure<ResourceRead>(ARX_RESOURCE_IO_READ_FAILED, {}, absolute, mount);
  ResourceRead result;
  result.data.resize(static_cast<std::size_t>(end));
  result.native_path = absolute;
  result.mount_id = mount;
  input.seekg(0);
  if (!result.data.empty())
    input.read(reinterpret_cast<char*>(result.data.data()), static_cast<std::streamsize>(result.data.size()));
  if (!input) return loadFailure<ResourceRead>(ARX_RESOURCE_IO_READ_FAILED, {}, absolute, mount);
  return ResourceIoResult<ResourceRead>::success(std::move(result));
}

std::vector<std::uint8_t> bytes(std::string_view text) {
  return {reinterpret_cast<const std::uint8_t*>(text.data()),
          reinterpret_cast<const std::uint8_t*>(text.data()) + text.size()};
}

std::filesystem::path utf8Path(std::string_view value) {
  const auto* first = reinterpret_cast<const char8_t*>(value.data());
  return std::filesystem::path(std::u8string_view(first, value.size()));
}

PendingOutput mountedOutput(
    std::string path, std::vector<std::uint8_t> data,
    pistoris::resource_io::ResourceOutputKind kind = pistoris::resource_io::ResourceOutputKind::kData,
    bool primary = false) {
  return {.address = pistoris::resource_io::ResourceOutputAddress::kLogical,
          .kind = kind,
          .primary = primary,
          .owner_kind = ARX_RESOURCE_KIND_NONE,
          .owner_identity = {},
          .resource_path = std::move(path),
          .native_path = {},
          .data = std::move(data)};
}

PendingOutput nativeOutput(
    std::filesystem::path path, std::vector<std::uint8_t> data,
    pistoris::resource_io::ResourceOutputKind kind = pistoris::resource_io::ResourceOutputKind::kData,
    bool primary = false) {
  return {.address = pistoris::resource_io::ResourceOutputAddress::kNative,
          .kind = kind,
          .primary = primary,
          .owner_kind = ARX_RESOURCE_KIND_NONE,
          .owner_identity = {},
          .resource_path = {},
          .native_path = std::move(path),
          .data = std::move(data)};
}

bool includesOutput(const pistoris::resource_io::ResourceOutputOptions& options,
                    pistoris::resource_io::ResourceOutputPart part) noexcept {
  return (options.outputs & static_cast<pistoris::resource_io::ResourceOutputParts>(part)) != 0;
}

pistoris::resource_io::ResourceOutputs preparedWrite(std::vector<PendingOutput> outputs, ArxResourceKind owner_kind,
                                                     std::string_view owner_identity,
                                                     pistoris::resource_io::ResourceIoFlags io_flags) {
  for (PendingOutput& output : outputs) {
    if (output.owner_kind == ARX_RESOURCE_KIND_NONE) output.owner_kind = owner_kind;
    if (output.owner_identity.empty()) output.owner_identity = owner_identity;
    if (output.address == pistoris::resource_io::ResourceOutputAddress::kLogical) output.io_flags = io_flags;
  }
  return outputs;
}

std::string withoutKnownExtension(std::string_view path, std::span<const std::string_view> extensions) {
  const std::string lower = asciiLower(extension(path));
  for (std::string_view candidate : extensions)
    if (lower == candidate) return std::string(path.substr(0, path.size() - lower.size()));
  return std::string(path);
}

std::vector<std::string> fallbackPaths(std::string_view requested, std::span<const std::string_view> extensions) {
  const std::string normalized = normalizedPath(requested);
  std::vector<std::string> result;
  result.push_back(normalized);
  const std::string stem = withoutKnownExtension(normalized, extensions);
  for (std::string_view candidate : extensions) {
    std::string path = stem;
    path += candidate;
    if (std::ranges::find(result, path) == result.end()) result.push_back(std::move(path));
  }
  return result;
}

ResourceIoResult<ResourceRead> readFallback(const ResourceMounts& mounts, std::span<const std::string> bases,
                                            std::span<const std::string_view> extensions, ResourceMountMask mask,
                                            pistoris::resource_io::ResourceIoFlags flags) {
  for (std::size_t base_index = 0; base_index < bases.size(); ++base_index) {
    const std::vector<std::string> candidates = fallbackPaths(bases[base_index], extensions);
    for (std::size_t candidate_index = 0; candidate_index < candidates.size(); ++candidate_index) {
      const std::string& candidate = candidates[candidate_index];
      auto read = mounts.read(candidate, {.mount_mask = mask, .flags = flags});
      const bool last = base_index + 1 == bases.size() && candidate_index + 1 == candidates.size();
      if (read || read.code() != ARX_RESOURCE_IO_NOT_FOUND || last) return read;
    }
  }
  return pistoris::resource_io::detail::resourceIoFailure<ResourceRead>(
      ARX_RESOURCE_IO_NOT_FOUND, ResourceIoOperation::kRead, {}, {}, mask);
}

ResourceIoResult<ResourceRead> readRelated(const ResourceMounts& mounts, const InputContext& input,
                                           std::string_view requested, std::span<const std::string_view> extensions,
                                           ResourceMountMask mask, pistoris::resource_io::ResourceIoFlags flags) {
  const std::vector<std::string> candidates = fallbackPaths(requested, extensions);
  if (input.layout == InputContext::Layout::kMountedGame) {
    const std::array bases = {normalizedPath(requested)};
    return readFallback(mounts, bases, extensions, mask, flags);
  }
  const std::filesystem::path parent = input.native_path.parent_path();
  for (std::size_t index = 0; index < candidates.size(); ++index) {
    const std::string& candidate = candidates[index];
    const std::filesystem::path reference = utf8Path(candidate);
    auto read = readNativeFile(reference.is_absolute() ? reference : parent / reference, input.mount);
    if (read || read.code() != ARX_RESOURCE_IO_NOT_FOUND || index + 1U == candidates.size()) return read;
  }
  return loadFailure<ResourceRead>(
      ARX_RESOURCE_IO_NOT_FOUND, requested, parent / utf8Path(normalizedPath(requested)), input.mount);
}

constexpr std::array<std::string_view, 5> kImageExtensions = {".png", ".jpg", ".jpeg", ".bmp", ".tga"};
constexpr std::array<std::string_view, 3> kAudioExtensions = {".wav", ".mp3", ".ogg"};

std::optional<std::size_t> audioExtension(std::string_view path) {
  const std::string suffix = lowerExtension(path);
  for (std::size_t index = 0; index < kAudioExtensions.size(); ++index)
    if (suffix == kAudioExtensions[index]) return index;
  return std::nullopt;
}

template <class Owner>
pistoris::resource_io::ResourceIoResult<void> loadTextures(Owner& owner,
                                                           const pistoris::resource_io::ResourceMounts& mounts,
                                                           const InputContext& input,
                                                           std::span<const std::string> sources, ResourceMountMask mask,
                                                           pistoris::resource_io::ResourceIoFlags flags) {
  const std::size_t count = std::min(owner.textureCount(), sources.size());
  for (std::size_t index = 0; index < count; ++index) {
    if (sources[index].empty()) continue;
    pistoris::log(ARX_LOG_DEBUG, "Resource I/O: resolving texture source '{}'", sources[index]);
    auto image = readRelated(mounts, input, sources[index], kImageExtensions, mask, flags);
    if (!image) {
      if (image.code() == ARX_RESOURCE_IO_NOT_FOUND) continue;
      return std::move(image).propagate<void>();
    }
    pistoris::log(ARX_LOG_DEBUG,
                  "Resource I/O: resolved texture source '{}' to '{}'",
                  sources[index],
                  pathToUtf8(image->native_path));
    const ArxReturnCode valid = pistoris::binary::validateEncodedImage(image->data);
    if (valid != ARX_OK) return loadFailure<void>(valid, sources[index], image->native_path, image->mount_id);
    const auto set =
        owner.setTextureImage(static_cast<pistoris::TextureIndex>(index), {image->data.data(), image->data.size()});
    if (!set) return conversionFailure<void>(set, sources[index], image->native_path, image->mount_id);
  }
  return pistoris::resource_io::ResourceIoResult<void>::success();
}

template <class Owner>
pistoris::resource_io::ResourceIoResult<void> loadSounds(Owner& owner,
                                                         const pistoris::resource_io::ResourceMounts& mounts,
                                                         const InputContext& input,
                                                         std::span<const pistoris::SoundSourceReference> sources,
                                                         ResourceMountMask mask,
                                                         pistoris::resource_io::ResourceIoFlags flags) {
  for (const pistoris::SoundSourceReference& source : sources) {
    if (source.sound == pistoris::kNoSound || source.path.empty()) continue;
    std::string logical = normalizedPath(source.path);
    if (input.layout == InputContext::Layout::kMountedGame && !logical.starts_with("sfx/")) logical.insert(0, "sfx/");
    auto audio = readRelated(mounts, input, logical, kAudioExtensions, mask, flags);
    if (!audio) {
      if (audio.code() == ARX_RESOURCE_IO_NOT_FOUND) continue;
      return std::move(audio).propagate<void>();
    }
    const ArxReturnCode valid = pistoris::binary::validateEncodedAudio(audio->data);
    if (valid != ARX_OK) return loadFailure<void>(valid, source.path, audio->native_path, audio->mount_id);
    const auto set = owner.setSoundData(source.sound, {audio->data.data(), audio->data.size()});
    if (!set) return conversionFailure<void>(set, source.path, audio->native_path, audio->mount_id);
  }
  return pistoris::resource_io::ResourceIoResult<void>::success();
}

pistoris::resource_io::ResourceIoResult<void> loadModelAnimationSounds(
    pistoris::resource_io::LoadedModel& loaded, const pistoris::resource_io::ResourceMounts& mounts,
    const InputContext& input, std::span<const pistoris::AnimationSoundSourceReference> sources, ResourceMountMask mask,
    pistoris::resource_io::ResourceIoFlags flags) {
  std::vector<std::vector<pistoris::SoundSourceReference>> grouped(loaded.animations.size());
  for (const pistoris::AnimationSoundSourceReference& source : sources) {
    if (source.animation_index >= grouped.size()) {
      return loadFailure<void>(ARX_INVALID_STATE,
                               input.logical_path,
                               input.native_path,
                               input.mount,
                               "GLB Animation sound source has an invalid Animation index");
    }
    grouped[source.animation_index].push_back(source.reference);
  }
  for (std::size_t index = 0; index < loaded.animations.size(); ++index) {
    auto hydrated = loadSounds(loaded.animations[index], mounts, input, grouped[index], mask, flags);
    if (!hydrated) return hydrated;
  }
  return pistoris::resource_io::ResourceIoResult<void>::success();
}

std::optional<std::string> modelPath(const pistoris::paths::ResourceSelector& resource) {
  if (resource.kind != ARX_RESOURCE_KIND_MODEL) return std::nullopt;
  std::string result;
  if (!pistoris::paths::modelFtl({resource.model_type, resource.name, resource.tweak}, result)) return std::nullopt;
  return result;
}

std::optional<std::string> animationPath(const pistoris::paths::ResourceSelector& resource) {
  if (resource.kind != ARX_RESOURCE_KIND_ANIMATION) return std::nullopt;
  std::string result;
  if (!pistoris::paths::animationTea({resource.animation_type, resource.name}, result)) return std::nullopt;
  return result;
}

std::optional<std::string> ambiancePath(const pistoris::paths::ResourceSelector& resource) {
  if (resource.kind != ARX_RESOURCE_KIND_AMBIANCE) return std::nullopt;
  std::string result;
  if (!pistoris::paths::ambianceAmb({resource.name}, result)) return std::nullopt;
  return result;
}

std::optional<std::string> cinematicPath(const pistoris::paths::ResourceSelector& resource) {
  if (resource.kind != ARX_RESOURCE_KIND_CINEMATIC) return std::nullopt;
  std::string result;
  if (!pistoris::paths::cinematicCin({resource.name}, result)) return std::nullopt;
  return result;
}

std::optional<std::string> selectorDocumentPath(const pistoris::paths::ResourceSelector& resource) {
  switch (resource.kind) {
    case ARX_RESOURCE_KIND_MODEL:
      return modelPath(resource);
    case ARX_RESOURCE_KIND_ANIMATION:
      return animationPath(resource);
    case ARX_RESOURCE_KIND_LEVEL:
      return pistoris::paths::levelDlf(resource.level);
    case ARX_RESOURCE_KIND_AMBIANCE:
      return ambiancePath(resource);
    case ARX_RESOURCE_KIND_CINEMATIC:
      return cinematicPath(resource);
    case ARX_RESOURCE_KIND_NONE:
    default:
      return std::nullopt;
  }
}

std::string speechPath(std::string_view source) {
  std::string result = normalizedPath(source);
  if (result.starts_with("speech/")) result.erase(0, std::string_view("speech/").size());
  if (result.ends_with('.')) result.pop_back();
  return result;
}

struct RelatedDirectoryEntry {
  std::string name;
  bool directory = false;
};

ResourceIoResult<std::vector<RelatedDirectoryEntry>> listRelatedDirectory(
    const ResourceMounts& mounts, const InputContext& input, std::string_view relative, ResourceMountMask mask,
    pistoris::resource_io::ResourceIoFlags flags) {
  std::vector<RelatedDirectoryEntry> result;
  if (input.layout == InputContext::Layout::kMountedGame) {
    const std::string logical = normalizedPath(relative);
    auto entries = mounts.listDirectory(logical, {.mount_mask = mask, .flags = flags});
    if (!entries) {
      if (entries.code() == ARX_RESOURCE_IO_NOT_FOUND)
        return ResourceIoResult<std::vector<RelatedDirectoryEntry>>::success({});
      return std::move(entries).propagate<std::vector<RelatedDirectoryEntry>>();
    }
    result.reserve(entries->size());
    for (const pistoris::resource_io::ResourceDirectoryEntry& entry : *entries) {
      result.push_back({entry.name, entry.kind == pistoris::resource_io::ResourceDirectoryEntry::Kind::kDirectory});
    }
    return ResourceIoResult<std::vector<RelatedDirectoryEntry>>::success(std::move(result));
  }

  const std::filesystem::path directory =
      (input.native_path.parent_path() / utf8Path(normalizedPath(relative))).lexically_normal();
  std::error_code error;
  std::filesystem::directory_iterator iterator(directory, error);
  if (error == std::errc::no_such_file_or_directory)
    return ResourceIoResult<std::vector<RelatedDirectoryEntry>>::success({});
  if (error)
    return loadFailure<std::vector<RelatedDirectoryEntry>>(
        ARX_RESOURCE_IO_STAT_FAILED, input.logical_path, directory, input.mount);
  const std::filesystem::directory_iterator end;
  while (iterator != end) {
    const std::filesystem::directory_entry& entry = *iterator;
    const bool directory_entry = entry.is_directory(error);
    if (error)
      return loadFailure<std::vector<RelatedDirectoryEntry>>(
          ARX_RESOURCE_IO_STAT_FAILED, input.logical_path, entry.path(), input.mount);
    const bool file_entry = directory_entry ? false : entry.is_regular_file(error);
    if (error)
      return loadFailure<std::vector<RelatedDirectoryEntry>>(
          ARX_RESOURCE_IO_STAT_FAILED, input.logical_path, entry.path(), input.mount);
    if (directory_entry || file_entry) result.push_back({pathToUtf8(entry.path().filename()), directory_entry});
    iterator.increment(error);
    if (error)
      return loadFailure<std::vector<RelatedDirectoryEntry>>(
          ARX_RESOURCE_IO_STAT_FAILED, input.logical_path, directory, input.mount);
  }
  std::ranges::sort(result, {}, &RelatedDirectoryEntry::name);
  return ResourceIoResult<std::vector<RelatedDirectoryEntry>>::success(std::move(result));
}

struct PreparedCinematicSound {
  pistoris::SoundHandle sound = pistoris::kNoSoundHandle;
  pistoris::SoundKind kind = pistoris::SoundKind::kEffect;
  std::string source_path;
  std::string canonical_path;
  std::optional<std::size_t> preferred_extension;
};

bool splitLanguageDecoration(std::string& path) {
  if (path.empty() || path.back() != ']') return false;
  const std::size_t separator = path.find_last_of('/');
  const std::size_t open = path.find_last_of('[');
  if (open == std::string::npos || open == 0 || (separator != std::string::npos && open < separator) ||
      open + 2U > path.size())
    return false;
  path.resize(open);
  return true;
}

ResourceIoResult<std::vector<PreparedCinematicSound>> prepareGlbSounds(
    pistoris::Cinematic& cinematic, std::span<const pistoris::CinematicSoundSourceReference> sources,
    const InputContext& input, ResourceMountMask mask) {
  std::vector<PreparedCinematicSound> result;
  result.reserve(sources.size());
  std::vector<std::pair<pistoris::SoundHandle, std::string>> paths;
  for (const pistoris::CinematicSoundSourceReference& source : sources) {
    PreparedCinematicSound prepared;
    prepared.sound = source.sound;
    if (pistoris::soundHandleKind(source.sound, prepared.kind) != ARX_OK)
      return loadFailure<std::vector<PreparedCinematicSound>>(ARX_CINEMATIC_BAD_SOUND_PATH,
                                                              input.logical_path,
                                                              input.native_path,
                                                              mask,
                                                              "GLB sound source has an invalid handle");
    prepared.source_path = normalizedPath(source.path);
    prepared.canonical_path = prepared.source_path;
    prepared.preferred_extension = audioExtension(prepared.canonical_path);
    if (prepared.preferred_extension)
      prepared.canonical_path.resize(prepared.canonical_path.size() -
                                     kAudioExtensions[*prepared.preferred_extension].size());
    if (prepared.kind == pistoris::SoundKind::kSpeech) (void)splitLanguageDecoration(prepared.canonical_path);
    if (prepared.canonical_path.ends_with('.')) prepared.canonical_path.pop_back();

    const auto existing = std::ranges::find(paths, source.sound, &std::pair<pistoris::SoundHandle, std::string>::first);
    if (existing == paths.end()) {
      auto set = cinematic.setSoundPath(source.sound, prepared.canonical_path);
      if (!set)
        return conversionFailure<std::vector<PreparedCinematicSound>>(set, input.logical_path, input.native_path, mask);
      paths.emplace_back(source.sound, asciiLower(prepared.canonical_path));
    } else if (existing->second != asciiLower(prepared.canonical_path)) {
      return loadFailure<std::vector<PreparedCinematicSound>>(ARX_CINEMATIC_DUPLICATE_SOUND_PATH,
                                                              input.logical_path,
                                                              input.native_path,
                                                              mask,
                                                              "one GLB sound resolves to multiple logical paths");
    }
    result.push_back(std::move(prepared));
  }
  return ResourceIoResult<std::vector<PreparedCinematicSound>>::success(std::move(result));
}

bool glbSpeechFilename(std::string_view filename, std::string_view expected_stem, std::string& language,
                       std::size_t& extension_index) {
  const std::optional<std::size_t> known = audioExtension(filename);
  if (!known) return false;
  const std::string_view suffix = kAudioExtensions[*known];
  const std::string_view stem = filename.substr(0, filename.size() - suffix.size());
  if (stem.size() <= expected_stem.size() + 2U || stem[expected_stem.size()] != '[' || stem.back() != ']' ||
      asciiLower(stem.substr(0, expected_stem.size())) != asciiLower(expected_stem))
    return false;
  language.assign(stem.substr(expected_stem.size() + 1U, stem.size() - expected_stem.size() - 2U));
  extension_index = *known;
  return !language.empty();
}

std::string_view textView(std::span<const std::uint8_t> data) {
  return {reinterpret_cast<const char*>(data.data()), data.size()};
}

std::string_view fixedText(const char* data, std::size_t capacity) {
  std::size_t size = 0;
  while (size < capacity && data[size] != '\0') ++size;
  return {data, size};
}

bool nativeText(std::string_view value, std::string& out) {
  if (pistoris::binary::classifyTextEncoding(value) != pistoris::binary::TextEncoding::kLatin1) {
    out.assign(value);
    return true;
  }
  return pistoris::binary::latin1ToUtf8(value, out) == ARX_OK;
}

std::string replaceExtension(std::string_view path, std::string_view replacement) {
  const std::string_view suffix = extension(path);
  std::string result(path.substr(0, path.size() - suffix.size()));
  result += replacement;
  return result;
}

std::string filenameStem(std::string_view path) {
  const std::size_t separator = path.find_last_of('/');
  const std::string_view filename = separator == std::string_view::npos ? path : path.substr(separator + 1U);
  return replaceExtension(filename, "");
}

template <class T>
ResourceIoResult<T> invalidInput(std::string_view path, const std::filesystem::path& native_path,
                                 ResourceMountMask mask, std::string_view detail) {
  return loadFailure<T>(ARX_INVALID_OPTIONS, path, native_path, mask, detail);
}

template <class T>
ResourceIoResult<T> invalidOutput(std::string_view path, const std::filesystem::path& native_path,
                                  std::string_view detail) {
  return writeFailure<T>(ARX_INVALID_OPTIONS, path, native_path, detail);
}

ResourceIoResult<void> setCinematicAudio(pistoris::Cinematic& cinematic, pistoris::SoundHandle sound,
                                         pistoris::LanguageId language, std::string_view source, ResourceRead&& audio) {
  const ArxReturnCode valid = pistoris::binary::validateEncodedAudio(audio.data);
  if (valid != ARX_OK) return loadFailure<void>(valid, source, audio.native_path, audio.mount_id);
  const auto set = cinematic.setSoundData(sound, language, {audio.data.data(), audio.data.size()});
  if (!set) return conversionFailure<void>(set, source, audio.native_path, audio.mount_id);
  return ResourceIoResult<void>::success();
}

ResourceIoResult<pistoris::LanguageId> cinematicLanguage(pistoris::Cinematic& cinematic, std::string_view name,
                                                         std::string_view source, const ResourceRead& audio) {
  const std::string canonical = asciiLower(name);
  if (const std::optional<pistoris::LanguageId> found = cinematic.findLanguage(canonical))
    return ResourceIoResult<pistoris::LanguageId>::success(*found);
  auto added = cinematic.addLanguage(canonical);
  if (!added) return conversionFailure<pistoris::LanguageId>(added, source, audio.native_path, audio.mount_id);
  return ResourceIoResult<pistoris::LanguageId>::success(*added);
}

ResourceIoResult<void> hydrateNativeCinematicSounds(pistoris::Cinematic& cinematic, const ResourceMounts& mounts,
                                                    const InputContext& input,
                                                    std::span<const pistoris::CinematicSoundSourceReference> sounds,
                                                    ResourceMountMask mask,
                                                    pistoris::resource_io::ResourceIoFlags flags) {
  auto languages = listRelatedDirectory(mounts, input, "speech", mask, flags);
  if (!languages) return std::move(languages).propagate<void>();
  for (const pistoris::CinematicSoundSourceReference& source : sounds) {
    pistoris::SoundKind kind = pistoris::SoundKind::kEffect;
    if (pistoris::soundHandleKind(source.sound, kind) != ARX_OK) continue;
    if (kind == pistoris::SoundKind::kEffect) {
      std::string logical = speechPath(source.path);
      if (!logical.starts_with("sfx/")) logical.insert(0, "sfx/");
      auto audio = readRelated(mounts, input, logical, kAudioExtensions, mask, flags);
      if (!audio) {
        if (audio.code() == ARX_RESOURCE_IO_NOT_FOUND) continue;
        return std::move(audio).propagate<void>();
      }
      auto set = setCinematicAudio(cinematic, source.sound, pistoris::kSoundEffects, source.path, std::move(*audio));
      if (!set) return set;
      continue;
    }

    const std::string path = speechPath(source.path);
    for (const RelatedDirectoryEntry& entry : *languages) {
      if (!entry.directory) continue;
      const std::string logical = appendLogicalPath(appendLogicalPath("speech", entry.name), path);
      auto audio = readRelated(mounts, input, logical, kAudioExtensions, mask, flags);
      if (!audio) {
        if (audio.code() == ARX_RESOURCE_IO_NOT_FOUND) continue;
        return std::move(audio).propagate<void>();
      }
      auto language = cinematicLanguage(cinematic, entry.name, source.path, *audio);
      if (!language) return std::move(language).propagate<void>();
      auto set = setCinematicAudio(cinematic, source.sound, *language, source.path, std::move(*audio));
      if (!set) return set;
    }
  }
  return ResourceIoResult<void>::success();
}

ResourceIoResult<void> hydrateGlbCinematicSounds(pistoris::Cinematic& cinematic, const ResourceMounts& mounts,
                                                 const InputContext& input,
                                                 std::span<const pistoris::CinematicSoundSourceReference> sounds,
                                                 ResourceMountMask mask, pistoris::resource_io::ResourceIoFlags flags) {
  auto prepared = prepareGlbSounds(cinematic, sounds, input, mask);
  if (!prepared) return std::move(prepared).propagate<void>();
  std::unordered_set<pistoris::SoundHandle> loaded_effects;
  std::unordered_set<std::string> loaded_speech;
  for (const PreparedCinematicSound& source : *prepared) {
    if (source.kind == pistoris::SoundKind::kEffect) {
      if (loaded_effects.contains(source.sound)) continue;
      auto audio = readRelated(mounts, input, source.source_path, kAudioExtensions, mask, flags);
      if (!audio) {
        if (audio.code() == ARX_RESOURCE_IO_NOT_FOUND) continue;
        return std::move(audio).propagate<void>();
      }
      auto set =
          setCinematicAudio(cinematic, source.sound, pistoris::kSoundEffects, source.source_path, std::move(*audio));
      if (!set) return set;
      loaded_effects.insert(source.sound);
      continue;
    }

    const std::size_t separator = source.canonical_path.find_last_of('/');
    const std::string parent =
        separator == std::string::npos ? std::string{} : source.canonical_path.substr(0, separator);
    const std::string_view stem = separator == std::string::npos
                                      ? std::string_view(source.canonical_path)
                                      : std::string_view(source.canonical_path).substr(separator + 1U);
    auto entries = listRelatedDirectory(mounts, input, parent, mask, flags);
    if (!entries) return std::move(entries).propagate<void>();
    struct Candidate {
      std::string name;
      std::string language;
      std::size_t extension = 0;
    };
    std::vector<Candidate> candidates;
    for (const RelatedDirectoryEntry& entry : *entries) {
      if (entry.directory) continue;
      std::string language;
      std::size_t extension_index = 0;
      if (glbSpeechFilename(entry.name, stem, language, extension_index))
        candidates.push_back({entry.name, std::move(language), extension_index});
    }
    std::ranges::stable_sort(candidates, [&](const Candidate& left, const Candidate& right) {
      const std::string left_language = asciiLower(left.language);
      const std::string right_language = asciiLower(right.language);
      if (left_language != right_language) return left_language < right_language;
      const auto priority = [&](std::size_t extension_index) {
        if (!source.preferred_extension) return extension_index;
        return extension_index == *source.preferred_extension ? 0U : 1U + extension_index;
      };
      return priority(left.extension) < priority(right.extension);
    });
    for (const Candidate& candidate : candidates) {
      std::string assignment = std::to_string(source.sound);
      assignment.push_back(':');
      assignment += asciiLower(candidate.language);
      if (loaded_speech.contains(assignment)) continue;
      auto audio = readRelated(mounts, input, appendLogicalPath(parent, candidate.name), {}, mask, flags);
      if (!audio) {
        if (audio.code() == ARX_RESOURCE_IO_NOT_FOUND) continue;
        return std::move(audio).propagate<void>();
      }
      auto language = cinematicLanguage(cinematic, candidate.language, source.source_path, *audio);
      if (!language) return std::move(language).propagate<void>();
      auto set = setCinematicAudio(cinematic, source.sound, *language, source.source_path, std::move(*audio));
      if (!set) return set;
      loaded_speech.insert(std::move(assignment));
    }
  }
  return ResourceIoResult<void>::success();
}

ResourceIoResult<void> hydrateCinematic(pistoris::Cinematic& cinematic, const ResourceMounts& mounts,
                                        const InputContext& input, std::vector<std::string> illustrations,
                                        std::span<const pistoris::CinematicSoundSourceReference> sounds, bool glb,
                                        ResourceMountMask mask, pistoris::resource_io::ResourceIoFlags flags) {
  for (std::string& illustration : illustrations) {
    illustration = normalizedPath(illustration);
    if (input.layout == InputContext::Layout::kMountedGame && illustration.find('/') == std::string::npos) {
      illustration.insert(0, 1, '/');
      illustration.insert(0, pistoris::paths::cinematicIllustrationDirectory());
    }
  }
  auto hydrated = loadTextures(cinematic, mounts, input, illustrations, mask, flags);
  if (!hydrated) return hydrated;
  return glb ? hydrateGlbCinematicSounds(cinematic, mounts, input, sounds, mask, flags)
             : hydrateNativeCinematicSounds(cinematic, mounts, input, sounds, mask, flags);
}

ResourceIoResult<std::optional<ResourceRead>> readOptionalRelated(const ResourceMounts& mounts,
                                                                  const InputContext& input, std::string_view requested,
                                                                  std::span<const std::string_view> extensions,
                                                                  ResourceMountMask mask,
                                                                  pistoris::resource_io::ResourceIoFlags flags) {
  auto read = readRelated(mounts, input, requested, extensions, mask, flags);
  if (read) {
    return ResourceIoResult<std::optional<ResourceRead>>::success(std::optional<ResourceRead>(std::move(*read)));
  }
  if (read.code() == ARX_RESOURCE_IO_NOT_FOUND)
    return ResourceIoResult<std::optional<ResourceRead>>::success(std::nullopt);
  return std::move(read).propagate<std::optional<ResourceRead>>();
}

bool modelEntityClassFromFtl(std::string_view path, std::string& out) {
  pistoris::paths::ModelPathView model;
  if (pistoris::paths::modelFromFtl(path, model)) return pistoris::paths::baseEntityClassFromModel(model, out);
  return pistoris::paths::entityClassFromFtl(path, out);
}

std::string looseSidecarStem(const InputContext& input, std::string_view suffix) {
  return filenameStem(pathToUtf8(input.native_path.filename())) + std::string(suffix);
}

ResourceIoResult<void> hydrateModelInventoryIcon(pistoris::Model& model, const ResourceMounts& mounts,
                                                 const InputContext& input, ResourceMountMask mask,
                                                 pistoris::resource_io::ResourceIoFlags flags) {
  std::string stem;
  if (input.layout == InputContext::Layout::kMountedGame) {
    std::string entity_class;
    if (!modelEntityClassFromFtl(input.logical_path, entity_class) ||
        !pistoris::paths::itemIconFromEntityClass(entity_class, stem) || stem.empty())
      return ResourceIoResult<void>::success();
  } else {
    stem = looseSidecarStem(input, "[icon]");
  }
  auto icon = readOptionalRelated(mounts, input, stem, kImageExtensions, mask, flags);
  if (!icon) return std::move(icon).propagate<void>();
  if (!*icon) return ResourceIoResult<void>::success();
  const auto set = model.setInventoryIcon({(*icon)->data.data(), (*icon)->data.size()});
  if (!set) return conversionFailure<void>(set, stem, (*icon)->native_path, (*icon)->mount_id);
  return ResourceIoResult<void>::success();
}

bool nextToken(std::string_view line, std::size_t& offset, std::string_view& out) noexcept {
  while (offset < line.size() && (line[offset] == ' ' || line[offset] == '\t' || line[offset] == '\r')) ++offset;
  const std::size_t begin = offset;
  while (offset < line.size() && line[offset] != ' ' && line[offset] != '\t' && line[offset] != '\r') ++offset;
  out = line.substr(begin, offset - begin);
  return !out.empty();
}

bool parseMinimapOffsetLine(std::string_view line, pistoris::ArxVector2& out) noexcept {
  std::size_t offset = 0;
  std::string_view label;
  std::string_view x;
  std::string_view y;
  return nextToken(line, offset, label) && nextToken(line, offset, x) && nextToken(line, offset, y) &&
         pistoris::parseFiniteFloat(x, out.x) && pistoris::parseFiniteFloat(y, out.y);
}

ResourceIoResult<pistoris::ArxVector2> parseMinimapOffsetRecord(std::span<const std::uint8_t> data,
                                                                std::uint64_t record, std::string_view resource_path,
                                                                const std::filesystem::path& native_path,
                                                                ResourceMountMask mount) {
  std::string_view text = textView(data);
  for (std::uint64_t index = 0; index <= record; ++index) {
    const std::size_t newline = text.find('\n');
    pistoris::ArxVector2 offset{};
    if (!parseMinimapOffsetLine(text.substr(0, newline), offset))
      return loadFailure<pistoris::ArxVector2>(
          ARX_RESOURCE_IO_INVALID_METADATA, resource_path, native_path, mount, "malformed minimap offset entry");
    if (index == record) return ResourceIoResult<pistoris::ArxVector2>::success(offset);
    if (newline == std::string_view::npos) break;
    text.remove_prefix(newline + 1U);
  }
  return loadFailure<pistoris::ArxVector2>(
      ARX_RESOURCE_IO_INVALID_METADATA, resource_path, native_path, mount, "missing minimap offset entry");
}

ResourceIoResult<pistoris::ArxVector2> resolveMinimapProjectionOffset(
    pistoris::resource_io::ResourceLayout layout, std::optional<std::uint32_t> level, pistoris::ArxVector2 stored,
    std::string_view resource_path, const std::filesystem::path& native_path, ResourceMountMask mount) {
  if (layout == pistoris::resource_io::ResourceLayout::kLoose)
    return ResourceIoResult<pistoris::ArxVector2>::success(stored);
  if (!level)
    return loadFailure<pistoris::ArxVector2>(
        ARX_INVALID_STATE, resource_path, native_path, mount, "mounted minimap has no level identity");
  pistoris::ArxVector2 projection{};
  const ArxReturnCode rc = pistoris::level_images::projectionOffsetForLevel(*level, stored, projection);
  if (rc != ARX_OK)
    return loadFailure<pistoris::ArxVector2>(
        rc, resource_path, native_path, mount, "minimap projection offset is not representable");
  return ResourceIoResult<pistoris::ArxVector2>::success(projection);
}

ResourceIoResult<pistoris::ArxVector2> mountedMinimapProjectionOffset(const ResourceMounts& mounts, std::uint32_t level,
                                                                      ResourceMountMask mask,
                                                                      pistoris::resource_io::ResourceIoFlags flags) {
  constexpr std::uint32_t kOffsetEntryCount = 29;
  pistoris::ArxVector2 stored{};
  if (level < kOffsetEntryCount) {
    auto input = mounts.read(pistoris::paths::minimapOffsetsFile(), {.mount_mask = mask, .flags = flags});
    if (input) {
      auto parsed = parseMinimapOffsetRecord(
          input->data, level, pistoris::paths::minimapOffsetsFile(), input->native_path, input->mount_id);
      if (!parsed) return parsed;
      stored = *parsed;
    } else if (input.code() != ARX_RESOURCE_IO_NOT_FOUND) {
      return std::move(input).propagate<pistoris::ArxVector2>();
    }
  }
  return resolveMinimapProjectionOffset(
      pistoris::resource_io::ResourceLayout::kGame, level, stored, pistoris::paths::minimapOffsetsFile(), {}, mask);
}

bool parseProjectionOffsetSuffix(std::string_view suffix, pistoris::ArxVector2& out) noexcept {
  constexpr std::string_view kPrefix = "[offset_";
  if (!suffix.starts_with(kPrefix) || !suffix.ends_with(']')) return false;
  suffix.remove_prefix(kPrefix.size());
  suffix.remove_suffix(1);
  const std::size_t separator = suffix.find('_');
  if (separator == std::string_view::npos || suffix.find('_', separator + 1U) != std::string_view::npos) return false;
  return pistoris::parseFiniteFloat(suffix.substr(0, separator), out.x) &&
         pistoris::parseFiniteFloat(suffix.substr(separator + 1U), out.y);
}

struct LooseMinimapCandidate {
  std::string filename;
  pistoris::ArxVector2 projection_offset{};
  std::size_t extension_priority = 0;
};

std::optional<LooseMinimapCandidate> looseMinimapCandidate(std::string_view filename, std::string_view expected_stem) {
  const std::optional<std::size_t> extension_index = [&]() -> std::optional<std::size_t> {
    const std::string suffix = lowerExtension(filename);
    for (std::size_t index = 0; index < kImageExtensions.size(); ++index)
      if (suffix == kImageExtensions[index]) return index;
    return std::nullopt;
  }();
  if (!extension_index) return std::nullopt;
  const std::string_view stem = filename.substr(0, filename.size() - kImageExtensions[*extension_index].size());
  if (!asciiLower(stem).starts_with(asciiLower(expected_stem))) return std::nullopt;
  pistoris::ArxVector2 offset{};
  if (!parseProjectionOffsetSuffix(stem.substr(expected_stem.size()), offset)) return std::nullopt;
  return LooseMinimapCandidate{std::string(filename), offset, *extension_index};
}

ResourceIoResult<void> hydrateLevelImages(pistoris::Level& level, const ResourceMounts& mounts,
                                          const InputContext& input, std::optional<std::uint32_t> level_index,
                                          ResourceMountMask mask, pistoris::resource_io::ResourceIoFlags flags) {
  const bool mounted = input.layout == InputContext::Layout::kMountedGame;
  if (mounted && !level_index)
    return loadFailure<void>(
        ARX_INVALID_STATE, input.logical_path, input.native_path, input.mount, "mounted Level has no level index");
  const std::uint32_t game_level = level_index.value_or(0);

  std::optional<ResourceRead> minimap;
  pistoris::ArxVector2 projection_offset{};
  std::string minimap_source;
  if (mounted) {
    minimap_source = pistoris::paths::levelMinimap(game_level);
    auto image = readOptionalRelated(mounts, input, minimap_source, kImageExtensions, mask, flags);
    if (!image) return std::move(image).propagate<void>();
    minimap = std::move(*image);
    if (minimap) {
      auto offset = mountedMinimapProjectionOffset(mounts, game_level, mask, flags);
      if (!offset) return std::move(offset).propagate<void>();
      projection_offset = *offset;
    }
  } else {
    minimap_source = looseSidecarStem(input, "[map]");
    auto exact = readOptionalRelated(mounts, input, minimap_source, kImageExtensions, mask, flags);
    if (!exact) return std::move(exact).propagate<void>();
    minimap = std::move(*exact);
    if (!minimap) {
      auto entries = listRelatedDirectory(mounts, input, {}, mask, flags);
      if (!entries) return std::move(entries).propagate<void>();
      std::vector<LooseMinimapCandidate> candidates;
      for (const RelatedDirectoryEntry& entry : *entries) {
        if (entry.directory) continue;
        if (auto candidate = looseMinimapCandidate(entry.name, minimap_source))
          candidates.push_back(std::move(*candidate));
      }
      std::ranges::sort(candidates, [](const LooseMinimapCandidate& left, const LooseMinimapCandidate& right) {
        if (left.extension_priority != right.extension_priority)
          return left.extension_priority < right.extension_priority;
        return asciiLower(left.filename) < asciiLower(right.filename);
      });
      if (!candidates.empty()) {
        if (candidates.size() > 1U) {
          pistoris::log(ARX_LOG_WARN,
                        "Multiple loose minimap sidecars match '{}'; using '{}' and ignoring {} alternative(s)",
                        minimap_source,
                        candidates.front().filename,
                        candidates.size() - 1U);
        }
        auto image = readRelated(mounts, input, candidates.front().filename, {}, mask, flags);
        if (!image) return std::move(image).propagate<void>();
        projection_offset = candidates.front().projection_offset;
        minimap_source = candidates.front().filename;
        minimap = std::move(*image);
      }
    }
  }
  if (minimap) {
    const auto set = level.setMinimapFromProjection({minimap->data.data(), minimap->data.size()}, projection_offset);
    if (!set) return conversionFailure<void>(set, minimap_source, minimap->native_path, minimap->mount_id);
  }

  const std::string loading_source =
      mounted ? pistoris::paths::levelLoadingScreen(game_level) : looseSidecarStem(input, "[loading]");
  auto loading = readOptionalRelated(mounts, input, loading_source, kImageExtensions, mask, flags);
  if (!loading) return std::move(loading).propagate<void>();
  if (*loading) {
    const auto set = level.setLoadingScreen({(*loading)->data.data(), (*loading)->data.size()});
    if (!set) return conversionFailure<void>(set, loading_source, (*loading)->native_path, (*loading)->mount_id);
  }
  return ResourceIoResult<void>::success();
}

std::string projectionOffsetSuffix(pistoris::ArxVector2 offset) {
  if (offset.x == 0.0f && offset.y == 0.0f) return {};
  if (offset.x == 0.0f) offset.x = 0.0f;
  if (offset.y == 0.0f) offset.y = 0.0f;
  std::ostringstream result;
  result.imbue(std::locale::classic());
  result << std::setprecision(std::numeric_limits<float>::max_digits10) << "[offset_" << offset.x << '_' << offset.y
         << ']';
  return result.str();
}

ResourceIoResult<std::vector<std::uint8_t>> renderInventoryIcon(const pistoris::Model& model,
                                                                pistoris::ImageFormat format,
                                                                std::string_view resource_path,
                                                                const std::filesystem::path& native_path) {
  pistoris::Model::InventoryIconRenderOptions options;
  options.format = format;
  auto rendered = model.renderIcon(options);
  if (!rendered) return writeConversionFailure<std::vector<std::uint8_t>>(rendered, resource_path, native_path);
  return ResourceIoResult<std::vector<std::uint8_t>>::success(std::move(*rendered));
}

ResourceIoResult<void> appendModelInventoryIconOutputs(const pistoris::Model& model, std::string_view logical_path,
                                                       const std::filesystem::path& native_path, bool game_native,
                                                       std::vector<PendingOutput>& outputs) {
  if (model.inventoryIcon().encoded_image.size == 0) return ResourceIoResult<void>::success();
  std::string stem;
  if (game_native) {
    std::string entity_class;
    if (!modelEntityClassFromFtl(logical_path, entity_class) ||
        !pistoris::paths::itemIconFromEntityClass(entity_class, stem) || stem.empty())
      return ResourceIoResult<void>::success();
    auto png = renderInventoryIcon(model, pistoris::ImageFormat::kPng, stem, {});
    if (!png) return std::move(png).propagate<void>();
    auto bmp = renderInventoryIcon(model, pistoris::ImageFormat::kBmp, stem, {});
    if (!bmp) return std::move(bmp).propagate<void>();
    outputs.push_back(mountedOutput(stem + ".png", std::move(*png), ResourceOutputKind::kImage));
    outputs.push_back(mountedOutput(stem + ".bmp", std::move(*bmp), ResourceOutputKind::kImage));
    return ResourceIoResult<void>::success();
  }

  auto png = renderInventoryIcon(model, pistoris::ImageFormat::kPng, logical_path, native_path);
  if (!png) return std::move(png).propagate<void>();
  if (!logical_path.empty()) {
    const std::string sibling =
        appendLogicalPath(parentLogicalPath(logical_path), filenameStem(logical_path) + "[icon].png");
    outputs.push_back(mountedOutput(sibling, std::move(*png), ResourceOutputKind::kImage));
  } else {
    const std::filesystem::path sibling =
        native_path.parent_path() / utf8Path(pathToUtf8(native_path.stem()) + "[icon].png");
    outputs.push_back(nativeOutput(sibling, std::move(*png), ResourceOutputKind::kImage));
  }
  return ResourceIoResult<void>::success();
}

ResourceIoResult<void> appendLevelImageOutputs(const pistoris::Level& level, const ResourceMounts& mounts,
                                               std::string_view logical_path, const std::filesystem::path& native_path,
                                               bool game_layout, std::optional<std::uint32_t> level_index,
                                               pistoris::resource_io::ResourceIoFlags flags,
                                               std::vector<PendingOutput>& outputs) {
  if (game_layout && !level_index)
    return writeFailure<void>(ARX_INVALID_STATE, logical_path, native_path, "mounted Level has no level index");
  const std::uint32_t game_level = level_index.value_or(0);
  if (level.minimap().encoded_image.size != 0) {
    pistoris::Level::MinimapRenderOptions options;
    pistoris::ArxVector2 offset{};
    if (game_layout) {
      auto resolved =
          mountedMinimapProjectionOffset(mounts, game_level, pistoris::resource_io::kAllResourceMounts, flags);
      if (!resolved) return std::move(resolved).propagate<void>();
      offset = *resolved;
      options.mode = pistoris::level_images::MinimapRenderMode::kGame;
      options.projection_offset = offset;
    }
    auto rendered = level.renderMinimap(options);
    if (!rendered) return writeConversionFailure<void>(rendered, logical_path, native_path);
    if (game_layout) {
      outputs.push_back(mountedOutput(pistoris::paths::levelMinimap(game_level) + ".png",
                                      std::move(rendered->encoded_image),
                                      ResourceOutputKind::kImage));
    } else if (!logical_path.empty()) {
      const std::string sibling = appendLogicalPath(
          parentLogicalPath(logical_path),
          filenameStem(logical_path) + "[map]" + projectionOffsetSuffix(rendered->projection_offset) + ".png");
      outputs.push_back(mountedOutput(sibling, std::move(rendered->encoded_image), ResourceOutputKind::kImage));
    } else {
      const std::string stem =
          pathToUtf8(native_path.stem()) + "[map]" + projectionOffsetSuffix(rendered->projection_offset) + ".png";
      outputs.push_back(nativeOutput(
          native_path.parent_path() / utf8Path(stem), std::move(rendered->encoded_image), ResourceOutputKind::kImage));
    }
  }
  if (level.loadingScreen().size != 0) {
    const auto layout = game_layout ? (game_level == 10 ? pistoris::level_images::LoadingScreenLayout::kFullscreen
                                                        : pistoris::level_images::LoadingScreenLayout::kNormal)
                                    : pistoris::level_images::LoadingScreenLayout::kOriginal;
    auto rendered = level.renderLoadingScreen({.layout = layout});
    if (!rendered) return writeConversionFailure<void>(rendered, logical_path, native_path);
    if (game_layout) {
      outputs.push_back(mountedOutput(
          pistoris::paths::levelLoadingScreen(game_level) + ".png", std::move(*rendered), ResourceOutputKind::kImage));
    } else if (!logical_path.empty()) {
      const std::string sibling =
          appendLogicalPath(parentLogicalPath(logical_path), filenameStem(logical_path) + "[loading].png");
      outputs.push_back(mountedOutput(sibling, std::move(*rendered), ResourceOutputKind::kImage));
    } else {
      outputs.push_back(
          nativeOutput(native_path.parent_path() / utf8Path(pathToUtf8(native_path.stem()) + "[loading].png"),
                       std::move(*rendered),
                       ResourceOutputKind::kImage));
    }
  }
  return ResourceIoResult<void>::success();
}

ResourceIoResult<pistoris::resource_io::LoadedModel> importModelResource(
    std::span<const std::uint8_t> input_bytes, pistoris::resource_io::ResourceFormat format,
    const ResourceMounts& mounts, const InputContext& input, ResourceMountMask mask,
    pistoris::resource_io::ResourceIoFlags flags, const pistoris::resource_io::ModelLoadOptions& options) {
  const std::string_view resource_path = input.logical_path;
  std::vector<std::string> textures;
  std::vector<pistoris::AnimationSoundSourceReference> sounds;
  pistoris::resource_io::LoadedModel loaded;
  if (format == pistoris::resource_io::ResourceFormat::kFtl) {
    auto native = pistoris::readFtl(input_bytes);
    if (!native)
      return conversionFailure<pistoris::resource_io::LoadedModel>(
          native, resource_path, input.native_path, input.mount);
    auto result = pistoris::Model::importNative(*native, &textures, options.native_text_mode);
    if (!result)
      return conversionFailure<pistoris::resource_io::LoadedModel>(
          result, resource_path, input.native_path, input.mount);
    loaded.model = std::move(*result);
  } else if (format == pistoris::resource_io::ResourceFormat::kJson) {
    auto native = pistoris::fromFtlJson(textView(input_bytes));
    if (!native)
      return conversionFailure<pistoris::resource_io::LoadedModel>(
          native, resource_path, input.native_path, input.mount);
    auto result = pistoris::Model::importNative(*native, &textures, pistoris::NativeTextMode::kUtf8);
    if (!result)
      return conversionFailure<pistoris::resource_io::LoadedModel>(
          result, resource_path, input.native_path, input.mount);
    loaded.model = std::move(*result);
  } else if (format == pistoris::resource_io::ResourceFormat::kGlb) {
    auto result = pistoris::Model::importGlbWithAnimations(input_bytes, options.glb, nullptr, &textures, &sounds);
    if (!result)
      return conversionFailure<pistoris::resource_io::LoadedModel>(
          result, resource_path, input.native_path, input.mount);
    loaded.model = std::move(result->model);
    loaded.animations = std::move(result->animations);
  } else if (format == pistoris::resource_io::ResourceFormat::kObj) {
    auto paths = pistoris::objMaterialLibraryPaths(textView(input_bytes));
    if (!paths)
      return conversionFailure<pistoris::resource_io::LoadedModel>(
          paths, resource_path, input.native_path, input.mount);
    std::vector<std::string> texts;
    std::vector<pistoris::ObjMaterialLibraryView> libraries;
    texts.reserve(paths->size());
    libraries.reserve(paths->size());
    for (const std::string& material_path : *paths) {
      auto material = readRelated(mounts, input, material_path, {}, mask, flags);
      if (!material) return std::move(material).propagate<pistoris::resource_io::LoadedModel>();
      texts.emplace_back(textView(material->data));
      libraries.push_back({material_path, texts.back()});
    }
    auto result = pistoris::Model::importObj(textView(input_bytes), libraries, &textures);
    if (!result)
      return conversionFailure<pistoris::resource_io::LoadedModel>(
          result, resource_path, input.native_path, input.mount);
    loaded.model = std::move(*result);
  } else {
    return invalidInput<pistoris::resource_io::LoadedModel>(
        resource_path, input.native_path, input.mount, "unsupported Model input format");
  }

  pistoris::paths::ModelPathView parsed;
  if (!input.logical_path.empty() && pistoris::paths::modelFromFtl(input.logical_path, parsed)) {
    auto identity = loaded.model.setResourcePath(input.logical_path);
    if (!identity)
      return conversionFailure<pistoris::resource_io::LoadedModel>(
          identity, resource_path, input.native_path, input.mount);
  }
  auto hydrated = loadTextures(loaded.model, mounts, input, textures, mask, flags);
  if (!hydrated) return std::move(hydrated).propagate<pistoris::resource_io::LoadedModel>();
  auto icon = hydrateModelInventoryIcon(loaded.model, mounts, input, mask, flags);
  if (!icon) return std::move(icon).propagate<pistoris::resource_io::LoadedModel>();
  auto animation_audio = loadModelAnimationSounds(loaded, mounts, input, sounds, mask, flags);
  if (!animation_audio) return std::move(animation_audio).propagate<pistoris::resource_io::LoadedModel>();
  return ResourceIoResult<pistoris::resource_io::LoadedModel>::success(std::move(loaded));
}

ResourceIoResult<pistoris::Level> importGlbLevelResource(std::span<const std::uint8_t> data,
                                                         const ResourceMounts& mounts, const InputContext& input,
                                                         ResourceMountMask mask,
                                                         pistoris::resource_io::ResourceIoFlags flags,
                                                         const pistoris::resource_io::LevelLoadOptions& options) {
  const std::string_view resource_path = input.logical_path;
  std::vector<std::string> textures;
  auto imported = pistoris::Level::importGlb(data, options.glb, nullptr, &textures);
  if (!imported) return conversionFailure<pistoris::Level>(imported, resource_path, input.native_path, input.mount);
  auto hydrated = loadTextures(*imported, mounts, input, textures, mask, flags);
  if (!hydrated) return std::move(hydrated).propagate<pistoris::Level>();
  auto images = hydrateLevelImages(*imported, mounts, input, std::nullopt, mask, flags);
  if (!images) return std::move(images).propagate<pistoris::Level>();
  return ResourceIoResult<pistoris::Level>::success(std::move(*imported));
}

}  // namespace

namespace pistoris::resource_io {

namespace {

ResourceOrigin resourceOrigin(const ResourceDocument& document) {
  return {.requested_path = document.requested_path,
          .logical_path = document.logical_path,
          .native_path = document.native_path,
          .classification = document.classification,
          .address = document.address,
          .layout = document.layout,
          .mount_mask = document.mount_mask,
          .mount_id = document.mount_id,
          .flags = document.flags,
          .selector_kind = document.selector_kind};
}

InputContext inputContext(const ResourceDocument& document) {
  return {.logical_path = document.logical_path,
          .native_path = document.native_path,
          .mount = document.mount_id,
          .layout = document.layout == ResourceLayout::kGame ? InputContext::Layout::kMountedGame
                                                             : InputContext::Layout::kLoose};
}

std::string relatedRequestKey(const InputContext& input, std::string_view path,
                              std::span<const std::string_view> extensions, ResourceMountMask mounts,
                              ResourceIoFlags flags) {
  std::string key = input.layout == InputContext::Layout::kMountedGame
                        ? "mounted:" + std::to_string(mounts) + ':' + std::to_string(flags)
                        : "loose:" + pathToUtf8(input.native_path.parent_path());
  key.push_back(':');
  key += asciiLower(normalizedPath(path));
  for (const std::string_view extension : extensions) {
    key.push_back('|');
    key += extension;
  }
  return key;
}

std::string nativeResourceKey(const std::filesystem::path& path) {
  std::string key = pathToUtf8(path.lexically_normal());
#ifdef _WIN32
  key = asciiLower(key);
#endif
  return key;
}

struct RelatedResourceRead {
  std::string logical_path;
  ResourceRead read;
};

ResourceIoResult<RelatedResourceRead> readRelatedResource(const ResourceMounts& mounts, const InputContext& input,
                                                          std::string_view requested,
                                                          std::span<const std::string_view> extensions,
                                                          ResourceMountMask mask, ResourceIoFlags flags) {
  const std::vector<std::string> candidates = fallbackPaths(requested, extensions);
  if (input.layout == InputContext::Layout::kMountedGame) {
    for (std::size_t index = 0; index < candidates.size(); ++index) {
      auto read = mounts.read(candidates[index], {.mount_mask = mask, .flags = flags});
      if (read) return ResourceIoResult<RelatedResourceRead>::success({candidates[index], std::move(*read)});
      if (read.code() != ARX_RESOURCE_IO_NOT_FOUND || index + 1U == candidates.size())
        return std::move(read).propagate<RelatedResourceRead>();
    }
  } else {
    const std::filesystem::path parent = input.native_path.parent_path();
    for (std::size_t index = 0; index < candidates.size(); ++index) {
      const std::filesystem::path reference = utf8Path(candidates[index]);
      auto read = readNativeFile(reference.is_absolute() ? reference : parent / reference, input.mount);
      if (read) return ResourceIoResult<RelatedResourceRead>::success({candidates[index], std::move(*read)});
      if (read.code() != ARX_RESOURCE_IO_NOT_FOUND || index + 1U == candidates.size())
        return std::move(read).propagate<RelatedResourceRead>();
    }
  }
  return loadFailure<RelatedResourceRead>(ARX_RESOURCE_IO_NOT_FOUND, requested, {}, mask);
}

}  // namespace

class NativeResourceSetBuilder {
 public:
  NativeResourceSetBuilder(const ResourceMounts& mounts, bool suppress_errors)
      : mounts_(mounts), suppress_errors_(suppress_errors) {}

  ResourceIoResult<void> add(const InputContext& input, ResourceMountMask mounts, ResourceIoFlags flags,
                             NativeResourceRole role, ArxResourceKind owner_kind, std::size_t owner_index,
                             std::uint64_t element, std::string_view authored_path,
                             std::span<const std::string_view> extensions, std::string_view language = {},
                             std::string_view fallback_path = {}, std::string_view lookup_path = {}) {
    NativeResourceReference reference;
    reference.role_ = role;
    reference.owner_kind_ = owner_kind;
    reference.owner_index_ = owner_index;
    reference.element_ = element;
    reference.language_ = std::string(language);
    reference.authored_path_ = std::string(authored_path);

    if (authored_path.empty()) {
      set_.references_.push_back(std::move(reference));
      return ResourceIoResult<void>::success();
    }

    const auto resolve = [&](std::string_view path) -> ResourceIoResult<CachedRead> {
      const std::string key = relatedRequestKey(input, path, extensions, mounts, flags);
      if (const auto found = reads_.find(key); found != reads_.end()) {
        if (found->second.status == ARX_OK) return ResourceIoResult<CachedRead>::success(found->second);
        return loadFailure<CachedRead>(found->second.status, path, {}, mounts);
      }
      auto read = readRelatedResource(mounts_, input, path, extensions, mounts, flags);
      if (!read) {
        reads_.emplace(key, CachedRead{read.code(), kNoNativeResource});
        return std::move(read).propagate<CachedRead>();
      }
      const std::size_t resource = addFile(std::move(*read));
      CachedRead cached{ARX_OK, resource};
      reads_.emplace(key, cached);
      return ResourceIoResult<CachedRead>::success(cached);
    };

    const std::string_view requested = lookup_path.empty() ? authored_path : lookup_path;
    auto resolved = resolve(requested);
    if (!resolved && resolved.code() == ARX_RESOURCE_IO_NOT_FOUND && !fallback_path.empty() &&
        asciiLower(normalizedPath(requested)) != asciiLower(normalizedPath(fallback_path))) {
      resolved = resolve(fallback_path);
    }
    if (resolved) {
      reference.status_ = ARX_OK;
      reference.resource_ = resolved->resource;
      set_.references_.push_back(std::move(reference));
      return ResourceIoResult<void>::success();
    }

    reference.status_ = resolved.code();
    set_.references_.push_back(std::move(reference));
    if (resolved.code() == ARX_RESOURCE_IO_NOT_FOUND) return ResourceIoResult<void>::success();
    if (!suppress_errors_) return std::move(resolved).propagate<void>();
    log(ARX_LOG_WARN,
        "Unable to read related resource '{}': {}",
        authored_path,
        resolved.error() ? describeError(*resolved.error()) : errorString(resolved.code()));
    return ResourceIoResult<void>::success();
  }

  [[nodiscard]] bool resolved(NativeResourceRole role, ArxResourceKind owner_kind, std::size_t owner_index,
                              std::uint64_t element) const noexcept {
    return std::ranges::any_of(set_.references_, [&](const NativeResourceReference& reference) {
      return reference.role() == role && reference.ownerKind() == owner_kind && reference.ownerIndex() == owner_index &&
             reference.element() == element && reference.status() == ARX_OK;
    });
  }

  [[nodiscard]] NativeResourceSet take() && { return std::move(set_); }

 private:
  struct CachedRead {
    ArxReturnCode status = ARX_OK;
    std::size_t resource = kNoNativeResource;
  };

  std::size_t addFile(RelatedResourceRead read) {
    std::string key = nativeResourceKey(read.read.native_path);
    if (const auto found = files_.find(key); found != files_.end()) return found->second;
    const std::size_t index = set_.files_.size();
    NativeResourceFile file;
    file.logical_path_ = std::move(read.logical_path);
    file.native_path_ = std::move(read.read.native_path);
    file.mount_id_ = read.read.mount_id;
    file.data_ = std::move(read.read.data);
    set_.files_.push_back(std::move(file));
    files_.emplace(std::move(key), index);
    return index;
  }

  const ResourceMounts& mounts_;
  bool suppress_errors_ = false;
  NativeResourceSet set_;
  std::unordered_map<std::string, CachedRead> reads_;
  std::unordered_map<std::string, std::size_t> files_;
};

namespace {

const NativeResourceFile* referencedFile(const NativeResourceSet& resources,
                                         const NativeResourceReference& reference) noexcept {
  if (reference.status() != ARX_OK || reference.resource() >= resources.files().size()) return nullptr;
  return &resources.files()[reference.resource()];
}

template <class T, class Result>
ResourceIoResult<T> bundleConversionFailure(const Result& result, const ResourceOrigin& source) {
  return conversionFailure<T>(result, source.logical_path, source.native_path, source.mount_id);
}

ResourceIoResult<Animation> materializeAnimation(const NativeAnimationMember& member,
                                                 const NativeResourceSet& resources, std::size_t owner_index) {
  auto imported = Animation::importNative(member.carrier(), nullptr, member.textMode());
  if (!imported) return bundleConversionFailure<Animation>(imported, member.source());
  paths::AnimationPathView parsed;
  if (paths::animationFromTea(member.source().logical_path, parsed)) {
    auto identity = imported->setResourcePath(member.source().logical_path);
    if (!identity) return bundleConversionFailure<Animation>(identity, member.source());
  }
  for (const NativeResourceReference& reference : resources.references()) {
    if (reference.ownerKind() != ARX_RESOURCE_KIND_ANIMATION || reference.ownerIndex() != owner_index ||
        reference.role() != NativeResourceRole::kSound)
      continue;
    const NativeResourceFile* file = referencedFile(resources, reference);
    if (!file) continue;
    const ArxReturnCode valid = binary::validateEncodedAudio(file->data());
    if (valid != ARX_OK)
      return loadFailure<Animation>(valid, reference.authoredPath(), file->nativePath(), file->mountId());
    const auto set = imported->setSoundData(static_cast<SoundIndex>(reference.element()),
                                            {file->data().data(), file->data().size()});
    if (!set) return conversionFailure<Animation>(set, reference.authoredPath(), file->nativePath(), file->mountId());
  }
  return ResourceIoResult<Animation>::success(std::move(*imported));
}

ResourceIoResult<LoadedModel> materializeModel(const ModelNativeBundle& bundle) {
  auto imported = Model::importNative(bundle.model().carrier(), nullptr, bundle.model().textMode());
  if (!imported) return bundleConversionFailure<LoadedModel>(imported, bundle.model().source());
  paths::ModelPathView parsed;
  if (paths::modelFromFtl(bundle.model().source().logical_path, parsed)) {
    auto identity = imported->setResourcePath(bundle.model().source().logical_path);
    if (!identity) return bundleConversionFailure<LoadedModel>(identity, bundle.model().source());
  }
  for (const NativeResourceReference& reference : bundle.resources().references()) {
    if (reference.ownerKind() != ARX_RESOURCE_KIND_MODEL || reference.ownerIndex() != 0) continue;
    const NativeResourceFile* file = referencedFile(bundle.resources(), reference);
    if (!file) continue;
    const ArxReturnCode valid = binary::validateEncodedImage(file->data());
    if (valid != ARX_OK)
      return loadFailure<LoadedModel>(valid, reference.authoredPath(), file->nativePath(), file->mountId());
    ModelResult<void> set = reference.role() == NativeResourceRole::kInventoryIcon
                                ? imported->setInventoryIcon({file->data().data(), file->data().size()})
                                : imported->setTextureImage(static_cast<TextureIndex>(reference.element()),
                                                            {file->data().data(), file->data().size()});
    if (!set) return conversionFailure<LoadedModel>(set, reference.authoredPath(), file->nativePath(), file->mountId());
  }
  LoadedModel result;
  result.model = std::move(*imported);
  result.animations.reserve(bundle.animations().size());
  for (std::size_t index = 0; index < bundle.animations().size(); ++index) {
    auto animation = materializeAnimation(bundle.animations()[index], bundle.resources(), index);
    if (!animation) return std::move(animation).propagate<LoadedModel>();
    result.animations.push_back(std::move(*animation));
  }
  return ResourceIoResult<LoadedModel>::success(std::move(result));
}

ResourceIoResult<Ambiance> materializeAmbiance(const AmbianceNativeBundle& bundle) {
  auto imported = Ambiance::importNative(bundle.ambiance().carrier(), nullptr, bundle.ambiance().textMode());
  if (!imported) return bundleConversionFailure<Ambiance>(imported, bundle.ambiance().source());
  paths::AmbiancePathView parsed;
  if (paths::ambianceFromAmb(bundle.ambiance().source().logical_path, parsed)) {
    auto identity = imported->setResourcePath(bundle.ambiance().source().logical_path);
    if (!identity) return bundleConversionFailure<Ambiance>(identity, bundle.ambiance().source());
  }
  for (const NativeResourceReference& reference : bundle.resources().references()) {
    if (reference.ownerKind() != ARX_RESOURCE_KIND_AMBIANCE || reference.role() != NativeResourceRole::kSound) continue;
    const NativeResourceFile* file = referencedFile(bundle.resources(), reference);
    if (!file) continue;
    const ArxReturnCode valid = binary::validateEncodedAudio(file->data());
    if (valid != ARX_OK)
      return loadFailure<Ambiance>(valid, reference.authoredPath(), file->nativePath(), file->mountId());
    const auto set = imported->setSoundData(static_cast<SoundIndex>(reference.element()),
                                            {file->data().data(), file->data().size()});
    if (!set) return conversionFailure<Ambiance>(set, reference.authoredPath(), file->nativePath(), file->mountId());
  }
  return ResourceIoResult<Ambiance>::success(std::move(*imported));
}

ResourceIoResult<Cinematic> materializeCinematic(const CinematicNativeBundle& bundle) {
  auto imported =
      Cinematic::importNative(bundle.cinematic().carrier(), nullptr, nullptr, bundle.cinematic().textMode());
  if (!imported) return bundleConversionFailure<Cinematic>(imported, bundle.cinematic().source());
  paths::CinematicPathView parsed;
  if (paths::cinematicFromCin(bundle.cinematic().source().logical_path, parsed)) {
    auto identity = imported->setResourcePath(bundle.cinematic().source().logical_path);
    if (!identity) return bundleConversionFailure<Cinematic>(identity, bundle.cinematic().source());
  }
  std::unordered_map<std::string, LanguageId> languages;
  for (const NativeResourceReference& reference : bundle.resources().references()) {
    if (reference.ownerKind() != ARX_RESOURCE_KIND_CINEMATIC) continue;
    const NativeResourceFile* file = referencedFile(bundle.resources(), reference);
    if (!file) continue;
    if (reference.role() == NativeResourceRole::kIllustration) {
      const ArxReturnCode valid = binary::validateEncodedImage(file->data());
      if (valid != ARX_OK)
        return loadFailure<Cinematic>(valid, reference.authoredPath(), file->nativePath(), file->mountId());
      const auto set = imported->setTextureImage(static_cast<TextureIndex>(reference.element()),
                                                 {file->data().data(), file->data().size()});
      if (!set) return conversionFailure<Cinematic>(set, reference.authoredPath(), file->nativePath(), file->mountId());
      continue;
    }
    if (reference.role() != NativeResourceRole::kSound) continue;
    const ArxReturnCode valid = binary::validateEncodedAudio(file->data());
    if (valid != ARX_OK)
      return loadFailure<Cinematic>(valid, reference.authoredPath(), file->nativePath(), file->mountId());
    LanguageId language = kSoundEffects;
    if (!reference.language().empty()) {
      const std::string name(reference.language());
      if (const auto found = languages.find(name); found != languages.end()) {
        language = found->second;
      } else {
        auto added = imported->addLanguage(name);
        if (!added)
          return conversionFailure<Cinematic>(added, reference.authoredPath(), file->nativePath(), file->mountId());
        language = *added;
        languages.emplace(name, language);
      }
    }
    const auto set = imported->setSoundData(
        static_cast<SoundHandle>(reference.element()), language, {file->data().data(), file->data().size()});
    if (!set) return conversionFailure<Cinematic>(set, reference.authoredPath(), file->nativePath(), file->mountId());
  }
  return ResourceIoResult<Cinematic>::success(std::move(*imported));
}

ResourceIoResult<std::optional<ArxVector2>> levelMinimapProjectionOffset(const LevelNativeBundle& bundle) {
  const NativeResourceReference* minimap = nullptr;
  for (const NativeResourceReference& reference : bundle.resources().references()) {
    if (reference.role() == NativeResourceRole::kMinimap) {
      minimap = &reference;
      break;
    }
  }
  if (!minimap) return ResourceIoResult<std::optional<ArxVector2>>::success(std::nullopt);
  const NativeResourceFile* minimap_file = referencedFile(bundle.resources(), *minimap);
  if (!minimap_file) return ResourceIoResult<std::optional<ArxVector2>>::success(std::nullopt);

  const ResourceLayout layout = bundle.geometry() ? bundle.geometry()->source().layout : ResourceLayout::kLoose;
  const std::optional<std::uint32_t> level_index = bundle.levelIndex();

  ArxVector2 offset{};
  for (const NativeResourceReference& reference : bundle.resources().references()) {
    if (reference.role() != NativeResourceRole::kMinimapOffsetMetadata) continue;
    const NativeResourceFile* file = referencedFile(bundle.resources(), reference);
    if (!file) continue;
    auto parsed = parseMinimapOffsetRecord(
        file->data(), reference.element(), reference.authoredPath(), file->nativePath(), file->mountId());
    if (!parsed) return std::move(parsed).propagate<std::optional<ArxVector2>>();
    offset = *parsed;
  }
  if (layout == ResourceLayout::kLoose) {
    const std::string stem = filenameStem(minimap_file->logicalPath());
    const std::size_t suffix = stem.find("[offset_");
    if (suffix != std::string::npos) (void)parseProjectionOffsetSuffix(stem.substr(suffix), offset);
  }
  auto resolved = resolveMinimapProjectionOffset(
      layout, level_index, offset, minimap->authoredPath(), minimap_file->nativePath(), minimap_file->mountId());
  if (!resolved) return std::move(resolved).propagate<std::optional<ArxVector2>>();
  return ResourceIoResult<std::optional<ArxVector2>>::success(*resolved);
}

ResourceIoResult<Level> materializeLevel(const LevelNativeBundle& bundle) {
  if (!bundle.geometry())
    return loadFailure<Level>(ARX_INVALID_STATE, {}, {}, 0, "Level native bundle has no geometry");
  const Llf* lighting = bundle.lighting() ? &bundle.lighting()->carrier() : nullptr;
  const Dlf* scene = bundle.scene() ? &bundle.scene()->carrier() : nullptr;
  auto imported =
      Level::importNative(bundle.geometry()->carrier(), lighting, scene, nullptr, bundle.geometry()->textMode());
  if (!imported) return bundleConversionFailure<Level>(imported, bundle.geometry()->source());
  if (bundle.levelIndex()) {
    const auto identity = imported->setResourcePath(paths::levelDlf(*bundle.levelIndex()));
    const ResourceOrigin& source = bundle.scene() ? bundle.scene()->source() : bundle.geometry()->source();
    if (!identity) return bundleConversionFailure<Level>(identity, source);
  }

  const ArxVector2 minimap_offset = bundle.minimapProjectionOffset().value_or(ArxVector2{});
  for (const NativeResourceReference& reference : bundle.resources().references()) {
    if (reference.ownerKind() != ARX_RESOURCE_KIND_LEVEL) continue;
    const NativeResourceFile* file = referencedFile(bundle.resources(), reference);
    if (!file) continue;
    if (reference.role() == NativeResourceRole::kTexture) {
      const ArxReturnCode valid = binary::validateEncodedImage(file->data());
      if (valid != ARX_OK)
        return loadFailure<Level>(valid, reference.authoredPath(), file->nativePath(), file->mountId());
      const auto set = imported->setTextureImage(static_cast<TextureIndex>(reference.element()),
                                                 {file->data().data(), file->data().size()});
      if (!set) return conversionFailure<Level>(set, reference.authoredPath(), file->nativePath(), file->mountId());
    } else if (reference.role() == NativeResourceRole::kMinimap) {
      const auto set = imported->setMinimapFromProjection({file->data().data(), file->data().size()}, minimap_offset);
      if (!set) return conversionFailure<Level>(set, reference.authoredPath(), file->nativePath(), file->mountId());
    } else if (reference.role() == NativeResourceRole::kLoadingScreen) {
      const auto set = imported->setLoadingScreen({file->data().data(), file->data().size()});
      if (!set) return conversionFailure<Level>(set, reference.authoredPath(), file->nativePath(), file->mountId());
    }
  }
  return ResourceIoResult<Level>::success(std::move(*imported));
}

}  // namespace

std::string describeError(const Error<ResourceIoLocation>& error) {
  std::string result;
  if (const auto& location = error.location()) {
    if (location->content_location) {
      std::visit(
          [&](const auto& content_location) {
            using Location = std::decay_t<decltype(content_location)>;
            auto inner = Result<void, Location>::failure(error.code(), content_location);
            result = pistoris::describeError(*inner.error());
          },
          *location->content_location);
    }
    if (result.empty()) result = errorString(error.code());
    if (!location->resource_path.empty()) {
      result += " for resource '";
      result += location->resource_path;
      result.push_back('\'');
    }
    if (!location->native_path.empty()) {
      result += " at native path '";
      result += pathToUtf8(location->native_path);
      result.push_back('\'');
    }
  }
  if (result.empty()) result = errorString(error.code());
  if (!error.detail().empty()) {
    result += ": ";
    result += error.detail();
  }
  return result;
}

ResourceIoResult<ResourceDocument> Resources::readDocument(const paths::ResourceSelector& resource,
                                                           const ResourceLookupOptions& lookup) const noexcept {
  return guardedLoad<ResourceDocument>([&] {
    const std::optional<std::string> path = selectorDocumentPath(resource);
    if (!path)
      return loadFailure<ResourceDocument>(ARX_INVALID_OPTIONS, {}, {}, lookup.mount_mask, "invalid resource selector");
    auto read = mounts_.read(*path, lookup);
    if (!read) return std::move(read).propagate<ResourceDocument>();
    return documentFromRead(
        std::move(*read), *path, *path, ResourceAddress::kLogical, resource.kind, lookup.mount_mask, lookup.flags);
  });
}

ResourceIoResult<ResourceDocument> Resources::readDocument(std::string_view logical_path,
                                                           const ResourceLookupOptions& lookup) const noexcept {
  return guardedLoad<ResourceDocument>([&] {
    auto read = mounts_.read(logical_path, lookup);
    if (!read) return std::move(read).propagate<ResourceDocument>();
    return documentFromRead(std::move(*read),
                            std::string(logical_path),
                            std::string(logical_path),
                            ResourceAddress::kLogical,
                            ARX_RESOURCE_KIND_NONE,
                            lookup.mount_mask,
                            lookup.flags);
  });
}

ResourceIoResult<ResourceDocument> Resources::readDocumentFile(const std::filesystem::path& path) const noexcept {
  return guardedLoad<ResourceDocument>([&] {
    auto read = readNativeFile(path, 0);
    if (!read) return std::move(read).propagate<ResourceDocument>();
    return documentFromRead(std::move(*read),
                            pathToUtf8(path),
                            {},
                            ResourceAddress::kNative,
                            ARX_RESOURCE_KIND_NONE,
                            0,
                            kResourceIoFlagNone);
  });
}

ResourceIoResult<ModelNativeBundle> Resources::loadModelNativeBundle(
    const ResourceDocument& model_document, std::span<const ResourceDocument> animation_documents,
    const NativeBundleLoadOptions& options) const noexcept {
  return guardedLoad<ModelNativeBundle>([&] {
    if (model_document.classification.payload != ResourcePayload::kModel)
      return invalidInput<ModelNativeBundle>(model_document.logical_path,
                                             model_document.native_path,
                                             model_document.mount_mask,
                                             "native Model bundle requires an FTL or FTL JSON document");

    ModelNativeBundle bundle;
    bundle.model_.source_ = resourceOrigin(model_document);
    bundle.model_.text_mode_ = options.native_text_mode;
    if (model_document.classification.format == ResourceFormat::kFtl) {
      auto decoded = readFtl(model_document.data);
      if (!decoded)
        return conversionFailure<ModelNativeBundle>(
            decoded, model_document.logical_path, model_document.native_path, model_document.mount_id);
      bundle.model_.carrier_ = std::move(*decoded);
    } else if (model_document.classification.format == ResourceFormat::kJson) {
      auto decoded = fromFtlJson(textView(model_document.data), options.native_text_mode);
      if (!decoded)
        return conversionFailure<ModelNativeBundle>(
            decoded, model_document.logical_path, model_document.native_path, model_document.mount_id);
      bundle.model_.carrier_ = std::move(*decoded);
    } else {
      return invalidInput<ModelNativeBundle>(model_document.logical_path,
                                             model_document.native_path,
                                             model_document.mount_mask,
                                             "unsupported native Model document format");
    }

    std::vector<std::string> texture_sources;
    auto imported_model = Model::importNative(bundle.model_.carrier_, &texture_sources, bundle.model_.text_mode_);
    if (!imported_model)
      return conversionFailure<ModelNativeBundle>(
          imported_model, model_document.logical_path, model_document.native_path, model_document.mount_id);

    NativeResourceSetBuilder resources(mounts_, options.suppress_related_resource_errors);
    const InputContext model_input = inputContext(model_document);
    const std::size_t texture_count = std::min(imported_model->textureCount(), texture_sources.size());
    for (std::size_t index = 0; index < texture_count; ++index) {
      const ArxTextureView texture = imported_model->textures()[index];
      const std::string_view fallback(texture.path.data, texture.path.size);
      auto added = resources.add(model_input,
                                 model_document.mount_mask,
                                 model_document.flags,
                                 NativeResourceRole::kTexture,
                                 ARX_RESOURCE_KIND_MODEL,
                                 0,
                                 index,
                                 texture_sources[index],
                                 kImageExtensions,
                                 {},
                                 fallback);
      if (!added) return std::move(added).propagate<ModelNativeBundle>();
    }

    std::string icon_path;
    if (model_document.layout == ResourceLayout::kGame) {
      std::string entity_class;
      if (modelEntityClassFromFtl(model_document.logical_path, entity_class))
        (void)paths::itemIconFromEntityClass(entity_class, icon_path);
    } else {
      icon_path = looseSidecarStem(model_input, "[icon]");
    }
    if (!icon_path.empty()) {
      auto added = resources.add(model_input,
                                 model_document.mount_mask,
                                 model_document.flags,
                                 NativeResourceRole::kInventoryIcon,
                                 ARX_RESOURCE_KIND_MODEL,
                                 0,
                                 0,
                                 icon_path,
                                 kImageExtensions);
      if (!added) return std::move(added).propagate<ModelNativeBundle>();
    }

    bundle.animations_.reserve(animation_documents.size());
    for (std::size_t animation_index = 0; animation_index < animation_documents.size(); ++animation_index) {
      const ResourceDocument& document = animation_documents[animation_index];
      if (document.classification.payload != ResourcePayload::kAnimation)
        return invalidInput<ModelNativeBundle>(document.logical_path,
                                               document.native_path,
                                               document.mount_mask,
                                               "Model native bundle animation must be TEA or TEA JSON");
      NativeAnimationMember animation;
      animation.source_ = resourceOrigin(document);
      animation.text_mode_ = options.native_text_mode;
      if (document.classification.format == ResourceFormat::kTea) {
        auto decoded = readTea(document.data);
        if (!decoded)
          return conversionFailure<ModelNativeBundle>(
              decoded, document.logical_path, document.native_path, document.mount_id);
        animation.carrier_ = std::move(*decoded);
      } else if (document.classification.format == ResourceFormat::kJson) {
        auto decoded = fromTeaJson(textView(document.data), options.native_text_mode);
        if (!decoded)
          return conversionFailure<ModelNativeBundle>(
              decoded, document.logical_path, document.native_path, document.mount_id);
        animation.carrier_ = std::move(*decoded);
      } else {
        return invalidInput<ModelNativeBundle>(document.logical_path,
                                               document.native_path,
                                               document.mount_mask,
                                               "unsupported native Animation document format");
      }
      std::vector<SoundSourceReference> sound_sources;
      auto imported = Animation::importNative(animation.carrier_, &sound_sources, animation.text_mode_);
      if (!imported)
        return conversionFailure<ModelNativeBundle>(
            imported, document.logical_path, document.native_path, document.mount_id);
      const InputContext animation_input = inputContext(document);
      for (const SoundSourceReference& source : sound_sources) {
        if (source.sound == kNoSound || source.path.empty()) continue;
        std::string logical = normalizedPath(source.path);
        if (document.layout == ResourceLayout::kGame && !logical.starts_with("sfx/")) logical.insert(0, "sfx/");
        std::string fallback;
        if (source.sound < imported->soundCount()) {
          const ArxSoundView sound = imported->sounds()[source.sound];
          fallback.assign(sound.path.data, sound.path.size);
        }
        auto added = resources.add(animation_input,
                                   document.mount_mask,
                                   document.flags,
                                   NativeResourceRole::kSound,
                                   ARX_RESOURCE_KIND_ANIMATION,
                                   animation_index,
                                   source.sound,
                                   source.path,
                                   kAudioExtensions,
                                   {},
                                   fallback,
                                   logical);
        if (!added) return std::move(added).propagate<ModelNativeBundle>();
      }
      bundle.animations_.push_back(std::move(animation));
    }
    bundle.resources_ = std::move(resources).take();
    return ResourceIoResult<ModelNativeBundle>::success(std::move(bundle));
  });
}

ResourceIoResult<AnimationNativeBundle> Resources::loadAnimationNativeBundle(
    const ResourceDocument& document, const NativeBundleLoadOptions& options) const noexcept {
  return guardedLoad<AnimationNativeBundle>([&] {
    if (document.classification.payload != ResourcePayload::kAnimation)
      return invalidInput<AnimationNativeBundle>(document.logical_path,
                                                 document.native_path,
                                                 document.mount_mask,
                                                 "native Animation bundle requires a TEA or TEA JSON document");
    AnimationNativeBundle bundle;
    bundle.animation_.source_ = resourceOrigin(document);
    bundle.animation_.text_mode_ = options.native_text_mode;
    if (document.classification.format == ResourceFormat::kTea) {
      auto decoded = readTea(document.data);
      if (!decoded)
        return conversionFailure<AnimationNativeBundle>(
            decoded, document.logical_path, document.native_path, document.mount_id);
      bundle.animation_.carrier_ = std::move(*decoded);
    } else if (document.classification.format == ResourceFormat::kJson) {
      auto decoded = fromTeaJson(textView(document.data), options.native_text_mode);
      if (!decoded)
        return conversionFailure<AnimationNativeBundle>(
            decoded, document.logical_path, document.native_path, document.mount_id);
      bundle.animation_.carrier_ = std::move(*decoded);
    } else {
      return invalidInput<AnimationNativeBundle>(document.logical_path,
                                                 document.native_path,
                                                 document.mount_mask,
                                                 "unsupported native Animation document format");
    }
    std::vector<SoundSourceReference> sound_sources;
    auto imported = Animation::importNative(bundle.animation_.carrier_, &sound_sources, bundle.animation_.text_mode_);
    if (!imported)
      return conversionFailure<AnimationNativeBundle>(
          imported, document.logical_path, document.native_path, document.mount_id);
    NativeResourceSetBuilder resources(mounts_, options.suppress_related_resource_errors);
    const InputContext input = inputContext(document);
    for (const SoundSourceReference& source : sound_sources) {
      if (source.sound == kNoSound || source.path.empty()) continue;
      std::string logical = normalizedPath(source.path);
      if (document.layout == ResourceLayout::kGame && !logical.starts_with("sfx/")) logical.insert(0, "sfx/");
      std::string fallback;
      if (source.sound < imported->soundCount()) {
        const ArxSoundView sound = imported->sounds()[source.sound];
        fallback.assign(sound.path.data, sound.path.size);
      }
      auto added = resources.add(input,
                                 document.mount_mask,
                                 document.flags,
                                 NativeResourceRole::kSound,
                                 ARX_RESOURCE_KIND_ANIMATION,
                                 0,
                                 source.sound,
                                 source.path,
                                 kAudioExtensions,
                                 {},
                                 fallback,
                                 logical);
      if (!added) return std::move(added).propagate<AnimationNativeBundle>();
    }
    bundle.resources_ = std::move(resources).take();
    return ResourceIoResult<AnimationNativeBundle>::success(std::move(bundle));
  });
}

ResourceIoResult<AmbianceNativeBundle> Resources::loadAmbianceNativeBundle(
    const ResourceDocument& document, const NativeBundleLoadOptions& options) const noexcept {
  return guardedLoad<AmbianceNativeBundle>([&] {
    if (document.classification.payload != ResourcePayload::kAmbiance)
      return invalidInput<AmbianceNativeBundle>(document.logical_path,
                                                document.native_path,
                                                document.mount_mask,
                                                "native Ambiance bundle requires an AMB or AMB JSON document");
    AmbianceNativeBundle bundle;
    bundle.ambiance_.source_ = resourceOrigin(document);
    bundle.ambiance_.text_mode_ = options.native_text_mode;
    if (document.classification.format == ResourceFormat::kAmb) {
      auto decoded = readAmb(document.data);
      if (!decoded)
        return conversionFailure<AmbianceNativeBundle>(
            decoded, document.logical_path, document.native_path, document.mount_id);
      bundle.ambiance_.carrier_ = std::move(*decoded);
    } else if (document.classification.format == ResourceFormat::kJson) {
      auto decoded = fromAmbJson(textView(document.data), options.native_text_mode);
      if (!decoded)
        return conversionFailure<AmbianceNativeBundle>(
            decoded, document.logical_path, document.native_path, document.mount_id);
      bundle.ambiance_.carrier_ = std::move(*decoded);
    } else {
      return invalidInput<AmbianceNativeBundle>(document.logical_path,
                                                document.native_path,
                                                document.mount_mask,
                                                "unsupported native Ambiance document format");
    }
    std::vector<SoundSourceReference> sound_sources;
    auto imported = Ambiance::importNative(bundle.ambiance_.carrier_, &sound_sources, bundle.ambiance_.text_mode_);
    if (!imported)
      return conversionFailure<AmbianceNativeBundle>(
          imported, document.logical_path, document.native_path, document.mount_id);
    NativeResourceSetBuilder resources(mounts_, options.suppress_related_resource_errors);
    const InputContext input = inputContext(document);
    for (const SoundSourceReference& source : sound_sources) {
      if (source.sound == kNoSound || source.path.empty()) continue;
      std::string logical = normalizedPath(source.path);
      if (document.layout == ResourceLayout::kGame && !logical.starts_with("sfx/")) logical.insert(0, "sfx/");
      std::string fallback;
      if (source.sound < imported->soundCount()) {
        const ArxSoundView sound = imported->sounds()[source.sound];
        fallback.assign(sound.path.data, sound.path.size);
      }
      auto added = resources.add(input,
                                 document.mount_mask,
                                 document.flags,
                                 NativeResourceRole::kSound,
                                 ARX_RESOURCE_KIND_AMBIANCE,
                                 0,
                                 source.sound,
                                 source.path,
                                 kAudioExtensions,
                                 {},
                                 fallback,
                                 logical);
      if (!added) return std::move(added).propagate<AmbianceNativeBundle>();
    }
    bundle.resources_ = std::move(resources).take();
    return ResourceIoResult<AmbianceNativeBundle>::success(std::move(bundle));
  });
}

ResourceIoResult<LevelNativeBundle> Resources::loadLevelNativeBundle(
    const ResourceDocument& primary, const ResourceDocument* lighting, const ResourceDocument* scene,
    const NativeBundleLoadOptions& options) const noexcept {
  return guardedLoad<LevelNativeBundle>([&] {
    LevelNativeBundle bundle;
    std::optional<std::uint32_t> level_index;
    const auto assign_level_index = [&](std::optional<std::uint32_t> candidate) {
      if (!level_index && candidate) {
        level_index = candidate;
        bundle.level_index_ = candidate;
      }
    };
    const auto dlf_level_index = [&](const ResourceDocument& document) -> std::optional<std::uint32_t> {
      std::uint32_t parsed = 0;
      if (!document.logical_path.empty() && paths::levelFromDlf(document.logical_path, parsed)) return parsed;
      std::string scene_path;
      if (!nativeText(fixedText(bundle.scene_.carrier_.scene_path, sizeof(bundle.scene_.carrier_.scene_path)),
                      scene_path))
        return std::nullopt;
      std::string fts_path;
      if (paths::ftsFromDlfScene(scene_path, fts_path) && paths::levelFromFts(fts_path, parsed)) return parsed;
      return std::nullopt;
    };
    const auto decode = [&](const ResourceDocument& document, bool primary_document) -> ResourceIoResult<void> {
      const NativeTextMode text_mode = options.native_text_mode;
      switch (document.classification.payload) {
        case ResourcePayload::kLevelGeometry: {
          if (bundle.geometry_present_)
            return invalidInput<void>(document.logical_path,
                                      document.native_path,
                                      document.mount_mask,
                                      "Level native bundle has more than one geometry document");
          bundle.geometry_.source_ = resourceOrigin(document);
          bundle.geometry_.text_mode_ = text_mode;
          if (document.classification.format == ResourceFormat::kFts) {
            auto decoded = readFts(document.data);
            if (!decoded)
              return conversionFailure<void>(decoded, document.logical_path, document.native_path, document.mount_id);
            bundle.geometry_.carrier_ = std::move(*decoded);
          } else if (document.classification.format == ResourceFormat::kJson) {
            auto decoded = fromFtsJson(textView(document.data), options.native_text_mode);
            if (!decoded)
              return conversionFailure<void>(decoded, document.logical_path, document.native_path, document.mount_id);
            if (primary_document) assign_level_index(decoded->level);
            bundle.geometry_.carrier_ = std::move(decoded->fts);
          } else {
            return invalidInput<void>(document.logical_path,
                                      document.native_path,
                                      document.mount_mask,
                                      "unsupported Level geometry document format");
          }
          bundle.geometry_present_ = true;
          if (primary_document && !level_index && !document.logical_path.empty()) {
            std::uint32_t parsed = 0;
            if (paths::levelFromFts(document.logical_path, parsed)) assign_level_index(parsed);
          }
          return ResourceIoResult<void>::success();
        }
        case ResourcePayload::kLevelLighting: {
          if (bundle.lighting_present_)
            return invalidInput<void>(document.logical_path,
                                      document.native_path,
                                      document.mount_mask,
                                      "Level native bundle has more than one lighting document");
          bundle.lighting_.source_ = resourceOrigin(document);
          bundle.lighting_.text_mode_ = NativeTextMode::kUtf8;
          if (document.classification.format == ResourceFormat::kLlf) {
            auto decoded = readLlf(document.data);
            if (!decoded)
              return conversionFailure<void>(decoded, document.logical_path, document.native_path, document.mount_id);
            bundle.lighting_.carrier_ = std::move(*decoded);
          } else if (document.classification.format == ResourceFormat::kJson) {
            auto decoded = fromLlfJson(textView(document.data));
            if (!decoded)
              return conversionFailure<void>(decoded, document.logical_path, document.native_path, document.mount_id);
            bundle.lighting_.carrier_ = std::move(*decoded);
          } else {
            return invalidInput<void>(document.logical_path,
                                      document.native_path,
                                      document.mount_mask,
                                      "unsupported Level lighting document format");
          }
          bundle.lighting_present_ = true;
          return ResourceIoResult<void>::success();
        }
        case ResourcePayload::kLevelScene: {
          if (bundle.scene_present_)
            return invalidInput<void>(document.logical_path,
                                      document.native_path,
                                      document.mount_mask,
                                      "Level native bundle has more than one scene document");
          bundle.scene_.source_ = resourceOrigin(document);
          bundle.scene_.text_mode_ = text_mode;
          if (document.classification.format == ResourceFormat::kDlf) {
            auto decoded = readDlf(document.data);
            if (!decoded)
              return conversionFailure<void>(decoded, document.logical_path, document.native_path, document.mount_id);
            bundle.scene_.carrier_ = std::move(decoded->dlf);
            if (!bundle.lighting_present_ && decoded->embedded_lighting) {
              bundle.lighting_.carrier_ = std::move(*decoded->embedded_lighting);
              bundle.lighting_.source_ = resourceOrigin(document);
              bundle.lighting_.text_mode_ = NativeTextMode::kUtf8;
              bundle.lighting_present_ = true;
            }
          } else if (document.classification.format == ResourceFormat::kJson) {
            auto decoded = fromDlfJson(textView(document.data), options.native_text_mode);
            if (!decoded)
              return conversionFailure<void>(decoded, document.logical_path, document.native_path, document.mount_id);
            bundle.scene_.carrier_ = std::move(*decoded);
          } else {
            return invalidInput<void>(document.logical_path,
                                      document.native_path,
                                      document.mount_mask,
                                      "unsupported Level scene document format");
          }
          bundle.scene_present_ = true;
          assign_level_index(dlf_level_index(document));
          return ResourceIoResult<void>::success();
        }
        default:
          return invalidInput<void>(document.logical_path,
                                    document.native_path,
                                    document.mount_mask,
                                    "document is not a native Level member");
      }
    };

    auto decoded = decode(primary, true);
    if (!decoded) return std::move(decoded).propagate<LevelNativeBundle>();
    if (lighting) {
      if (lighting->classification.payload != ResourcePayload::kLevelLighting)
        return invalidInput<LevelNativeBundle>(lighting->logical_path,
                                               lighting->native_path,
                                               lighting->mount_mask,
                                               "explicit lighting document is not LLF or LLF JSON");
      if (bundle.lighting_present_ && bundle.scene_present_) bundle.lighting_present_ = false;
      decoded = decode(*lighting, false);
      if (!decoded) return std::move(decoded).propagate<LevelNativeBundle>();
    }
    if (scene) {
      if (scene->classification.payload != ResourcePayload::kLevelScene)
        return invalidInput<LevelNativeBundle>(scene->logical_path,
                                               scene->native_path,
                                               scene->mount_mask,
                                               "explicit scene document is not DLF or DLF JSON");
      decoded = decode(*scene, false);
      if (!decoded) return std::move(decoded).propagate<LevelNativeBundle>();
    }

    std::optional<ResourceDocument> discovered_geometry;
    std::optional<ResourceDocument> discovered_lighting;
    std::optional<ResourceDocument> discovered_scene;
    if (bundle.scene_present_ && !bundle.geometry_present_ && bundle.scene_.source_.layout == ResourceLayout::kGame) {
      std::string scene_path;
      if (!nativeText(fixedText(bundle.scene_.carrier_.scene_path, sizeof(bundle.scene_.carrier_.scene_path)),
                      scene_path))
        return loadFailure<LevelNativeBundle>(ARX_TEXT_INVALID_UTF8,
                                              bundle.scene_.source_.logical_path,
                                              bundle.scene_.source_.native_path,
                                              bundle.scene_.source_.mount_mask,
                                              "DLF scene path cannot be decoded");
      std::string fts_path;
      if (!paths::ftsFromDlfScene(scene_path, fts_path))
        return loadFailure<LevelNativeBundle>(ARX_DLF_BAD_SCENE_PATH,
                                              bundle.scene_.source_.logical_path,
                                              bundle.scene_.source_.native_path,
                                              bundle.scene_.source_.mount_mask,
                                              "DLF scene path cannot resolve an FTS");
      auto document = readDocument(
          fts_path, {.mount_mask = bundle.scene_.source_.mount_mask, .flags = bundle.scene_.source_.flags});
      if (!document) return std::move(document).propagate<LevelNativeBundle>();
      document->layout = ResourceLayout::kGame;
      discovered_geometry = std::move(*document);
      decoded = decode(*discovered_geometry, false);
      if (!decoded) return std::move(decoded).propagate<LevelNativeBundle>();
    }
    if (bundle.scene_present_ && !lighting && bundle.scene_.source_.layout == ResourceLayout::kGame) {
      const std::string llf_path = replaceExtension(bundle.scene_.source_.logical_path, ".llf");
      auto document = readDocument(
          llf_path, {.mount_mask = bundle.scene_.source_.mount_mask, .flags = bundle.scene_.source_.flags});
      if (document) {
        document->layout = ResourceLayout::kGame;
        if (bundle.lighting_present_) bundle.lighting_present_ = false;
        discovered_lighting = std::move(*document);
        decoded = decode(*discovered_lighting, false);
        if (!decoded) return std::move(decoded).propagate<LevelNativeBundle>();
      } else if (document.code() != ARX_RESOURCE_IO_NOT_FOUND) {
        return std::move(document).propagate<LevelNativeBundle>();
      }
    }
    if (bundle.geometry_present_ && bundle.geometry_.source_.layout == ResourceLayout::kLoose) {
      if (!lighting) {
        auto companion = levelCompanionPath(bundle.geometry_.source_.native_path, std::nullopt, ".llf", true);
        if (!companion) return std::move(companion).propagate<LevelNativeBundle>();
        if (*companion) {
          auto document = readDocumentFile(**companion);
          if (!document) return std::move(document).propagate<LevelNativeBundle>();
          if (bundle.lighting_present_) bundle.lighting_present_ = false;
          discovered_lighting = std::move(*document);
          decoded = decode(*discovered_lighting, false);
          if (!decoded) return std::move(decoded).propagate<LevelNativeBundle>();
        }
      }
      if (!bundle.scene_present_ && !scene) {
        auto companion = levelCompanionPath(bundle.geometry_.source_.native_path, std::nullopt, ".dlf", true);
        if (!companion) return std::move(companion).propagate<LevelNativeBundle>();
        if (*companion) {
          auto document = readDocumentFile(**companion);
          if (!document) return std::move(document).propagate<LevelNativeBundle>();
          discovered_scene = std::move(*document);
          decoded = decode(*discovered_scene, false);
          if (!decoded) return std::move(decoded).propagate<LevelNativeBundle>();
        }
      }
    }

    NativeResourceSetBuilder resources(mounts_, options.suppress_related_resource_errors);
    if (bundle.geometry_present_) {
      const ResourceOrigin& origin = bundle.geometry_.source_;
      const ResourceDocument* geometry_document = nullptr;
      if (primary.classification.payload == ResourcePayload::kLevelGeometry) geometry_document = &primary;
      if (lighting && lighting->classification.payload == ResourcePayload::kLevelGeometry) geometry_document = lighting;
      if (scene && scene->classification.payload == ResourcePayload::kLevelGeometry) geometry_document = scene;
      if (discovered_geometry) geometry_document = &*discovered_geometry;
      if (!geometry_document)
        return loadFailure<LevelNativeBundle>(ARX_INTERNAL_ERROR,
                                              origin.logical_path,
                                              origin.native_path,
                                              origin.mount_mask,
                                              "Level geometry source was lost");
      const InputContext input = inputContext(*geometry_document);
      std::vector<std::string> texture_sources;
      const Llf* native_lighting = bundle.lighting_present_ ? &bundle.lighting_.carrier_ : nullptr;
      const Dlf* native_scene = bundle.scene_present_ ? &bundle.scene_.carrier_ : nullptr;
      auto imported = Level::importNative(
          bundle.geometry_.carrier_, native_lighting, native_scene, &texture_sources, bundle.geometry_.text_mode_);
      if (!imported)
        return conversionFailure<LevelNativeBundle>(imported, origin.logical_path, origin.native_path, origin.mount_id);
      const std::size_t texture_count = std::min(imported->textureCount(), texture_sources.size());
      for (std::size_t index = 0; index < texture_count; ++index) {
        const ArxTextureView texture = imported->textures()[index];
        const std::string_view fallback(texture.path.data, texture.path.size);
        auto added = resources.add(input,
                                   origin.mount_mask,
                                   origin.flags,
                                   NativeResourceRole::kTexture,
                                   ARX_RESOURCE_KIND_LEVEL,
                                   0,
                                   index,
                                   texture_sources[index],
                                   kImageExtensions,
                                   {},
                                   fallback);
        if (!added) return std::move(added).propagate<LevelNativeBundle>();
      }

      std::string minimap_path;
      std::string loading_path;
      if (origin.layout == ResourceLayout::kGame) {
        if (!level_index) {
          std::uint32_t parsed = 0;
          if (paths::levelFromFts(origin.logical_path, parsed)) {
            level_index = parsed;
            bundle.level_index_ = parsed;
          }
        }
        if (level_index) {
          minimap_path = paths::levelMinimap(*level_index);
          loading_path = paths::levelLoadingScreen(*level_index);
        }
      } else {
        minimap_path = looseSidecarStem(input, "[map]");
        loading_path = looseSidecarStem(input, "[loading]");
        auto entries = listRelatedDirectory(mounts_, input, {}, origin.mount_mask, origin.flags);
        if (!entries) return std::move(entries).propagate<LevelNativeBundle>();
        std::vector<LooseMinimapCandidate> candidates;
        for (const RelatedDirectoryEntry& entry : *entries) {
          if (entry.directory) continue;
          if (auto candidate = looseMinimapCandidate(entry.name, minimap_path))
            candidates.push_back(std::move(*candidate));
        }
        std::ranges::sort(candidates, [](const LooseMinimapCandidate& left, const LooseMinimapCandidate& right) {
          if (left.extension_priority != right.extension_priority)
            return left.extension_priority < right.extension_priority;
          return asciiLower(left.filename) < asciiLower(right.filename);
        });
        if (!candidates.empty()) minimap_path = candidates.front().filename;
      }
      if (!minimap_path.empty()) {
        auto added = resources.add(input,
                                   origin.mount_mask,
                                   origin.flags,
                                   NativeResourceRole::kMinimap,
                                   ARX_RESOURCE_KIND_LEVEL,
                                   0,
                                   0,
                                   minimap_path,
                                   kImageExtensions);
        if (!added) return std::move(added).propagate<LevelNativeBundle>();
      }
      const bool minimap_resolved = resources.resolved(NativeResourceRole::kMinimap, ARX_RESOURCE_KIND_LEVEL, 0, 0);
      if (origin.layout == ResourceLayout::kGame && level_index && *level_index < 29U && minimap_resolved) {
        auto added = resources.add(input,
                                   origin.mount_mask,
                                   origin.flags,
                                   NativeResourceRole::kMinimapOffsetMetadata,
                                   ARX_RESOURCE_KIND_LEVEL,
                                   0,
                                   *level_index,
                                   paths::minimapOffsetsFile(),
                                   {});
        if (!added) return std::move(added).propagate<LevelNativeBundle>();
      }
      if (!loading_path.empty()) {
        auto added = resources.add(input,
                                   origin.mount_mask,
                                   origin.flags,
                                   NativeResourceRole::kLoadingScreen,
                                   ARX_RESOURCE_KIND_LEVEL,
                                   0,
                                   0,
                                   loading_path,
                                   kImageExtensions);
        if (!added) return std::move(added).propagate<LevelNativeBundle>();
      }
    }
    bundle.resources_ = std::move(resources).take();
    auto minimap_offset = levelMinimapProjectionOffset(bundle);
    if (!minimap_offset) return std::move(minimap_offset).propagate<LevelNativeBundle>();
    bundle.minimap_projection_offset_ = *minimap_offset;
    return ResourceIoResult<LevelNativeBundle>::success(std::move(bundle));
  });
}

ResourceIoResult<CinematicNativeBundle> Resources::loadCinematicNativeBundle(
    const ResourceDocument& document, const NativeBundleLoadOptions& options) const noexcept {
  return guardedLoad<CinematicNativeBundle>([&] {
    if (document.classification.payload != ResourcePayload::kCinematic ||
        document.classification.format != ResourceFormat::kCin)
      return invalidInput<CinematicNativeBundle>(document.logical_path,
                                                 document.native_path,
                                                 document.mount_mask,
                                                 "native Cinematic bundle requires a CIN document");
    CinematicNativeBundle bundle;
    bundle.cinematic_.source_ = resourceOrigin(document);
    bundle.cinematic_.text_mode_ = options.native_text_mode;
    auto decoded = readCin(document.data);
    if (!decoded)
      return conversionFailure<CinematicNativeBundle>(
          decoded, document.logical_path, document.native_path, document.mount_id);
    bundle.cinematic_.carrier_ = std::move(*decoded);

    std::vector<std::string> illustration_sources;
    std::vector<CinematicSoundSourceReference> sound_sources;
    auto imported = Cinematic::importNative(
        bundle.cinematic_.carrier_, &illustration_sources, &sound_sources, bundle.cinematic_.text_mode_);
    if (!imported)
      return conversionFailure<CinematicNativeBundle>(
          imported, document.logical_path, document.native_path, document.mount_id);

    NativeResourceSetBuilder resources(mounts_, options.suppress_related_resource_errors);
    const InputContext input = inputContext(document);
    const std::size_t illustration_count = std::min(imported->textureCount(), illustration_sources.size());
    for (std::size_t index = 0; index < illustration_count; ++index) {
      std::string logical = normalizedPath(illustration_sources[index]);
      if (document.layout == ResourceLayout::kGame && logical.find('/') == std::string::npos) {
        logical.insert(0, 1, '/');
        logical.insert(0, paths::cinematicIllustrationDirectory());
      }
      const ArxTextureView texture = imported->textures()[index];
      const std::string_view fallback(texture.path.data, texture.path.size);
      auto added = resources.add(input,
                                 document.mount_mask,
                                 document.flags,
                                 NativeResourceRole::kIllustration,
                                 ARX_RESOURCE_KIND_CINEMATIC,
                                 0,
                                 index,
                                 logical,
                                 kImageExtensions,
                                 {},
                                 fallback);
      if (!added) return std::move(added).propagate<CinematicNativeBundle>();
    }

    auto languages = listRelatedDirectory(mounts_, input, "speech", document.mount_mask, document.flags);
    if (!languages) {
      if (!options.suppress_related_resource_errors) return std::move(languages).propagate<CinematicNativeBundle>();
      log(ARX_LOG_WARN,
          "Unable to enumerate Cinematic speech languages for '{}': {}",
          document.requested_path,
          languages.error() ? describeError(*languages.error()) : errorString(languages.code()));
      languages = ResourceIoResult<std::vector<RelatedDirectoryEntry>>::success({});
    }
    for (const CinematicSoundSourceReference& source : sound_sources) {
      SoundKind kind = SoundKind::kEffect;
      if (soundHandleKind(source.sound, kind) != ARX_OK) continue;
      if (kind == SoundKind::kEffect) {
        std::string logical = speechPath(source.path);
        if (!logical.starts_with("sfx/")) logical.insert(0, "sfx/");
        auto added = resources.add(input,
                                   document.mount_mask,
                                   document.flags,
                                   NativeResourceRole::kSound,
                                   ARX_RESOURCE_KIND_CINEMATIC,
                                   0,
                                   source.sound,
                                   source.path,
                                   kAudioExtensions,
                                   {},
                                   {},
                                   logical);
        if (!added) return std::move(added).propagate<CinematicNativeBundle>();
        continue;
      }
      const std::string speech = speechPath(source.path);
      for (const RelatedDirectoryEntry& language : *languages) {
        if (!language.directory) continue;
        const std::string logical = appendLogicalPath(appendLogicalPath("speech", language.name), speech);
        auto added = resources.add(input,
                                   document.mount_mask,
                                   document.flags,
                                   NativeResourceRole::kSound,
                                   ARX_RESOURCE_KIND_CINEMATIC,
                                   0,
                                   source.sound,
                                   source.path,
                                   kAudioExtensions,
                                   asciiLower(language.name),
                                   {},
                                   logical);
        if (!added) return std::move(added).propagate<CinematicNativeBundle>();
      }
    }
    bundle.resources_ = std::move(resources).take();
    return ResourceIoResult<CinematicNativeBundle>::success(std::move(bundle));
  });
}

ResourceIoResult<LoadedModel> Resources::loadModel(const paths::ResourceSelector& resource,
                                                   const ModelLoadOptions& options,
                                                   const ResourceLookupOptions& lookup) const noexcept {
  const std::optional<std::string> path = modelPath(resource);
  if (!path) return loadFailure<LoadedModel>(ARX_INVALID_OPTIONS, {}, {}, lookup.mount_mask, "selector is not a Model");
  return loadModel(*path, options, lookup);
}

ResourceIoResult<LoadedModel> Resources::loadModel(paths::ModelPathView resource, const ModelLoadOptions& options,
                                                   const ResourceLookupOptions& lookup) const noexcept {
  std::string path;
  if (!paths::modelFtl(resource, path))
    return loadFailure<LoadedModel>(ARX_INVALID_OPTIONS, {}, {}, lookup.mount_mask, "invalid Model path");
  return loadModel(path, options, lookup);
}

ResourceIoResult<LoadedModel> Resources::loadModel(const ResourceDocument& document,
                                                   const ModelLoadOptions& options) const noexcept {
  return guardedLoad<LoadedModel>([&] {
    if (document.classification.payload == ResourcePayload::kModel) {
      auto bundle = loadModelNativeBundle(document, {}, {options.native_text_mode, false});
      if (!bundle) return std::move(bundle).propagate<LoadedModel>();
      return materializeModel(*bundle);
    }
    return importModelResource(document.data,
                               document.classification.format,
                               mounts_,
                               inputContext(document),
                               document.mount_mask,
                               document.flags,
                               options);
  });
}

ResourceIoResult<LoadedModel> Resources::loadModel(std::string_view logical_path, const ModelLoadOptions& options,
                                                   const ResourceLookupOptions& lookup) const noexcept {
  return guardedLoad<LoadedModel>([&] {
    const std::string path = withDefaultExtension(logical_path, ".ftl");
    auto document = readDocument(path, lookup);
    if (!document) return std::move(document).propagate<LoadedModel>();
    return loadModel(*document, options);
  });
}

ResourceIoResult<LoadedModel> Resources::loadModelFile(const std::filesystem::path& path,
                                                       const ModelLoadOptions& options) const noexcept {
  return guardedLoad<LoadedModel>([&] {
    auto document = readDocumentFile(path);
    if (!document) return std::move(document).propagate<LoadedModel>();
    return loadModel(*document, options);
  });
}

ResourceIoResult<Animation> Resources::loadAnimation(const paths::ResourceSelector& resource,
                                                     const AnimationLoadOptions& options,
                                                     const ResourceLookupOptions& lookup) const noexcept {
  const std::optional<std::string> path = animationPath(resource);
  if (!path)
    return loadFailure<Animation>(ARX_INVALID_OPTIONS, {}, {}, lookup.mount_mask, "selector is not an Animation");
  return loadAnimation(*path, options, lookup);
}

ResourceIoResult<Animation> Resources::loadAnimation(paths::AnimationPathView resource,
                                                     const AnimationLoadOptions& options,
                                                     const ResourceLookupOptions& lookup) const noexcept {
  std::string path;
  if (!paths::animationTea(resource, path))
    return loadFailure<Animation>(ARX_INVALID_OPTIONS, {}, {}, lookup.mount_mask, "invalid Animation path");
  return loadAnimation(path, options, lookup);
}

ResourceIoResult<Animation> Resources::loadAnimation(const ResourceDocument& document,
                                                     const AnimationLoadOptions& options) const noexcept {
  return guardedLoad<Animation>([&] {
    auto bundle = loadAnimationNativeBundle(document, {options.native_text_mode, false});
    if (!bundle) return std::move(bundle).propagate<Animation>();
    return materializeAnimation(bundle->animation(), bundle->resources(), 0);
  });
}

ResourceIoResult<Animation> Resources::loadAnimation(std::string_view logical_path, const AnimationLoadOptions& options,
                                                     const ResourceLookupOptions& lookup) const noexcept {
  return guardedLoad<Animation>([&] {
    const std::string path = withDefaultExtension(logical_path, ".tea");
    auto document = readDocument(path, lookup);
    if (!document) return std::move(document).propagate<Animation>();
    return loadAnimation(*document, options);
  });
}

ResourceIoResult<Animation> Resources::loadAnimationFile(const std::filesystem::path& path,
                                                         const AnimationLoadOptions& options) const noexcept {
  return guardedLoad<Animation>([&] {
    auto document = readDocumentFile(path);
    if (!document) return std::move(document).propagate<Animation>();
    return loadAnimation(*document, options);
  });
}

ResourceIoResult<Level> Resources::loadLevel(std::uint32_t level, const LevelLoadOptions& options,
                                             const ResourceLookupOptions& lookup) const noexcept {
  return loadLevel(paths::levelDlf(level), options, lookup);
}

ResourceIoResult<Level> Resources::loadLevel(const paths::ResourceSelector& resource, const LevelLoadOptions& options,
                                             const ResourceLookupOptions& lookup) const noexcept {
  if (resource.kind != ARX_RESOURCE_KIND_LEVEL)
    return loadFailure<Level>(ARX_INVALID_OPTIONS, {}, {}, lookup.mount_mask, "selector is not a Level");
  return loadLevel(resource.level, options, lookup);
}

ResourceIoResult<Level> Resources::loadLevel(const ResourceDocument& primary, const ResourceDocument* lighting,
                                             const ResourceDocument* scene,
                                             const LevelLoadOptions& options) const noexcept {
  return guardedLoad<Level>([&] {
    if (primary.classification.format == ResourceFormat::kGlb) {
      if (lighting || scene)
        return invalidInput<Level>(primary.logical_path,
                                   primary.native_path,
                                   primary.mount_mask,
                                   "GLB Level input does not accept native companions");
      return importGlbLevelResource(
          primary.data, mounts_, inputContext(primary), primary.mount_mask, primary.flags, options);
    }
    if (primary.layout == ResourceLayout::kLoose && primary.classification.payload != ResourcePayload::kLevelGeometry)
      return invalidInput<Level>(primary.logical_path,
                                 primary.native_path,
                                 primary.mount_mask,
                                 "loose Level input must use FTS or FTS JSON as its primary document");
    auto bundle = loadLevelNativeBundle(primary, lighting, scene, {options.native_text_mode, false});
    if (!bundle) return std::move(bundle).propagate<Level>();
    return materializeLevel(*bundle);
  });
}

ResourceIoResult<Level> Resources::loadLevel(std::string_view logical_path, const LevelLoadOptions& options,
                                             const ResourceLookupOptions& lookup) const noexcept {
  return guardedLoad<Level>([&] {
    const std::string primary_path = withDefaultExtension(logical_path, ".dlf");
    auto document = readDocument(primary_path, lookup);
    if (!document) return std::move(document).propagate<Level>();
    return loadLevel(*document, nullptr, nullptr, options);
  });
}

ResourceIoResult<Level> Resources::loadLevelFile(const std::filesystem::path& fts_path,
                                                 const std::optional<std::filesystem::path>& llf_path,
                                                 const std::optional<std::filesystem::path>& dlf_path,
                                                 const LevelLoadOptions& options) const noexcept {
  return guardedLoad<Level>([&] {
    auto primary = readDocumentFile(fts_path);
    if (!primary) return std::move(primary).propagate<Level>();
    std::optional<ResourceDocument> lighting;
    std::optional<ResourceDocument> scene;
    if (llf_path) {
      auto document = readDocumentFile(*llf_path);
      if (!document) return std::move(document).propagate<Level>();
      lighting = std::move(*document);
    }
    if (dlf_path) {
      auto document = readDocumentFile(*dlf_path);
      if (!document) return std::move(document).propagate<Level>();
      scene = std::move(*document);
    }
    return loadLevel(*primary, lighting ? &*lighting : nullptr, scene ? &*scene : nullptr, options);
  });
}

ResourceIoResult<Ambiance> Resources::loadAmbiance(const paths::ResourceSelector& resource,
                                                   const AmbianceLoadOptions& options,
                                                   const ResourceLookupOptions& lookup) const noexcept {
  const std::optional<std::string> path = ambiancePath(resource);
  if (!path)
    return loadFailure<Ambiance>(ARX_INVALID_OPTIONS, {}, {}, lookup.mount_mask, "selector is not an Ambiance");
  return loadAmbiance(*path, options, lookup);
}

ResourceIoResult<Ambiance> Resources::loadAmbiance(paths::AmbiancePathView resource, const AmbianceLoadOptions& options,
                                                   const ResourceLookupOptions& lookup) const noexcept {
  std::string path;
  if (!paths::ambianceAmb(resource, path))
    return loadFailure<Ambiance>(ARX_INVALID_OPTIONS, {}, {}, lookup.mount_mask, "invalid Ambiance path");
  return loadAmbiance(path, options, lookup);
}

ResourceIoResult<Ambiance> Resources::loadAmbiance(const ResourceDocument& document,
                                                   const AmbianceLoadOptions& options) const noexcept {
  return guardedLoad<Ambiance>([&] {
    if (document.classification.payload == ResourcePayload::kAmbiance) {
      auto bundle = loadAmbianceNativeBundle(document, {options.native_text_mode, false});
      if (!bundle) return std::move(bundle).propagate<Ambiance>();
      return materializeAmbiance(*bundle);
    }
    if (document.classification.format != ResourceFormat::kGlb)
      return invalidInput<Ambiance>(
          document.logical_path, document.native_path, document.mount_mask, "unsupported Ambiance input format");
    std::vector<SoundSourceReference> sounds;
    auto result = Ambiance::importGlb(document.data, options.glb, &sounds);
    if (!result)
      return conversionFailure<Ambiance>(result, document.logical_path, document.native_path, document.mount_id);
    std::optional<Ambiance> imported = std::move(*result);
    paths::AmbiancePathView parsed;
    if (paths::ambianceFromAmb(document.logical_path, parsed)) {
      auto identity = imported->setResourcePath(document.logical_path);
      if (!identity)
        return conversionFailure<Ambiance>(identity, document.logical_path, document.native_path, document.mount_id);
    }
    const InputContext input = inputContext(document);
    auto hydrated = loadSounds(*imported, mounts_, input, sounds, document.mount_mask, document.flags);
    if (!hydrated) return std::move(hydrated).propagate<Ambiance>();
    return ResourceIoResult<Ambiance>::success(std::move(*imported));
  });
}

ResourceIoResult<Ambiance> Resources::loadAmbiance(std::string_view logical_path, const AmbianceLoadOptions& options,
                                                   const ResourceLookupOptions& lookup) const noexcept {
  return guardedLoad<Ambiance>([&] {
    const std::string path = withDefaultExtension(logical_path, ".amb");
    auto document = readDocument(path, lookup);
    if (!document) return std::move(document).propagate<Ambiance>();
    return loadAmbiance(*document, options);
  });
}

ResourceIoResult<Ambiance> Resources::loadAmbianceFile(const std::filesystem::path& path,
                                                       const AmbianceLoadOptions& options) const noexcept {
  return guardedLoad<Ambiance>([&] {
    auto document = readDocumentFile(path);
    if (!document) return std::move(document).propagate<Ambiance>();
    return loadAmbiance(*document, options);
  });
}

ResourceIoResult<Cinematic> Resources::loadCinematic(const paths::ResourceSelector& resource,
                                                     const CinematicLoadOptions& options,
                                                     const ResourceLookupOptions& lookup) const noexcept {
  const std::optional<std::string> path = cinematicPath(resource);
  if (!path)
    return loadFailure<Cinematic>(ARX_INVALID_OPTIONS, {}, {}, lookup.mount_mask, "selector is not a Cinematic");
  return loadCinematic(*path, options, lookup);
}

ResourceIoResult<Cinematic> Resources::loadCinematic(paths::CinematicPathView resource,
                                                     const CinematicLoadOptions& options,
                                                     const ResourceLookupOptions& lookup) const noexcept {
  std::string path;
  if (!paths::cinematicCin(resource, path))
    return loadFailure<Cinematic>(ARX_INVALID_OPTIONS, {}, {}, lookup.mount_mask, "invalid Cinematic path");
  return loadCinematic(path, options, lookup);
}

ResourceIoResult<Cinematic> Resources::loadCinematic(const ResourceDocument& document,
                                                     const CinematicLoadOptions& options) const noexcept {
  return guardedLoad<Cinematic>([&] {
    if (document.classification.payload == ResourcePayload::kCinematic) {
      auto bundle = loadCinematicNativeBundle(document, {options.native_text_mode, false});
      if (!bundle) return std::move(bundle).propagate<Cinematic>();
      return materializeCinematic(*bundle);
    }
    if (document.classification.format != ResourceFormat::kGlb)
      return invalidInput<Cinematic>(
          document.logical_path, document.native_path, document.mount_mask, "unsupported Cinematic input format");
    std::vector<std::string> illustrations;
    std::vector<CinematicSoundSourceReference> sounds;
    auto result = Cinematic::importGlb(document.data, &sounds);
    if (!result)
      return conversionFailure<Cinematic>(result, document.logical_path, document.native_path, document.mount_id);
    std::optional<Cinematic> imported = std::move(*result);
    paths::CinematicPathView parsed;
    if (paths::cinematicFromCin(document.logical_path, parsed)) {
      auto identity = imported->setResourcePath(document.logical_path);
      if (!identity)
        return conversionFailure<Cinematic>(identity, document.logical_path, document.native_path, document.mount_id);
    }
    const InputContext input = inputContext(document);
    auto hydrated = hydrateCinematic(
        *imported, mounts_, input, std::move(illustrations), sounds, true, document.mount_mask, document.flags);
    if (!hydrated) return std::move(hydrated).propagate<Cinematic>();
    return ResourceIoResult<Cinematic>::success(std::move(*imported));
  });
}

ResourceIoResult<Cinematic> Resources::loadCinematic(std::string_view logical_path, const CinematicLoadOptions& options,
                                                     const ResourceLookupOptions& lookup) const noexcept {
  return guardedLoad<Cinematic>([&] {
    const std::string path = withDefaultExtension(logical_path, ".cin");
    auto document = readDocument(path, lookup);
    if (!document) return std::move(document).propagate<Cinematic>();
    return loadCinematic(*document, options);
  });
}

ResourceIoResult<Cinematic> Resources::loadCinematicFile(const std::filesystem::path& path,
                                                         const CinematicLoadOptions& options) const noexcept {
  return guardedLoad<Cinematic>([&] {
    auto document = readDocumentFile(path);
    if (!document) return std::move(document).propagate<Cinematic>();
    return loadCinematic(*document, options);
  });
}

ResourceIoResult<ResourceOutputs> Resources::prepareModelOutputs(const Model& model, std::string_view logical_path,
                                                                 const ModelWriteOptions& options) const noexcept {
  return guardedWrite<ResourceOutputs>(options.resource, [&] {
    const std::string path = withDefaultExtension(logical_path, ".ftl");
    const std::string format = lowerExtension(path);
    if (options.glb && format != ".glb")
      return invalidOutput<ResourceOutputs>(path, {}, "GLB options do not apply to this Model output format");
    if (options.native_text_mode && format != ".ftl" && format != ".json")
      return invalidOutput<ResourceOutputs>(path, {}, "native text mode does not apply to this Model output format");
    if (options.compress && format != ".ftl")
      return invalidOutput<ResourceOutputs>(path, {}, "compression does not apply to this Model output format");
    std::vector<PendingOutput> outputs;
    if (format == ".ftl" || format == ".json") {
      auto bundle =
          model.bakeNativeBundle({.include_texture_files = includesOutput(options.resource, kResourceOutputTextures),
                                  .text_mode = options.native_text_mode.value_or(NativeTextMode::kAuto)});
      if (!bundle) return writeConversionFailure<ResourceOutputs>(bundle, path, {});
      if (format == ".ftl" && includesOutput(options.resource, kResourceOutputPrimary)) {
        auto encoded = writeFtl(bundle->ftl, options.compress.value_or(true));
        if (!encoded) return writeConversionFailure<ResourceOutputs>(encoded, path, {});
        outputs.push_back(mountedOutput(path, std::move(*encoded), ResourceOutputKind::kData, true));
      } else if (format == ".json" && includesOutput(options.resource, kResourceOutputPrimary)) {
        auto encoded = toFtlJson(bundle->ftl, true);
        if (!encoded) return writeConversionFailure<ResourceOutputs>(encoded, path, {});
        outputs.push_back(mountedOutput(path, bytes(*encoded), ResourceOutputKind::kData, true));
      }
      if (includesOutput(options.resource, kResourceOutputTextures)) {
        const std::string parent = parentLogicalPath(path);
        for (auto& file : bundle->texture_files) {
          const std::string texture_path = normalizedPath(file.resource_path);
          outputs.push_back(mountedOutput(format == ".ftl" ? texture_path : appendLogicalPath(parent, texture_path),
                                          std::move(file.encoded_image),
                                          ResourceOutputKind::kImage));
        }
      }
    } else if (format == ".obj") {
      const std::string parent = parentLogicalPath(path);
      const std::string stem = filenameStem(path);
      auto bundle = model.exportObj(stem, {.include_files = includesOutput(options.resource, kResourceOutputTextures)});
      if (!bundle) return writeConversionFailure<ResourceOutputs>(bundle, path, {});
      if (includesOutput(options.resource, kResourceOutputPrimary))
        outputs.push_back(mountedOutput(path, bytes(bundle->text), ResourceOutputKind::kData, true));
      if (includesOutput(options.resource, kResourceOutputCompanions) && !bundle->mtl.empty())
        outputs.push_back(mountedOutput(appendLogicalPath(parent, stem + ".mtl"), bytes(bundle->mtl)));
      if (includesOutput(options.resource, kResourceOutputTextures)) {
        for (auto& file : bundle->texture_files)
          outputs.push_back(mountedOutput(
              appendLogicalPath(parent, file.path), std::move(file.encoded_image), ResourceOutputKind::kImage));
      }
    } else if (format == ".glb") {
      auto encoded = model.exportGlb(options.glb.value_or(Model::GlbExportOptions{}));
      if (!encoded) return writeConversionFailure<ResourceOutputs>(encoded, path, {});
      if (includesOutput(options.resource, kResourceOutputPrimary))
        outputs.push_back(mountedOutput(path, std::move(*encoded), ResourceOutputKind::kData, true));
    } else {
      return invalidOutput<ResourceOutputs>(path, {}, "unsupported Model output format");
    }
    if (includesOutput(options.resource, kResourceOutputImages)) {
      auto icon = appendModelInventoryIconOutputs(model, path, {}, format == ".ftl", outputs);
      if (!icon) return std::move(icon).propagate<ResourceOutputs>();
    }
    const std::string identity = model.resourcePath().empty() ? path : std::string(model.resourcePath());
    return ResourceIoResult<ResourceOutputs>::success(
        preparedWrite(std::move(outputs), ARX_RESOURCE_KIND_MODEL, identity, options.resource.io_flags));
  });
}

ResourceIoResult<ResourceOutputs> Resources::prepareModelFileOutputs(const Model& model,
                                                                     const std::filesystem::path& path,
                                                                     const ModelWriteOptions& options) const noexcept {
  return guardedWrite<ResourceOutputs>(options.resource, [&] {
    const std::string format = asciiLower(pathToUtf8(path.extension()));
    if (options.glb && format != ".glb")
      return invalidOutput<ResourceOutputs>({}, path, "GLB options do not apply to this Model output format");
    if (options.native_text_mode && format != ".ftl" && format != ".json")
      return invalidOutput<ResourceOutputs>({}, path, "native text mode does not apply to this Model output format");
    if (options.compress && format != ".ftl")
      return invalidOutput<ResourceOutputs>({}, path, "compression does not apply to this Model output format");
    const std::filesystem::path parent = path.parent_path();
    std::vector<PendingOutput> outputs;
    if (format == ".ftl" || format == ".json") {
      auto bundle =
          model.bakeNativeBundle({.include_texture_files = includesOutput(options.resource, kResourceOutputTextures),
                                  .text_mode = options.native_text_mode.value_or(NativeTextMode::kAuto)});
      if (!bundle) return writeConversionFailure<ResourceOutputs>(bundle, {}, path);
      if (format == ".ftl" && includesOutput(options.resource, kResourceOutputPrimary)) {
        auto encoded = writeFtl(bundle->ftl, options.compress.value_or(true));
        if (!encoded) return writeConversionFailure<ResourceOutputs>(encoded, {}, path);
        outputs.push_back(nativeOutput(path, std::move(*encoded), ResourceOutputKind::kData, true));
      } else if (format == ".json" && includesOutput(options.resource, kResourceOutputPrimary)) {
        auto encoded = toFtlJson(bundle->ftl, true);
        if (!encoded) return writeConversionFailure<ResourceOutputs>(encoded, {}, path);
        outputs.push_back(nativeOutput(path, bytes(*encoded), ResourceOutputKind::kData, true));
      }
      if (includesOutput(options.resource, kResourceOutputTextures)) {
        for (auto& file : bundle->texture_files)
          outputs.push_back(nativeOutput(parent / utf8Path(normalizedPath(file.resource_path)),
                                         std::move(file.encoded_image),
                                         ResourceOutputKind::kImage));
      }
    } else if (format == ".obj") {
      const std::string stem = pathToUtf8(path.stem());
      auto bundle = model.exportObj(stem, {.include_files = includesOutput(options.resource, kResourceOutputTextures)});
      if (!bundle) return writeConversionFailure<ResourceOutputs>(bundle, {}, path);
      if (includesOutput(options.resource, kResourceOutputPrimary))
        outputs.push_back(nativeOutput(path, bytes(bundle->text), ResourceOutputKind::kData, true));
      if (includesOutput(options.resource, kResourceOutputCompanions) && !bundle->mtl.empty())
        outputs.push_back(
            nativeOutput(path.parent_path() / utf8Path(pathToUtf8(path.stem()) + ".mtl"), bytes(bundle->mtl)));
      if (includesOutput(options.resource, kResourceOutputTextures)) {
        for (auto& file : bundle->texture_files)
          outputs.push_back(nativeOutput(
              parent / utf8Path(normalizedPath(file.path)), std::move(file.encoded_image), ResourceOutputKind::kImage));
      }
    } else if (format == ".glb") {
      auto encoded = model.exportGlb(options.glb.value_or(Model::GlbExportOptions{}));
      if (!encoded) return writeConversionFailure<ResourceOutputs>(encoded, {}, path);
      if (includesOutput(options.resource, kResourceOutputPrimary))
        outputs.push_back(nativeOutput(path, std::move(*encoded), ResourceOutputKind::kData, true));
    } else {
      return invalidOutput<ResourceOutputs>({}, path, "unsupported Model output format");
    }
    if (includesOutput(options.resource, kResourceOutputImages)) {
      auto icon = appendModelInventoryIconOutputs(model, {}, path, false, outputs);
      if (!icon) return std::move(icon).propagate<ResourceOutputs>();
    }
    const std::string identity = model.resourcePath().empty() ? pathToUtf8(path) : std::string(model.resourcePath());
    return ResourceIoResult<ResourceOutputs>::success(
        preparedWrite(std::move(outputs), ARX_RESOURCE_KIND_MODEL, identity, options.resource.io_flags));
  });
}

ResourceIoResult<ResourceOutputs> Resources::prepareModelOutputs(const LoadedModel& loaded,
                                                                 std::string_view logical_path,
                                                                 const ModelWriteOptions& options) const noexcept {
  if (loaded.animations.empty()) return prepareModelOutputs(loaded.model, logical_path, options);
  return guardedWrite<ResourceOutputs>(options.resource, [&] {
    const std::string path = withDefaultExtension(logical_path, ".glb");
    if (lowerExtension(path) != ".glb")
      return invalidOutput<ResourceOutputs>(
          path, {}, "Model output with Animations must use GLB; write other formats separately");
    if (options.native_text_mode || options.compress)
      return invalidOutput<ResourceOutputs>(
          path, {}, "native Model options do not apply to Model-with-Animations GLB output");

    std::vector<const Animation*> animations;
    animations.reserve(loaded.animations.size());
    for (const Animation& animation : loaded.animations) animations.push_back(&animation);
    auto bundle = loaded.model.exportGlbBundle(animations, options.glb.value_or(Model::GlbExportOptions{}));
    if (!bundle) return writeConversionFailure<ResourceOutputs>(bundle, path, {});

    std::vector<PendingOutput> outputs;
    if (includesOutput(options.resource, kResourceOutputPrimary))
      outputs.push_back(mountedOutput(path, std::move(bundle->glb), ResourceOutputKind::kData, true));
    if (includesOutput(options.resource, kResourceOutputAudio)) {
      const std::string parent = parentLogicalPath(path);
      for (auto& sound : bundle->sound_files) {
        if (sound.animation_index >= loaded.animations.size())
          return invalidOutput<ResourceOutputs>(path, {}, "GLB output produced an invalid Animation index");
        PendingOutput output = mountedOutput(appendLogicalPath(parent, sound.file.path),
                                             std::move(sound.file.encoded_audio),
                                             ResourceOutputKind::kAudio);
        const Animation& animation = loaded.animations[sound.animation_index];
        output.owner_kind = ARX_RESOURCE_KIND_ANIMATION;
        output.owner_identity = animation.resourcePath().empty()
                                    ? (animation.name().empty() ? "animation " + std::to_string(sound.animation_index)
                                                                : std::string(animation.name()))
                                    : std::string(animation.resourcePath());
        outputs.push_back(std::move(output));
      }
    }
    if (includesOutput(options.resource, kResourceOutputImages)) {
      auto icon = appendModelInventoryIconOutputs(loaded.model, path, {}, false, outputs);
      if (!icon) return std::move(icon).propagate<ResourceOutputs>();
    }
    const std::string identity = loaded.model.resourcePath().empty() ? path : std::string(loaded.model.resourcePath());
    return ResourceIoResult<ResourceOutputs>::success(
        preparedWrite(std::move(outputs), ARX_RESOURCE_KIND_MODEL, identity, options.resource.io_flags));
  });
}

ResourceIoResult<ResourceOutputs> Resources::prepareModelFileOutputs(const LoadedModel& loaded,
                                                                     const std::filesystem::path& path,
                                                                     const ModelWriteOptions& options) const noexcept {
  if (loaded.animations.empty()) return prepareModelFileOutputs(loaded.model, path, options);
  return guardedWrite<ResourceOutputs>(options.resource, [&] {
    if (asciiLower(pathToUtf8(path.extension())) != ".glb")
      return invalidOutput<ResourceOutputs>(
          {}, path, "Model output with Animations must use GLB; write other formats separately");
    if (options.native_text_mode || options.compress)
      return invalidOutput<ResourceOutputs>(
          {}, path, "native Model options do not apply to Model-with-Animations GLB output");

    std::vector<const Animation*> animations;
    animations.reserve(loaded.animations.size());
    for (const Animation& animation : loaded.animations) animations.push_back(&animation);
    auto bundle = loaded.model.exportGlbBundle(animations, options.glb.value_or(Model::GlbExportOptions{}));
    if (!bundle) return writeConversionFailure<ResourceOutputs>(bundle, {}, path);

    std::vector<PendingOutput> outputs;
    if (includesOutput(options.resource, kResourceOutputPrimary))
      outputs.push_back(nativeOutput(path, std::move(bundle->glb), ResourceOutputKind::kData, true));
    if (includesOutput(options.resource, kResourceOutputAudio)) {
      for (auto& sound : bundle->sound_files) {
        if (sound.animation_index >= loaded.animations.size())
          return invalidOutput<ResourceOutputs>({}, path, "GLB output produced an invalid Animation index");
        PendingOutput output = nativeOutput(path.parent_path() / utf8Path(normalizedPath(sound.file.path)),
                                            std::move(sound.file.encoded_audio),
                                            ResourceOutputKind::kAudio);
        const Animation& animation = loaded.animations[sound.animation_index];
        output.owner_kind = ARX_RESOURCE_KIND_ANIMATION;
        output.owner_identity = animation.resourcePath().empty()
                                    ? (animation.name().empty() ? "animation " + std::to_string(sound.animation_index)
                                                                : std::string(animation.name()))
                                    : std::string(animation.resourcePath());
        outputs.push_back(std::move(output));
      }
    }
    if (includesOutput(options.resource, kResourceOutputImages)) {
      auto icon = appendModelInventoryIconOutputs(loaded.model, {}, path, false, outputs);
      if (!icon) return std::move(icon).propagate<ResourceOutputs>();
    }
    const std::string identity =
        loaded.model.resourcePath().empty() ? pathToUtf8(path) : std::string(loaded.model.resourcePath());
    return ResourceIoResult<ResourceOutputs>::success(
        preparedWrite(std::move(outputs), ARX_RESOURCE_KIND_MODEL, identity, options.resource.io_flags));
  });
}

ResourceIoResult<ResourceWriteReport> Resources::writeModel(const Model& model, std::string_view logical_path,
                                                            const ModelWriteOptions& options,
                                                            const ResourceWriteOptions& write_options) const noexcept {
  auto prepared = prepareModelOutputs(model, logical_path, options);
  if (!prepared) return std::move(prepared).propagate<ResourceWriteReport>();
  return writeOutputs(std::move(*prepared), write_options);
}

ResourceIoResult<ResourceWriteReport> Resources::writeModel(const LoadedModel& loaded, std::string_view logical_path,
                                                            const ModelWriteOptions& options,
                                                            const ResourceWriteOptions& write_options) const noexcept {
  auto prepared = prepareModelOutputs(loaded, logical_path, options);
  if (!prepared) return std::move(prepared).propagate<ResourceWriteReport>();
  return writeOutputs(std::move(*prepared), write_options);
}

ResourceIoResult<ResourceWriteReport> Resources::writeModelFile(
    const Model& model, const std::filesystem::path& path, const ModelWriteOptions& options,
    const ResourceWriteOptions& write_options) const noexcept {
  auto prepared = prepareModelFileOutputs(model, path, options);
  if (!prepared) return std::move(prepared).propagate<ResourceWriteReport>();
  return writeOutputs(std::move(*prepared), write_options);
}

ResourceIoResult<ResourceWriteReport> Resources::writeModelFile(
    const LoadedModel& loaded, const std::filesystem::path& path, const ModelWriteOptions& options,
    const ResourceWriteOptions& write_options) const noexcept {
  auto prepared = prepareModelFileOutputs(loaded, path, options);
  if (!prepared) return std::move(prepared).propagate<ResourceWriteReport>();
  return writeOutputs(std::move(*prepared), write_options);
}

ResourceIoResult<ResourceOutputs> Resources::prepareAnimationOutputs(
    const Animation& animation, std::string_view logical_path, const AnimationWriteOptions& options) const noexcept {
  return guardedWrite<ResourceOutputs>(options.resource, [&] {
    const std::string path = withDefaultExtension(logical_path, ".tea");
    const std::string format = lowerExtension(path);
    if (format != ".tea" && format != ".json")
      return invalidOutput<ResourceOutputs>(path, {}, "unsupported Animation output format");
    auto bundle =
        animation.bakeNativeBundle({.include_sound_files = includesOutput(options.resource, kResourceOutputAudio),
                                    .text_mode = options.native_text_mode});
    if (!bundle) return writeConversionFailure<ResourceOutputs>(bundle, path, {});
    std::vector<PendingOutput> outputs;
    if (format == ".tea" && includesOutput(options.resource, kResourceOutputPrimary)) {
      auto encoded = writeTea(bundle->tea);
      if (!encoded) return writeConversionFailure<ResourceOutputs>(encoded, path, {});
      outputs.push_back(mountedOutput(path, std::move(*encoded), ResourceOutputKind::kData, true));
    } else if (format == ".json" && includesOutput(options.resource, kResourceOutputPrimary)) {
      auto encoded = toTeaJson(bundle->tea, true);
      if (!encoded) return writeConversionFailure<ResourceOutputs>(encoded, path, {});
      outputs.push_back(mountedOutput(path, bytes(*encoded), ResourceOutputKind::kData, true));
    }
    if (includesOutput(options.resource, kResourceOutputAudio)) {
      const std::string parent = parentLogicalPath(path);
      for (auto& file : bundle->sound_files) {
        const std::string sound_path = normalizedPath(file.path);
        outputs.push_back(mountedOutput(format == ".tea" ? sound_path : appendLogicalPath(parent, sound_path),
                                        std::move(file.encoded_audio),
                                        ResourceOutputKind::kAudio));
      }
    }
    const std::string identity = animation.resourcePath().empty() ? path : std::string(animation.resourcePath());
    return ResourceIoResult<ResourceOutputs>::success(
        preparedWrite(std::move(outputs), ARX_RESOURCE_KIND_ANIMATION, identity, options.resource.io_flags));
  });
}

ResourceIoResult<ResourceOutputs> Resources::prepareAnimationFileOutputs(
    const Animation& animation, const std::filesystem::path& path,
    const AnimationWriteOptions& options) const noexcept {
  return guardedWrite<ResourceOutputs>(options.resource, [&] {
    const std::string format = asciiLower(pathToUtf8(path.extension()));
    if (format != ".tea" && format != ".json")
      return invalidOutput<ResourceOutputs>({}, path, "unsupported Animation output format");
    auto bundle =
        animation.bakeNativeBundle({.include_sound_files = includesOutput(options.resource, kResourceOutputAudio),
                                    .text_mode = options.native_text_mode});
    if (!bundle) return writeConversionFailure<ResourceOutputs>(bundle, {}, path);
    std::vector<PendingOutput> outputs;
    if (format == ".tea" && includesOutput(options.resource, kResourceOutputPrimary)) {
      auto encoded = writeTea(bundle->tea);
      if (!encoded) return writeConversionFailure<ResourceOutputs>(encoded, {}, path);
      outputs.push_back(nativeOutput(path, std::move(*encoded), ResourceOutputKind::kData, true));
    } else if (format == ".json" && includesOutput(options.resource, kResourceOutputPrimary)) {
      auto encoded = toTeaJson(bundle->tea, true);
      if (!encoded) return writeConversionFailure<ResourceOutputs>(encoded, {}, path);
      outputs.push_back(nativeOutput(path, bytes(*encoded), ResourceOutputKind::kData, true));
    }
    if (includesOutput(options.resource, kResourceOutputAudio)) {
      for (auto& file : bundle->sound_files)
        outputs.push_back(nativeOutput(path.parent_path() / utf8Path(normalizedPath(file.path)),
                                       std::move(file.encoded_audio),
                                       ResourceOutputKind::kAudio));
    }
    const std::string identity =
        animation.resourcePath().empty() ? pathToUtf8(path) : std::string(animation.resourcePath());
    return ResourceIoResult<ResourceOutputs>::success(
        preparedWrite(std::move(outputs), ARX_RESOURCE_KIND_ANIMATION, identity, options.resource.io_flags));
  });
}

ResourceIoResult<ResourceWriteReport> Resources::writeAnimation(
    const Animation& animation, std::string_view logical_path, const AnimationWriteOptions& options,
    const ResourceWriteOptions& write_options) const noexcept {
  auto prepared = prepareAnimationOutputs(animation, logical_path, options);
  if (!prepared) return std::move(prepared).propagate<ResourceWriteReport>();
  return writeOutputs(std::move(*prepared), write_options);
}

ResourceIoResult<ResourceWriteReport> Resources::writeAnimationFile(
    const Animation& animation, const std::filesystem::path& path, const AnimationWriteOptions& options,
    const ResourceWriteOptions& write_options) const noexcept {
  auto prepared = prepareAnimationFileOutputs(animation, path, options);
  if (!prepared) return std::move(prepared).propagate<ResourceWriteReport>();
  return writeOutputs(std::move(*prepared), write_options);
}

ResourceIoResult<ResourceOutputs> Resources::prepareLevelOutputs(const Level& level, std::string_view logical_path,
                                                                 const LevelWriteOptions& options) const noexcept {
  return prepareLevelOutputs(level, logical_path, std::nullopt, std::nullopt, std::nullopt, options);
}

ResourceIoResult<ResourceOutputs> Resources::prepareLevelOutputs(const Level& level, std::string_view logical_path,
                                                                 std::optional<std::string_view> llf_path,
                                                                 std::optional<std::string_view> dlf_path,
                                                                 std::optional<std::uint32_t> level_index,
                                                                 const LevelWriteOptions& options) const noexcept {
  return guardedWrite<ResourceOutputs>(options.resource, [&] {
    const std::string path = withDefaultExtension(logical_path, ".dlf");
    const std::string format = lowerExtension(path);
    if (options.glb && format != ".glb")
      return invalidOutput<ResourceOutputs>(path, {}, "GLB options do not apply to this Level output format");
    if (format == ".glb" && (options.native_text_mode || options.reconstruct_quads || options.compress ||
                             options.embed_lighting || options.signer))
      return invalidOutput<ResourceOutputs>(path, {}, "native options do not apply to GLB Level output");
    std::vector<PendingOutput> outputs;

    if (format == ".json" && isLevelCompanionJson(path))
      return invalidOutput<ResourceOutputs>(
          path, {}, "loose Level output must use FTS or FTS JSON as its primary document");

    if (format == ".glb") {
      if (llf_path || dlf_path)
        return invalidOutput<ResourceOutputs>(path, {}, "GLB Level output does not accept native companions");
      if (level_index) return invalidOutput<ResourceOutputs>(path, {}, "GLB Level output does not use a level index");
      auto encoded = level.exportGlb(options.glb.value_or(Level::GlbExportOptions{}));
      if (!encoded) return writeConversionFailure<ResourceOutputs>(encoded, path, {});
      if (includesOutput(options.resource, kResourceOutputPrimary))
        outputs.push_back(mountedOutput(path, std::move(*encoded), ResourceOutputKind::kData, true));
      if (includesOutput(options.resource, kResourceOutputImages)) {
        auto images =
            appendLevelImageOutputs(level, mounts_, path, {}, false, std::nullopt, options.resource.io_flags, outputs);
        if (!images) return std::move(images).propagate<ResourceOutputs>();
      }
    } else if (format == ".fts" || format == ".json") {
      if (level_index && format != ".json")
        return invalidOutput<ResourceOutputs>(path, {}, "level_index applies only to FTS JSON output");
      if (!level_index && format == ".json") {
        std::uint32_t parsed = 0;
        if (paths::levelFromDlf(level.resourcePath(), parsed)) level_index = parsed;
        if (!level_index)
          return invalidOutput<ResourceOutputs>(path, {}, "Level JSON output requires a canonical Level resource path");
      }
      const std::uint32_t json_level = level_index.value_or(0);
      std::string level_name = format == ".json" ? "level" + std::to_string(json_level) : filenameStem(path);
      if (level_name.ends_with(".fts")) level_name.resize(level_name.size() - 4U);
      auto bundle =
          level.bakeNativeBundle({.level_name = level_name,
                                  .include_texture_files = includesOutput(options.resource, kResourceOutputTextures),
                                  .text_mode = options.native_text_mode.value_or(NativeTextMode::kAuto),
                                  .reconstruct_quads = options.reconstruct_quads.value_or(true)});
      if (!bundle) return writeConversionFailure<ResourceOutputs>(bundle, path, {});
      if (includesOutput(options.resource, kResourceOutputPrimary)) {
        if (format == ".fts") {
          auto encoded = writeFts(bundle->fts, options.compress.value_or(true));
          if (!encoded) return writeConversionFailure<ResourceOutputs>(encoded, path, {});
          outputs.push_back(mountedOutput(path, std::move(*encoded), ResourceOutputKind::kData, true));
        } else {
          auto encoded = toFtsJson(bundle->fts, json_level, true);
          if (!encoded) return writeConversionFailure<ResourceOutputs>(encoded, path, {});
          outputs.push_back(mountedOutput(path, bytes(*encoded), ResourceOutputKind::kData, true));
        }
      }
      if (includesOutput(options.resource, kResourceOutputCompanions)) {
        const bool json_companions = format == ".json";
        const std::string llf_output =
            llf_path ? normalizedPath(*llf_path) : derivedLevelCompanion(path, ".llf", json_companions);
        const std::string dlf_output =
            dlf_path ? normalizedPath(*dlf_path) : derivedLevelCompanion(path, ".dlf", json_companions);
        if (lowerExtension(llf_output) == ".json") {
          auto encoded = toLlfJson(bundle->llf, true);
          if (!encoded) return writeConversionFailure<ResourceOutputs>(encoded, llf_output, {});
          outputs.push_back(mountedOutput(llf_output, bytes(*encoded)));
        } else {
          auto encoded = writeLlf(
              bundle->llf, LlfWriteOptions{.signer = options.signer.value_or("")}, options.compress.value_or(true));
          if (!encoded) return writeConversionFailure<ResourceOutputs>(encoded, llf_output, {});
          outputs.push_back(mountedOutput(llf_output, std::move(*encoded)));
        }
        if (lowerExtension(dlf_output) == ".json") {
          auto encoded = toDlfJson(bundle->dlf, true);
          if (!encoded) return writeConversionFailure<ResourceOutputs>(encoded, dlf_output, {});
          outputs.push_back(mountedOutput(dlf_output, bytes(*encoded)));
        } else {
          auto encoded = writeDlf(bundle->dlf,
                                  {.embedded_lighting = options.embed_lighting.value_or(false) ? &bundle->llf : nullptr,
                                   .signer = options.signer.value_or("")},
                                  options.compress.value_or(true));
          if (!encoded) return writeConversionFailure<ResourceOutputs>(encoded, dlf_output, {});
          outputs.push_back(mountedOutput(dlf_output, std::move(*encoded)));
        }
      }
      if (includesOutput(options.resource, kResourceOutputTextures)) {
        const std::string parent = parentLogicalPath(path);
        for (auto& file : bundle->texture_files)
          outputs.push_back(mountedOutput(appendLogicalPath(parent, normalizedPath(file.resource_path)),
                                          std::move(file.encoded_image),
                                          ResourceOutputKind::kImage));
      }
      if (includesOutput(options.resource, kResourceOutputImages)) {
        auto images =
            appendLevelImageOutputs(level, mounts_, path, {}, false, level_index, options.resource.io_flags, outputs);
        if (!images) return std::move(images).propagate<ResourceOutputs>();
      }
    } else if (format == ".dlf") {
      if (llf_path || dlf_path)
        return invalidOutput<ResourceOutputs>(
            path, {}, "game-layout Level output does not accept loose companion destinations");
      if (level_index)
        return invalidOutput<ResourceOutputs>(path, {}, "game-layout Level output does not accept level_index");
      std::uint32_t level_index = 0;
      if (!paths::levelFromDlf(path, level_index))
        return invalidOutput<ResourceOutputs>(path, {}, "game-layout Level output must use a canonical DLF path");
      const std::string level_name = "level" + std::to_string(level_index);
      auto bundle =
          level.bakeNativeBundle({.level_name = level_name,
                                  .include_texture_files = includesOutput(options.resource, kResourceOutputTextures),
                                  .text_mode = options.native_text_mode.value_or(NativeTextMode::kAuto),
                                  .reconstruct_quads = options.reconstruct_quads.value_or(true)});
      if (!bundle) return writeConversionFailure<ResourceOutputs>(bundle, path, {});
      if (includesOutput(options.resource, kResourceOutputPrimary)) {
        auto encoded = writeDlf(bundle->dlf,
                                {.embedded_lighting = options.embed_lighting.value_or(false) ? &bundle->llf : nullptr,
                                 .signer = options.signer.value_or("")},
                                options.compress.value_or(true));
        if (!encoded) return writeConversionFailure<ResourceOutputs>(encoded, path, {});
        outputs.push_back(mountedOutput(path, std::move(*encoded), ResourceOutputKind::kData, true));
      }
      if (includesOutput(options.resource, kResourceOutputCompanions)) {
        auto fts = writeFts(bundle->fts, options.compress.value_or(true));
        if (!fts) return writeConversionFailure<ResourceOutputs>(fts, paths::levelFts(level_index), {});
        auto llf = writeLlf(
            bundle->llf, LlfWriteOptions{.signer = options.signer.value_or("")}, options.compress.value_or(true));
        if (!llf) return writeConversionFailure<ResourceOutputs>(llf, paths::levelLlf(level_index), {});
        outputs.push_back(mountedOutput(paths::levelFts(level_index), std::move(*fts)));
        outputs.push_back(mountedOutput(paths::levelLlf(level_index), std::move(*llf)));
      }
      if (includesOutput(options.resource, kResourceOutputTextures)) {
        for (auto& file : bundle->texture_files)
          outputs.push_back(mountedOutput(
              normalizedPath(file.resource_path), std::move(file.encoded_image), ResourceOutputKind::kImage));
      }
      if (includesOutput(options.resource, kResourceOutputImages)) {
        auto images =
            appendLevelImageOutputs(level, mounts_, path, {}, true, level_index, options.resource.io_flags, outputs);
        if (!images) return std::move(images).propagate<ResourceOutputs>();
      }
    } else {
      return invalidOutput<ResourceOutputs>(path, {}, "unsupported Level output format");
    }

    const std::string identity = level.resourcePath().empty() ? path : std::string(level.resourcePath());
    return ResourceIoResult<ResourceOutputs>::success(
        preparedWrite(std::move(outputs), ARX_RESOURCE_KIND_LEVEL, identity, options.resource.io_flags));
  });
}

ResourceIoResult<ResourceOutputs> Resources::prepareLevelFileOutputs(
    const Level& level, const std::filesystem::path& primary_path, const std::optional<std::filesystem::path>& llf_path,
    const std::optional<std::filesystem::path>& dlf_path, std::optional<std::uint32_t> level_index,
    const LevelWriteOptions& options) const noexcept {
  return guardedWrite<ResourceOutputs>(options.resource, [&] {
    const std::string primary_utf8 = pathToUtf8(primary_path);
    const std::string format = asciiLower(pathToUtf8(primary_path.extension()));
    if (options.glb && format != ".glb")
      return invalidOutput<ResourceOutputs>({}, primary_path, "GLB options do not apply to this Level output format");
    if (format == ".glb" && (options.native_text_mode || options.reconstruct_quads || options.compress ||
                             options.embed_lighting || options.signer))
      return invalidOutput<ResourceOutputs>({}, primary_path, "native options do not apply to GLB Level output");
    std::vector<PendingOutput> outputs;
    if (format == ".json" && isLevelCompanionJson(primary_utf8))
      return invalidOutput<ResourceOutputs>(
          {}, primary_path, "loose Level output must use FTS or FTS JSON as its primary document");
    if (format == ".glb") {
      if (llf_path || dlf_path)
        return invalidOutput<ResourceOutputs>({}, primary_path, "GLB Level output does not accept native companions");
      if (level_index)
        return invalidOutput<ResourceOutputs>({}, primary_path, "GLB Level output does not use a level index");
      auto encoded = level.exportGlb(options.glb.value_or(Level::GlbExportOptions{}));
      if (!encoded) return writeConversionFailure<ResourceOutputs>(encoded, {}, primary_path);
      if (includesOutput(options.resource, kResourceOutputPrimary))
        outputs.push_back(nativeOutput(primary_path, std::move(*encoded), ResourceOutputKind::kData, true));
      if (includesOutput(options.resource, kResourceOutputImages)) {
        auto images = appendLevelImageOutputs(
            level, mounts_, {}, primary_path, false, std::nullopt, options.resource.io_flags, outputs);
        if (!images) return std::move(images).propagate<ResourceOutputs>();
      }
    } else {
      if (format != ".fts" && format != ".json")
        return invalidOutput<ResourceOutputs>({}, primary_path, "loose Level output must be FTS, FTS JSON, or GLB");
      if (level_index && format != ".json")
        return invalidOutput<ResourceOutputs>({}, primary_path, "level_index applies only to FTS JSON output");
      if (!level_index && format == ".json") {
        std::uint32_t parsed = 0;
        if (paths::levelFromDlf(level.resourcePath(), parsed)) level_index = parsed;
      }
      if (format == ".json" && !level_index)
        return invalidOutput<ResourceOutputs>(
            {}, primary_path, "Level JSON output requires level_index or a canonical Level resource path");

      const std::uint32_t json_level = level_index.value_or(0);
      const bool json_companions = format == ".json";
      const auto companion_path = [&](const std::optional<std::filesystem::path>& explicit_path,
                                      std::string_view role) {
        if (explicit_path) return *explicit_path;
        std::filesystem::path result = looseLevelBase(primary_path);
        result += role;
        if (json_companions) result += ".json";
        return result;
      };
      const std::filesystem::path llf_output = companion_path(llf_path, ".llf");
      const std::filesystem::path dlf_output = companion_path(dlf_path, ".dlf");
      std::string level_name =
          format == ".json" ? "level" + std::to_string(json_level) : pathToUtf8(primary_path.stem());
      if (level_name.ends_with(".fts")) level_name.resize(level_name.size() - 4U);
      auto bundle =
          level.bakeNativeBundle({.level_name = level_name,
                                  .include_texture_files = includesOutput(options.resource, kResourceOutputTextures),
                                  .text_mode = options.native_text_mode.value_or(NativeTextMode::kAuto),
                                  .reconstruct_quads = options.reconstruct_quads.value_or(true)});
      if (!bundle) return writeConversionFailure<ResourceOutputs>(bundle, {}, primary_path);
      if (includesOutput(options.resource, kResourceOutputPrimary)) {
        if (format == ".fts") {
          auto encoded = writeFts(bundle->fts, options.compress.value_or(true));
          if (!encoded) return writeConversionFailure<ResourceOutputs>(encoded, {}, primary_path);
          outputs.push_back(nativeOutput(primary_path, std::move(*encoded), ResourceOutputKind::kData, true));
        } else {
          auto encoded = toFtsJson(bundle->fts, json_level, true);
          if (!encoded) return writeConversionFailure<ResourceOutputs>(encoded, {}, primary_path);
          outputs.push_back(nativeOutput(primary_path, bytes(*encoded), ResourceOutputKind::kData, true));
        }
      }
      if (includesOutput(options.resource, kResourceOutputCompanions)) {
        if (asciiLower(pathToUtf8(llf_output.extension())) == ".json") {
          auto encoded = toLlfJson(bundle->llf, true);
          if (!encoded) return writeConversionFailure<ResourceOutputs>(encoded, {}, llf_output);
          outputs.push_back(nativeOutput(llf_output, bytes(*encoded)));
        } else {
          auto encoded = writeLlf(
              bundle->llf, LlfWriteOptions{.signer = options.signer.value_or("")}, options.compress.value_or(true));
          if (!encoded) return writeConversionFailure<ResourceOutputs>(encoded, {}, llf_output);
          outputs.push_back(nativeOutput(llf_output, std::move(*encoded)));
        }
        if (asciiLower(pathToUtf8(dlf_output.extension())) == ".json") {
          auto encoded = toDlfJson(bundle->dlf, true);
          if (!encoded) return writeConversionFailure<ResourceOutputs>(encoded, {}, dlf_output);
          outputs.push_back(nativeOutput(dlf_output, bytes(*encoded)));
        } else {
          auto encoded = writeDlf(bundle->dlf,
                                  {.embedded_lighting = options.embed_lighting.value_or(false) ? &bundle->llf : nullptr,
                                   .signer = options.signer.value_or("")},
                                  options.compress.value_or(true));
          if (!encoded) return writeConversionFailure<ResourceOutputs>(encoded, {}, dlf_output);
          outputs.push_back(nativeOutput(dlf_output, std::move(*encoded)));
        }
      }
      if (includesOutput(options.resource, kResourceOutputTextures)) {
        for (auto& file : bundle->texture_files)
          outputs.push_back(nativeOutput(primary_path.parent_path() / utf8Path(normalizedPath(file.resource_path)),
                                         std::move(file.encoded_image),
                                         ResourceOutputKind::kImage));
      }
      if (includesOutput(options.resource, kResourceOutputImages)) {
        auto images = appendLevelImageOutputs(
            level, mounts_, {}, primary_path, false, level_index, options.resource.io_flags, outputs);
        if (!images) return std::move(images).propagate<ResourceOutputs>();
      }
    }

    const std::string identity =
        level.resourcePath().empty() ? pathToUtf8(primary_path) : std::string(level.resourcePath());
    return ResourceIoResult<ResourceOutputs>::success(
        preparedWrite(std::move(outputs), ARX_RESOURCE_KIND_LEVEL, identity, options.resource.io_flags));
  });
}

ResourceIoResult<ResourceWriteReport> Resources::writeLevel(const Level& level, std::string_view logical_path,
                                                            const LevelWriteOptions& options,
                                                            const ResourceWriteOptions& write_options) const noexcept {
  auto prepared = prepareLevelOutputs(level, logical_path, options);
  if (!prepared) return std::move(prepared).propagate<ResourceWriteReport>();
  return writeOutputs(std::move(*prepared), write_options);
}

ResourceIoResult<ResourceWriteReport> Resources::writeLevel(const Level& level, std::string_view logical_path,
                                                            std::optional<std::string_view> llf_path,
                                                            std::optional<std::string_view> dlf_path,
                                                            std::optional<std::uint32_t> level_index,
                                                            const LevelWriteOptions& options,
                                                            const ResourceWriteOptions& write_options) const noexcept {
  auto prepared = prepareLevelOutputs(level, logical_path, llf_path, dlf_path, level_index, options);
  if (!prepared) return std::move(prepared).propagate<ResourceWriteReport>();
  return writeOutputs(std::move(*prepared), write_options);
}

ResourceIoResult<ResourceWriteReport> Resources::writeLevelFile(
    const Level& level, const std::filesystem::path& primary_path, const std::optional<std::filesystem::path>& llf_path,
    const std::optional<std::filesystem::path>& dlf_path, std::optional<std::uint32_t> level_index,
    const LevelWriteOptions& options, const ResourceWriteOptions& write_options) const noexcept {
  auto prepared = prepareLevelFileOutputs(level, primary_path, llf_path, dlf_path, level_index, options);
  if (!prepared) return std::move(prepared).propagate<ResourceWriteReport>();
  return writeOutputs(std::move(*prepared), write_options);
}

ResourceIoResult<ResourceOutputs> Resources::prepareAmbianceOutputs(
    const Ambiance& ambiance, std::string_view logical_path, const AmbianceWriteOptions& options) const noexcept {
  return guardedWrite<ResourceOutputs>(options.resource, [&] {
    const std::string path = withDefaultExtension(logical_path, ".amb");
    const std::string format = lowerExtension(path);
    if (options.glb && format != ".glb")
      return invalidOutput<ResourceOutputs>(path, {}, "GLB options do not apply to this Ambiance output format");
    if (options.native_text_mode && format != ".amb" && format != ".json")
      return invalidOutput<ResourceOutputs>(path, {}, "native text mode does not apply to this Ambiance output format");
    std::vector<PendingOutput> outputs;
    if (format == ".amb" || format == ".json") {
      auto bundle =
          ambiance.bakeNativeBundle({.include_sound_files = includesOutput(options.resource, kResourceOutputAudio),
                                     .text_mode = options.native_text_mode.value_or(NativeTextMode::kAuto)});
      if (!bundle) return writeConversionFailure<ResourceOutputs>(bundle, path, {});
      if (format == ".amb" && includesOutput(options.resource, kResourceOutputPrimary)) {
        auto encoded = writeAmb(bundle->amb);
        if (!encoded) return writeConversionFailure<ResourceOutputs>(encoded, path, {});
        outputs.push_back(mountedOutput(path, std::move(*encoded), ResourceOutputKind::kData, true));
      } else if (format == ".json" && includesOutput(options.resource, kResourceOutputPrimary)) {
        auto encoded = toAmbJson(bundle->amb, true);
        if (!encoded) return writeConversionFailure<ResourceOutputs>(encoded, path, {});
        outputs.push_back(mountedOutput(path, bytes(*encoded), ResourceOutputKind::kData, true));
      }
      if (includesOutput(options.resource, kResourceOutputAudio)) {
        for (auto& file : bundle->sound_files) {
          std::string sound_path = normalizedPath(file.path);
          if (format == ".amb" && !sound_path.starts_with("sfx/")) sound_path.insert(0, "sfx/");
          if (format == ".json") sound_path = appendLogicalPath(parentLogicalPath(path), sound_path);
          outputs.push_back(
              mountedOutput(std::move(sound_path), std::move(file.encoded_audio), ResourceOutputKind::kAudio));
        }
      }
    } else if (format == ".glb") {
      auto bundle = ambiance.exportGlbBundle(options.glb.value_or(Ambiance::GlbExportOptions{}), nullptr);
      if (!bundle) return writeConversionFailure<ResourceOutputs>(bundle, path, {});
      if (includesOutput(options.resource, kResourceOutputPrimary))
        outputs.push_back(mountedOutput(path, std::move(bundle->glb), ResourceOutputKind::kData, true));
      if (includesOutput(options.resource, kResourceOutputAudio)) {
        const std::string parent = parentLogicalPath(path);
        for (auto& file : bundle->sound_files)
          outputs.push_back(mountedOutput(
              appendLogicalPath(parent, file.path), std::move(file.encoded_audio), ResourceOutputKind::kAudio));
      }
    } else {
      return invalidOutput<ResourceOutputs>(path, {}, "unsupported Ambiance output format");
    }
    const std::string identity = ambiance.resourcePath().empty() ? path : std::string(ambiance.resourcePath());
    return ResourceIoResult<ResourceOutputs>::success(
        preparedWrite(std::move(outputs), ARX_RESOURCE_KIND_AMBIANCE, identity, options.resource.io_flags));
  });
}

ResourceIoResult<ResourceOutputs> Resources::prepareAmbianceFileOutputs(
    const Ambiance& ambiance, const std::filesystem::path& path, const AmbianceWriteOptions& options) const noexcept {
  return guardedWrite<ResourceOutputs>(options.resource, [&] {
    const std::string format = asciiLower(pathToUtf8(path.extension()));
    if (options.glb && format != ".glb")
      return invalidOutput<ResourceOutputs>({}, path, "GLB options do not apply to this Ambiance output format");
    if (options.native_text_mode && format != ".amb" && format != ".json")
      return invalidOutput<ResourceOutputs>({}, path, "native text mode does not apply to this Ambiance output format");
    std::vector<PendingOutput> outputs;
    if (format == ".amb" || format == ".json") {
      auto bundle =
          ambiance.bakeNativeBundle({.include_sound_files = includesOutput(options.resource, kResourceOutputAudio),
                                     .text_mode = options.native_text_mode.value_or(NativeTextMode::kAuto)});
      if (!bundle) return writeConversionFailure<ResourceOutputs>(bundle, {}, path);
      if (format == ".amb" && includesOutput(options.resource, kResourceOutputPrimary)) {
        auto encoded = writeAmb(bundle->amb);
        if (!encoded) return writeConversionFailure<ResourceOutputs>(encoded, {}, path);
        outputs.push_back(nativeOutput(path, std::move(*encoded), ResourceOutputKind::kData, true));
      } else if (format == ".json" && includesOutput(options.resource, kResourceOutputPrimary)) {
        auto encoded = toAmbJson(bundle->amb, true);
        if (!encoded) return writeConversionFailure<ResourceOutputs>(encoded, {}, path);
        outputs.push_back(nativeOutput(path, bytes(*encoded), ResourceOutputKind::kData, true));
      }
      if (includesOutput(options.resource, kResourceOutputAudio)) {
        for (auto& file : bundle->sound_files)
          outputs.push_back(nativeOutput(path.parent_path() / utf8Path(normalizedPath(file.path)),
                                         std::move(file.encoded_audio),
                                         ResourceOutputKind::kAudio));
      }
    } else if (format == ".glb") {
      auto bundle = ambiance.exportGlbBundle(options.glb.value_or(Ambiance::GlbExportOptions{}), nullptr);
      if (!bundle) return writeConversionFailure<ResourceOutputs>(bundle, {}, path);
      if (includesOutput(options.resource, kResourceOutputPrimary))
        outputs.push_back(nativeOutput(path, std::move(bundle->glb), ResourceOutputKind::kData, true));
      if (includesOutput(options.resource, kResourceOutputAudio)) {
        for (auto& file : bundle->sound_files)
          outputs.push_back(nativeOutput(path.parent_path() / utf8Path(normalizedPath(file.path)),
                                         std::move(file.encoded_audio),
                                         ResourceOutputKind::kAudio));
      }
    } else {
      return invalidOutput<ResourceOutputs>({}, path, "unsupported Ambiance output format");
    }
    const std::string identity =
        ambiance.resourcePath().empty() ? pathToUtf8(path) : std::string(ambiance.resourcePath());
    return ResourceIoResult<ResourceOutputs>::success(
        preparedWrite(std::move(outputs), ARX_RESOURCE_KIND_AMBIANCE, identity, options.resource.io_flags));
  });
}

ResourceIoResult<ResourceWriteReport> Resources::writeAmbiance(
    const Ambiance& ambiance, std::string_view logical_path, const AmbianceWriteOptions& options,
    const ResourceWriteOptions& write_options) const noexcept {
  auto prepared = prepareAmbianceOutputs(ambiance, logical_path, options);
  if (!prepared) return std::move(prepared).propagate<ResourceWriteReport>();
  return writeOutputs(std::move(*prepared), write_options);
}

ResourceIoResult<ResourceWriteReport> Resources::writeAmbianceFile(
    const Ambiance& ambiance, const std::filesystem::path& path, const AmbianceWriteOptions& options,
    const ResourceWriteOptions& write_options) const noexcept {
  auto prepared = prepareAmbianceFileOutputs(ambiance, path, options);
  if (!prepared) return std::move(prepared).propagate<ResourceWriteReport>();
  return writeOutputs(std::move(*prepared), write_options);
}

ResourceIoResult<ResourceOutputs> Resources::prepareCinematicOutputs(
    const Cinematic& cinematic, std::string_view logical_path, const CinematicWriteOptions& options) const noexcept {
  return guardedWrite<ResourceOutputs>(options.resource, [&] {
    const std::string path = withDefaultExtension(logical_path, ".cin");
    const std::string format = lowerExtension(path);
    if (format != ".cin" && (options.native_text_mode || options.illustration_format))
      return invalidOutput<ResourceOutputs>(path, {}, "native options do not apply to this Cinematic output format");
    std::vector<PendingOutput> outputs;
    if (format == ".cin") {
      auto bundle = cinematic.bakeNativeBundle(
          {.include_illustration_files = includesOutput(options.resource, kResourceOutputImages),
           .include_sound_files = includesOutput(options.resource, kResourceOutputAudio),
           .illustration_format = options.illustration_format.value_or(ARX_IMAGE_FORMAT_UNKNOWN),
           .text_mode = options.native_text_mode.value_or(NativeTextMode::kAuto)});
      if (!bundle) return writeConversionFailure<ResourceOutputs>(bundle, path, {});
      if (includesOutput(options.resource, kResourceOutputPrimary)) {
        auto encoded = writeCin(bundle->cin);
        if (!encoded) return writeConversionFailure<ResourceOutputs>(encoded, path, {});
        outputs.push_back(mountedOutput(path, std::move(*encoded), ResourceOutputKind::kData, true));
      }
      if (includesOutput(options.resource, kResourceOutputImages)) {
        for (auto& file : bundle->illustration_files)
          outputs.push_back(mountedOutput(
              normalizedPath(file.resource_path), std::move(file.encoded_image), ResourceOutputKind::kImage));
      }
      if (includesOutput(options.resource, kResourceOutputAudio)) {
        for (auto& file : bundle->sound_files)
          outputs.push_back(
              mountedOutput(normalizedPath(file.path), std::move(file.encoded_audio), ResourceOutputKind::kAudio));
      }
    } else if (format == ".glb") {
      auto bundle = cinematic.exportGlbBundle();
      if (!bundle) return writeConversionFailure<ResourceOutputs>(bundle, path, {});
      if (includesOutput(options.resource, kResourceOutputPrimary))
        outputs.push_back(mountedOutput(path, std::move(bundle->glb), ResourceOutputKind::kData, true));
      if (includesOutput(options.resource, kResourceOutputAudio)) {
        const std::string parent = parentLogicalPath(path);
        for (auto& file : bundle->sound_files)
          outputs.push_back(mountedOutput(
              appendLogicalPath(parent, file.path), std::move(file.encoded_audio), ResourceOutputKind::kAudio));
      }
    } else {
      return invalidOutput<ResourceOutputs>(path, {}, "unsupported Cinematic output format");
    }
    const std::string identity = cinematic.resourcePath().empty() ? path : std::string(cinematic.resourcePath());
    return ResourceIoResult<ResourceOutputs>::success(
        preparedWrite(std::move(outputs), ARX_RESOURCE_KIND_CINEMATIC, identity, options.resource.io_flags));
  });
}

ResourceIoResult<ResourceOutputs> Resources::prepareCinematicFileOutputs(
    const Cinematic& cinematic, const std::filesystem::path& path,
    const CinematicWriteOptions& options) const noexcept {
  return guardedWrite<ResourceOutputs>(options.resource, [&] {
    const std::string format = asciiLower(pathToUtf8(path.extension()));
    if (format != ".cin" && (options.native_text_mode || options.illustration_format))
      return invalidOutput<ResourceOutputs>({}, path, "native options do not apply to this Cinematic output format");
    std::vector<PendingOutput> outputs;
    if (format == ".cin") {
      auto bundle = cinematic.bakeNativeBundle(
          {.include_illustration_files = includesOutput(options.resource, kResourceOutputImages),
           .include_sound_files = includesOutput(options.resource, kResourceOutputAudio),
           .illustration_format = options.illustration_format.value_or(ARX_IMAGE_FORMAT_UNKNOWN),
           .text_mode = options.native_text_mode.value_or(NativeTextMode::kAuto)});
      if (!bundle) return writeConversionFailure<ResourceOutputs>(bundle, {}, path);
      if (includesOutput(options.resource, kResourceOutputPrimary)) {
        auto encoded = writeCin(bundle->cin);
        if (!encoded) return writeConversionFailure<ResourceOutputs>(encoded, {}, path);
        outputs.push_back(nativeOutput(path, std::move(*encoded), ResourceOutputKind::kData, true));
      }
      if (includesOutput(options.resource, kResourceOutputImages)) {
        for (auto& file : bundle->illustration_files)
          outputs.push_back(nativeOutput(path.parent_path() / utf8Path(normalizedPath(file.resource_path)),
                                         std::move(file.encoded_image),
                                         ResourceOutputKind::kImage));
      }
      if (includesOutput(options.resource, kResourceOutputAudio)) {
        for (auto& file : bundle->sound_files)
          outputs.push_back(nativeOutput(path.parent_path() / utf8Path(normalizedPath(file.path)),
                                         std::move(file.encoded_audio),
                                         ResourceOutputKind::kAudio));
      }
    } else if (format == ".glb") {
      auto bundle = cinematic.exportGlbBundle();
      if (!bundle) return writeConversionFailure<ResourceOutputs>(bundle, {}, path);
      if (includesOutput(options.resource, kResourceOutputPrimary))
        outputs.push_back(nativeOutput(path, std::move(bundle->glb), ResourceOutputKind::kData, true));
      if (includesOutput(options.resource, kResourceOutputAudio)) {
        for (auto& file : bundle->sound_files)
          outputs.push_back(nativeOutput(path.parent_path() / utf8Path(normalizedPath(file.path)),
                                         std::move(file.encoded_audio),
                                         ResourceOutputKind::kAudio));
      }
    } else {
      return invalidOutput<ResourceOutputs>({}, path, "unsupported Cinematic output format");
    }
    const std::string identity =
        cinematic.resourcePath().empty() ? pathToUtf8(path) : std::string(cinematic.resourcePath());
    return ResourceIoResult<ResourceOutputs>::success(
        preparedWrite(std::move(outputs), ARX_RESOURCE_KIND_CINEMATIC, identity, options.resource.io_flags));
  });
}

ResourceIoResult<ResourceWriteReport> Resources::writeCinematic(
    const Cinematic& cinematic, std::string_view logical_path, const CinematicWriteOptions& options,
    const ResourceWriteOptions& write_options) const noexcept {
  auto prepared = prepareCinematicOutputs(cinematic, logical_path, options);
  if (!prepared) return std::move(prepared).propagate<ResourceWriteReport>();
  return writeOutputs(std::move(*prepared), write_options);
}

ResourceIoResult<ResourceWriteReport> Resources::writeCinematicFile(
    const Cinematic& cinematic, const std::filesystem::path& path, const CinematicWriteOptions& options,
    const ResourceWriteOptions& write_options) const noexcept {
  auto prepared = prepareCinematicFileOutputs(cinematic, path, options);
  if (!prepared) return std::move(prepared).propagate<ResourceWriteReport>();
  return writeOutputs(std::move(*prepared), write_options);
}

ResourceIoResult<ResourceWriteReport> Resources::writeOutputs(ResourceOutputs outputs,
                                                              const ResourceWriteOptions& options) const noexcept {
  return guardedWrite<ResourceWriteReport>([&] {
    auto plan = prepareWrite(std::move(outputs), options);
    if (!plan) return std::move(plan).propagate<ResourceWriteReport>();
    return plan->execute();
  });
}

}  // namespace pistoris::resource_io
