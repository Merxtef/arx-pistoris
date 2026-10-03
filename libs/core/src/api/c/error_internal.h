// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#pragma once

#include "arx_pistoris/ambiance/location.hpp"
#include "arx_pistoris/animation/location.hpp"
#include "arx_pistoris/base/error.h"
#include "arx_pistoris/base/result.hpp"
#include "arx_pistoris/cinematic/location.hpp"
#include "arx_pistoris/glb/location.hpp"
#include "arx_pistoris/json/location.hpp"
#include "arx_pistoris/level/location.hpp"
#include "arx_pistoris/model/location.hpp"
#include "arx_pistoris/model/obj_location.hpp"
#include "arx_pistoris/native/location.hpp"

#include "api/result_failure.h"

#include <memory>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <variant>

namespace pistoris::c_api {

struct ErrorBacking {
  std::string element_name;
  std::string resource_path;
  std::string label;
  std::string property;
  std::string source_path;
  std::string field;
  std::string json_pointer;
  std::string detail;
};

#define ARX_C_ERROR_ELEMENT_CASE(cpp_value, c_value) \
  case cpp_value:                                    \
    return c_value

constexpr ArxErrorElement cErrorElement(LevelElement element) noexcept {
  switch (element) {
    ARX_C_ERROR_ELEMENT_CASE(LevelElement::kResource, ARX_ERROR_ELEMENT_LEVEL_RESOURCE);
    ARX_C_ERROR_ELEMENT_CASE(LevelElement::kMinimap, ARX_ERROR_ELEMENT_LEVEL_MINIMAP);
    ARX_C_ERROR_ELEMENT_CASE(LevelElement::kLoadingScreen, ARX_ERROR_ELEMENT_LEVEL_LOADING_SCREEN);
    ARX_C_ERROR_ELEMENT_CASE(LevelElement::kVertex, ARX_ERROR_ELEMENT_LEVEL_VERTEX);
    ARX_C_ERROR_ELEMENT_CASE(LevelElement::kFace, ARX_ERROR_ELEMENT_LEVEL_FACE);
    ARX_C_ERROR_ELEMENT_CASE(LevelElement::kTexture, ARX_ERROR_ELEMENT_LEVEL_TEXTURE);
    ARX_C_ERROR_ELEMENT_CASE(LevelElement::kRoom, ARX_ERROR_ELEMENT_LEVEL_ROOM);
    ARX_C_ERROR_ELEMENT_CASE(LevelElement::kPortal, ARX_ERROR_ELEMENT_LEVEL_PORTAL);
    ARX_C_ERROR_ELEMENT_CASE(LevelElement::kRoomDistance, ARX_ERROR_ELEMENT_LEVEL_ROOM_DISTANCE);
    ARX_C_ERROR_ELEMENT_CASE(LevelElement::kNavSurfaceVertex, ARX_ERROR_ELEMENT_LEVEL_NAV_SURFACE_VERTEX);
    ARX_C_ERROR_ELEMENT_CASE(LevelElement::kNavSurfaceTriangle, ARX_ERROR_ELEMENT_LEVEL_NAV_SURFACE_TRIANGLE);
    ARX_C_ERROR_ELEMENT_CASE(LevelElement::kAnchor, ARX_ERROR_ELEMENT_LEVEL_ANCHOR);
    ARX_C_ERROR_ELEMENT_CASE(LevelElement::kAnchorConnection, ARX_ERROR_ELEMENT_LEVEL_ANCHOR_CONNECTION);
    ARX_C_ERROR_ELEMENT_CASE(LevelElement::kLight, ARX_ERROR_ELEMENT_LEVEL_LIGHT);
    ARX_C_ERROR_ELEMENT_CASE(LevelElement::kPlayerSpawn, ARX_ERROR_ELEMENT_LEVEL_PLAYER_SPAWN);
    ARX_C_ERROR_ELEMENT_CASE(LevelElement::kEntity, ARX_ERROR_ELEMENT_LEVEL_ENTITY);
    ARX_C_ERROR_ELEMENT_CASE(LevelElement::kFog, ARX_ERROR_ELEMENT_LEVEL_FOG);
    ARX_C_ERROR_ELEMENT_CASE(LevelElement::kZone, ARX_ERROR_ELEMENT_LEVEL_ZONE);
    ARX_C_ERROR_ELEMENT_CASE(LevelElement::kZonePerimeterPoint, ARX_ERROR_ELEMENT_LEVEL_ZONE_PERIMETER_POINT);
    ARX_C_ERROR_ELEMENT_CASE(LevelElement::kPath, ARX_ERROR_ELEMENT_LEVEL_PATH);
    ARX_C_ERROR_ELEMENT_CASE(LevelElement::kPathNode, ARX_ERROR_ELEMENT_LEVEL_PATH_NODE);
  }
  return ARX_ERROR_ELEMENT_NONE;
}

constexpr ArxErrorElement cErrorElement(ModelElement element) noexcept {
  switch (element) {
    ARX_C_ERROR_ELEMENT_CASE(ModelElement::kResource, ARX_ERROR_ELEMENT_MODEL_RESOURCE);
    ARX_C_ERROR_ELEMENT_CASE(ModelElement::kInventoryIcon, ARX_ERROR_ELEMENT_MODEL_INVENTORY_ICON);
    ARX_C_ERROR_ELEMENT_CASE(ModelElement::kVertex, ARX_ERROR_ELEMENT_MODEL_VERTEX);
    ARX_C_ERROR_ELEMENT_CASE(ModelElement::kFace, ARX_ERROR_ELEMENT_MODEL_FACE);
    ARX_C_ERROR_ELEMENT_CASE(ModelElement::kTexture, ARX_ERROR_ELEMENT_MODEL_TEXTURE);
    ARX_C_ERROR_ELEMENT_CASE(ModelElement::kBone, ARX_ERROR_ELEMENT_MODEL_BONE);
    ARX_C_ERROR_ELEMENT_CASE(ModelElement::kActionPoint, ARX_ERROR_ELEMENT_MODEL_ACTION_POINT);
    ARX_C_ERROR_ELEMENT_CASE(ModelElement::kSelection, ARX_ERROR_ELEMENT_MODEL_SELECTION);
  }
  return ARX_ERROR_ELEMENT_NONE;
}

constexpr ArxErrorElement cErrorElement(AnimationElement element) noexcept {
  switch (element) {
    ARX_C_ERROR_ELEMENT_CASE(AnimationElement::kResource, ARX_ERROR_ELEMENT_ANIMATION_RESOURCE);
    ARX_C_ERROR_ELEMENT_CASE(AnimationElement::kKeyframe, ARX_ERROR_ELEMENT_ANIMATION_KEYFRAME);
    ARX_C_ERROR_ELEMENT_CASE(AnimationElement::kGroup, ARX_ERROR_ELEMENT_ANIMATION_GROUP);
    ARX_C_ERROR_ELEMENT_CASE(AnimationElement::kGroupTransform, ARX_ERROR_ELEMENT_ANIMATION_GROUP_TRANSFORM);
    ARX_C_ERROR_ELEMENT_CASE(AnimationElement::kSound, ARX_ERROR_ELEMENT_ANIMATION_SOUND);
  }
  return ARX_ERROR_ELEMENT_NONE;
}

constexpr ArxErrorElement cErrorElement(AmbianceElement element) noexcept {
  switch (element) {
    ARX_C_ERROR_ELEMENT_CASE(AmbianceElement::kResource, ARX_ERROR_ELEMENT_AMBIANCE_RESOURCE);
    ARX_C_ERROR_ELEMENT_CASE(AmbianceElement::kTrack, ARX_ERROR_ELEMENT_AMBIANCE_TRACK);
    ARX_C_ERROR_ELEMENT_CASE(AmbianceElement::kKey, ARX_ERROR_ELEMENT_AMBIANCE_KEY);
    ARX_C_ERROR_ELEMENT_CASE(AmbianceElement::kSound, ARX_ERROR_ELEMENT_AMBIANCE_SOUND);
  }
  return ARX_ERROR_ELEMENT_NONE;
}

constexpr ArxErrorElement cErrorElement(CinematicElement element) noexcept {
  switch (element) {
    ARX_C_ERROR_ELEMENT_CASE(CinematicElement::kResource, ARX_ERROR_ELEMENT_CINEMATIC_RESOURCE);
    ARX_C_ERROR_ELEMENT_CASE(CinematicElement::kTimeline, ARX_ERROR_ELEMENT_CINEMATIC_TIMELINE);
    ARX_C_ERROR_ELEMENT_CASE(CinematicElement::kIllustration, ARX_ERROR_ELEMENT_CINEMATIC_ILLUSTRATION);
    ARX_C_ERROR_ELEMENT_CASE(CinematicElement::kKeyframe, ARX_ERROR_ELEMENT_CINEMATIC_KEYFRAME);
    ARX_C_ERROR_ELEMENT_CASE(CinematicElement::kTexture, ARX_ERROR_ELEMENT_CINEMATIC_TEXTURE);
    ARX_C_ERROR_ELEMENT_CASE(CinematicElement::kSound, ARX_ERROR_ELEMENT_CINEMATIC_SOUND);
    ARX_C_ERROR_ELEMENT_CASE(CinematicElement::kLanguage, ARX_ERROR_ELEMENT_CINEMATIC_LANGUAGE);
    ARX_C_ERROR_ELEMENT_CASE(CinematicElement::kSoundEncoding, ARX_ERROR_ELEMENT_CINEMATIC_SOUND_ENCODING);
  }
  return ARX_ERROR_ELEMENT_NONE;
}

constexpr ArxErrorElement cErrorElement(GlbElement element) noexcept {
  switch (element) {
    ARX_C_ERROR_ELEMENT_CASE(GlbElement::kDocument, ARX_ERROR_ELEMENT_GLB_DOCUMENT);
    ARX_C_ERROR_ELEMENT_CASE(GlbElement::kScene, ARX_ERROR_ELEMENT_GLB_SCENE);
    ARX_C_ERROR_ELEMENT_CASE(GlbElement::kNode, ARX_ERROR_ELEMENT_GLB_NODE);
    ARX_C_ERROR_ELEMENT_CASE(GlbElement::kMesh, ARX_ERROR_ELEMENT_GLB_MESH);
    ARX_C_ERROR_ELEMENT_CASE(GlbElement::kPrimitive, ARX_ERROR_ELEMENT_GLB_PRIMITIVE);
    ARX_C_ERROR_ELEMENT_CASE(GlbElement::kAccessor, ARX_ERROR_ELEMENT_GLB_ACCESSOR);
    ARX_C_ERROR_ELEMENT_CASE(GlbElement::kMaterial, ARX_ERROR_ELEMENT_GLB_MATERIAL);
    ARX_C_ERROR_ELEMENT_CASE(GlbElement::kTexture, ARX_ERROR_ELEMENT_GLB_TEXTURE);
    ARX_C_ERROR_ELEMENT_CASE(GlbElement::kImage, ARX_ERROR_ELEMENT_GLB_IMAGE);
    ARX_C_ERROR_ELEMENT_CASE(GlbElement::kSkin, ARX_ERROR_ELEMENT_GLB_SKIN);
    ARX_C_ERROR_ELEMENT_CASE(GlbElement::kAnimation, ARX_ERROR_ELEMENT_GLB_ANIMATION);
    ARX_C_ERROR_ELEMENT_CASE(GlbElement::kChannel, ARX_ERROR_ELEMENT_GLB_CHANNEL);
    ARX_C_ERROR_ELEMENT_CASE(GlbElement::kSampler, ARX_ERROR_ELEMENT_GLB_SAMPLER);
  }
  return ARX_ERROR_ELEMENT_NONE;
}

constexpr ArxErrorElement cErrorElement(ObjSource source) noexcept {
  switch (source) {
    ARX_C_ERROR_ELEMENT_CASE(ObjSource::kObj, ARX_ERROR_ELEMENT_OBJ_DOCUMENT);
    ARX_C_ERROR_ELEMENT_CASE(ObjSource::kMaterialLibrary, ARX_ERROR_ELEMENT_OBJ_MATERIAL_LIBRARY);
  }
  return ARX_ERROR_ELEMENT_NONE;
}

#define ARX_NATIVE_C_ERROR_ELEMENTS(cpp_type, cases)                   \
  constexpr ArxErrorElement cErrorElement(cpp_type element) noexcept { \
    switch (element) { cases }                                         \
    return ARX_ERROR_ELEMENT_NONE;                                     \
  }

ARX_NATIVE_C_ERROR_ELEMENTS(AmbElement, ARX_C_ERROR_ELEMENT_CASE(AmbElement::kHeader, ARX_ERROR_ELEMENT_AMB_HEADER);
                            ARX_C_ERROR_ELEMENT_CASE(AmbElement::kTrack, ARX_ERROR_ELEMENT_AMB_TRACK);
                            ARX_C_ERROR_ELEMENT_CASE(AmbElement::kKey, ARX_ERROR_ELEMENT_AMB_KEY);
                            ARX_C_ERROR_ELEMENT_CASE(AmbElement::kSound, ARX_ERROR_ELEMENT_AMB_SOUND);)
ARX_NATIVE_C_ERROR_ELEMENTS(CinElement, ARX_C_ERROR_ELEMENT_CASE(CinElement::kHeader, ARX_ERROR_ELEMENT_CIN_HEADER);
                            ARX_C_ERROR_ELEMENT_CASE(CinElement::kIllustration, ARX_ERROR_ELEMENT_CIN_ILLUSTRATION);
                            ARX_C_ERROR_ELEMENT_CASE(CinElement::kKeyframe, ARX_ERROR_ELEMENT_CIN_KEYFRAME);
                            ARX_C_ERROR_ELEMENT_CASE(CinElement::kSound, ARX_ERROR_ELEMENT_CIN_SOUND);)
ARX_NATIVE_C_ERROR_ELEMENTS(DlfElement, ARX_C_ERROR_ELEMENT_CASE(DlfElement::kHeader, ARX_ERROR_ELEMENT_DLF_HEADER);
                            ARX_C_ERROR_ELEMENT_CASE(DlfElement::kEntity, ARX_ERROR_ELEMENT_DLF_ENTITY);
                            ARX_C_ERROR_ELEMENT_CASE(DlfElement::kFog, ARX_ERROR_ELEMENT_DLF_FOG);
                            ARX_C_ERROR_ELEMENT_CASE(DlfElement::kZone, ARX_ERROR_ELEMENT_DLF_ZONE);
                            ARX_C_ERROR_ELEMENT_CASE(DlfElement::kZonePoint, ARX_ERROR_ELEMENT_DLF_ZONE_POINT);
                            ARX_C_ERROR_ELEMENT_CASE(DlfElement::kPath, ARX_ERROR_ELEMENT_DLF_PATH);
                            ARX_C_ERROR_ELEMENT_CASE(DlfElement::kPathNode, ARX_ERROR_ELEMENT_DLF_PATH_NODE);)
ARX_NATIVE_C_ERROR_ELEMENTS(FtlElement, ARX_C_ERROR_ELEMENT_CASE(FtlElement::kHeader, ARX_ERROR_ELEMENT_FTL_HEADER);
                            ARX_C_ERROR_ELEMENT_CASE(FtlElement::kVertex, ARX_ERROR_ELEMENT_FTL_VERTEX);
                            ARX_C_ERROR_ELEMENT_CASE(FtlElement::kFace, ARX_ERROR_ELEMENT_FTL_FACE);
                            ARX_C_ERROR_ELEMENT_CASE(FtlElement::kTexture, ARX_ERROR_ELEMENT_FTL_TEXTURE);
                            ARX_C_ERROR_ELEMENT_CASE(FtlElement::kBone, ARX_ERROR_ELEMENT_FTL_BONE);
                            ARX_C_ERROR_ELEMENT_CASE(FtlElement::kActionPoint, ARX_ERROR_ELEMENT_FTL_ACTION_POINT);
                            ARX_C_ERROR_ELEMENT_CASE(FtlElement::kSelection, ARX_ERROR_ELEMENT_FTL_SELECTION);)
ARX_NATIVE_C_ERROR_ELEMENTS(FtsElement, ARX_C_ERROR_ELEMENT_CASE(FtsElement::kHeader, ARX_ERROR_ELEMENT_FTS_HEADER);
                            ARX_C_ERROR_ELEMENT_CASE(FtsElement::kCell, ARX_ERROR_ELEMENT_FTS_CELL);
                            ARX_C_ERROR_ELEMENT_CASE(FtsElement::kVertex, ARX_ERROR_ELEMENT_FTS_VERTEX);
                            ARX_C_ERROR_ELEMENT_CASE(FtsElement::kFace, ARX_ERROR_ELEMENT_FTS_FACE);
                            ARX_C_ERROR_ELEMENT_CASE(FtsElement::kTexture, ARX_ERROR_ELEMENT_FTS_TEXTURE);
                            ARX_C_ERROR_ELEMENT_CASE(FtsElement::kRoom, ARX_ERROR_ELEMENT_FTS_ROOM);
                            ARX_C_ERROR_ELEMENT_CASE(FtsElement::kPortal, ARX_ERROR_ELEMENT_FTS_PORTAL);
                            ARX_C_ERROR_ELEMENT_CASE(FtsElement::kRoomDistance, ARX_ERROR_ELEMENT_FTS_ROOM_DISTANCE);
                            ARX_C_ERROR_ELEMENT_CASE(FtsElement::kAnchor, ARX_ERROR_ELEMENT_FTS_ANCHOR);
                            ARX_C_ERROR_ELEMENT_CASE(FtsElement::kAnchorConnection,
                                                     ARX_ERROR_ELEMENT_FTS_ANCHOR_CONNECTION);)
ARX_NATIVE_C_ERROR_ELEMENTS(LlfElement, ARX_C_ERROR_ELEMENT_CASE(LlfElement::kHeader, ARX_ERROR_ELEMENT_LLF_HEADER);
                            ARX_C_ERROR_ELEMENT_CASE(LlfElement::kLight, ARX_ERROR_ELEMENT_LLF_LIGHT);
                            ARX_C_ERROR_ELEMENT_CASE(LlfElement::kVertexColor, ARX_ERROR_ELEMENT_LLF_VERTEX_COLOR);)
ARX_NATIVE_C_ERROR_ELEMENTS(TeaElement, ARX_C_ERROR_ELEMENT_CASE(TeaElement::kHeader, ARX_ERROR_ELEMENT_TEA_HEADER);
                            ARX_C_ERROR_ELEMENT_CASE(TeaElement::kKeyframe, ARX_ERROR_ELEMENT_TEA_KEYFRAME);
                            ARX_C_ERROR_ELEMENT_CASE(TeaElement::kGroupTransform,
                                                     ARX_ERROR_ELEMENT_TEA_GROUP_TRANSFORM);
                            ARX_C_ERROR_ELEMENT_CASE(TeaElement::kSound, ARX_ERROR_ELEMENT_TEA_SOUND);)

#undef ARX_NATIVE_C_ERROR_ELEMENTS
#undef ARX_C_ERROR_ELEMENT_CASE

inline void clearError(ArxError* error) noexcept {
  if (!error) return;
  delete static_cast<ErrorBacking*>(error->private_data);
  *error = ARX_ERROR_INIT;
}

inline ArxStringView errorView(std::string_view value) noexcept {
  return value.empty() ? ArxStringView{} : ArxStringView{value.data(), value.size()};
}

inline ArxStringView presentErrorView(std::string_view value) noexcept { return {value.data(), value.size()}; }

template <class Element>
std::string_view errorElementName(Element element) noexcept {
  return api_detail::result_failure_detail::elementName(element);
}

inline std::string_view errorElementName(ObjSource source) noexcept {
  return source == ObjSource::kObj ? "OBJ document" : "material library";
}

template <class Element>
void captureElement(Element element, ArxErrorLocation& out, ErrorBacking& backing) {
  out.element = cErrorElement(element);
  backing.element_name = errorElementName(element);
  out.element_name = errorView(backing.element_name);
}

inline ArxReturnCode publishCode(ArxReturnCode code, ArxError* out_error) noexcept {
  if (!out_error) return code;
  clearError(out_error);
  out_error->code = code;
  return code;
}

template <class Element>
constexpr ArxErrorLocationKind resourceLocationKind() noexcept;

template <>
constexpr ArxErrorLocationKind resourceLocationKind<LevelElement>() noexcept {
  return ARX_ERROR_LOCATION_LEVEL;
}
template <>
constexpr ArxErrorLocationKind resourceLocationKind<ModelElement>() noexcept {
  return ARX_ERROR_LOCATION_MODEL;
}
template <>
constexpr ArxErrorLocationKind resourceLocationKind<AnimationElement>() noexcept {
  return ARX_ERROR_LOCATION_ANIMATION;
}
template <>
constexpr ArxErrorLocationKind resourceLocationKind<AmbianceElement>() noexcept {
  return ARX_ERROR_LOCATION_AMBIANCE;
}
template <>
constexpr ArxErrorLocationKind resourceLocationKind<CinematicElement>() noexcept {
  return ARX_ERROR_LOCATION_CINEMATIC;
}

template <class Element>
void captureLocation(const ResourceLocation<Element>& source, ArxErrorLocation& out, ErrorBacking& backing) {
  out.kind = resourceLocationKind<Element>();
  captureElement(source.element, out, backing);
  out.input_index = source.input_index;
  out.index = source.index;
  out.subindex = source.subindex;
  backing.resource_path = source.resource_path;
  backing.label = source.label;
  out.resource_path = errorView(backing.resource_path);
  out.label = errorView(backing.label);
}

inline void captureLocation(const CinematicLocation& source, ArxErrorLocation& out, ErrorBacking& backing) {
  out.kind = ARX_ERROR_LOCATION_CINEMATIC;
  captureElement(source.element, out, backing);
  out.input_index = source.input_index;
  out.index = source.index;
  out.subindex = source.subindex;
  out.sound_handle = source.sound_handle;
  out.language_id = source.language_id;
  backing.resource_path = source.resource_path;
  backing.label = source.label;
  out.resource_path = errorView(backing.resource_path);
  out.label = errorView(backing.label);
}

inline void captureLocation(const GlbLocation& source, ArxErrorLocation& out, ErrorBacking& backing) {
  out.kind = ARX_ERROR_LOCATION_GLB;
  captureElement(source.element, out, backing);
  out.index = source.index;
  out.subindex = source.subindex;
  backing.label = source.label;
  backing.property = source.property;
  out.label = errorView(backing.label);
  out.property = errorView(backing.property);
}

inline void captureLocation(const ObjLocation& source, ArxErrorLocation& out, ErrorBacking& backing) {
  out.kind = ARX_ERROR_LOCATION_OBJ;
  captureElement(source.source, out, backing);
  out.input_index = source.source_index;
  out.line = source.line;
  backing.source_path = source.source_path;
  out.source_path = errorView(backing.source_path);
}

inline void captureLocation(const JsonLocation& source, ArxErrorLocation& out, ErrorBacking& backing) {
  out.kind = ARX_ERROR_LOCATION_JSON;
  out.byte_offset = source.byte_offset;
  backing.json_pointer = source.pointer;
  out.json_pointer = presentErrorView(backing.json_pointer);
}

template <class Element>
constexpr ArxErrorLocationKind nativeLocationKind() noexcept;

#define ARX_NATIVE_LOCATION_KIND(element, carrier, binary)                      \
  template <>                                                                   \
  constexpr ArxErrorLocationKind nativeLocationKind<element>() noexcept {       \
    return carrier;                                                             \
  }                                                                             \
  template <>                                                                   \
  constexpr ArxErrorLocationKind nativeBinaryLocationKind<element>() noexcept { \
    return binary;                                                              \
  }

template <class Element>
constexpr ArxErrorLocationKind nativeBinaryLocationKind() noexcept;

ARX_NATIVE_LOCATION_KIND(AmbElement, ARX_ERROR_LOCATION_AMB, ARX_ERROR_LOCATION_AMB_BINARY)
ARX_NATIVE_LOCATION_KIND(CinElement, ARX_ERROR_LOCATION_CIN, ARX_ERROR_LOCATION_CIN_BINARY)
ARX_NATIVE_LOCATION_KIND(DlfElement, ARX_ERROR_LOCATION_DLF, ARX_ERROR_LOCATION_DLF_BINARY)
ARX_NATIVE_LOCATION_KIND(FtlElement, ARX_ERROR_LOCATION_FTL, ARX_ERROR_LOCATION_FTL_BINARY)
ARX_NATIVE_LOCATION_KIND(FtsElement, ARX_ERROR_LOCATION_FTS, ARX_ERROR_LOCATION_FTS_BINARY)
ARX_NATIVE_LOCATION_KIND(LlfElement, ARX_ERROR_LOCATION_LLF, ARX_ERROR_LOCATION_LLF_BINARY)
ARX_NATIVE_LOCATION_KIND(TeaElement, ARX_ERROR_LOCATION_TEA, ARX_ERROR_LOCATION_TEA_BINARY)

#undef ARX_NATIVE_LOCATION_KIND

template <class Element>
void captureLocation(const NativeLocation<Element>& source, ArxErrorLocation& out, ErrorBacking& backing) {
  out.kind = nativeLocationKind<Element>();
  captureElement(source.element, out, backing);
  out.index = source.index;
  out.subindex = source.subindex;
  backing.field = source.field;
  out.field = errorView(backing.field);
}

template <class Element>
void captureLocation(const NativeBinaryLocation<Element>& source, ArxErrorLocation& out, ErrorBacking& backing) {
  out.kind = nativeBinaryLocationKind<Element>();
  captureElement(source.element, out, backing);
  out.index = source.index;
  out.subindex = source.subindex;
  out.binary_region =
      source.region == NativeBinaryRegion::kStored ? ARX_ERROR_BINARY_STORED : ARX_ERROR_BINARY_DECODED_PAYLOAD;
  out.byte_offset = source.byte_offset;
  out.requested_bytes = source.requested_bytes;
  backing.field = source.field;
  out.field = errorView(backing.field);
}

inline void captureLocation(const DlfBinaryLocation& source, ArxErrorLocation& out, ErrorBacking& backing) {
  out.kind = ARX_ERROR_LOCATION_DLF_BINARY;
  std::visit([&](auto element) { captureElement(element, out, backing); }, source.element);
  out.index = source.index;
  out.subindex = source.subindex;
  out.binary_region =
      source.region == NativeBinaryRegion::kStored ? ARX_ERROR_BINARY_STORED : ARX_ERROR_BINARY_DECODED_PAYLOAD;
  out.byte_offset = source.byte_offset;
  out.requested_bytes = source.requested_bytes;
  backing.field = source.field;
  out.field = errorView(backing.field);
}

template <class... Locations>
void captureLocation(const std::variant<Locations...>& source, ArxErrorLocation& out, ErrorBacking& backing) {
  std::visit([&](const auto& location) { captureLocation(location, out, backing); }, source);
}

template <class Value, class Location>
ArxReturnCode publish(const Result<Value, Location>& result, ArxError* out_error) noexcept {
  if (!out_error) return result.code();
  if (result) {
    clearError(out_error);
    return ARX_OK;
  }

  const Error<Location>* source = result.error();
  if (!source) {
    clearError(out_error);
    out_error->code = ARX_INTERNAL_ERROR;
    return ARX_INTERNAL_ERROR;
  }

  try {
    auto backing = std::make_unique<ErrorBacking>();
    ArxError snapshot = ARX_ERROR_INIT;
    snapshot.code = source->code();
    if (source->location()) captureLocation(*source->location(), snapshot.location, *backing);
    backing->detail = source->detail();
    snapshot.detail = errorView(backing->detail);
    clearError(out_error);
    snapshot.private_data = backing.release();
    *out_error = snapshot;
  } catch (...) {
    clearError(out_error);
    out_error->code = source->code();
  }
  return source->code();
}

}  // namespace pistoris::c_api
