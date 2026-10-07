// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#pragma once

#include "arx_pistoris/base/math.h"
#include "arx_pistoris/native/amb.hpp"
#include "arx_pistoris/native/cin.hpp"
#include "arx_pistoris/native/dlf.hpp"
#include "arx_pistoris/native/ftl.hpp"
#include "arx_pistoris/native/fts.hpp"
#include "arx_pistoris/native/llf.hpp"
#include "arx_pistoris/native/tea.hpp"
#include "arx_pistoris/native/text.hpp"
#include "arx_pistoris/resource_io/document.hpp"
#include "arx_pistoris/resource_io/status.h"

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <limits>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace pistoris::resource_io {

struct ResourceOrigin {
  std::string requested_path;
  std::string logical_path;
  std::filesystem::path native_path;
  ResourceClassification classification;
  ResourceAddress address = ResourceAddress::kNative;
  ResourceLayout layout = ResourceLayout::kLoose;
  ResourceMountMask mount_mask = 0;
  ResourceMountMask mount_id = 0;
  ResourceIoFlags flags = kResourceIoFlagNone;
  ArxResourceKind selector_kind = ARX_RESOURCE_KIND_NONE;
};

enum class NativeResourceRole : std::uint8_t {
  kTexture,
  kInventoryIcon,
  kSound,
  kIllustration,
  kMinimap,
  kMinimapOffsetMetadata,
  kLoadingScreen,
};

inline constexpr std::size_t kNoNativeResource = std::numeric_limits<std::size_t>::max();

class NativeResourceFile {
 public:
  [[nodiscard]] std::string_view logicalPath() const noexcept { return logical_path_; }
  [[nodiscard]] const std::filesystem::path& nativePath() const noexcept { return native_path_; }
  [[nodiscard]] ResourceMountMask mountId() const noexcept { return mount_id_; }
  [[nodiscard]] std::span<const std::uint8_t> data() const noexcept { return data_; }

 private:
  std::string logical_path_;
  std::filesystem::path native_path_;
  ResourceMountMask mount_id_ = 0;
  std::vector<std::uint8_t> data_;

  friend class Resources;
  friend class NativeResourceSetBuilder;
};

class NativeResourceReference {
 public:
  [[nodiscard]] NativeResourceRole role() const noexcept { return role_; }
  [[nodiscard]] ArxResourceKind ownerKind() const noexcept { return owner_kind_; }
  [[nodiscard]] std::size_t ownerIndex() const noexcept { return owner_index_; }
  [[nodiscard]] std::uint64_t element() const noexcept { return element_; }
  [[nodiscard]] std::string_view language() const noexcept { return language_; }
  [[nodiscard]] std::string_view authoredPath() const noexcept { return authored_path_; }
  [[nodiscard]] std::size_t resource() const noexcept { return resource_; }
  [[nodiscard]] ArxReturnCode status() const noexcept { return status_; }

 private:
  NativeResourceRole role_ = NativeResourceRole::kTexture;
  ArxResourceKind owner_kind_ = ARX_RESOURCE_KIND_NONE;
  std::size_t owner_index_ = 0;
  std::uint64_t element_ = 0;
  std::string language_;
  std::string authored_path_;
  std::size_t resource_ = kNoNativeResource;
  ArxReturnCode status_ = ARX_RESOURCE_IO_NOT_FOUND;

  friend class Resources;
  friend class NativeResourceSetBuilder;
};

class NativeResourceSet {
 public:
  [[nodiscard]] std::span<const NativeResourceFile> files() const noexcept { return files_; }
  [[nodiscard]] std::span<const NativeResourceReference> references() const noexcept { return references_; }

 private:
  std::vector<NativeResourceFile> files_;
  std::vector<NativeResourceReference> references_;

  friend class Resources;
  friend class NativeResourceSetBuilder;
};

template <class Carrier>
class NativeMember {  // NOLINT(bugprone-exception-escape): MSVC debug STL misreports carrier container moves
 public:
  [[nodiscard]] const Carrier& carrier() const noexcept { return carrier_; }
  [[nodiscard]] const ResourceOrigin& source() const noexcept { return source_; }
  [[nodiscard]] NativeTextMode textMode() const noexcept { return text_mode_; }

 private:
  Carrier carrier_;
  ResourceOrigin source_;
  NativeTextMode text_mode_ = NativeTextMode::kUtf8;

  friend class Resources;
};

using NativeModelMember = NativeMember<Ftl>;
using NativeAnimationMember = NativeMember<Tea>;
using NativeLevelGeometryMember = NativeMember<Fts>;
using NativeLevelLightingMember = NativeMember<Llf>;
using NativeLevelSceneMember = NativeMember<Dlf>;
using NativeAmbianceMember = NativeMember<Amb>;
using NativeCinematicMember = NativeMember<Cin>;

class ModelNativeBundle {
 public:
  [[nodiscard]] const NativeModelMember& model() const noexcept { return model_; }
  [[nodiscard]] std::span<const NativeAnimationMember> animations() const noexcept { return animations_; }
  [[nodiscard]] const NativeResourceSet& resources() const noexcept { return resources_; }

 private:
  NativeModelMember model_;
  std::vector<NativeAnimationMember> animations_;
  NativeResourceSet resources_;
  friend class Resources;
};

class AnimationNativeBundle {
 public:
  [[nodiscard]] const NativeAnimationMember& animation() const noexcept { return animation_; }
  [[nodiscard]] const NativeResourceSet& resources() const noexcept { return resources_; }

 private:
  NativeAnimationMember animation_;
  NativeResourceSet resources_;
  friend class Resources;
};

class LevelNativeBundle {  // NOLINT(bugprone-exception-escape): MSVC debug STL misreports carrier container moves
 public:
  [[nodiscard]] const NativeLevelGeometryMember* geometry() const noexcept {
    return geometry_present_ ? &geometry_ : nullptr;
  }
  [[nodiscard]] const NativeLevelLightingMember* lighting() const noexcept {
    return lighting_present_ ? &lighting_ : nullptr;
  }
  [[nodiscard]] const NativeLevelSceneMember* scene() const noexcept { return scene_present_ ? &scene_ : nullptr; }
  [[nodiscard]] std::optional<std::uint32_t> levelIndex() const noexcept { return level_index_; }
  [[nodiscard]] std::optional<ArxVector2> minimapProjectionOffset() const noexcept {
    return minimap_projection_offset_;
  }
  [[nodiscard]] const NativeResourceSet& resources() const noexcept { return resources_; }

 private:
  NativeLevelGeometryMember geometry_;
  NativeLevelLightingMember lighting_;
  NativeLevelSceneMember scene_;
  bool geometry_present_ = false;
  bool lighting_present_ = false;
  bool scene_present_ = false;
  std::optional<std::uint32_t> level_index_;
  std::optional<ArxVector2> minimap_projection_offset_;
  NativeResourceSet resources_;
  friend class Resources;
};

class AmbianceNativeBundle {
 public:
  [[nodiscard]] const NativeAmbianceMember& ambiance() const noexcept { return ambiance_; }
  [[nodiscard]] const NativeResourceSet& resources() const noexcept { return resources_; }

 private:
  NativeAmbianceMember ambiance_;
  NativeResourceSet resources_;
  friend class Resources;
};

class CinematicNativeBundle {
 public:
  [[nodiscard]] const NativeCinematicMember& cinematic() const noexcept { return cinematic_; }
  [[nodiscard]] const NativeResourceSet& resources() const noexcept { return resources_; }

 private:
  NativeCinematicMember cinematic_;
  NativeResourceSet resources_;
  friend class Resources;
};

struct NativeBundleLoadOptions {
  NativeTextMode native_text_mode = NativeTextMode::kAuto;
  bool suppress_related_resource_errors = false;
};

}  // namespace pistoris::resource_io
