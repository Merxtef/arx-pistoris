// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#pragma once

#include "arx_pistoris/ambiance/location.hpp"
#include "arx_pistoris/animation/location.hpp"
#include "arx_pistoris/base/location.hpp"
#include "arx_pistoris/cinematic/location.hpp"
#include "arx_pistoris/glb/location.hpp"
#include "arx_pistoris/json/location.hpp"
#include "arx_pistoris/level/location.hpp"
#include "arx_pistoris/model/location.hpp"
#include "arx_pistoris/model/obj_location.hpp"
#include "arx_pistoris/native/location.hpp"
#include "arx_pistoris/sound.hpp"

#include "api/strerror.h"
#include "utils/log.h"

#include <cstddef>
#include <format>
#include <new>
#include <optional>
#include <source_location>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <variant>

namespace pistoris::api_detail {
namespace result_failure_detail {

template <class Location, class Input>
std::optional<Location> storeLocation(Input&& input) {
  using Stored = std::remove_cvref_t<Input>;
  if constexpr (std::is_same_v<Stored, std::nullopt_t>) {
    return std::nullopt;
  } else if constexpr (std::is_same_v<Stored, std::optional<Location>>) {
    return std::forward<Input>(input);
  } else {
    static_assert(std::is_same_v<Stored, Location>);
    return std::optional<Location>(std::in_place, std::forward<Input>(input));
  }
}

inline ArxReturnCode normalizeFailureCode(ArxReturnCode code, std::source_location where) noexcept {
  if (code != ARX_OK) return code;
  log(ARX_LOG_DEBUG,
      "ARX_OK passed to failure construction at {}:{} in {}; using ARX_INTERNAL_ERROR",
      where.file_name(),
      where.line(),
      where.function_name());
  return ARX_INTERNAL_ERROR;
}

inline std::string_view elementName(LevelElement value) noexcept {
  switch (value) {
    case LevelElement::kResource:
      return "resource";
    case LevelElement::kMinimap:
      return "minimap";
    case LevelElement::kLoadingScreen:
      return "loading screen";
    case LevelElement::kVertex:
      return "vertex";
    case LevelElement::kFace:
      return "face";
    case LevelElement::kTexture:
      return "texture";
    case LevelElement::kRoom:
      return "room";
    case LevelElement::kPortal:
      return "portal";
    case LevelElement::kRoomDistance:
      return "room distance";
    case LevelElement::kNavSurfaceVertex:
      return "navigation vertex";
    case LevelElement::kNavSurfaceTriangle:
      return "navigation triangle";
    case LevelElement::kAnchor:
      return "anchor";
    case LevelElement::kAnchorConnection:
      return "anchor connection";
    case LevelElement::kLight:
      return "light";
    case LevelElement::kPlayerSpawn:
      return "player spawn";
    case LevelElement::kEntity:
      return "entity";
    case LevelElement::kFog:
      return "fog";
    case LevelElement::kZone:
      return "zone";
    case LevelElement::kZonePerimeterPoint:
      return "zone perimeter point";
    case LevelElement::kPath:
      return "path";
    case LevelElement::kPathNode:
      return "path node";
  }
  return "unknown Level element";
}

inline std::string_view elementName(ModelElement value) noexcept {
  switch (value) {
    case ModelElement::kResource:
      return "resource";
    case ModelElement::kInventoryIcon:
      return "inventory icon";
    case ModelElement::kVertex:
      return "vertex";
    case ModelElement::kFace:
      return "face";
    case ModelElement::kTexture:
      return "texture";
    case ModelElement::kBone:
      return "bone";
    case ModelElement::kActionPoint:
      return "action point";
    case ModelElement::kSelection:
      return "selection";
  }
  return "unknown Model element";
}

inline std::string_view elementName(AnimationElement value) noexcept {
  switch (value) {
    case AnimationElement::kResource:
      return "resource";
    case AnimationElement::kKeyframe:
      return "keyframe";
    case AnimationElement::kGroup:
      return "group";
    case AnimationElement::kGroupTransform:
      return "group transform";
    case AnimationElement::kSound:
      return "sound";
  }
  return "unknown Animation element";
}

inline std::string_view elementName(AmbianceElement value) noexcept {
  switch (value) {
    case AmbianceElement::kResource:
      return "resource";
    case AmbianceElement::kTrack:
      return "track";
    case AmbianceElement::kKey:
      return "key";
    case AmbianceElement::kSound:
      return "sound";
  }
  return "unknown Ambiance element";
}

inline std::string_view elementName(CinematicElement value) noexcept {
  switch (value) {
    case CinematicElement::kResource:
      return "resource";
    case CinematicElement::kTimeline:
      return "timeline";
    case CinematicElement::kIllustration:
      return "illustration";
    case CinematicElement::kKeyframe:
      return "keyframe";
    case CinematicElement::kTexture:
      return "texture";
    case CinematicElement::kSound:
      return "sound";
    case CinematicElement::kLanguage:
      return "language";
    case CinematicElement::kSoundEncoding:
      return "sound encoding";
  }
  return "unknown Cinematic element";
}

inline std::string_view elementName(GlbElement value) noexcept {
  switch (value) {
    case GlbElement::kDocument:
      return "document";
    case GlbElement::kScene:
      return "scene";
    case GlbElement::kNode:
      return "node";
    case GlbElement::kMesh:
      return "mesh";
    case GlbElement::kPrimitive:
      return "primitive";
    case GlbElement::kAccessor:
      return "accessor";
    case GlbElement::kMaterial:
      return "material";
    case GlbElement::kTexture:
      return "texture";
    case GlbElement::kImage:
      return "image";
    case GlbElement::kSkin:
      return "skin";
    case GlbElement::kAnimation:
      return "animation";
    case GlbElement::kChannel:
      return "channel";
    case GlbElement::kSampler:
      return "sampler";
  }
  return "unknown GLB element";
}

inline std::string_view elementName(AmbElement value) noexcept {
  switch (value) {
    case AmbElement::kHeader:
      return "AMB header";
    case AmbElement::kTrack:
      return "AMB track";
    case AmbElement::kKey:
      return "AMB key";
    case AmbElement::kSound:
      return "AMB sound";
  }
  return "unknown AMB element";
}

inline std::string_view elementName(CinElement value) noexcept {
  switch (value) {
    case CinElement::kHeader:
      return "CIN header";
    case CinElement::kIllustration:
      return "CIN illustration";
    case CinElement::kKeyframe:
      return "CIN keyframe";
    case CinElement::kSound:
      return "CIN sound";
  }
  return "unknown CIN element";
}

inline std::string_view elementName(DlfElement value) noexcept {
  switch (value) {
    case DlfElement::kHeader:
      return "DLF header";
    case DlfElement::kEntity:
      return "DLF entity";
    case DlfElement::kFog:
      return "DLF fog";
    case DlfElement::kZone:
      return "DLF zone";
    case DlfElement::kZonePoint:
      return "DLF zone point";
    case DlfElement::kPath:
      return "DLF path";
    case DlfElement::kPathNode:
      return "DLF path node";
  }
  return "unknown DLF element";
}

inline std::string_view elementName(FtlElement value) noexcept {
  switch (value) {
    case FtlElement::kHeader:
      return "FTL header";
    case FtlElement::kVertex:
      return "FTL vertex";
    case FtlElement::kFace:
      return "FTL face";
    case FtlElement::kTexture:
      return "FTL texture";
    case FtlElement::kBone:
      return "FTL bone";
    case FtlElement::kActionPoint:
      return "FTL action point";
    case FtlElement::kSelection:
      return "FTL selection";
  }
  return "unknown FTL element";
}

inline std::string_view elementName(FtsElement value) noexcept {
  switch (value) {
    case FtsElement::kHeader:
      return "FTS header";
    case FtsElement::kCell:
      return "FTS cell";
    case FtsElement::kVertex:
      return "FTS vertex";
    case FtsElement::kFace:
      return "FTS face";
    case FtsElement::kTexture:
      return "FTS texture";
    case FtsElement::kRoom:
      return "FTS room";
    case FtsElement::kPortal:
      return "FTS portal";
    case FtsElement::kRoomDistance:
      return "FTS room distance";
    case FtsElement::kAnchor:
      return "FTS anchor";
    case FtsElement::kAnchorConnection:
      return "FTS anchor connection";
  }
  return "unknown FTS element";
}

inline std::string_view elementName(LlfElement value) noexcept {
  switch (value) {
    case LlfElement::kHeader:
      return "LLF header";
    case LlfElement::kLight:
      return "LLF light";
    case LlfElement::kVertexColor:
      return "LLF vertex color";
  }
  return "unknown LLF element";
}

inline std::string_view elementName(TeaElement value) noexcept {
  switch (value) {
    case TeaElement::kHeader:
      return "TEA header";
    case TeaElement::kKeyframe:
      return "TEA keyframe";
    case TeaElement::kGroupTransform:
      return "TEA group transform";
    case TeaElement::kSound:
      return "TEA sound";
  }
  return "unknown TEA element";
}

template <class Element>
void appendResourceLocation(std::string& out, const ResourceLocation<Element>& location,
                            std::string_view element_name) {
  out += std::format(" at {}", element_name);
  if (location.index != kNoElementIndex) out += std::format("[{}]", location.index);
  if (location.subindex != kNoElementIndex) out += std::format("[{}]", location.subindex);
  if (!location.label.empty()) out += std::format(" '{}'", location.label);
  if (!location.resource_path.empty()) out += std::format(" in '{}'", location.resource_path);
  if (location.input_index != kNoInputIndex) out += std::format(" (input {})", location.input_index);
}

inline void appendCinematicLocation(std::string& out, const CinematicLocation& location) {
  const std::string_view element_name = elementName(location.element);
  const bool sound_element =
      location.element == CinematicElement::kSound || location.element == CinematicElement::kSoundEncoding;
  const bool language_element = location.element == CinematicElement::kLanguage;
  SoundKind sound_kind = SoundKind::kEffect;
  SoundIndex sound_index = kNoSound;
  const bool valid_sound = sound_element && location.sound_handle != kNoSoundHandle &&
                           soundHandleKind(location.sound_handle, sound_kind) == ARX_OK &&
                           soundHandleIndex(location.sound_handle, sound_index) == ARX_OK;

  if (language_element && location.language_id != kInvalidLanguageId) {
    out += std::format(" at language[{}]", location.language_id);
  } else if (valid_sound) {
    out +=
        std::format(" at {} {}[{}]", sound_kind == SoundKind::kSpeech ? "speech" : "effect", element_name, sound_index);
  } else {
    out += std::format(" at {}", element_name);
    if (sound_element && location.sound_handle != kNoSoundHandle)
      out += std::format(" for sound handle {}", location.sound_handle);
    else if (location.index != kNoElementIndex)
      out += std::format("[{}]", location.index);
  }
  if (location.subindex != kNoElementIndex) out += std::format("[{}]", location.subindex);
  if (!language_element && location.language_id != kInvalidLanguageId)
    out += std::format(" language[{}]", location.language_id);
  if (!location.label.empty()) out += std::format(" '{}'", location.label);
  if (!location.resource_path.empty()) out += std::format(" in '{}'", location.resource_path);
  if (location.input_index != kNoInputIndex) out += std::format(" (input {})", location.input_index);
}

template <class Element>
void appendNativeLocation(std::string& out, const NativeLocation<Element>& location, std::string_view element_name) {
  out += std::format(" at {}", element_name);
  if (location.index != kNoElementIndex) out += std::format("[{}]", location.index);
  if (location.subindex != kNoElementIndex) out += std::format("[{}]", location.subindex);
  if (!location.field.empty()) out += std::format(" field '{}'", location.field);
}

template <class Element>
void appendNativeBinaryLocation(std::string& out, const NativeBinaryLocation<Element>& location,
                                std::string_view element_name) {
  out += std::format(" at {}", element_name);
  if (location.index != kNoElementIndex) out += std::format("[{}]", location.index);
  if (location.subindex != kNoElementIndex) out += std::format("[{}]", location.subindex);
  if (!location.field.empty()) out += std::format(" field '{}'", location.field);
  out += location.region == NativeBinaryRegion::kStored ? " in stored bytes" : " in decoded payload";
  if (location.byte_offset != kNoElementIndex) {
    out += std::format(" at byte {}", location.byte_offset);
    if (location.requested_bytes != 0) out += std::format(" (needed {})", location.requested_bytes);
  }
}

inline void appendJsonLocation(std::string& out, const JsonLocation& location) {
  out += " in JSON";
  if (!location.pointer.empty()) out += std::format(" at '{}'", location.pointer);
  if (location.byte_offset != kNoElementIndex) out += std::format(" at byte {}", location.byte_offset);
}

inline void appendGlbLocation(std::string& out, const GlbLocation& location) {
  if (location.element == GlbElement::kPrimitive && location.index != kNoElementIndex) {
    out += std::format(" at GLB mesh[{}]", location.index);
    if (location.subindex != kNoElementIndex) out += std::format(" primitive[{}]", location.subindex);
  } else if ((location.element == GlbElement::kChannel || location.element == GlbElement::kSampler) &&
             location.index != kNoElementIndex) {
    out += std::format(" at GLB animation[{}]", location.index);
    if (location.subindex != kNoElementIndex)
      out += std::format(" {}[{}]", elementName(location.element), location.subindex);
  } else {
    out += std::format(" at GLB {}", elementName(location.element));
    if (location.index != kNoElementIndex) out += std::format("[{}]", location.index);
    if (location.subindex != kNoElementIndex) out += std::format("[{}]", location.subindex);
  }
  if (!location.label.empty()) out += std::format(" '{}'", location.label);
  if (!location.property.empty()) out += std::format(" property '{}'", location.property);
}

inline void appendObjLocation(std::string& out, const ObjLocation& location) {
  out += location.source == ObjSource::kObj ? " at OBJ" : " at OBJ material library";
  if (location.source_index != kNoInputIndex) out += std::format("[{}]", location.source_index);
  if (!location.source_path.empty()) out += std::format(" '{}'", location.source_path);
  if (location.line != 0) out += std::format(":{}", location.line);
}

template <class Location, class Append>
std::string describeErrorImpl(const Error<Location>& error, Append&& append) {
  std::string message = std::format("{} (code {})", arx_pistoris_strerror(error.code()), error.code());
  if (error.location()) std::forward<Append>(append)(message, *error.location());
  if (!error.detail().empty()) message += std::format(": {}", error.detail());
  return message;
}

#define ARX_DEFINE_RESOURCE_ERROR_DESCRIPTION(Location)                                  \
  inline std::string describeError(const Error<Location>& error) {                       \
    return describeErrorImpl(error, [](std::string& message, const Location& location) { \
      appendResourceLocation(message, location, elementName(location.element));          \
    });                                                                                  \
  }

ARX_DEFINE_RESOURCE_ERROR_DESCRIPTION(LevelLocation)
ARX_DEFINE_RESOURCE_ERROR_DESCRIPTION(ModelLocation)
ARX_DEFINE_RESOURCE_ERROR_DESCRIPTION(AnimationLocation)
ARX_DEFINE_RESOURCE_ERROR_DESCRIPTION(AmbianceLocation)

#undef ARX_DEFINE_RESOURCE_ERROR_DESCRIPTION

inline std::string describeError(const Error<CinematicLocation>& error) {
  return describeErrorImpl(error, [](std::string& message, const CinematicLocation& location) {
    appendCinematicLocation(message, location);
  });
}

inline std::string describeError(const Error<GlbLocation>& error) {
  return describeErrorImpl(
      error, [](std::string& message, const GlbLocation& location) { appendGlbLocation(message, location); });
}

inline std::string describeError(const Error<ObjLocation>& error) {
  return describeErrorImpl(
      error, [](std::string& message, const ObjLocation& location) { appendObjLocation(message, location); });
}

#define ARX_DEFINE_NATIVE_ERROR_DESCRIPTION(Location)                                    \
  inline std::string describeError(const Error<Location>& error) {                       \
    return describeErrorImpl(error, [](std::string& message, const Location& location) { \
      appendNativeLocation(message, location, elementName(location.element));            \
    });                                                                                  \
  }

ARX_DEFINE_NATIVE_ERROR_DESCRIPTION(AmbLocation)
ARX_DEFINE_NATIVE_ERROR_DESCRIPTION(CinLocation)
ARX_DEFINE_NATIVE_ERROR_DESCRIPTION(DlfLocation)
ARX_DEFINE_NATIVE_ERROR_DESCRIPTION(FtlLocation)
ARX_DEFINE_NATIVE_ERROR_DESCRIPTION(FtsLocation)
ARX_DEFINE_NATIVE_ERROR_DESCRIPTION(LlfLocation)
ARX_DEFINE_NATIVE_ERROR_DESCRIPTION(TeaLocation)

#undef ARX_DEFINE_NATIVE_ERROR_DESCRIPTION

inline std::string describeError(const Error<LevelNativeLocation>& error) {
  return describeErrorImpl(error, [](std::string& message, const LevelNativeLocation& location) {
    std::visit([&](const auto& stored) { appendNativeLocation(message, stored, elementName(stored.element)); },
               location);
  });
}

inline std::string describeError(const Error<DlfWriteLocation>& error) {
  return describeErrorImpl(error, [](std::string& message, const DlfWriteLocation& location) {
    std::visit([&](const auto& stored) { appendNativeLocation(message, stored, elementName(stored.element)); },
               location);
  });
}

inline std::string describeError(const Error<JsonLocation>& error) {
  return describeErrorImpl(
      error, [](std::string& message, const JsonLocation& location) { appendJsonLocation(message, location); });
}

#define ARX_DEFINE_NATIVE_BINARY_ERROR_DESCRIPTION(Location)                             \
  inline std::string describeError(const Error<Location>& error) {                       \
    return describeErrorImpl(error, [](std::string& message, const Location& location) { \
      appendNativeBinaryLocation(message, location, elementName(location.element));      \
    });                                                                                  \
  }

ARX_DEFINE_NATIVE_BINARY_ERROR_DESCRIPTION(AmbBinaryLocation)
ARX_DEFINE_NATIVE_BINARY_ERROR_DESCRIPTION(CinBinaryLocation)
ARX_DEFINE_NATIVE_BINARY_ERROR_DESCRIPTION(FtlBinaryLocation)
ARX_DEFINE_NATIVE_BINARY_ERROR_DESCRIPTION(FtsBinaryLocation)
ARX_DEFINE_NATIVE_BINARY_ERROR_DESCRIPTION(LlfBinaryLocation)
ARX_DEFINE_NATIVE_BINARY_ERROR_DESCRIPTION(TeaBinaryLocation)

#undef ARX_DEFINE_NATIVE_BINARY_ERROR_DESCRIPTION

inline std::string describeError(const Error<DlfBinaryLocation>& error) {
  return describeErrorImpl(error, [](std::string& message, const DlfBinaryLocation& location) {
    std::visit([&](auto element) { appendNativeBinaryLocation(message, location, elementName(element)); },
               location.element);
  });
}

#define ARX_DEFINE_RESOURCE_VARIANT_ERROR_DESCRIPTION(Location)                                                     \
  inline std::string describeError(const Error<Location>& error) {                                                  \
    return describeErrorImpl(error, [](std::string& message, const Location& location) {                            \
      std::visit([&](const auto& stored) { appendResourceLocation(message, stored, elementName(stored.element)); }, \
                 location);                                                                                         \
    });                                                                                                             \
  }

ARX_DEFINE_RESOURCE_VARIANT_ERROR_DESCRIPTION(LevelGlbExportLocation)
ARX_DEFINE_RESOURCE_VARIANT_ERROR_DESCRIPTION(ModelGlbExportLocation)
ARX_DEFINE_RESOURCE_VARIANT_ERROR_DESCRIPTION(AmbianceGlbExportLocation)

#undef ARX_DEFINE_RESOURCE_VARIANT_ERROR_DESCRIPTION

template <class Location>
std::string sourceFailureMessage(std::string_view operation, const Error<Location>& error, std::source_location where) {
  return std::format("{} source failure: {} at {}:{} in {}",
                     operation,
                     describeError(error),
                     where.file_name(),
                     where.line(),
                     where.function_name());
}

inline void logFallback(std::string_view operation, ArxReturnCode code) noexcept {
  log(ARX_LOG_DEBUG, "{} source failure: {} (code {})", operation, arx_pistoris_strerror(code), code);
}

}  // namespace result_failure_detail

template <class T, class LocationInput, class Detail = std::string_view>
[[nodiscard]] LevelResult<T> levelFailure(ArxReturnCode code, LocationInput&& location, Detail&& detail = {},
                                          std::string_view operation = "Level operation",
                                          std::source_location where = std::source_location::current()) noexcept {
  code = result_failure_detail::normalizeFailureCode(code, where);
  try {
    std::optional<LevelLocation> stored_location =
        result_failure_detail::storeLocation<LevelLocation>(std::forward<LocationInput>(location));
    LevelResult<T> result =
        LevelResult<T>::failure(code, std::move(stored_location), std::string(std::forward<Detail>(detail)));
    const auto* error = result.error();
    if (!error) {
      result_failure_detail::logFallback(operation, ARX_INTERNAL_ERROR);
      return LevelResult<T>::failure(ARX_INTERNAL_ERROR, std::nullopt);
    }
    logLazy(ARX_LOG_DEBUG, [&] { return result_failure_detail::sourceFailureMessage(operation, *error, where); });
    return result;
  } catch (const std::bad_alloc&) {
    result_failure_detail::logFallback(operation, ARX_BAD_ALLOC);
    return LevelResult<T>::failure(ARX_BAD_ALLOC, std::nullopt);
  } catch (...) {
    result_failure_detail::logFallback(operation, ARX_INTERNAL_ERROR);
    return LevelResult<T>::failure(ARX_INTERNAL_ERROR, std::nullopt);
  }
}

template <class T, class U>
[[nodiscard]] LevelResult<T> levelFailure(LevelResult<U>&& failure, std::string_view operation = "Level operation",
                                          std::source_location where = std::source_location::current()) noexcept {
  if (failure)
    return levelFailure<T>(ARX_INTERNAL_ERROR, std::nullopt, "expected a failed Level result", operation, where);
  return std::move(failure).template propagate<T>();
}

template <class T, class LocationInput, class Detail = std::string_view>
[[nodiscard]] ModelResult<T> modelFailure(ArxReturnCode code, LocationInput&& location, Detail&& detail = {},
                                          std::string_view operation = "Model operation",
                                          std::source_location where = std::source_location::current()) noexcept {
  code = result_failure_detail::normalizeFailureCode(code, where);
  try {
    std::optional<ModelLocation> stored_location =
        result_failure_detail::storeLocation<ModelLocation>(std::forward<LocationInput>(location));
    ModelResult<T> result =
        ModelResult<T>::failure(code, std::move(stored_location), std::string(std::forward<Detail>(detail)));
    const auto* error = result.error();
    if (!error) {
      result_failure_detail::logFallback(operation, ARX_INTERNAL_ERROR);
      return ModelResult<T>::failure(ARX_INTERNAL_ERROR, std::nullopt);
    }
    logLazy(ARX_LOG_DEBUG, [&] { return result_failure_detail::sourceFailureMessage(operation, *error, where); });
    return result;
  } catch (const std::bad_alloc&) {
    result_failure_detail::logFallback(operation, ARX_BAD_ALLOC);
    return ModelResult<T>::failure(ARX_BAD_ALLOC, std::nullopt);
  } catch (...) {
    result_failure_detail::logFallback(operation, ARX_INTERNAL_ERROR);
    return ModelResult<T>::failure(ARX_INTERNAL_ERROR, std::nullopt);
  }
}

template <class T, class U>
[[nodiscard]] ModelResult<T> modelFailure(ModelResult<U>&& failure, std::string_view operation = "Model operation",
                                          std::source_location where = std::source_location::current()) noexcept {
  if (failure)
    return modelFailure<T>(ARX_INTERNAL_ERROR, std::nullopt, "expected a failed Model result", operation, where);
  return std::move(failure).template propagate<T>();
}

template <class T, class LocationInput, class Detail = std::string_view>
[[nodiscard]] AnimationResult<T> animationFailure(
    ArxReturnCode code, LocationInput&& location, Detail&& detail = {},
    std::string_view operation = "Animation operation",
    std::source_location where = std::source_location::current()) noexcept {
  code = result_failure_detail::normalizeFailureCode(code, where);
  try {
    std::optional<AnimationLocation> stored_location =
        result_failure_detail::storeLocation<AnimationLocation>(std::forward<LocationInput>(location));
    AnimationResult<T> result =
        AnimationResult<T>::failure(code, std::move(stored_location), std::string(std::forward<Detail>(detail)));
    const auto* error = result.error();
    if (!error) {
      result_failure_detail::logFallback(operation, ARX_INTERNAL_ERROR);
      return AnimationResult<T>::failure(ARX_INTERNAL_ERROR, std::nullopt);
    }
    logLazy(ARX_LOG_DEBUG, [&] { return result_failure_detail::sourceFailureMessage(operation, *error, where); });
    return result;
  } catch (const std::bad_alloc&) {
    result_failure_detail::logFallback(operation, ARX_BAD_ALLOC);
    return AnimationResult<T>::failure(ARX_BAD_ALLOC, std::nullopt);
  } catch (...) {
    result_failure_detail::logFallback(operation, ARX_INTERNAL_ERROR);
    return AnimationResult<T>::failure(ARX_INTERNAL_ERROR, std::nullopt);
  }
}

template <class T, class U>
[[nodiscard]] AnimationResult<T> animationFailure(
    AnimationResult<U>&& failure, std::string_view operation = "Animation operation",
    std::source_location where = std::source_location::current()) noexcept {
  if (failure)
    return animationFailure<T>(
        ARX_INTERNAL_ERROR, std::nullopt, "expected a failed Animation result", operation, where);
  return std::move(failure).template propagate<T>();
}

template <class T, class LocationInput, class Detail = std::string_view>
[[nodiscard]] AmbianceResult<T> ambianceFailure(ArxReturnCode code, LocationInput&& location, Detail&& detail = {},
                                                std::string_view operation = "Ambiance operation",
                                                std::source_location where = std::source_location::current()) noexcept {
  code = result_failure_detail::normalizeFailureCode(code, where);
  try {
    std::optional<AmbianceLocation> stored_location =
        result_failure_detail::storeLocation<AmbianceLocation>(std::forward<LocationInput>(location));
    AmbianceResult<T> result =
        AmbianceResult<T>::failure(code, std::move(stored_location), std::string(std::forward<Detail>(detail)));
    const auto* error = result.error();
    if (!error) {
      result_failure_detail::logFallback(operation, ARX_INTERNAL_ERROR);
      return AmbianceResult<T>::failure(ARX_INTERNAL_ERROR, std::nullopt);
    }
    logLazy(ARX_LOG_DEBUG, [&] { return result_failure_detail::sourceFailureMessage(operation, *error, where); });
    return result;
  } catch (const std::bad_alloc&) {
    result_failure_detail::logFallback(operation, ARX_BAD_ALLOC);
    return AmbianceResult<T>::failure(ARX_BAD_ALLOC, std::nullopt);
  } catch (...) {
    result_failure_detail::logFallback(operation, ARX_INTERNAL_ERROR);
    return AmbianceResult<T>::failure(ARX_INTERNAL_ERROR, std::nullopt);
  }
}

template <class T, class U>
[[nodiscard]] AmbianceResult<T> ambianceFailure(AmbianceResult<U>&& failure,
                                                std::string_view operation = "Ambiance operation",
                                                std::source_location where = std::source_location::current()) noexcept {
  if (failure)
    return ambianceFailure<T>(ARX_INTERNAL_ERROR, std::nullopt, "expected a failed Ambiance result", operation, where);
  return std::move(failure).template propagate<T>();
}

template <class T, class LocationInput, class Detail = std::string_view>
[[nodiscard]] CinematicResult<T> cinematicFailure(
    ArxReturnCode code, LocationInput&& location, Detail&& detail = {},
    std::string_view operation = "Cinematic operation",
    std::source_location where = std::source_location::current()) noexcept {
  code = result_failure_detail::normalizeFailureCode(code, where);
  try {
    std::optional<CinematicLocation> stored_location =
        result_failure_detail::storeLocation<CinematicLocation>(std::forward<LocationInput>(location));
    CinematicResult<T> result =
        CinematicResult<T>::failure(code, std::move(stored_location), std::string(std::forward<Detail>(detail)));
    const auto* error = result.error();
    if (!error) {
      result_failure_detail::logFallback(operation, ARX_INTERNAL_ERROR);
      return CinematicResult<T>::failure(ARX_INTERNAL_ERROR, std::nullopt);
    }
    logLazy(ARX_LOG_DEBUG, [&] { return result_failure_detail::sourceFailureMessage(operation, *error, where); });
    return result;
  } catch (const std::bad_alloc&) {
    result_failure_detail::logFallback(operation, ARX_BAD_ALLOC);
    return CinematicResult<T>::failure(ARX_BAD_ALLOC, std::nullopt);
  } catch (...) {
    result_failure_detail::logFallback(operation, ARX_INTERNAL_ERROR);
    return CinematicResult<T>::failure(ARX_INTERNAL_ERROR, std::nullopt);
  }
}

template <class T, class U>
[[nodiscard]] CinematicResult<T> cinematicFailure(
    CinematicResult<U>&& failure, std::string_view operation = "Cinematic operation",
    std::source_location where = std::source_location::current()) noexcept {
  if (failure)
    return cinematicFailure<T>(
        ARX_INTERNAL_ERROR, std::nullopt, "expected a failed Cinematic result", operation, where);
  return std::move(failure).template propagate<T>();
}

template <class T, class LocationInput, class Detail = std::string_view>
[[nodiscard]] GlbResult<T> glbFailure(ArxReturnCode code, LocationInput&& location, Detail&& detail = {},
                                      std::string_view operation = "GLB import",
                                      std::source_location where = std::source_location::current()) noexcept {
  code = result_failure_detail::normalizeFailureCode(code, where);
  try {
    std::optional<GlbLocation> stored_location =
        result_failure_detail::storeLocation<GlbLocation>(std::forward<LocationInput>(location));
    GlbResult<T> result =
        GlbResult<T>::failure(code, std::move(stored_location), std::string(std::forward<Detail>(detail)));
    const auto* error = result.error();
    if (!error) {
      result_failure_detail::logFallback(operation, ARX_INTERNAL_ERROR);
      return GlbResult<T>::failure(ARX_INTERNAL_ERROR, std::nullopt);
    }
    logLazy(ARX_LOG_DEBUG, [&] { return result_failure_detail::sourceFailureMessage(operation, *error, where); });
    return result;
  } catch (const std::bad_alloc&) {
    result_failure_detail::logFallback(operation, ARX_BAD_ALLOC);
    return GlbResult<T>::failure(ARX_BAD_ALLOC, std::nullopt);
  } catch (...) {
    result_failure_detail::logFallback(operation, ARX_INTERNAL_ERROR);
    return GlbResult<T>::failure(ARX_INTERNAL_ERROR, std::nullopt);
  }
}

template <class T, class LocationInput, class Detail = std::string_view>
[[nodiscard]] ObjResult<T> objFailure(ArxReturnCode code, LocationInput&& location, Detail&& detail = {},
                                      std::string_view operation = "OBJ -> Model conversion",
                                      std::source_location where = std::source_location::current()) noexcept {
  code = result_failure_detail::normalizeFailureCode(code, where);
  try {
    std::optional<ObjLocation> stored_location =
        result_failure_detail::storeLocation<ObjLocation>(std::forward<LocationInput>(location));
    ObjResult<T> result =
        ObjResult<T>::failure(code, std::move(stored_location), std::string(std::forward<Detail>(detail)));
    const auto* error = result.error();
    if (!error) {
      result_failure_detail::logFallback(operation, ARX_INTERNAL_ERROR);
      return ObjResult<T>::failure(ARX_INTERNAL_ERROR, std::nullopt);
    }
    logLazy(ARX_LOG_DEBUG, [&] { return result_failure_detail::sourceFailureMessage(operation, *error, where); });
    return result;
  } catch (const std::bad_alloc&) {
    result_failure_detail::logFallback(operation, ARX_BAD_ALLOC);
    return ObjResult<T>::failure(ARX_BAD_ALLOC, std::nullopt);
  } catch (...) {
    result_failure_detail::logFallback(operation, ARX_INTERNAL_ERROR);
    return ObjResult<T>::failure(ARX_INTERNAL_ERROR, std::nullopt);
  }
}

template <class T, class LocationInput, class Detail = std::string_view>
[[nodiscard]] AmbResult<T> ambFailure(ArxReturnCode code, LocationInput&& location, Detail&& detail = {},
                                      std::string_view operation = "AMB -> Ambiance conversion",
                                      std::source_location where = std::source_location::current()) noexcept {
  code = result_failure_detail::normalizeFailureCode(code, where);
  try {
    std::optional<AmbLocation> stored_location =
        result_failure_detail::storeLocation<AmbLocation>(std::forward<LocationInput>(location));
    AmbResult<T> result =
        AmbResult<T>::failure(code, std::move(stored_location), std::string(std::forward<Detail>(detail)));
    const auto* error = result.error();
    if (!error) {
      result_failure_detail::logFallback(operation, ARX_INTERNAL_ERROR);
      return AmbResult<T>::failure(ARX_INTERNAL_ERROR, std::nullopt);
    }
    logLazy(ARX_LOG_DEBUG, [&] { return result_failure_detail::sourceFailureMessage(operation, *error, where); });
    return result;
  } catch (const std::bad_alloc&) {
    result_failure_detail::logFallback(operation, ARX_BAD_ALLOC);
    return AmbResult<T>::failure(ARX_BAD_ALLOC, std::nullopt);
  } catch (...) {
    result_failure_detail::logFallback(operation, ARX_INTERNAL_ERROR);
    return AmbResult<T>::failure(ARX_INTERNAL_ERROR, std::nullopt);
  }
}

template <class T, class LocationInput, class Detail = std::string_view>
[[nodiscard]] CinResult<T> cinFailure(ArxReturnCode code, LocationInput&& location, Detail&& detail = {},
                                      std::string_view operation = "CIN -> Cinematic conversion",
                                      std::source_location where = std::source_location::current()) noexcept {
  code = result_failure_detail::normalizeFailureCode(code, where);
  try {
    std::optional<CinLocation> stored_location =
        result_failure_detail::storeLocation<CinLocation>(std::forward<LocationInput>(location));
    CinResult<T> result =
        CinResult<T>::failure(code, std::move(stored_location), std::string(std::forward<Detail>(detail)));
    const auto* error = result.error();
    if (!error) {
      result_failure_detail::logFallback(operation, ARX_INTERNAL_ERROR);
      return CinResult<T>::failure(ARX_INTERNAL_ERROR, std::nullopt);
    }
    logLazy(ARX_LOG_DEBUG, [&] { return result_failure_detail::sourceFailureMessage(operation, *error, where); });
    return result;
  } catch (const std::bad_alloc&) {
    result_failure_detail::logFallback(operation, ARX_BAD_ALLOC);
    return CinResult<T>::failure(ARX_BAD_ALLOC, std::nullopt);
  } catch (...) {
    result_failure_detail::logFallback(operation, ARX_INTERNAL_ERROR);
    return CinResult<T>::failure(ARX_INTERNAL_ERROR, std::nullopt);
  }
}

template <class T, class LocationInput, class Detail = std::string_view>
[[nodiscard]] FtlResult<T> ftlFailure(ArxReturnCode code, LocationInput&& location, Detail&& detail = {},
                                      std::string_view operation = "FTL -> Model conversion",
                                      std::source_location where = std::source_location::current()) noexcept {
  code = result_failure_detail::normalizeFailureCode(code, where);
  try {
    std::optional<FtlLocation> stored_location =
        result_failure_detail::storeLocation<FtlLocation>(std::forward<LocationInput>(location));
    FtlResult<T> result =
        FtlResult<T>::failure(code, std::move(stored_location), std::string(std::forward<Detail>(detail)));
    const auto* error = result.error();
    if (!error) {
      result_failure_detail::logFallback(operation, ARX_INTERNAL_ERROR);
      return FtlResult<T>::failure(ARX_INTERNAL_ERROR, std::nullopt);
    }
    logLazy(ARX_LOG_DEBUG, [&] { return result_failure_detail::sourceFailureMessage(operation, *error, where); });
    return result;
  } catch (const std::bad_alloc&) {
    result_failure_detail::logFallback(operation, ARX_BAD_ALLOC);
    return FtlResult<T>::failure(ARX_BAD_ALLOC, std::nullopt);
  } catch (...) {
    result_failure_detail::logFallback(operation, ARX_INTERNAL_ERROR);
    return FtlResult<T>::failure(ARX_INTERNAL_ERROR, std::nullopt);
  }
}

template <class T, class LocationInput, class Detail = std::string_view>
[[nodiscard]] TeaResult<T> teaFailure(ArxReturnCode code, LocationInput&& location, Detail&& detail = {},
                                      std::string_view operation = "TEA -> Animation conversion",
                                      std::source_location where = std::source_location::current()) noexcept {
  code = result_failure_detail::normalizeFailureCode(code, where);
  try {
    std::optional<TeaLocation> stored_location =
        result_failure_detail::storeLocation<TeaLocation>(std::forward<LocationInput>(location));
    TeaResult<T> result =
        TeaResult<T>::failure(code, std::move(stored_location), std::string(std::forward<Detail>(detail)));
    const auto* error = result.error();
    if (!error) {
      result_failure_detail::logFallback(operation, ARX_INTERNAL_ERROR);
      return TeaResult<T>::failure(ARX_INTERNAL_ERROR, std::nullopt);
    }
    logLazy(ARX_LOG_DEBUG, [&] { return result_failure_detail::sourceFailureMessage(operation, *error, where); });
    return result;
  } catch (const std::bad_alloc&) {
    result_failure_detail::logFallback(operation, ARX_BAD_ALLOC);
    return TeaResult<T>::failure(ARX_BAD_ALLOC, std::nullopt);
  } catch (...) {
    result_failure_detail::logFallback(operation, ARX_INTERNAL_ERROR);
    return TeaResult<T>::failure(ARX_INTERNAL_ERROR, std::nullopt);
  }
}

template <class T, class LocationInput, class Detail = std::string_view>
[[nodiscard]] LevelNativeResult<T> levelNativeFailure(
    ArxReturnCode code, LocationInput&& location, Detail&& detail = {},
    std::string_view operation = "FTS + LLF + DLF -> Level conversion",
    std::source_location where = std::source_location::current()) noexcept {
  code = result_failure_detail::normalizeFailureCode(code, where);
  try {
    std::optional<LevelNativeLocation> stored_location =
        result_failure_detail::storeLocation<LevelNativeLocation>(std::forward<LocationInput>(location));
    LevelNativeResult<T> result =
        LevelNativeResult<T>::failure(code, std::move(stored_location), std::string(std::forward<Detail>(detail)));
    const auto* error = result.error();
    if (!error) {
      result_failure_detail::logFallback(operation, ARX_INTERNAL_ERROR);
      return LevelNativeResult<T>::failure(ARX_INTERNAL_ERROR, std::nullopt);
    }
    logLazy(ARX_LOG_DEBUG, [&] { return result_failure_detail::sourceFailureMessage(operation, *error, where); });
    return result;
  } catch (const std::bad_alloc&) {
    result_failure_detail::logFallback(operation, ARX_BAD_ALLOC);
    return LevelNativeResult<T>::failure(ARX_BAD_ALLOC, std::nullopt);
  } catch (...) {
    result_failure_detail::logFallback(operation, ARX_INTERNAL_ERROR);
    return LevelNativeResult<T>::failure(ARX_INTERNAL_ERROR, std::nullopt);
  }
}

template <class T, class LocationInput, class Detail = std::string_view>
[[nodiscard]] DlfResult<T> dlfFailure(ArxReturnCode code, LocationInput&& location, Detail&& detail = {},
                                      std::string_view operation = "DLF operation",
                                      std::source_location where = std::source_location::current()) noexcept {
  code = result_failure_detail::normalizeFailureCode(code, where);
  try {
    std::optional<DlfLocation> stored_location =
        result_failure_detail::storeLocation<DlfLocation>(std::forward<LocationInput>(location));
    auto result = DlfResult<T>::failure(code, std::move(stored_location), std::string(std::forward<Detail>(detail)));
    const auto* error = result.error();
    if (!error) {
      result_failure_detail::logFallback(operation, ARX_INTERNAL_ERROR);
      return DlfResult<T>::failure(ARX_INTERNAL_ERROR, std::nullopt);
    }
    logLazy(ARX_LOG_DEBUG, [&] { return result_failure_detail::sourceFailureMessage(operation, *error, where); });
    return result;
  } catch (const std::bad_alloc&) {
    result_failure_detail::logFallback(operation, ARX_BAD_ALLOC);
    return DlfResult<T>::failure(ARX_BAD_ALLOC, std::nullopt);
  } catch (...) {
    result_failure_detail::logFallback(operation, ARX_INTERNAL_ERROR);
    return DlfResult<T>::failure(ARX_INTERNAL_ERROR, std::nullopt);
  }
}

template <class T, class LocationInput, class Detail = std::string_view>
[[nodiscard]] FtsResult<T> ftsFailure(ArxReturnCode code, LocationInput&& location, Detail&& detail = {},
                                      std::string_view operation = "FTS operation",
                                      std::source_location where = std::source_location::current()) noexcept {
  code = result_failure_detail::normalizeFailureCode(code, where);
  try {
    std::optional<FtsLocation> stored_location =
        result_failure_detail::storeLocation<FtsLocation>(std::forward<LocationInput>(location));
    auto result = FtsResult<T>::failure(code, std::move(stored_location), std::string(std::forward<Detail>(detail)));
    const auto* error = result.error();
    if (!error) {
      result_failure_detail::logFallback(operation, ARX_INTERNAL_ERROR);
      return FtsResult<T>::failure(ARX_INTERNAL_ERROR, std::nullopt);
    }
    logLazy(ARX_LOG_DEBUG, [&] { return result_failure_detail::sourceFailureMessage(operation, *error, where); });
    return result;
  } catch (const std::bad_alloc&) {
    result_failure_detail::logFallback(operation, ARX_BAD_ALLOC);
    return FtsResult<T>::failure(ARX_BAD_ALLOC, std::nullopt);
  } catch (...) {
    result_failure_detail::logFallback(operation, ARX_INTERNAL_ERROR);
    return FtsResult<T>::failure(ARX_INTERNAL_ERROR, std::nullopt);
  }
}

template <class T, class LocationInput, class Detail = std::string_view>
[[nodiscard]] LlfResult<T> llfFailure(ArxReturnCode code, LocationInput&& location, Detail&& detail = {},
                                      std::string_view operation = "LLF operation",
                                      std::source_location where = std::source_location::current()) noexcept {
  code = result_failure_detail::normalizeFailureCode(code, where);
  try {
    std::optional<LlfLocation> stored_location =
        result_failure_detail::storeLocation<LlfLocation>(std::forward<LocationInput>(location));
    auto result = LlfResult<T>::failure(code, std::move(stored_location), std::string(std::forward<Detail>(detail)));
    const auto* error = result.error();
    if (!error) {
      result_failure_detail::logFallback(operation, ARX_INTERNAL_ERROR);
      return LlfResult<T>::failure(ARX_INTERNAL_ERROR, std::nullopt);
    }
    logLazy(ARX_LOG_DEBUG, [&] { return result_failure_detail::sourceFailureMessage(operation, *error, where); });
    return result;
  } catch (const std::bad_alloc&) {
    result_failure_detail::logFallback(operation, ARX_BAD_ALLOC);
    return LlfResult<T>::failure(ARX_BAD_ALLOC, std::nullopt);
  } catch (...) {
    result_failure_detail::logFallback(operation, ARX_INTERNAL_ERROR);
    return LlfResult<T>::failure(ARX_INTERNAL_ERROR, std::nullopt);
  }
}

template <class T, class LocationInput, class Detail = std::string_view>
[[nodiscard]] DlfWriteResult<T> dlfWriteFailure(ArxReturnCode code, LocationInput&& location, Detail&& detail = {},
                                                std::string_view operation = "DLF binary write",
                                                std::source_location where = std::source_location::current()) noexcept {
  code = result_failure_detail::normalizeFailureCode(code, where);
  try {
    std::optional<DlfWriteLocation> stored_location =
        result_failure_detail::storeLocation<DlfWriteLocation>(std::forward<LocationInput>(location));
    auto result =
        DlfWriteResult<T>::failure(code, std::move(stored_location), std::string(std::forward<Detail>(detail)));
    const auto* error = result.error();
    if (!error) {
      result_failure_detail::logFallback(operation, ARX_INTERNAL_ERROR);
      return DlfWriteResult<T>::failure(ARX_INTERNAL_ERROR, std::nullopt);
    }
    logLazy(ARX_LOG_DEBUG, [&] { return result_failure_detail::sourceFailureMessage(operation, *error, where); });
    return result;
  } catch (const std::bad_alloc&) {
    result_failure_detail::logFallback(operation, ARX_BAD_ALLOC);
    return DlfWriteResult<T>::failure(ARX_BAD_ALLOC, std::nullopt);
  } catch (...) {
    result_failure_detail::logFallback(operation, ARX_INTERNAL_ERROR);
    return DlfWriteResult<T>::failure(ARX_INTERNAL_ERROR, std::nullopt);
  }
}

template <class T, class LocationInput, class Detail = std::string_view>
[[nodiscard]] JsonResult<T> jsonFailure(ArxReturnCode code, LocationInput&& location, Detail&& detail = {},
                                        std::string_view operation = "JSON import",
                                        std::source_location where = std::source_location::current()) noexcept {
  code = result_failure_detail::normalizeFailureCode(code, where);
  try {
    std::optional<JsonLocation> stored_location =
        result_failure_detail::storeLocation<JsonLocation>(std::forward<LocationInput>(location));
    auto result = JsonResult<T>::failure(code, std::move(stored_location), std::string(std::forward<Detail>(detail)));
    const auto* error = result.error();
    if (!error) {
      result_failure_detail::logFallback(operation, ARX_INTERNAL_ERROR);
      return JsonResult<T>::failure(ARX_INTERNAL_ERROR, std::nullopt);
    }
    logLazy(ARX_LOG_DEBUG, [&] { return result_failure_detail::sourceFailureMessage(operation, *error, where); });
    return result;
  } catch (const std::bad_alloc&) {
    result_failure_detail::logFallback(operation, ARX_BAD_ALLOC);
    return JsonResult<T>::failure(ARX_BAD_ALLOC, std::nullopt);
  } catch (...) {
    result_failure_detail::logFallback(operation, ARX_INTERNAL_ERROR);
    return JsonResult<T>::failure(ARX_INTERNAL_ERROR, std::nullopt);
  }
}

template <class T, class LocationInput, class Detail = std::string_view>
[[nodiscard]] AmbBinaryResult<T> ambBinaryFailure(
    ArxReturnCode code, LocationInput&& location, Detail&& detail = {}, std::string_view operation = "AMB binary read",
    std::source_location where = std::source_location::current()) noexcept {
  code = result_failure_detail::normalizeFailureCode(code, where);
  try {
    std::optional<AmbBinaryLocation> stored_location =
        result_failure_detail::storeLocation<AmbBinaryLocation>(std::forward<LocationInput>(location));
    auto result =
        AmbBinaryResult<T>::failure(code, std::move(stored_location), std::string(std::forward<Detail>(detail)));
    const auto* error = result.error();
    if (!error) {
      result_failure_detail::logFallback(operation, ARX_INTERNAL_ERROR);
      return AmbBinaryResult<T>::failure(ARX_INTERNAL_ERROR, std::nullopt);
    }
    logLazy(ARX_LOG_DEBUG, [&] { return result_failure_detail::sourceFailureMessage(operation, *error, where); });
    return result;
  } catch (const std::bad_alloc&) {
    result_failure_detail::logFallback(operation, ARX_BAD_ALLOC);
    return AmbBinaryResult<T>::failure(ARX_BAD_ALLOC, std::nullopt);
  } catch (...) {
    result_failure_detail::logFallback(operation, ARX_INTERNAL_ERROR);
    return AmbBinaryResult<T>::failure(ARX_INTERNAL_ERROR, std::nullopt);
  }
}

template <class T, class LocationInput, class Detail = std::string_view>
[[nodiscard]] CinBinaryResult<T> cinBinaryFailure(
    ArxReturnCode code, LocationInput&& location, Detail&& detail = {}, std::string_view operation = "CIN binary read",
    std::source_location where = std::source_location::current()) noexcept {
  code = result_failure_detail::normalizeFailureCode(code, where);
  try {
    std::optional<CinBinaryLocation> stored_location =
        result_failure_detail::storeLocation<CinBinaryLocation>(std::forward<LocationInput>(location));
    auto result =
        CinBinaryResult<T>::failure(code, std::move(stored_location), std::string(std::forward<Detail>(detail)));
    const auto* error = result.error();
    if (!error) {
      result_failure_detail::logFallback(operation, ARX_INTERNAL_ERROR);
      return CinBinaryResult<T>::failure(ARX_INTERNAL_ERROR, std::nullopt);
    }
    logLazy(ARX_LOG_DEBUG, [&] { return result_failure_detail::sourceFailureMessage(operation, *error, where); });
    return result;
  } catch (const std::bad_alloc&) {
    result_failure_detail::logFallback(operation, ARX_BAD_ALLOC);
    return CinBinaryResult<T>::failure(ARX_BAD_ALLOC, std::nullopt);
  } catch (...) {
    result_failure_detail::logFallback(operation, ARX_INTERNAL_ERROR);
    return CinBinaryResult<T>::failure(ARX_INTERNAL_ERROR, std::nullopt);
  }
}

template <class T, class LocationInput, class Detail = std::string_view>
[[nodiscard]] DlfBinaryResult<T> dlfBinaryFailure(
    ArxReturnCode code, LocationInput&& location, Detail&& detail = {}, std::string_view operation = "DLF binary read",
    std::source_location where = std::source_location::current()) noexcept {
  code = result_failure_detail::normalizeFailureCode(code, where);
  try {
    std::optional<DlfBinaryLocation> stored_location =
        result_failure_detail::storeLocation<DlfBinaryLocation>(std::forward<LocationInput>(location));
    auto result =
        DlfBinaryResult<T>::failure(code, std::move(stored_location), std::string(std::forward<Detail>(detail)));
    const auto* error = result.error();
    if (!error) {
      result_failure_detail::logFallback(operation, ARX_INTERNAL_ERROR);
      return DlfBinaryResult<T>::failure(ARX_INTERNAL_ERROR, std::nullopt);
    }
    logLazy(ARX_LOG_DEBUG, [&] { return result_failure_detail::sourceFailureMessage(operation, *error, where); });
    return result;
  } catch (const std::bad_alloc&) {
    result_failure_detail::logFallback(operation, ARX_BAD_ALLOC);
    return DlfBinaryResult<T>::failure(ARX_BAD_ALLOC, std::nullopt);
  } catch (...) {
    result_failure_detail::logFallback(operation, ARX_INTERNAL_ERROR);
    return DlfBinaryResult<T>::failure(ARX_INTERNAL_ERROR, std::nullopt);
  }
}

template <class T, class LocationInput, class Detail = std::string_view>
[[nodiscard]] FtlBinaryResult<T> ftlBinaryFailure(
    ArxReturnCode code, LocationInput&& location, Detail&& detail = {}, std::string_view operation = "FTL binary read",
    std::source_location where = std::source_location::current()) noexcept {
  code = result_failure_detail::normalizeFailureCode(code, where);
  try {
    std::optional<FtlBinaryLocation> stored_location =
        result_failure_detail::storeLocation<FtlBinaryLocation>(std::forward<LocationInput>(location));
    auto result =
        FtlBinaryResult<T>::failure(code, std::move(stored_location), std::string(std::forward<Detail>(detail)));
    const auto* error = result.error();
    if (!error) {
      result_failure_detail::logFallback(operation, ARX_INTERNAL_ERROR);
      return FtlBinaryResult<T>::failure(ARX_INTERNAL_ERROR, std::nullopt);
    }
    logLazy(ARX_LOG_DEBUG, [&] { return result_failure_detail::sourceFailureMessage(operation, *error, where); });
    return result;
  } catch (const std::bad_alloc&) {
    result_failure_detail::logFallback(operation, ARX_BAD_ALLOC);
    return FtlBinaryResult<T>::failure(ARX_BAD_ALLOC, std::nullopt);
  } catch (...) {
    result_failure_detail::logFallback(operation, ARX_INTERNAL_ERROR);
    return FtlBinaryResult<T>::failure(ARX_INTERNAL_ERROR, std::nullopt);
  }
}

template <class T, class LocationInput, class Detail = std::string_view>
[[nodiscard]] FtsBinaryResult<T> ftsBinaryFailure(
    ArxReturnCode code, LocationInput&& location, Detail&& detail = {}, std::string_view operation = "FTS binary read",
    std::source_location where = std::source_location::current()) noexcept {
  code = result_failure_detail::normalizeFailureCode(code, where);
  try {
    std::optional<FtsBinaryLocation> stored_location =
        result_failure_detail::storeLocation<FtsBinaryLocation>(std::forward<LocationInput>(location));
    auto result =
        FtsBinaryResult<T>::failure(code, std::move(stored_location), std::string(std::forward<Detail>(detail)));
    const auto* error = result.error();
    if (!error) {
      result_failure_detail::logFallback(operation, ARX_INTERNAL_ERROR);
      return FtsBinaryResult<T>::failure(ARX_INTERNAL_ERROR, std::nullopt);
    }
    logLazy(ARX_LOG_DEBUG, [&] { return result_failure_detail::sourceFailureMessage(operation, *error, where); });
    return result;
  } catch (const std::bad_alloc&) {
    result_failure_detail::logFallback(operation, ARX_BAD_ALLOC);
    return FtsBinaryResult<T>::failure(ARX_BAD_ALLOC, std::nullopt);
  } catch (...) {
    result_failure_detail::logFallback(operation, ARX_INTERNAL_ERROR);
    return FtsBinaryResult<T>::failure(ARX_INTERNAL_ERROR, std::nullopt);
  }
}

template <class T, class LocationInput, class Detail = std::string_view>
[[nodiscard]] LlfBinaryResult<T> llfBinaryFailure(
    ArxReturnCode code, LocationInput&& location, Detail&& detail = {}, std::string_view operation = "LLF binary read",
    std::source_location where = std::source_location::current()) noexcept {
  code = result_failure_detail::normalizeFailureCode(code, where);
  try {
    std::optional<LlfBinaryLocation> stored_location =
        result_failure_detail::storeLocation<LlfBinaryLocation>(std::forward<LocationInput>(location));
    auto result =
        LlfBinaryResult<T>::failure(code, std::move(stored_location), std::string(std::forward<Detail>(detail)));
    const auto* error = result.error();
    if (!error) {
      result_failure_detail::logFallback(operation, ARX_INTERNAL_ERROR);
      return LlfBinaryResult<T>::failure(ARX_INTERNAL_ERROR, std::nullopt);
    }
    logLazy(ARX_LOG_DEBUG, [&] { return result_failure_detail::sourceFailureMessage(operation, *error, where); });
    return result;
  } catch (const std::bad_alloc&) {
    result_failure_detail::logFallback(operation, ARX_BAD_ALLOC);
    return LlfBinaryResult<T>::failure(ARX_BAD_ALLOC, std::nullopt);
  } catch (...) {
    result_failure_detail::logFallback(operation, ARX_INTERNAL_ERROR);
    return LlfBinaryResult<T>::failure(ARX_INTERNAL_ERROR, std::nullopt);
  }
}

template <class T, class LocationInput, class Detail = std::string_view>
[[nodiscard]] TeaBinaryResult<T> teaBinaryFailure(
    ArxReturnCode code, LocationInput&& location, Detail&& detail = {}, std::string_view operation = "TEA binary read",
    std::source_location where = std::source_location::current()) noexcept {
  code = result_failure_detail::normalizeFailureCode(code, where);
  try {
    std::optional<TeaBinaryLocation> stored_location =
        result_failure_detail::storeLocation<TeaBinaryLocation>(std::forward<LocationInput>(location));
    auto result =
        TeaBinaryResult<T>::failure(code, std::move(stored_location), std::string(std::forward<Detail>(detail)));
    const auto* error = result.error();
    if (!error) {
      result_failure_detail::logFallback(operation, ARX_INTERNAL_ERROR);
      return TeaBinaryResult<T>::failure(ARX_INTERNAL_ERROR, std::nullopt);
    }
    logLazy(ARX_LOG_DEBUG, [&] { return result_failure_detail::sourceFailureMessage(operation, *error, where); });
    return result;
  } catch (const std::bad_alloc&) {
    result_failure_detail::logFallback(operation, ARX_BAD_ALLOC);
    return TeaBinaryResult<T>::failure(ARX_BAD_ALLOC, std::nullopt);
  } catch (...) {
    result_failure_detail::logFallback(operation, ARX_INTERNAL_ERROR);
    return TeaBinaryResult<T>::failure(ARX_INTERNAL_ERROR, std::nullopt);
  }
}

template <class T, class LocationInput, class Detail = std::string_view>
[[nodiscard]] LevelGlbExportResult<T> levelGlbExportFailure(
    ArxReturnCode code, LocationInput&& location, Detail&& detail = {},
    std::string_view operation = "Level -> GLB conversion",
    std::source_location where = std::source_location::current()) noexcept {
  code = result_failure_detail::normalizeFailureCode(code, where);
  try {
    std::optional<LevelGlbExportLocation> stored_location =
        result_failure_detail::storeLocation<LevelGlbExportLocation>(std::forward<LocationInput>(location));
    LevelGlbExportResult<T> result =
        LevelGlbExportResult<T>::failure(code, std::move(stored_location), std::string(std::forward<Detail>(detail)));
    const auto* error = result.error();
    if (!error) {
      result_failure_detail::logFallback(operation, ARX_INTERNAL_ERROR);
      return LevelGlbExportResult<T>::failure(ARX_INTERNAL_ERROR, std::nullopt);
    }
    logLazy(ARX_LOG_DEBUG, [&] { return result_failure_detail::sourceFailureMessage(operation, *error, where); });
    return result;
  } catch (const std::bad_alloc&) {
    result_failure_detail::logFallback(operation, ARX_BAD_ALLOC);
    return LevelGlbExportResult<T>::failure(ARX_BAD_ALLOC, std::nullopt);
  } catch (...) {
    result_failure_detail::logFallback(operation, ARX_INTERNAL_ERROR);
    return LevelGlbExportResult<T>::failure(ARX_INTERNAL_ERROR, std::nullopt);
  }
}

template <class T, class LocationInput, class Detail = std::string_view>
[[nodiscard]] ModelGlbExportResult<T> modelGlbExportFailure(
    ArxReturnCode code, LocationInput&& location, Detail&& detail = {},
    std::string_view operation = "Model -> GLB conversion",
    std::source_location where = std::source_location::current()) noexcept {
  code = result_failure_detail::normalizeFailureCode(code, where);
  try {
    std::optional<ModelGlbExportLocation> stored_location =
        result_failure_detail::storeLocation<ModelGlbExportLocation>(std::forward<LocationInput>(location));
    ModelGlbExportResult<T> result =
        ModelGlbExportResult<T>::failure(code, std::move(stored_location), std::string(std::forward<Detail>(detail)));
    const auto* error = result.error();
    if (!error) {
      result_failure_detail::logFallback(operation, ARX_INTERNAL_ERROR);
      return ModelGlbExportResult<T>::failure(ARX_INTERNAL_ERROR, std::nullopt);
    }
    logLazy(ARX_LOG_DEBUG, [&] { return result_failure_detail::sourceFailureMessage(operation, *error, where); });
    return result;
  } catch (const std::bad_alloc&) {
    result_failure_detail::logFallback(operation, ARX_BAD_ALLOC);
    return ModelGlbExportResult<T>::failure(ARX_BAD_ALLOC, std::nullopt);
  } catch (...) {
    result_failure_detail::logFallback(operation, ARX_INTERNAL_ERROR);
    return ModelGlbExportResult<T>::failure(ARX_INTERNAL_ERROR, std::nullopt);
  }
}

template <class T, class LocationInput, class Detail = std::string_view>
[[nodiscard]] AmbianceGlbExportResult<T> ambianceGlbExportFailure(
    ArxReturnCode code, LocationInput&& location, Detail&& detail = {},
    std::string_view operation = "Ambiance -> GLB conversion",
    std::source_location where = std::source_location::current()) noexcept {
  code = result_failure_detail::normalizeFailureCode(code, where);
  try {
    std::optional<AmbianceGlbExportLocation> stored_location =
        result_failure_detail::storeLocation<AmbianceGlbExportLocation>(std::forward<LocationInput>(location));
    AmbianceGlbExportResult<T> result = AmbianceGlbExportResult<T>::failure(
        code, std::move(stored_location), std::string(std::forward<Detail>(detail)));
    const auto* error = result.error();
    if (!error) {
      result_failure_detail::logFallback(operation, ARX_INTERNAL_ERROR);
      return AmbianceGlbExportResult<T>::failure(ARX_INTERNAL_ERROR, std::nullopt);
    }
    logLazy(ARX_LOG_DEBUG, [&] { return result_failure_detail::sourceFailureMessage(operation, *error, where); });
    return result;
  } catch (const std::bad_alloc&) {
    result_failure_detail::logFallback(operation, ARX_BAD_ALLOC);
    return AmbianceGlbExportResult<T>::failure(ARX_BAD_ALLOC, std::nullopt);
  } catch (...) {
    result_failure_detail::logFallback(operation, ARX_INTERNAL_ERROR);
    return AmbianceGlbExportResult<T>::failure(ARX_INTERNAL_ERROR, std::nullopt);
  }
}

}  // namespace pistoris::api_detail
