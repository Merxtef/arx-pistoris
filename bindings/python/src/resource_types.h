// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#pragma once

#include "arx_pistoris/pistoris.hpp"
#include "arx_pistoris/sound.h"
#include "arx_pistoris/texture.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace pistoris::python {

enum class AmbianceAutomationMode : std::uint8_t {
  kConstant = ARX_AMBIANCE_AUTOMATION_CONSTANT,
  kStep = ARX_AMBIANCE_AUTOMATION_STEP,
  kRandomStep = ARX_AMBIANCE_AUTOMATION_RANDOM_STEP,
  kInterpolated = ARX_AMBIANCE_AUTOMATION_INTERPOLATED,
  kRandomInterpolated = ARX_AMBIANCE_AUTOMATION_RANDOM_INTERPOLATED,
};

enum class AmbianceTrackKind : std::uint8_t {
  kPanned = ARX_AMBIANCE_TRACK_PANNED,
  kPositioned = ARX_AMBIANCE_TRACK_POSITIONED,
};

enum class CinematicIllustrationFormat : ArxImageFormat {
  kAuto = ARX_IMAGE_FORMAT_UNKNOWN,
  kBmp = ARX_IMAGE_FORMAT_BMP,
  kTga = ARX_IMAGE_FORMAT_TGA,
};

enum class PortalShape : std::uint8_t {
  kTriangle = ARX_PORTAL_TRIANGLE,
  kQuad = ARX_PORTAL_QUAD,
};

enum class ZoneHeightMode : std::uint8_t {
  kFinite = ARX_ZONE_HEIGHT_FINITE,
  kInfinite = ARX_ZONE_HEIGHT_INFINITE,
};

enum class PathNodeType : std::uint8_t {
  kStandard = ARX_PATH_NODE_STANDARD,
  kBezier = ARX_PATH_NODE_BEZIER,
};

enum class AnchorFlagBitmask : std::uint16_t {  // NOLINT(performance-enum-size)
  kBlocked = ARX_ANCHOR_FLAG_BLOCKED,
};

inline std::string copyString(ArxStringView value) {
  return value.data ? std::string(value.data, value.size) : std::string{};
}

inline ArxStringView view(const std::string& value) { return {value.data(), value.size()}; }

inline ArxEncodedImageView imageView(const std::vector<std::uint8_t>& value) {
  return {value.empty() ? nullptr : value.data(), value.size()};
}

inline ArxEncodedAudioView audioView(const std::vector<std::uint8_t>& value) {
  return {value.empty() ? nullptr : value.data(), value.size()};
}

struct Texture {
  std::string path;
  std::vector<std::uint8_t> encoded_image;
  std::string external_image_extension;

  Texture() = default;
  explicit Texture(const ArxTextureView& value)
      : path(copyString(value.path)), external_image_extension(copyString(value.external_image_extension)) {
    if (value.encoded_image.data) {
      encoded_image.assign(value.encoded_image.data, value.encoded_image.data + value.encoded_image.size);
    }
  }

  [[nodiscard]] ArxTextureView asView() const {
    return {view(path), imageView(encoded_image), view(external_image_extension)};
  }
};

struct Sound {
  std::string path;
  std::vector<std::uint8_t> encoded_audio;

  Sound() = default;
  explicit Sound(const ArxSoundView& value) : path(copyString(value.path)) {
    if (value.encoded_audio.data) {
      encoded_audio.assign(value.encoded_audio.data, value.encoded_audio.data + value.encoded_audio.size);
    }
  }

  [[nodiscard]] ArxSoundView asView() const { return {view(path), audioView(encoded_audio)}; }
};

struct ModelSelection {
  std::string name;

  ModelSelection() = default;
  explicit ModelSelection(std::string value) : name(std::move(value)) {}
  bool operator==(const ModelSelection&) const = default;
};

struct ModelSelectionLeadingVertex {
  ArxVector3 position{};
  std::optional<std::string> bone;
};

struct ModelVertex {
  ArxVector3 position{};
  std::optional<std::string> bone;
  std::vector<ModelSelection> selections;
};

struct ModelCorner {
  ModelVertex vertex;
  ArxVector3 normal{};
  float u = 0.0f;
  float v = 0.0f;
};

struct ModelFace {
  std::array<ModelCorner, 3> corners{};
  ArxVector3 normal{};
  std::optional<std::string> texture;
  ArxFaceType flags = 0;
  float transval = 0.0f;
};

struct ModelBone {
  std::string name;
  ArxVector3 position{};
  std::optional<std::string> parent;
  float blob_shadow_size = 0.0f;
  std::vector<ModelSelection> selections;

  ModelBone() = default;
};

struct ModelActionPoint {
  std::string name;
  ArxVector3 position{};
  std::optional<std::string> bone;
  std::vector<ModelSelection> selections;

  ModelActionPoint() = default;
};

struct ModelOrigin {
  std::optional<std::string> bone;
  std::vector<ModelSelection> selections;
};

struct LevelRoom {
  std::string name;
  LevelRoom() = default;
  explicit LevelRoom(const ArxLevelRoom& value) : name(copyString(value.name)) {}
  [[nodiscard]] ArxLevelRoom asValue() const { return {view(name)}; }
};

struct LevelCorner {
  ArxLevelVertex vertex{};
  ArxVector3 normal{};
  float u = 0.0f;
  float v = 0.0f;
  ArxColor3 color{0.5f, 0.5f, 0.5f};
};

struct LevelFace {
  std::array<LevelCorner, 3> corners{};
  std::optional<ArxVector3> normal;
  std::optional<std::string> texture;
  std::optional<std::string> room;
  ArxFaceType flags = 0;
  float transval = 0.0f;
};

inline ArxVector3 levelFaceNormal(const LevelFace& value) noexcept {
  const ArxVector3& p0 = value.corners[0].vertex.position;
  const ArxVector3& p1 = value.corners[1].vertex.position;
  const ArxVector3& p2 = value.corners[2].vertex.position;
  const double ax = static_cast<double>(p1.x) - p0.x, ay = static_cast<double>(p1.y) - p0.y,
               az = static_cast<double>(p1.z) - p0.z;
  const double bx = static_cast<double>(p2.x) - p0.x, by = static_cast<double>(p2.y) - p0.y,
               bz = static_cast<double>(p2.z) - p0.z;
  const double nx = ay * bz - az * by, ny = az * bx - ax * bz, nz = ax * by - ay * bx;
  const double length = std::hypot(nx, ny, nz);
  if (length == 0.0 || !std::isfinite(length)) return {};
  return {static_cast<float>(nx / length), static_cast<float>(ny / length), static_cast<float>(nz / length)};
}

struct LevelPortal {
  std::string name;
  std::optional<std::string> room_front;
  std::optional<std::string> room_back;
  ArxPortalShape shape = ARX_PORTAL_QUAD;
  std::array<ArxVector3, 4> vertices{};

  LevelPortal() = default;
};

struct LevelAnchor {
  ArxVector3 position{};
  float radius = kDefaultAnchorRadius;
  float height = kDefaultAnchorHeight;
  std::int16_t flags = 0;
  std::string name;

  LevelAnchor() = default;
  explicit LevelAnchor(const ArxLevelAnchor& value)
      : position(value.position),
        radius(value.radius),
        height(value.height),
        flags(value.flags),
        name(copyString(value.name)) {}
  [[nodiscard]] ArxLevelAnchor asValue() const { return {position, radius, height, flags, view(name)}; }
};

struct LevelLight {
  std::string name;
  ArxVector3 position{};
  ArxColor3 color{};
  float fallstart = 0.0f;
  float fallend = 1.0f;
  float intensity = 0.0f;
  ArxColor3 flicker{};
  float effect_radius = 0.0f;
  float effect_frequency = 0.0f;
  float effect_size = 0.0f;
  float effect_speed = 0.0f;
  float flare_size = 0.0f;
  std::uint32_t flags = 0;

  LevelLight() = default;
  explicit LevelLight(const ArxLevelLight& value)
      : name(copyString(value.name)),
        position(value.position),
        color(value.color),
        fallstart(value.fallstart),
        fallend(value.fallend),
        intensity(value.intensity),
        flicker(value.flicker),
        effect_radius(value.effect_radius),
        effect_frequency(value.effect_frequency),
        effect_size(value.effect_size),
        effect_speed(value.effect_speed),
        flare_size(value.flare_size),
        flags(value.flags) {}
  [[nodiscard]] ArxLevelLight asValue() const {
    return {view(name),
            position,
            color,
            fallstart,
            fallend,
            intensity,
            flicker,
            effect_radius,
            effect_frequency,
            effect_size,
            effect_speed,
            flare_size,
            flags};
  }
};

struct LevelEntity {
  std::string class_path;
  std::int32_t ident = -1;
  ArxVector3 position{};
  ArxQuat rotation{};
  std::string name;

  LevelEntity() = default;
  explicit LevelEntity(const ArxLevelEntity& value)
      : class_path(copyString(value.class_path)),
        ident(value.ident),
        position(value.position),
        rotation(value.rotation),
        name(copyString(value.name)) {}
  [[nodiscard]] ArxLevelEntity asValue() const { return {view(class_path), ident, position, rotation, view(name)}; }
};

struct LevelFog {
  ArxVector3 position{};
  ArxColor3 color{};
  float size = 0.0f;
  bool directional = false;
  float scale = 0.0f;
  ArxQuat rotation{};
  float speed = 0.0f;
  float rotate_speed = 0.0f;
  std::int32_t lifetime_ms = 0;
  float frequency = 0.0f;
  std::string name;

  LevelFog() = default;
  explicit LevelFog(const ArxLevelFog& value)
      : position(value.position),
        color(value.color),
        size(value.size),
        directional(value.directional != 0),
        scale(value.scale),
        rotation(value.rotation),
        speed(value.speed),
        rotate_speed(value.rotate_speed),
        lifetime_ms(value.lifetime_ms),
        frequency(value.frequency),
        name(copyString(value.name)) {}
  [[nodiscard]] ArxLevelFog asValue() const {
    return {position,
            color,
            size,
            static_cast<std::uint8_t>(directional),
            scale,
            rotation,
            speed,
            rotate_speed,
            lifetime_ms,
            frequency,
            view(name)};
  }
};

struct LevelZoneAmbiance {
  std::string name;
  float volume = 100.0f;
};

struct LevelZone {
  std::string name;
  std::vector<ArxVector2> perimeter_xz;
  float reference_y = 0.0f;
  ArxZoneHeightMode height_mode = ARX_ZONE_HEIGHT_FINITE;
  float height = 0.0f;
  std::optional<ArxColor3> color;
  std::optional<float> farclip;
  std::optional<LevelZoneAmbiance> ambiance;
};

struct LevelPath {
  std::string name;
  ArxVector3 position{};
  std::vector<ArxLevelPathNode> nodes;
  std::size_t nodes_revision = 0;
};

struct LevelRoomDistanceValue {
  float distance = -1.0f;
  std::optional<std::string> portal_a;
  std::optional<std::string> portal_b;
};

struct LevelAnchorConnection {
  std::string first;
  std::string second;
};

struct LevelNavSurfaceTriangleValue {
  std::array<ArxLevelVertex, 3> vertices{};
};

struct SoundSourceReferenceValue {
  std::string sound_path;
  std::string source_path;
};

struct CinematicSpeechEncodingValue {
  std::string language;
  std::vector<std::uint8_t> encoded_audio;
};

struct CinematicSpeechValue {
  std::string path;
  std::vector<CinematicSpeechEncodingValue> encodings;
};

struct CinematicIllustrationValue {
  std::string path;
  std::vector<std::uint8_t> encoded_image;
  std::string external_image_extension;
  std::int32_t subdivision_scale = 1;
};

struct CinematicKeyframeValue {
  std::int32_t frame = 0;
  ArxVector3 camera_position{};
  float camera_roll = 0.0f;
  ArxColor3 color{1.0f, 1.0f, 1.0f};
  ArxColor3 secondary_color{1.0f, 1.0f, 1.0f};
  ArxColor3 flash_color{1.0f, 1.0f, 1.0f};
  float flash_decay = 0.0f;
  std::optional<ArxCinematicLight> light;
  float outgoing_speed = 1.0f;
  std::optional<std::string> sound_path;
  SoundKind sound_kind = SoundKind::kEffect;
  ArxCinematicInterpolation interpolation = ARX_CINEMATIC_INTERPOLATION_LINEAR;
  ArxCinematicBaseEffect base_effect = ARX_CINEMATIC_BASE_EFFECT_NONE;
  ArxCinematicPostEffect post_effect = ARX_CINEMATIC_POST_EFFECT_NONE;
  bool crossfade = false;
  bool dream = false;
};

struct CinematicSoundSourceReferenceValue {
  SoundKind kind = SoundKind::kEffect;
  std::string sound_path;
  std::string source_path;
};

struct ObjMaterialLibrary {
  std::string path;
  std::string text;

  [[nodiscard]] ObjMaterialLibraryView asView() const { return {path, text}; }
};

struct AnimationFrame {
  struct Keyframe {
    std::uint32_t frame = 0;
    ArxVector3 root_translation{};
    ArxQuat root_rotation{};
    bool footstep = false;
    std::optional<std::string> sound;
  } keyframe;
  std::vector<ArxAnimationGroupTransform> group_transforms;
  std::size_t group_transforms_revision = 0;
};

using AnimationKeyframeValue = AnimationFrame::Keyframe;

struct AmbiancePannedTrackValue {
  std::optional<std::string> sound;
  std::vector<ArxAmbiancePannedKey> keys;
  std::size_t keys_revision = 0;
};

struct AmbiancePositionedTrackValue {
  std::optional<std::string> sound;
  std::vector<ArxAmbiancePositionedKey> keys;
  std::size_t keys_revision = 0;
};

struct MinimapSamplerValue {
  std::vector<std::uint8_t> encoded_image;
  ArxColor3 color{};

  [[nodiscard]] Level::MinimapSampler asValue() const { return {imageView(encoded_image), color}; }
};

struct TextureFile {
  std::string source_path;
  std::string path;
  std::vector<std::uint8_t> encoded_image;
};

struct SoundFileValue {
  std::string source_path;
  std::string path;
  std::vector<std::uint8_t> encoded_audio;
};

struct CinematicSoundFileValue {
  SoundKind kind = SoundKind::kEffect;
  std::string source_path;
  std::optional<std::string> language;
  std::string path;
  std::vector<std::uint8_t> encoded_audio;
};

}  // namespace pistoris::python
