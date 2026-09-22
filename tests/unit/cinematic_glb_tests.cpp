// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#include "doctest/doctest.h"

#include "arx_pistoris/base/indices.h"
#include "arx_pistoris/base/math.h"
#include "arx_pistoris/base/status.h"
#include "arx_pistoris/cinematic.hpp"
#include "arx_pistoris/cinematic/sound.hpp"
#include "arx_pistoris/cinematic/types.h"
#include "arx_pistoris/runtime.hpp"
#include "arx_pistoris/runtime/types.h"
#include "arx_pistoris/sound.hpp"
#include "arx_pistoris/texture.h"

#include "cgltf/cgltf.h"
#include "external/glb/accessor.h"
#include "external/glb/cinematic/coordinates.h"
#include "external/glb/cinematic/names.h"
#include "external/glb/cinematic/surface.h"
#include "external/glb/container.h"
#include "external/glb/utils/names.h"
#include "external/glb/writer.h"
#include "image_helpers.h"
#include "modules/cinematic.h"
#include "nlohmann/json.hpp"
#include "stb/stb_image_write.h"
#include "utils/encoded_image.h"
#include "utils/math/quat.h"

#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <numbers>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace {

bool near(float left, float right) { return std::abs(left - right) <= 1.0e-5f; }

enum class ImageSource : std::uint8_t {
  kEmbedded,
  kExternal,
};

int addIllustration(pistoris::glb::Builder& builder, int root, ImageSource source, std::uint32_t ordinal = 0,
                    std::string_view image_name = "story/scene.png", std::array<float, 4> heights = {},
                    std::span<const std::uint8_t> embedded_image = {}) {
  const std::array<pistoris::glb::Vec3, 4> positions = {
      {{-1.0f, heights[0], -0.5f}, {1.0f, heights[1], -0.5f}, {1.0f, heights[2], 0.5f}, {-1.0f, heights[3], 0.5f}}};
  const std::array<pistoris::glb::Vec2, 4> texcoords = {{{0.0f, 0.0f}, {1.0f, 0.0f}, {1.0f, 1.0f}, {0.0f, 1.0f}}};
  const std::array<std::uint32_t, 6> indices = {0, 1, 2, 0, 2, 3};

  int texture = -1;
  if (source == ImageSource::kEmbedded) {
    std::vector<std::uint8_t> png;
    if (embedded_image.empty())
      REQUIRE(pistoris::image::transcodeToPng(makeSolidTestBmp(2, 1), png) == pistoris::image::Error::kNone);
    const std::span<const std::uint8_t> image =
        embedded_image.empty() ? std::span<const std::uint8_t>(png) : embedded_image;
    texture = builder.addEmbeddedTexture(std::string(image_name), "image/png", image);
  } else {
    texture = builder.addExternalTexture(std::string(image_name), std::string(image_name));
  }
  const int material = builder.addMaterial("illustration", texture, {.unlit = true});
  pistoris::glb::Primitive primitive;
  primitive.indices =
      builder.addAccessor(std::span<const std::uint32_t>(indices), cgltf_component_type_r_32u, cgltf_type_scalar);
  primitive.material = material;
  primitive.attributes = {
      {"POSITION", builder.addVec3Accessor(positions)},
      {"TEXCOORD_0",
       builder.addAccessor(
           std::span<const pistoris::glb::Vec2>(texcoords), cgltf_component_type_r_32f, cgltf_type_vec2)},
  };
  const int mesh = builder.addMesh("illustration", {std::move(primitive)});
  const int illustration = builder.addNode("arx_illustration__" + std::to_string(ordinal) + "__illustration", mesh);
  builder.addChild(root, illustration);
  return illustration;
}

enum class PrimitiveImages : std::uint8_t {
  kShared,
  kDuplicate,
  kDifferent,
};

int addMultiPrimitiveIllustration(pistoris::glb::Builder& builder, int root,
                                  PrimitiveImages images = PrimitiveImages::kShared) {
  std::vector<std::uint8_t> png;
  REQUIRE(pistoris::image::transcodeToPng(makeSolidTestBmp(2, 1), png) == pistoris::image::Error::kNone);
  const int texture = builder.addEmbeddedTexture("story/scene.png", "image/png", png);
  const int material = builder.addMaterial("illustration", texture, {.unlit = true});
  int second_material = material;
  if (images != PrimitiveImages::kShared) {
    std::vector<std::uint8_t> second_png = png;
    if (images == PrimitiveImages::kDifferent)
      REQUIRE(pistoris::image::transcodeToPng(makeSolidTestBmp(2, 1, 0, 255), second_png) ==
              pistoris::image::Error::kNone);
    const int second_texture = builder.addEmbeddedTexture("story/second.png", "image/png", second_png);
    second_material = builder.addMaterial("illustration_2", second_texture, {.unlit = true});
  }
  const std::array<pistoris::glb::Vec3, 3> first_positions = {
      {{-1.0f, 0.0f, -0.5f}, {1.0f, 0.0f, -0.5f}, {1.0f, 0.0f, 0.5f}}};
  const std::array<pistoris::glb::Vec2, 3> first_texcoords = {{{0.0f, 0.0f}, {1.0f, 0.0f}, {1.0f, 1.0f}}};
  const std::array<pistoris::glb::Vec3, 3> second_positions = {
      {{-1.0f, 0.0f, -0.5f}, {1.0f, 0.0f, 0.5f}, {-1.0f, 0.0f, 0.5f}}};
  const std::array<pistoris::glb::Vec2, 3> second_texcoords = {{{0.0f, 0.0f}, {1.0f, 1.0f}, {0.0f, 2.0f}}};
  const std::array<std::uint32_t, 3> indices = {0, 1, 2};
  const auto primitive = [&](std::span<const pistoris::glb::Vec3> positions,
                             std::span<const pistoris::glb::Vec2>
                                 texcoords,
                             int primitive_material) {
    pistoris::glb::Primitive result;
    result.indices =
        builder.addAccessor(std::span<const std::uint32_t>(indices), cgltf_component_type_r_32u, cgltf_type_scalar);
    result.material = primitive_material;
    result.attributes = {
        {"POSITION", builder.addVec3Accessor(positions)},
        {"TEXCOORD_0", builder.addAccessor(texcoords, cgltf_component_type_r_32f, cgltf_type_vec2)},
    };
    return result;
  };
  const int mesh = builder.addMesh("illustration",
                                   {primitive(first_positions, first_texcoords, material),
                                    primitive(second_positions, second_texcoords, second_material)});
  const int illustration = builder.addNode("arx_illustration__0__illustration", mesh);
  builder.addChild(root, illustration);
  return illustration;
}

void addSoundKey(pistoris::glb::Builder& builder, int illustration, int frame, std::string_view path,
                 pistoris::SoundKind kind = pistoris::SoundKind::kEffect) {
  const int key = builder.addNode("KEY_" + std::to_string(frame) + "__key");
  builder.setNodeRotation(key, pistoris::glb_cinematic::toGlbRotation(0.0f));
  const int sound = builder.addNode(pistoris::glb_cinematic::soundName(kind));
  builder.addChild(sound, builder.addNode(pistoris::glb_cinematic::soundPathName(path)));
  builder.addChild(key, sound);
  builder.addChild(illustration, key);
}

void addValidKey(pistoris::glb::Builder& builder, int illustration, int frame) {
  const int key = builder.addNode("KEY_" + std::to_string(frame) + "__key");
  builder.setNodeRotation(key, pistoris::glb_cinematic::toGlbRotation(0.0f));
  builder.addChild(illustration, key);
}

std::vector<std::uint8_t> write(pistoris::glb::Builder& builder) {
  std::vector<std::uint8_t> encoded;
  REQUIRE(builder.write(encoded) == ARX_OK);
  return encoded;
}

nlohmann::json parseGlbJson(std::span<const std::uint8_t> source) {
  REQUIRE(source.size() >= 20U);
  std::uint32_t json_size = 0;
  std::memcpy(&json_size, source.data() + 12U, sizeof(json_size));
  REQUIRE(20U + json_size <= source.size());
  return nlohmann::json::parse(source.begin() + 20, source.begin() + 20 + json_size);
}

std::vector<std::uint8_t> replaceGlbJson(std::span<const std::uint8_t> source, const nlohmann::json& gltf) {
  REQUIRE(source.size() >= 20U);
  std::uint32_t old_json_size = 0;
  std::memcpy(&old_json_size, source.data() + 12U, sizeof(old_json_size));
  const std::size_t binary_header = 20U + old_json_size;
  REQUIRE(binary_header + 8U <= source.size());
  std::uint32_t binary_size = 0;
  std::memcpy(&binary_size, source.data() + binary_header, sizeof(binary_size));
  REQUIRE(binary_header + 8U + binary_size <= source.size());

  std::string json = gltf.dump();
  while (json.size() % 4U != 0) json.push_back(' ');
  const std::uint32_t total_size = static_cast<std::uint32_t>(20U + json.size() + 8U + binary_size);
  std::vector<std::uint8_t> result;
  result.reserve(total_size);
  const auto append_u32 = [&](std::uint32_t value) {
    const auto* bytes = reinterpret_cast<const std::uint8_t*>(&value);
    result.insert(result.end(), bytes, bytes + sizeof(value));
  };
  append_u32(0x46546C67U);
  append_u32(2U);
  append_u32(total_size);
  append_u32(static_cast<std::uint32_t>(json.size()));
  append_u32(0x4E4F534AU);
  result.insert(result.end(), json.begin(), json.end());
  append_u32(binary_size);
  append_u32(0x004E4942U);
  result.insert(result.end(),
                source.begin() + static_cast<std::ptrdiff_t>(binary_header + 8U),
                source.begin() + static_cast<std::ptrdiff_t>(binary_header + 8U + binary_size));
  return result;
}

struct DebugCapture {
  std::vector<std::string> messages;
  std::vector<std::string> info;
  std::vector<std::string> warnings;
  std::vector<std::string> errors;

  DebugCapture() {
    pistoris::setLogCallback(
        [](ArxLogLevel level, const char* message, void* userdata) {
          if (message == nullptr) return;
          auto& capture = *static_cast<DebugCapture*>(userdata);
          if (level == ARX_LOG_DEBUG) capture.messages.emplace_back(message);
          if (level == ARX_LOG_INFO) capture.info.emplace_back(message);
          if (level == ARX_LOG_WARN) capture.warnings.emplace_back(message);
          if (level == ARX_LOG_ERROR) capture.errors.emplace_back(message);
        },
        this);
  }

  ~DebugCapture() { pistoris::setLogCallback(nullptr, nullptr); }

  bool contains(std::string_view value) const {
    for (const std::string& message : messages)
      if (message.find(value) != std::string::npos) return true;
    return false;
  }

  bool containsWarning(std::string_view value) const {
    for (const std::string& warning : warnings)
      if (warning.find(value) != std::string::npos) return true;
    return false;
  }

  bool containsInfo(std::string_view value) const {
    for (const std::string& message : info)
      if (message.find(value) != std::string::npos) return true;
    return false;
  }

  bool containsError(std::string_view value) const {
    for (const std::string& error : errors)
      if (error.find(value) != std::string::npos) return true;
    return false;
  }
};

}  // namespace

TEST_SUITE("Cinematic GLB contract") {
  TEST_CASE("Levels KEY orientation while preserving its in-plane roll") {
    const auto resolve = [](const ArxQuat& authored) {
      pistoris::glb_cinematic::KeyOrientation orientation;
      REQUIRE(pistoris::glb_cinematic::resolveKeyOrientation(pistoris::math::quatToRotation(authored), orientation));
      const ArxVector3 forward = pistoris::math::rotate(orientation.rotation, {0.0f, 0.0f, -1.0f});
      CHECK(near(forward.x, 0.0f));
      CHECK(near(forward.y, -1.0f));
      CHECK(near(forward.z, 0.0f));
      return orientation;
    };

    const ArxQuat downward = pistoris::math::axisAngle(1.0f, 0.0f, 0.0f, -std::numbers::pi_v<float> * 0.5f);
    for (float visual_roll : {0.0f, 45.0f, -90.0f, 170.0f}) {
      CAPTURE(visual_roll);
      const ArxQuat authored = pistoris::math::normalize(
          pistoris::math::axisAngle(0.0f, 1.0f, 0.0f, visual_roll * std::numbers::pi_v<float> / 180.0f) * downward);
      const auto orientation = resolve(authored);
      CHECK(std::abs(orientation.roll + visual_roll) <= 1.0e-4f);
      CHECK_FALSE(orientation.corrected);
    }

    for (float native_roll : {45.0f, -90.0f, 170.0f}) {
      CAPTURE(native_roll);
      const ArxQuat exported = pistoris::math::normalize(
          pistoris::glb_cinematic::kContentBasis * pistoris::glb_cinematic::toGlbRotation(native_roll) *
          pistoris::math::conjugate(pistoris::glb_cinematic::kContentBasis));
      const ArxVector3 right = pistoris::math::rotate(exported, {1.0f, 0.0f, 0.0f});
      const float radians = native_roll * std::numbers::pi_v<float> / 180.0f;
      CHECK(near(right.x, std::cos(radians)));
      CHECK(near(right.y, 0.0f));
      CHECK(near(right.z, std::sin(radians)));
      const auto orientation = resolve(exported);
      CHECK(std::abs(orientation.roll - native_roll) <= 1.0e-4f);
    }

    const auto empty = resolve(pistoris::math::kIdentityQuat);
    CHECK(near(empty.roll, 0.0f));
    CHECK(empty.corrected);

    for (float direction : {-1.0f, 1.0f}) {
      const auto sideways =
          resolve(pistoris::math::axisAngle(0.0f, 0.0f, 1.0f, direction * std::numbers::pi_v<float> * 0.5f));
      CHECK(near(sideways.roll, direction * -90.0f));
      CHECK(sideways.corrected);
    }

    const auto upward = resolve(pistoris::math::axisAngle(1.0f, 0.0f, 0.0f, std::numbers::pi_v<float> * 0.5f));
    CHECK(near(upward.roll, 0.0f));
    CHECK(upward.corrected);

    const ArxQuat rolled = pistoris::math::normalize(
        pistoris::math::axisAngle(0.0f, 1.0f, 0.0f, 37.0f * std::numbers::pi_v<float> / 180.0f) * downward);
    const auto tilted = resolve(pistoris::math::axisAngle(1.0f, 0.0f, 0.0f, 0.4f) * rolled);
    CHECK(near(tilted.roll, -37.0f));
    CHECK(tilted.corrected);
  }

  TEST_CASE("Roundtrips semantic names including sound path separators") {
    pistoris::CinematicKeyframe source;
    source.frame = 12;
    source.interpolation = pistoris::CinematicInterpolation::kBezier;
    source.outgoing_speed = 1.25f;
    source.base_effect = pistoris::CinematicBaseEffect::kFadeOut;
    source.color = {1, 2, 3};
    source.secondary_color = {4, 5, 6};
    source.crossfade = true;
    source.dream = true;

    pistoris::CinematicKeyframe key;
    REQUIRE(pistoris::glb_cinematic::parseKeyName(pistoris::glb_cinematic::keyName(source), key));
    CHECK(key.frame == 12);
    CHECK(key.interpolation == pistoris::CinematicInterpolation::kBezier);
    CHECK(near(key.outgoing_speed, 1.25f));
    CHECK(key.base_effect == pistoris::CinematicBaseEffect::kFadeOut);
    CHECK(key.color.r == 1);
    CHECK(key.secondary_color.b == 6);
    CHECK(key.crossfade);
    CHECK(key.dream);

    REQUIRE(pistoris::glb_cinematic::parseKeyName("KEY_0__FADE_IN__COLOR_1_0.5_0__SECONDARY_0_0_0__key", key));
    CHECK(key.color.r == 255);
    CHECK(key.color.g == 128);
    CHECK(key.color.b == 0);
    pistoris::CinematicKeyframe hidden_flash;
    hidden_flash.post_effect = pistoris::CinematicPostEffect::kSuppressFlash;
    CHECK(pistoris::glb_cinematic::flashName(hidden_flash) == "FLASH__HIDDEN__flash");

    const std::string logical_path = "effects/my__sound.v2";
    std::string parsed_path;
    REQUIRE(
        pistoris::glb_cinematic::parseSoundPathName(pistoris::glb_cinematic::soundPathName(logical_path), parsed_path));
    CHECK(parsed_path == logical_path);

    pistoris::SoundKind kind = pistoris::SoundKind::kEffect;
    REQUIRE(pistoris::glb_cinematic::parseSoundName(pistoris::glb_cinematic::soundName(pistoris::SoundKind::kSpeech),
                                                    kind));
    CHECK(kind == pistoris::SoundKind::kSpeech);
  }

  TEST_CASE("Parses unlabeled Cinematic names without treating options as labels") {
    pistoris::glb::ParsedLabel label;
    pistoris::glb_cinematic::RootName root;
    REQUIRE(pistoris::glb_cinematic::parseRootName("arx_cinematic__FPS_30", root, &label));
    CHECK(label.presence == pistoris::glb::LabelPresence::kMissing);
    REQUIRE(root.fps.has_value());
    CHECK(near(*root.fps, 30.0f));
    REQUIRE(pistoris::glb_cinematic::parseRootName("arx_cinematic__FPS_30__cinematic", root, &label));
    CHECK(label.presence == pistoris::glb::LabelPresence::kPresent);
    REQUIRE(pistoris::glb_cinematic::parseRootName("arx_cinematic__END_90", root, &label));
    REQUIRE(root.end_frame.has_value());
    CHECK(*root.end_frame == 90);

    pistoris::glb_cinematic::IllustrationName illustration;
    REQUIRE(pistoris::glb_cinematic::parseIllustrationName("arx_illustration__2__SUBDIVISION_8", illustration, &label));
    CHECK(label.presence == pistoris::glb::LabelPresence::kMissing);
    CHECK(illustration.ordinal == 2);
    CHECK(illustration.subdivision_scale == 8);

    pistoris::CinematicKeyframe key;
    REQUIRE(pistoris::glb_cinematic::parseKeyName("KEY_7__BEZIER", key, &label));
    CHECK(label.presence == pistoris::glb::LabelPresence::kMissing);
    CHECK(key.frame == 7);
    CHECK(key.interpolation == pistoris::CinematicInterpolation::kBezier);
    REQUIRE(pistoris::glb_cinematic::parseKeyName("KEY_7__BEZIER__key", key, &label));
    CHECK(label.presence == pistoris::glb::LabelPresence::kPresent);
    REQUIRE(pistoris::glb_cinematic::parseKeyName("KEY_7__DREAM.001", key, &label));
    CHECK(label.presence == pistoris::glb::LabelPresence::kPresent);
    CHECK(label.text == "DREAM.001");
    CHECK_FALSE(key.dream);
    REQUIRE(pistoris::glb_cinematic::parseFlashName("FLASH__HIDDEN", key, &label));
    CHECK(label.presence == pistoris::glb::LabelPresence::kMissing);
    CHECK(key.post_effect == pistoris::CinematicPostEffect::kSuppressFlash);

    pistoris::CinematicLight light;
    REQUIRE(pistoris::glb_cinematic::parseLightName("LIGHT__OFF", light, &label));
    CHECK(label.presence == pistoris::glb::LabelPresence::kMissing);
    CHECK(light.intensity < 0.0f);
    pistoris::SoundKind kind = pistoris::SoundKind::kEffect;
    REQUIRE(pistoris::glb_cinematic::parseSoundName("SOUND__SPEECH", kind, &label));
    CHECK(label.presence == pistoris::glb::LabelPresence::kMissing);
    CHECK(kind == pistoris::SoundKind::kSpeech);

    std::string path;
    CHECK_FALSE(pistoris::glb_cinematic::parseSoundPathName("PATH_voice/line", path));
    CHECK_FALSE(pistoris::glb_cinematic::parseRootName("arx_cinematic__FPS_bad", root));
    CHECK_FALSE(pistoris::glb_cinematic::parseRootName("arx_cinematic__FPS_bad__root", root));
    CHECK_FALSE(pistoris::glb_cinematic::parseRootName("arx_cinematic__END_bad", root));
    CHECK_FALSE(pistoris::glb_cinematic::parseKeyName("KEY_7__SPEED_bad", key));
    CHECK_FALSE(pistoris::glb_cinematic::parseKeyName("KEY_7__BEZIER__NONE__key", key));
  }

  TEST_CASE("Imports unlabeled semantic nodes and ignores unrelated helper names") {
    pistoris::glb::Builder builder;
    const int root = builder.addNode("arx_cinematic__FPS_30");
    builder.addRoot(root);
    const int illustration = addIllustration(builder, root, ImageSource::kEmbedded);
    const int key = builder.addNode("KEY_0__BEZIER");
    builder.setNodeRotation(key, pistoris::glb_cinematic::toGlbRotation(0.0f));
    builder.addChild(key, builder.addNode("FLASH__HIDDEN"));
    builder.addChild(key, builder.addNode("LIGHT__OFF"));
    const int sound = builder.addNode("SOUND__EFFECT");
    builder.addChild(sound, builder.addNode("PATH_effects/hit__path"));
    builder.addChild(sound, builder.addNode("DECORATION__sound_child"));
    builder.addChild(key, sound);
    for (std::string_view name : {"FLASHLIGHT", "LIGHTING", "SOUNDTRACK", "PRESENTATIONAL"})
      builder.addChild(key, builder.addNode(std::string(name)));
    builder.addChild(illustration, key);
    addValidKey(builder, illustration, 10);

    std::vector<std::uint8_t> encoded = write(builder);
    nlohmann::json gltf = parseGlbJson(encoded);
    gltf["nodes"][static_cast<std::size_t>(illustration)]["name"] = "arx_illustration__0";

    DebugCapture logs;
    pistoris::Cinematic cinematic;
    REQUIRE(pistoris::Cinematic::importGlb(cinematic, replaceGlbJson(encoded, gltf)) == ARX_OK);
    CHECK(near(cinematic.fps(), 30.0f));
    std::array<ArxCinematicKeyframe, 2> keys{};
    REQUIRE(cinematic.copyKeyframes(0, keys.size(), keys.data()) == ARX_OK);
    CHECK(keys[0].interpolation == ARX_CINEMATIC_INTERPOLATION_BEZIER);
    CHECK(keys[0].post_effect == ARX_CINEMATIC_POST_EFFECT_SUPPRESS_FLASH);
    CHECK(keys[0].light_active == 1);
    CHECK(keys[0].sound != ARX_NO_SOUND_HANDLE);
    CHECK(logs.containsWarning("arx_cinematic__FPS_30' has no final label"));
    CHECK(logs.containsWarning("arx_illustration__0' has no final label"));
    CHECK(logs.containsWarning("KEY_0__BEZIER' has no final label"));
    CHECK(logs.containsWarning("FLASH__HIDDEN' has no final label"));
    CHECK(logs.containsWarning("LIGHT__OFF' has no final label"));
    CHECK(logs.containsWarning("SOUND__EFFECT' has no final label"));
    CHECK(logs.containsWarning("unexpected SOUND child 'DECORATION__sound_child' ignored"));
    CHECK(logs.containsWarning("unexpected key helper 'LIGHTING' ignored"));
    CHECK(logs.containsWarning("unexpected key helper 'PRESENTATIONAL' ignored"));
  }

  TEST_CASE("Requires exactly one valid PATH child on a claimed SOUND helper") {
    const auto import_with_paths = [](int path_count, std::string_view sound_name) {
      pistoris::glb::Builder builder;
      const int root = builder.addNode("arx_cinematic__cinematic");
      builder.addRoot(root);
      const int illustration = addIllustration(builder, root, ImageSource::kEmbedded);
      const int key = builder.addNode("KEY_0__key");
      builder.setNodeRotation(key, pistoris::glb_cinematic::toGlbRotation(0.0f));
      const int sound = builder.addNode(std::string(sound_name));
      for (int index = 0; index < path_count; ++index)
        builder.addChild(sound, builder.addNode("PATH_effects/hit__path"));
      builder.addChild(key, sound);
      builder.addChild(illustration, key);
      addValidKey(builder, illustration, 1);
      pistoris::Cinematic cinematic;
      return pistoris::Cinematic::importGlb(cinematic, write(builder));
    };
    CHECK(import_with_paths(0, "SOUND__EFFECT__sound") == ARX_GLB_BAD_CINEMATIC_HELPER);
    CHECK(import_with_paths(2, "SOUND__EFFECT__sound") == ARX_GLB_BAD_CINEMATIC_HELPER);
    CHECK(import_with_paths(1, "SOUND__OTHER__sound") == ARX_GLB_BAD_CINEMATIC_HELPER);
    CHECK(import_with_paths(1, "SOUNDTRACK") == ARX_OK);
  }

  TEST_CASE("Silently ignores presentation transforms on helper nodes") {
    pistoris::glb::Builder builder;
    const int root = builder.addNode("arx_cinematic__cinematic");
    builder.addRoot(root);
    const int illustration = addIllustration(builder, root, ImageSource::kEmbedded);

    const int first = builder.addNode("KEY_0__key");
    builder.setNodeRotation(first, pistoris::glb_cinematic::toGlbRotation(0.0f));
    const int flash = builder.addNode("FLASH__RGB_1_0_0__DECAY_0.5__flash");
    const int light_off = builder.addNode("LIGHT__OFF__light");
    const int sound = builder.addNode("SOUND__EFFECT__sound");
    const int path = builder.addNode("PATH_effects/hit__path");
    builder.addChild(sound, path);
    builder.addChild(first, flash);
    builder.addChild(first, light_off);
    builder.addChild(first, sound);
    builder.addChild(illustration, first);

    const int second = builder.addNode("KEY_1__key");
    builder.setNodeRotation(second, pistoris::glb_cinematic::toGlbRotation(0.0f));
    const int light = builder.addNode("LIGHT__FALL_IN_2__FALL_OUT_8__RGB_1_1_1__INTENSITY_0.8__RANDOM_0__light");
    builder.setNodeTranslation(light, {1.0f, 0.0f, -3.5f});
    builder.addChild(second, light);
    builder.addChild(illustration, second);

    const std::vector<std::uint8_t> encoded = write(builder);
    nlohmann::json gltf = parseGlbJson(encoded);
    const std::array<int, 4> presentation_nodes = {flash, light_off, sound, path};
    for (int node : presentation_nodes) {
      gltf["nodes"][static_cast<std::size_t>(node)]["translation"] = {0.25f, -0.5f, 1.0f};
      gltf["nodes"][static_cast<std::size_t>(node)]["rotation"] = {0.0f, 0.0f, 0.38268343f, 0.92387953f};
      gltf["nodes"][static_cast<std::size_t>(node)]["scale"] = {2.0f, 3.0f, 4.0f};
    }
    gltf["nodes"][static_cast<std::size_t>(light)]["rotation"] = {0.0f, 0.0f, 0.38268343f, 0.92387953f};
    gltf["nodes"][static_cast<std::size_t>(light)]["scale"] = {2.0f, 3.0f, 4.0f};

    DebugCapture logs;
    pistoris::Cinematic cinematic;
    REQUIRE(pistoris::Cinematic::importGlb(cinematic, replaceGlbJson(encoded, gltf)) == ARX_OK);
    CHECK(logs.warnings.empty());
    std::array<ArxCinematicKeyframe, 2> keys{};
    REQUIRE(cinematic.copyKeyframes(0, keys.size(), keys.data()) == ARX_OK);
    CHECK(keys[0].post_effect == ARX_CINEMATIC_POST_EFFECT_FLASH);
    CHECK(keys[0].light_active == 1);
    CHECK(keys[0].light.intensity < 0.0f);
    CHECK(keys[0].sound != ARX_NO_SOUND_HANDLE);
    CHECK(keys[1].light_active == 1);
    CHECK(keys[1].light.intensity == doctest::Approx(0.8f));
  }

  TEST_CASE("Fills a missing frame zero with a cue-free visual hold") {
    pistoris::glb::Builder builder;
    const int root = builder.addNode("arx_cinematic__FPS_30__cinematic");
    builder.addRoot(root);
    const int illustration = addIllustration(builder, root, ImageSource::kEmbedded);
    const int first = builder.addNode("KEY_5__SPEED_2__FADE_IN__COLOR_1_1_1__SECONDARY_0_0_0__CROSSFADE__DREAM__key");
    builder.setNodeRotation(first, pistoris::glb_cinematic::toGlbRotation(25.0f));
    builder.setNodeTranslation(first, {0.25f, 1.0f, 0.0f});
    builder.addChild(first, builder.addNode("FLASH__RGB_1_0_0__DECAY_0.5__flash"));
    builder.addChild(first, builder.addNode("LIGHT__OFF__light"));
    const int sound = builder.addNode("SOUND__EFFECT__sound");
    builder.addChild(sound, builder.addNode("PATH_effects/start__path"));
    builder.addChild(first, sound);
    builder.addChild(illustration, first);
    addValidKey(builder, illustration, 10);

    DebugCapture logs;
    pistoris::Cinematic cinematic;
    REQUIRE(pistoris::Cinematic::importGlb(cinematic, write(builder)) == ARX_OK);
    std::array<ArxCinematicKeyframe, 3> keys{};
    REQUIRE(cinematic.copyKeyframes(0, keys.size(), keys.data()) == ARX_OK);
    CHECK(keys[0].frame == 0);
    CHECK(keys[1].frame == 5);
    CHECK(keys[2].frame == 10);
    CHECK(keys[0].illustration == keys[1].illustration);
    CHECK(near(keys[0].camera_position.x, keys[1].camera_position.x));
    CHECK(near(keys[0].camera_position.y, keys[1].camera_position.y));
    CHECK(near(keys[0].camera_position.z, keys[1].camera_position.z));
    CHECK(near(keys[0].camera_roll, keys[1].camera_roll));
    CHECK(keys[0].light_active == keys[1].light_active);
    CHECK(keys[0].light.intensity == keys[1].light.intensity);
    CHECK(keys[0].interpolation == ARX_CINEMATIC_INTERPOLATION_NONE);
    CHECK(near(keys[0].outgoing_speed, 1.0f));
    CHECK(keys[0].sound == ARX_NO_SOUND_HANDLE);
    CHECK(keys[0].base_effect == ARX_CINEMATIC_BASE_EFFECT_NONE);
    CHECK(keys[0].post_effect == ARX_CINEMATIC_POST_EFFECT_NONE);
    CHECK(keys[0].crossfade == 0);
    CHECK(keys[0].dream == 0);
    CHECK(keys[1].sound != ARX_NO_SOUND_HANDLE);
    CHECK(keys[1].post_effect == ARX_CINEMATIC_POST_EFFECT_FLASH);
    CHECK(keys[1].base_effect == ARX_CINEMATIC_BASE_EFFECT_FADE_IN);
    CHECK(near(keys[1].outgoing_speed, 2.0f));
    CHECK(logs.containsWarning("KEY_0 missing; inserted a static hold through frame 5"));
  }

  TEST_CASE("Reports malformed key timelines and transforms at the import boundary") {
    pistoris::glb::Builder builder;
    const int root = builder.addNode("arx_cinematic__cinematic");
    builder.addRoot(root);
    const int illustration = addIllustration(builder, root, ImageSource::kEmbedded);

    SUBCASE("duplicate frame") {
      addValidKey(builder, illustration, 0);
      addValidKey(builder, illustration, 0);
      DebugCapture logs;
      pistoris::Cinematic cinematic;
      CHECK(pistoris::Cinematic::importGlb(cinematic, write(builder)) == ARX_CINEMATIC_BAD_KEY_FRAME);
      CHECK(logs.containsError("duplicate KEY frame 0"));
    }
    SUBCASE("negative first frame") {
      addValidKey(builder, illustration, -1);
      addValidKey(builder, illustration, 1);
      DebugCapture logs;
      pistoris::Cinematic cinematic;
      CHECK(pistoris::Cinematic::importGlb(cinematic, write(builder)) == ARX_CINEMATIC_BAD_KEY_FRAME);
      CHECK(logs.containsError("KEY frame -1 is negative"));
    }
    SUBCASE("invalid key scale") {
      const int key = builder.addNode("KEY_0__key");
      builder.addChild(illustration, key);
      addValidKey(builder, illustration, 1);
      std::vector<std::uint8_t> encoded = write(builder);
      nlohmann::json gltf = parseGlbJson(encoded);
      gltf["nodes"][static_cast<std::size_t>(key)]["scale"] = {0.0f, 1.0f, 1.0f};
      DebugCapture logs;
      pistoris::Cinematic cinematic;
      CHECK(pistoris::Cinematic::importGlb(cinematic, replaceGlbJson(encoded, gltf)) ==
            ARX_GLB_BAD_CINEMATIC_KEY_TRANSFORM);
      CHECK(logs.containsError("KEY 'KEY_0__key' has an invalid transform or nonpositive scale"));
    }
    SUBCASE("claimed malformed helper") {
      const int key = builder.addNode("KEY_0__key");
      builder.setNodeRotation(key, pistoris::glb_cinematic::toGlbRotation(0.0f));
      builder.addChild(key, builder.addNode("LIGHT__BAD__light"));
      builder.addChild(illustration, key);
      addValidKey(builder, illustration, 1);
      pistoris::Cinematic cinematic;
      CHECK(pistoris::Cinematic::importGlb(cinematic, write(builder)) == ARX_GLB_BAD_CINEMATIC_HELPER);
    }
  }

  TEST_CASE("Exports camera views and separates illustration workspaces") {
    pistoris::glb::Builder builder;
    const int root = builder.addNode("arx_cinematic__cinematic");
    builder.addRoot(root);
    for (std::uint32_t index = 0; index < 5; ++index) {
      const int illustration = addIllustration(builder, root, ImageSource::kEmbedded, index);
      addValidKey(builder, illustration, static_cast<int>(index));
    }

    pistoris::Cinematic cinematic;
    REQUIRE(pistoris::Cinematic::importGlb(cinematic, write(builder)) == ARX_OK);
    std::vector<std::uint8_t> encoded;
    REQUIRE(cinematic.exportGlb(encoded) == ARX_OK);
    pistoris::glb::Asset asset;
    REQUIRE(pistoris::glb::parse(encoded, asset) == ARX_OK);
    REQUIRE(asset.data()->cameras_count == 1);
    CHECK(asset.data()->cameras[0].type == cgltf_camera_type_perspective);
    const auto& perspective = asset.data()->cameras[0].data.perspective;
    CHECK(near(perspective.yfov, pistoris::glb_cinematic::gameVerticalFov()));
    CHECK(near(perspective.aspect_ratio, 4.0f / 3.0f));

    std::array<const cgltf_node*, 5> illustrations{};
    const cgltf_node* first_key = nullptr;
    std::size_t cameras_on_keys = 0;
    for (std::size_t index = 0; index < asset.data()->nodes_count; ++index) {
      const cgltf_node& node = asset.data()->nodes[index];
      const std::string_view name = node.name != nullptr ? node.name : "";
      pistoris::glb_cinematic::IllustrationName parsed;
      if (pistoris::glb_cinematic::parseIllustrationName(name, parsed)) {
        REQUIRE(parsed.ordinal < illustrations.size());
        illustrations[parsed.ordinal] = &node;
      }
      if (name.starts_with("KEY_")) {
        if (first_key == nullptr) first_key = &node;
        CHECK(node.camera == &asset.data()->cameras[0]);
        ++cameras_on_keys;
      }
    }
    CHECK(cameras_on_keys == 5);
    REQUIRE(first_key != nullptr);
    std::array<cgltf_float, 16> key_transform{};
    cgltf_node_transform_local(first_key, key_transform.data());
    CHECK(key_transform[0] > 0.99f);
    CHECK(key_transform[6] < -0.99f);
    CHECK(key_transform[9] > 0.99f);
    for (const cgltf_node* node : illustrations) REQUIRE(node != nullptr);
    CHECK(near(illustrations[0]->translation[0], 0.0f));
    CHECK(near(illustrations[1]->translation[0], 0.52f));
    CHECK(near(illustrations[2]->translation[0], 1.04f));
    CHECK(near(illustrations[3]->translation[0], 0.0f));
    CHECK(near(illustrations[3]->translation[2], 0.51f));
    CHECK(near(illustrations[4]->translation[2], 0.51f));

    const cgltf_primitive& primitive = illustrations[0]->mesh->primitives[0];
    const cgltf_accessor* positions = cgltf_find_accessor(&primitive, cgltf_attribute_type_position, 0);
    const cgltf_accessor* texcoords = cgltf_find_accessor(&primitive, cgltf_attribute_type_texcoord, 0);
    REQUIRE(positions != nullptr);
    REQUIRE(texcoords != nullptr);
    REQUIRE(primitive.indices != nullptr);
    pistoris::glb::AccessorCache accessors(asset, 2);
    const pistoris::glb::AccessorView* vertices = nullptr;
    const pistoris::glb::AccessorView* uv = nullptr;
    REQUIRE(accessors.get(positions, vertices) == ARX_OK);
    REQUIRE(accessors.get(texcoords, uv) == ARX_OK);
    CHECK(pistoris::glb::readVec3(*vertices, 0).z < 0.0f);
    CHECK(pistoris::glb::readVec3(*vertices, 2).z > 0.0f);
    CHECK(near(pistoris::glb::readVec2(*uv, 0).x, 0.0f));
    CHECK(near(pistoris::glb::readVec2(*uv, 1).x, 1.0f));
    CHECK(near(pistoris::glb::readVec2(*uv, 0).y, 0.0f));
    CHECK(near(pistoris::glb::readVec2(*uv, 2).y, 1.0f));
    const pistoris::glb::Vec3 a = pistoris::glb::readVec3(*vertices, cgltf_accessor_read_index(primitive.indices, 0));
    const pistoris::glb::Vec3 b = pistoris::glb::readVec3(*vertices, cgltf_accessor_read_index(primitive.indices, 1));
    const pistoris::glb::Vec3 c = pistoris::glb::readVec3(*vertices, cgltf_accessor_read_index(primitive.indices, 2));
    CHECK((b.z - a.z) * (c.x - a.x) - (b.x - a.x) * (c.z - a.z) > 0.0f);
  }

  TEST_CASE("Places negative-depth cameras above their illustration") {
    pistoris::glb::Builder builder;
    const int root = builder.addNode("arx_cinematic__cinematic");
    builder.addRoot(root);
    const int illustration = addIllustration(builder, root, ImageSource::kEmbedded);
    for (int frame : {0, 10}) {
      const int key = builder.addNode("KEY_" + std::to_string(frame) + "__key");
      builder.setNodeTranslation(key, {0.0f, 1.0f, 0.0f});
      builder.setNodeRotation(key, pistoris::glb_cinematic::toGlbRotation(0.0f));
      builder.addChild(illustration, key);
    }

    pistoris::Cinematic cinematic;
    REQUIRE(pistoris::Cinematic::importGlb(cinematic, write(builder)) == ARX_OK);
    std::array<ArxCinematicKeyframe, 2> keys{};
    REQUIRE(cinematic.copyKeyframes(0, keys.size(), keys.data()) == ARX_OK);
    CHECK(near(keys[0].camera_position.z, -1.0f));

    std::vector<std::uint8_t> encoded;
    REQUIRE(cinematic.exportGlb(encoded) == ARX_OK);
    pistoris::glb::Asset asset;
    REQUIRE(pistoris::glb::parse(encoded, asset) == ARX_OK);
    const cgltf_node* exported_key = nullptr;
    for (std::size_t index = 0; index < asset.data()->nodes_count; ++index) {
      const cgltf_node& node = asset.data()->nodes[index];
      if (node.name != nullptr && std::string_view(node.name).starts_with("KEY_")) exported_key = &node;
    }
    REQUIRE(exported_key != nullptr);
    CHECK(near(exported_key->translation[1], 0.01f));
    std::array<cgltf_float, 16> transform{};
    cgltf_node_transform_local(exported_key, transform.data());
    CHECK(transform[9] > 0.99f);
  }

  TEST_CASE("Measures camera depth from referenced illustration vertices") {
    const std::array<std::array<float, 4>, 2> heights = {{{1.0f, 1.0f, 1.0f, 1.0f}, {0.0f, 0.0f, 0.0f, 4.0f}}};
    for (const auto& mesh_heights : heights) {
      CAPTURE(mesh_heights[0]);
      CAPTURE(mesh_heights[3]);
      pistoris::glb::Builder builder;
      const int root = builder.addNode("arx_cinematic__cinematic");
      builder.addRoot(root);
      const int illustration =
          addIllustration(builder, root, ImageSource::kEmbedded, 0, "story/scene.png", mesh_heights);
      for (int frame : {0, 10}) {
        const int key = builder.addNode("KEY_" + std::to_string(frame) + "__key");
        builder.setNodeTranslation(key, {0.0f, 3.0f, 0.0f});
        builder.setNodeRotation(key, pistoris::glb_cinematic::toGlbRotation(0.0f));
        builder.addChild(illustration, key);
      }

      pistoris::Cinematic cinematic;
      REQUIRE(pistoris::Cinematic::importGlb(cinematic, write(builder)) == ARX_OK);
      std::array<ArxCinematicKeyframe, 2> keys{};
      REQUIRE(cinematic.copyKeyframes(0, keys.size(), keys.data()) == ARX_OK);
      CHECK(near(keys[0].camera_position.z, -2.0f));
      CHECK(near(keys[1].camera_position.z, -2.0f));
    }
  }

  TEST_CASE("Applies illustration scale to the image chart and camera depth") {
    pistoris::glb::Builder builder;
    const int root = builder.addNode("arx_cinematic__cinematic");
    builder.addRoot(root);
    const int illustration =
        addIllustration(builder, root, ImageSource::kEmbedded, 0, "story/scene.png", {1.0f, 1.0f, 1.0f, 1.0f});
    const int key = builder.addNode("KEY_0__key");
    builder.setNodeTranslation(key, {0.5f, 3.0f, 0.25f});
    builder.setNodeRotation(key, pistoris::glb_cinematic::toGlbRotation(0.0f));
    builder.addChild(illustration, key);
    addValidKey(builder, illustration, 10);
    const std::vector<std::uint8_t> encoded = write(builder);
    const nlohmann::json original = parseGlbJson(encoded);

    const auto check_scale = [&](std::array<float, 3> scale, float expected_depth) {
      nlohmann::json gltf = original;
      gltf["nodes"][illustration]["scale"] = scale;
      gltf["nodes"][key]["scale"] = {0.5f, 1.0f, 1.0f};
      pistoris::Cinematic cinematic;
      REQUIRE(pistoris::Cinematic::importGlb(cinematic, replaceGlbJson(encoded, gltf)) == ARX_OK);
      ArxCinematicKeyframe imported{};
      REQUIRE(cinematic.copyKeyframes(0, 1, &imported) == ARX_OK);
      CHECK(near(imported.camera_position.x, 0.5f));
      CHECK(near(imported.camera_position.y, 0.25f));
      CHECK(near(imported.camera_position.z, expected_depth));
    };

    check_scale({5.0f, 1.0f, 1.0f}, -2.0f);
    check_scale({2.0f, 2.0f, 4.0f}, -1.0f);
    check_scale({3.0f, 3.0f, 3.0f}, -2.0f);
  }

  TEST_CASE("Uses scaled bounds when illustration UVs are absent") {
    pistoris::glb::Builder builder;
    const int root = builder.addNode("arx_cinematic__cinematic");
    builder.addRoot(root);
    const int illustration = addIllustration(builder, root, ImageSource::kEmbedded);
    const int key = builder.addNode("KEY_0__key");
    builder.setNodeTranslation(key, {0.5f, 1.0f, 0.25f});
    builder.setNodeRotation(key, pistoris::glb_cinematic::toGlbRotation(0.0f));
    builder.addChild(illustration, key);
    addValidKey(builder, illustration, 10);
    const std::vector<std::uint8_t> encoded = write(builder);
    nlohmann::json gltf = parseGlbJson(encoded);
    gltf["meshes"][0]["primitives"][0]["attributes"].erase("TEXCOORD_0");
    gltf["nodes"][illustration]["scale"] = {2.0f, 3.0f, 4.0f};

    pistoris::Cinematic cinematic;
    REQUIRE(pistoris::Cinematic::importGlb(cinematic, replaceGlbJson(encoded, gltf)) == ARX_OK);
    ArxCinematicKeyframe imported{};
    REQUIRE(cinematic.copyKeyframes(0, 1, &imported) == ARX_OK);
    CHECK(near(imported.camera_position.x, 0.5f));
    CHECK(near(imported.camera_position.y, 0.25f));
    CHECK(near(imported.camera_position.z, -0.75f));
  }

  TEST_CASE("Ignores organizational transforms around scaled illustrations") {
    pistoris::glb::Builder builder;
    const int root = builder.addNode("arx_cinematic__cinematic");
    builder.addRoot(root);
    const int illustration = addIllustration(builder, root, ImageSource::kEmbedded);
    const int key = builder.addNode("KEY_0__key");
    builder.setNodeTranslation(key, {0.25f, 1.0f, 0.0f});
    builder.setNodeRotation(key, pistoris::glb_cinematic::toGlbRotation(0.0f));
    builder.addChild(illustration, key);
    addValidKey(builder, illustration, 10);
    const std::vector<std::uint8_t> encoded = write(builder);
    nlohmann::json gltf = parseGlbJson(encoded);
    gltf["nodes"][illustration]["scale"] = {2.0f, 1.0f, 1.0f};
    pistoris::Cinematic baseline;
    REQUIRE(pistoris::Cinematic::importGlb(baseline, replaceGlbJson(encoded, gltf)) == ARX_OK);
    ArxCinematicKeyframe expected{};
    REQUIRE(baseline.copyKeyframes(0, 1, &expected) == ARX_OK);

    gltf["nodes"][root]["translation"] = {20.0f, -3.0f, 5.0f};
    gltf["nodes"][root]["rotation"] = {0.0f, 0.0f, 0.70710678f, 0.70710678f};
    gltf["nodes"][illustration]["translation"] = {-4.0f, 7.0f, 2.0f};
    gltf["nodes"][illustration]["rotation"] = {0.0f, 0.70710678f, 0.0f, 0.70710678f};
    pistoris::Cinematic moved;
    REQUIRE(pistoris::Cinematic::importGlb(moved, replaceGlbJson(encoded, gltf)) == ARX_OK);
    ArxCinematicKeyframe imported{};
    REQUIRE(moved.copyKeyframes(0, 1, &imported) == ARX_OK);
    CHECK(near(imported.camera_position.x, expected.camera_position.x));
    CHECK(near(imported.camera_position.y, expected.camera_position.y));
    CHECK(near(imported.camera_position.z, expected.camera_position.z));
  }

  TEST_CASE("Rejects nonpositive illustration and key scale at their own boundary") {
    pistoris::glb::Builder builder;
    const int root = builder.addNode("arx_cinematic__cinematic");
    builder.addRoot(root);
    const int illustration = addIllustration(builder, root, ImageSource::kEmbedded);
    const int key = builder.addNode("KEY_0__key");
    builder.setNodeRotation(key, pistoris::glb_cinematic::toGlbRotation(0.0f));
    builder.addChild(illustration, key);
    addValidKey(builder, illustration, 10);
    const std::vector<std::uint8_t> encoded = write(builder);
    const nlohmann::json original = parseGlbJson(encoded);

    constexpr std::array<std::array<float, 3>, 3> kInvalidScales = {{
        {1.0f, 0.0f, 1.0f},
        {1.0f, -1.0f, 1.0f},
        {-1.0f, -1.0f, 1.0f},
    }};
    for (const std::array<float, 3>& invalid : kInvalidScales) {
      CAPTURE(invalid[0]);
      CAPTURE(invalid[1]);
      nlohmann::json gltf = original;
      gltf["nodes"][illustration]["scale"] = invalid;
      pistoris::Cinematic cinematic;
      CHECK(pistoris::Cinematic::importGlb(cinematic, replaceGlbJson(encoded, gltf)) ==
            ARX_GLB_BAD_CINEMATIC_ILLUSTRATION);

      gltf = original;
      gltf["nodes"][key]["scale"] = invalid;
      CHECK(pistoris::Cinematic::importGlb(cinematic, replaceGlbJson(encoded, gltf)) ==
            ARX_GLB_BAD_CINEMATIC_KEY_TRANSFORM);
    }
  }

  TEST_CASE("Reports a key whose projected depth cannot be represented") {
    pistoris::glb::Builder builder;
    const int root = builder.addNode("arx_cinematic__cinematic");
    builder.addRoot(root);
    std::vector<std::uint8_t> large_image;
    REQUIRE(pistoris::image::transcodeToPng(makeSolidTestBmp(128, 64), large_image) == pistoris::image::Error::kNone);
    const int illustration =
        addIllustration(builder, root, ImageSource::kEmbedded, 0, "story/scene.png", {}, large_image);
    const int key = builder.addNode("KEY_0__key");
    builder.setNodeRotation(key, pistoris::glb_cinematic::toGlbRotation(0.0f));
    builder.setNodeTranslation(key, {0.0f, 3.0e38f, 0.0f});
    builder.addChild(illustration, key);
    addValidKey(builder, illustration, 10);
    pistoris::Cinematic cinematic;
    DebugCapture logs;
    CHECK(pistoris::Cinematic::importGlb(cinematic, write(builder)) == ARX_GLB_BAD_CINEMATIC_KEY_PLACEMENT);
    CHECK(logs.containsError("KEY_0__key"));
  }

  TEST_CASE("Reports missing and convention-looking key labels") {
    pistoris::glb::Builder builder;
    const int root = builder.addNode("arx_cinematic__cinematic");
    builder.addRoot(root);
    const int illustration = addIllustration(builder, root, ImageSource::kEmbedded);
    const int first = builder.addNode("KEY_0");
    builder.setNodeRotation(first, pistoris::glb_cinematic::toGlbRotation(0.0f));
    builder.addChild(illustration, first);
    const int last = builder.addNode("KEY_10__FINAL_LABEL");
    builder.setNodeRotation(last, pistoris::glb_cinematic::toGlbRotation(0.0f));
    builder.addChild(illustration, last);
    pistoris::Cinematic cinematic;
    DebugCapture logs;
    CHECK(pistoris::Cinematic::importGlb(cinematic, write(builder)) == ARX_OK);
    CHECK(logs.containsWarning("has no final label"));
    CHECK(logs.containsInfo("FINAL_LABEL"));
  }

  TEST_CASE("Identifies the Cinematic node that failed import at debug level") {
    pistoris::glb::Builder builder;
    const int root = builder.addNode("arx_cinematic__cinematic");
    builder.addRoot(root);
    const int illustration = addIllustration(builder, root, ImageSource::kEmbedded);
    builder.addChild(illustration, builder.addNode("KEY_bad__key"));

    const std::vector<std::uint8_t> encoded = write(builder);
    DebugCapture logs;
    pistoris::Cinematic cinematic;
    CHECK(pistoris::Cinematic::importGlb(cinematic, encoded) == ARX_GLB_BAD_CINEMATIC_KEY_NAME);
    CHECK(logs.contains("processing root child node"));
    CHECK(logs.contains("processing illustration child node"));
    CHECK(logs.contains("KEY_bad__key' failed with code"));
  }

  TEST_CASE("Maps camera FOV and light states into Cinematic values") {
    pistoris::glb::Builder builder;
    const int root = builder.addNode("arx_cinematic__cinematic");
    builder.addRoot(root);
    const int illustration = addIllustration(builder, root, ImageSource::kEmbedded);
    const int camera = builder.addPerspectiveCamera("camera",
                                                    {.vertical_fov = pistoris::glb_cinematic::gameVerticalFov(),
                                                     .aspect_ratio = 4.0f / 3.0f,
                                                     .znear = 0.01f,
                                                     .zfar = {}});
    for (int frame = 0; frame < 3; ++frame) {
      const int key = builder.addNode("KEY_" + std::to_string(frame) + "__key");
      builder.setNodeRotation(key, pistoris::glb_cinematic::toGlbRotation(0.0f));
      builder.setNodeTranslation(key, {0.0f, -1.0f, 0.0f});
      builder.setNodeCamera(key, camera);
      builder.addChild(illustration, key);
      if (frame == 0) {
        builder.addChild(key, builder.addNode("LIGHT__OFF__light"));
      } else if (frame == 2) {
        const int light =
            builder.addNode("LIGHT__FALL_IN_2__FALL_OUT_8__RGB_1_0.5_0__INTENSITY_0.8__RANDOM_0.1__light");
        builder.setNodeTranslation(light, {1.0f, 0.0f, -3.5f});
        builder.addChild(key, light);
      }
    }
    const std::vector<std::uint8_t> encoded = write(builder);
    pistoris::Cinematic original;
    REQUIRE(pistoris::Cinematic::importGlb(original, encoded) == ARX_OK);
    std::array<ArxCinematicKeyframe, 3> keys{};
    REQUIRE(original.copyKeyframes(0, keys.size(), keys.data()) == ARX_OK);
    CHECK(near(keys[0].camera_position.z, 1.0f));
    CHECK(keys[0].light_active == 1);
    CHECK(keys[0].light.intensity < 0.0f);
    CHECK(keys[1].light_active == 0);
    CHECK(keys[2].light_active == 1);
    CHECK(near(keys[2].light.position.x, 100.0f));
    CHECK(std::abs(keys[2].light.position.y) <= 1.0e-4f);
    CHECK(near(keys[2].light.position.z, 0.0f));
    CHECK(near(keys[2].light.color.r, 255.0f));
    CHECK(near(keys[2].light.color.g, 127.5f));

    nlohmann::json gltf = parseGlbJson(encoded);
    for (float aspect_ratio : {1.0f, 4.0f / 3.0f, 16.0f / 10.0f, 21.0f / 9.0f, 24.0f / 10.0f}) {
      CAPTURE(aspect_ratio);
      gltf["cameras"][0]["perspective"]["aspectRatio"] = aspect_ratio;
      pistoris::Cinematic imported;
      REQUIRE(pistoris::Cinematic::importGlb(imported, replaceGlbJson(encoded, gltf)) == ARX_OK);
      REQUIRE(imported.copyKeyframes(0, keys.size(), keys.data()) == ARX_OK);
      CHECK(near(keys[0].camera_position.z, 1.0f));
      CHECK(near(keys[2].light.position.x, 100.0f));
    }

    gltf["cameras"][0]["perspective"].erase("aspectRatio");
    pistoris::Cinematic without_aspect;
    REQUIRE(pistoris::Cinematic::importGlb(without_aspect, replaceGlbJson(encoded, gltf)) == ARX_OK);

    const float base_fov = pistoris::glb_cinematic::gameVerticalFov();
    gltf["cameras"][0]["perspective"]["aspectRatio"] = 24.0f / 10.0f;
    gltf["cameras"][0]["perspective"]["yfov"] = 2.0f * std::atan(2.0f * std::tan(base_fov * 0.5f));
    pistoris::Cinematic wider;
    REQUIRE(pistoris::Cinematic::importGlb(wider, replaceGlbJson(encoded, gltf)) == ARX_OK);
    REQUIRE(wider.copyKeyframes(0, keys.size(), keys.data()) == ARX_OK);
    CHECK(near(keys[0].camera_position.z, 2.0f));
    CHECK(near(keys[2].light.position.x, 50.0f));

    gltf["cameras"][0]["perspective"]["aspectRatio"] = 0.0f;
    CHECK(pistoris::Cinematic::importGlb(wider, replaceGlbJson(encoded, gltf)) == ARX_GLB_BAD_CINEMATIC_CAMERA);
  }

  TEST_CASE("Levels tilted camera and empty KEYs together with their LIGHT child") {
    pistoris::glb::Builder builder;
    const int root = builder.addNode("arx_cinematic__cinematic");
    builder.addRoot(root);
    const int illustration = addIllustration(builder, root, ImageSource::kEmbedded);
    const int camera = builder.addPerspectiveCamera("camera",
                                                    {.vertical_fov = pistoris::glb_cinematic::gameVerticalFov(),
                                                     .aspect_ratio = 4.0f / 3.0f,
                                                     .znear = 0.01f,
                                                     .zfar = {}});
    const int key = builder.addNode("KEY_0__key");
    builder.setNodeRotation(key, pistoris::glb_cinematic::toGlbRotation(0.0f));
    builder.setNodeTranslation(key, {0.25f, 1.0f, 0.0f});
    builder.setNodeCamera(key, camera);
    const int light = builder.addNode("LIGHT__FALL_IN_1__FALL_OUT_2__RGB_1_1_1__INTENSITY_1__RANDOM_0__light");
    builder.setNodeTranslation(light, {1.0f, 0.5f, -3.5f});
    builder.addChild(key, light);
    builder.addChild(illustration, key);
    addValidKey(builder, illustration, 10);

    const std::vector<std::uint8_t> encoded = write(builder);
    nlohmann::json gltf = parseGlbJson(encoded);
    gltf["nodes"][illustration]["scale"] = {1.0f, 2.0f, 1.0f};
    gltf["nodes"][key]["scale"] = {2.0f, 1.0f, 1.5f};

    DebugCapture logs;
    pistoris::Cinematic canonical;
    REQUIRE(pistoris::Cinematic::importGlb(canonical, replaceGlbJson(encoded, gltf)) == ARX_OK);
    ArxCinematicKeyframe expected{};
    REQUIRE(canonical.copyKeyframes(0, 1, &expected) == ARX_OK);
    CHECK_FALSE(logs.containsWarning("orientation leveled"));

    gltf["nodes"][key]["rotation"] = {0.0f, 0.0f, 0.0f, 1.0f};
    for (bool camera_attached : {true, false}) {
      CAPTURE(camera_attached);
      if (!camera_attached) gltf["nodes"][key].erase("camera");
      logs.warnings.clear();
      pistoris::Cinematic corrected;
      REQUIRE(pistoris::Cinematic::importGlb(corrected, replaceGlbJson(encoded, gltf)) == ARX_OK);
      ArxCinematicKeyframe actual{};
      REQUIRE(corrected.copyKeyframes(0, 1, &actual) == ARX_OK);
      CHECK(near(actual.camera_roll, expected.camera_roll));
      CHECK(near(actual.camera_position.x, expected.camera_position.x));
      CHECK(near(actual.camera_position.y, expected.camera_position.y));
      CHECK(near(actual.camera_position.z, expected.camera_position.z));
      CHECK(near(actual.light.position.x, expected.light.position.x));
      CHECK(std::abs(actual.light.position.y - expected.light.position.y) <= 1.0e-4f);
      CHECK(near(actual.light.position.z, expected.light.position.z));
      CHECK(logs.containsWarning("KEY 'KEY_0__key' is not facing the illustration; orientation leveled"));
    }
  }

  TEST_CASE("Composes illustration and key scale for positional lights only") {
    pistoris::glb::Builder builder;
    const int root = builder.addNode("arx_cinematic__cinematic");
    builder.addRoot(root);
    const int illustration = addIllustration(builder, root, ImageSource::kEmbedded);
    const int key = builder.addNode("KEY_0__key");
    builder.setNodeTranslation(key, {0.25f, 1.0f, 0.0f});
    builder.setNodeRotation(key, pistoris::glb_cinematic::toGlbRotation(0.0f));
    const int light = builder.addNode("LIGHT__FALL_IN_1__FALL_OUT_2__RGB_1_1_1__INTENSITY_1__RANDOM_0__light");
    builder.setNodeTranslation(light, {1.0f, 0.5f, -3.5f});
    builder.addChild(key, light);
    builder.addChild(illustration, key);
    addValidKey(builder, illustration, 10);
    const std::vector<std::uint8_t> encoded = write(builder);
    const nlohmann::json original = parseGlbJson(encoded);

    const auto imported_key = [&](const nlohmann::json& gltf) {
      pistoris::Cinematic cinematic;
      REQUIRE(pistoris::Cinematic::importGlb(cinematic, replaceGlbJson(encoded, gltf)) == ARX_OK);
      ArxCinematicKeyframe imported{};
      REQUIRE(cinematic.copyKeyframes(0, 1, &imported) == ARX_OK);
      return imported;
    };

    const ArxCinematicKeyframe baseline = imported_key(original);
    CHECK(std::abs(baseline.light.position.x - 100.0f) <= 1.0e-4f);
    CHECK(std::abs(baseline.light.position.y + 50.0f) <= 1.0e-4f);

    nlohmann::json gltf = original;
    gltf["nodes"][key]["scale"] = {2.0f, 1.0f, 1.0f};
    const ArxCinematicKeyframe key_scaled = imported_key(gltf);
    CHECK(std::abs(key_scaled.light.position.x - 200.0f) <= 1.0e-4f);
    CHECK(std::abs(key_scaled.light.position.y + 50.0f) <= 1.0e-4f);
    CHECK(near(key_scaled.camera_position.x, baseline.camera_position.x));
    CHECK(near(key_scaled.camera_position.z, baseline.camera_position.z));

    gltf["nodes"][illustration]["scale"] = {0.5f, 1.0f, 1.0f};
    const ArxCinematicKeyframe combined = imported_key(gltf);
    CHECK(std::abs(combined.light.position.x - 100.0f) <= 1.0e-4f);
    CHECK(near(combined.camera_position.x, baseline.camera_position.x));
    CHECK(near(combined.camera_position.z, baseline.camera_position.z));

    gltf = original;
    gltf["nodes"][key]["scale"] = {1.0f, 1.0f, 2.0f};
    const ArxCinematicKeyframe depth_scaled = imported_key(gltf);
    CHECK(std::abs(depth_scaled.light.position.x - 50.0f) <= 1.0e-4f);
    CHECK(near(depth_scaled.camera_position.z, baseline.camera_position.z));

    gltf = original;
    gltf["nodes"][illustration]["scale"] = {1.0f, 2.0f, 2.0f};
    const ArxCinematicKeyframe illustration_scaled = imported_key(gltf);
    CHECK(std::abs(illustration_scaled.light.position.x - 50.0f) <= 1.0e-4f);
    CHECK(std::abs(illustration_scaled.light.position.y + 50.0f) <= 1.0e-4f);
    CHECK(near(illustration_scaled.camera_position.z, baseline.camera_position.z));
  }

  TEST_CASE("Uses one affine UV chart beyond a canonical illustration") {
    pistoris::glb_cinematic::IllustrationSurface surface;
    const pistoris::glb_cinematic::SurfaceTriangle triangle{
        .position = {{{0.0f, 0.0f}, {2.0f, 0.0f}, {0.0f, 1.0f}}},
        .texcoord = {{{0.0f, 0.0f}, {1.0f, 0.0f}, {0.0f, 1.0f}}},
    };
    surface.addTriangle(triangle);
    for (const ArxVector2& position : triangle.position) surface.includePosition(position);
    REQUIRE(surface.finalize());
    CHECK(surface.mappingMode() == pistoris::glb_cinematic::SurfaceMappingMode::kAffineUv);

    pistoris::glb_cinematic::SurfaceSample sample;
    REQUIRE(surface.sample({4.0f, 2.0f}, sample));
    CHECK(near(sample.texcoord.x, 2.0f));
    CHECK(near(sample.texcoord.y, 2.0f));
    CHECK_FALSE(sample.outside);
  }

  TEST_CASE("Recognizes an affine chart across subdivided geometry") {
    pistoris::glb_cinematic::IllustrationSurface surface;
    const std::array<pistoris::glb_cinematic::SurfaceTriangle, 3> triangles = {{
        {
            .position = {{{0.0f, 0.0f}, {0.0f, 10.0f}, {5.0f, 10.0f}}},
            .texcoord = {{{0.0f, 0.0f}, {0.0f, 1.0f}, {0.5f, 1.0f}}},
        },
        {
            .position = {{{0.0f, 0.0f}, {5.0f, 10.0f}, {10.0f, 0.0f}}},
            .texcoord = {{{0.0f, 0.0f}, {0.5f, 1.0f}, {1.0f, 0.0f}}},
        },
        {
            .position = {{{5.0f, 10.0f}, {10.0f, 10.0f}, {10.0f, 0.0f}}},
            .texcoord = {{{0.5f, 1.0f}, {1.0f, 1.0f}, {1.0f, 0.0f}}},
        },
    }};
    for (const auto& triangle : triangles) {
      surface.addTriangle(triangle);
      for (const ArxVector2& position : triangle.position) surface.includePosition(position);
    }
    REQUIRE(surface.finalize());
    CHECK(surface.mappingMode() == pistoris::glb_cinematic::SurfaceMappingMode::kAffineUv);
  }

  TEST_CASE("Extrapolates a distorted chart from the nearest triangle") {
    pistoris::glb_cinematic::IllustrationSurface surface;
    const pistoris::glb_cinematic::SurfaceTriangle lower{
        .position = {{{0.0f, 0.0f}, {0.0f, 10.0f}, {5.0f, 5.0f}}},
        .texcoord = {{{0.0f, 0.0f}, {0.0f, 1.0f}, {0.5f, 1.0f}}},
    };
    const pistoris::glb_cinematic::SurfaceTriangle middle{
        .position = {{{0.0f, 0.0f}, {5.0f, 5.0f}, {10.0f, 0.0f}}},
        .texcoord = {{{0.0f, 0.0f}, {0.5f, 1.0f}, {1.0f, 0.0f}}},
    };
    const pistoris::glb_cinematic::SurfaceTriangle upper{
        .position = {{{5.0f, 5.0f}, {10.0f, 10.0f}, {10.0f, 0.0f}}},
        .texcoord = {{{0.5f, 1.0f}, {1.0f, 1.0f}, {1.0f, 0.0f}}},
    };
    surface.addTriangle(lower);
    surface.addTriangle(middle);
    surface.addTriangle(upper);
    for (const ArxVector2& position : lower.position) surface.includePosition(position);
    for (const ArxVector2& position : middle.position) surface.includePosition(position);
    for (const ArxVector2& position : upper.position) surface.includePosition(position);
    REQUIRE(surface.finalize());
    CHECK(surface.mappingMode() == pistoris::glb_cinematic::SurfaceMappingMode::kPiecewiseUv);

    pistoris::glb_cinematic::SurfaceSample sample;
    REQUIRE(surface.sample({5.0f, 8.0f}, sample));
    CHECK(sample.outside);
    CHECK(near(sample.texcoord.x, 0.5f));
    CHECK(near(sample.texcoord.y, 1.3f));
  }

  TEST_CASE("Falls back to geometry bounds when texture coordinates are unusable") {
    pistoris::glb_cinematic::IllustrationSurface surface;
    surface.includePosition({2.0f, 4.0f});
    surface.includePosition({2.0f, 8.0f});
    surface.noteMissingTexcoords();
    REQUIRE(surface.finalize());
    CHECK(surface.mappingMode() == pistoris::glb_cinematic::SurfaceMappingMode::kBounds);
    CHECK(surface.collapsedAxis());

    pistoris::glb_cinematic::SurfaceSample sample;
    REQUIRE(surface.sample({10.0f, 6.0f}, sample));
    CHECK(near(sample.texcoord.x, 0.5f));
    CHECK(near(sample.texcoord.y, 0.5f));
    CHECK(sample.outside);
  }

  TEST_CASE("Repairs colliding illustration image paths after importing the complete document") {
    pistoris::glb::Builder builder;
    const int root = builder.addNode("arx_cinematic__cinematic");
    builder.addRoot(root);
    const int first = addIllustration(builder, root, ImageSource::kEmbedded, 0);
    const int second = addIllustration(builder, root, ImageSource::kEmbedded, 1);
    addValidKey(builder, first, 0);
    addValidKey(builder, second, 1);

    pistoris::Cinematic cinematic;
    REQUIRE(pistoris::Cinematic::importGlb(cinematic, write(builder)) == ARX_OK);
    REQUIRE(cinematic.textureCount() == 2);
    ArxTextureView textures[2]{};
    REQUIRE(cinematic.copyTextureViews(0, 2, textures) == ARX_OK);
    const std::string_view first_path(textures[0].path.data, textures[0].path.size);
    const std::string_view second_path(textures[1].path.data, textures[1].path.size);
    CHECK((first_path != second_path));
  }

  TEST_CASE("Combines every illustration primitive into one UV chart") {
    pistoris::glb::Builder builder;
    const int root = builder.addNode("arx_cinematic__cinematic");
    builder.addRoot(root);
    const int illustration = addMultiPrimitiveIllustration(builder, root);
    const int key = builder.addNode("KEY_0__key");
    builder.setNodeRotation(key, pistoris::glb_cinematic::toGlbRotation(0.0f));
    builder.setNodeTranslation(key, {-0.5f, 0.0f, 0.25f});
    builder.addChild(illustration, key);
    addValidKey(builder, illustration, 1);

    pistoris::Cinematic cinematic;
    REQUIRE(pistoris::Cinematic::importGlb(cinematic, write(builder)) == ARX_OK);
    ArxCinematicKeyframe imported;
    REQUIRE(cinematic.copyKeyframes(0, 1, &imported) == ARX_OK);
    CHECK(near(imported.camera_position.x, -0.5f));
    CHECK(near(imported.camera_position.y, 0.75f));
  }

  TEST_CASE("Accepts separately embedded identical images across illustration primitives") {
    pistoris::glb::Builder builder;
    const int root = builder.addNode("arx_cinematic__cinematic");
    builder.addRoot(root);
    const int illustration = addMultiPrimitiveIllustration(builder, root, PrimitiveImages::kDuplicate);
    addValidKey(builder, illustration, 0);
    addValidKey(builder, illustration, 1);

    pistoris::Cinematic cinematic;
    REQUIRE(pistoris::Cinematic::importGlb(cinematic, write(builder)) == ARX_OK);
    CHECK(cinematic.textureCount() == 1);
  }

  TEST_CASE("Rejects conflicting image contents across illustration primitives") {
    pistoris::glb::Builder builder;
    const int root = builder.addNode("arx_cinematic__cinematic");
    builder.addRoot(root);
    const int illustration = addMultiPrimitiveIllustration(builder, root, PrimitiveImages::kDifferent);
    addValidKey(builder, illustration, 0);
    addValidKey(builder, illustration, 1);

    DebugCapture logs;
    pistoris::Cinematic cinematic;
    CHECK(pistoris::Cinematic::importGlb(cinematic, write(builder)) == ARX_GLB_BAD_CINEMATIC_IMAGE);
    CHECK(logs.containsError("illustration primitives reference different embedded images"));
  }

  TEST_CASE("Applies KHR texture transforms to the illustration UV chart") {
    pistoris::glb::Builder builder;
    const int root = builder.addNode("arx_cinematic__cinematic");
    builder.addRoot(root);
    const int illustration = addIllustration(builder, root, ImageSource::kEmbedded);
    addValidKey(builder, illustration, 0);
    addValidKey(builder, illustration, 1);
    std::vector<std::uint8_t> encoded = write(builder);
    nlohmann::json gltf = parseGlbJson(encoded);
    gltf["extensionsUsed"].push_back("KHR_texture_transform");
    gltf["extensionsRequired"].push_back("KHR_texture_transform");
    gltf["materials"][0]["pbrMetallicRoughness"]["baseColorTexture"]["extensions"]["KHR_texture_transform"] = {
        {"offset", {0.25f, 0.1f}}, {"scale", {0.5f, 0.5f}}};

    pistoris::Cinematic cinematic;
    REQUIRE(pistoris::Cinematic::importGlb(cinematic, replaceGlbJson(encoded, gltf)) == ARX_OK);
    ArxCinematicKeyframe imported;
    REQUIRE(cinematic.copyKeyframes(0, 1, &imported) == ARX_OK);
    CHECK(near(imported.camera_position.x, 0.0f));
    CHECK(near(imported.camera_position.y, -0.15f));
  }

  TEST_CASE("Warns when GLB animation targets the Cinematic hierarchy") {
    pistoris::glb::Builder builder;
    const int root = builder.addNode("arx_cinematic__cinematic");
    builder.addRoot(root);
    const int illustration = addIllustration(builder, root, ImageSource::kEmbedded);
    const int key = builder.addNode("KEY_0__key");
    builder.setNodeRotation(key, pistoris::glb_cinematic::toGlbRotation(0.0f));
    builder.addChild(illustration, key);
    addValidKey(builder, illustration, 10);
    const std::array<float, 2> times{0.0f, 1.0f};
    const std::array<pistoris::glb::Vec3, 2> positions{{{0.0f, 0.0f, 0.0f}, {1.0f, 0.0f, 0.0f}}};
    builder.addAnimation("camera_move",
                         {{builder.addTimeAccessor(times),
                           builder.addVec3Accessor(positions),
                           key,
                           pistoris::glb::AnimationPath::kTranslation}});

    DebugCapture logs;
    pistoris::Cinematic cinematic;
    REQUIRE(pistoris::Cinematic::importGlb(cinematic, write(builder)) == ARX_OK);
    CHECK(logs.containsWarning("1 animation channel(s) target Cinematic nodes"));
  }

  TEST_CASE("Warns when illustration material appearance is not imported") {
    pistoris::glb::Builder builder;
    const int root = builder.addNode("arx_cinematic__cinematic");
    builder.addRoot(root);
    const int illustration = addIllustration(builder, root, ImageSource::kEmbedded);
    addValidKey(builder, illustration, 0);
    addValidKey(builder, illustration, 1);
    std::vector<std::uint8_t> encoded = write(builder);
    nlohmann::json gltf = parseGlbJson(encoded);

    SUBCASE("base-color tint") {
      gltf["materials"][0]["pbrMetallicRoughness"]["baseColorFactor"] = {0.5f, 1.0f, 1.0f, 1.0f};
    }
    SUBCASE("emissive color") { gltf["materials"][0]["emissiveFactor"] = {0.2f, 0.0f, 0.0f}; }

    DebugCapture logs;
    pistoris::Cinematic cinematic;
    REQUIRE(pistoris::Cinematic::importGlb(cinematic, replaceGlbJson(encoded, gltf)) == ARX_OK);
    CHECK(logs.containsWarning("material tint or emission; only its image is imported"));
    CHECK_FALSE(logs.containsWarning("image has an alpha channel"));
  }

  TEST_CASE("Warns when illustration image alpha overrides non-blending material") {
    const std::array<std::uint8_t, 4> pixel = {255, 0, 0, 128};
    std::vector<std::uint8_t> png;
    const auto append = [](void* context, void* data, int size) {
      auto& out = *static_cast<std::vector<std::uint8_t>*>(context);
      const auto* bytes = static_cast<const std::uint8_t*>(data);
      out.insert(out.end(), bytes, bytes + size);
    };
    REQUIRE(stbi_write_png_to_func(append, &png, 1, 1, 4, pixel.data(), 4) != 0);

    pistoris::glb::Builder builder;
    const int root = builder.addNode("arx_cinematic__cinematic");
    builder.addRoot(root);
    const int illustration = addIllustration(builder, root, ImageSource::kEmbedded, 0, "story/scene.png", {}, png);
    addValidKey(builder, illustration, 0);
    addValidKey(builder, illustration, 1);
    const std::vector<std::uint8_t> encoded = write(builder);
    nlohmann::json gltf = parseGlbJson(encoded);
    bool expect_warning = false;

    SUBCASE("opaque") {
      gltf["materials"][0]["alphaMode"] = "OPAQUE";
      expect_warning = true;
    }
    SUBCASE("mask") {
      gltf["materials"][0]["alphaMode"] = "MASK";
      expect_warning = true;
    }
    SUBCASE("blend") { gltf["materials"][0]["alphaMode"] = "BLEND"; }

    DebugCapture logs;
    pistoris::Cinematic cinematic;
    REQUIRE(pistoris::Cinematic::importGlb(cinematic, replaceGlbJson(encoded, gltf)) == ARX_OK);
    CHECK(logs.containsWarning("image has an alpha channel but its material is not BLEND") == expect_warning);

    std::vector<std::uint8_t> canonical;
    REQUIRE(cinematic.exportGlb(canonical) == ARX_OK);
    CHECK(parseGlbJson(canonical)["materials"][0]["alphaMode"] == "BLEND");
  }

  TEST_CASE("Preserves distinct sound source spellings that share one identity") {
    pistoris::glb::Builder builder;
    const int root = builder.addNode("arx_cinematic__cinematic");
    builder.addRoot(root);
    const int illustration = addIllustration(builder, root, ImageSource::kEmbedded);
    addSoundKey(builder, illustration, 0, "Effects\\Hit");
    addSoundKey(builder, illustration, 1, "effects/hit");
    addSoundKey(builder, illustration, 2, "effects/hit");

    pistoris::Cinematic cinematic;
    std::vector<pistoris::CinematicSoundSourceReference> sources;
    REQUIRE(pistoris::Cinematic::importGlb(cinematic, write(builder), &sources) == ARX_OK);
    CHECK(cinematic.soundCount(pistoris::SoundKind::kEffect) == 1);
    REQUIRE(sources.size() == 2);
    CHECK(sources[0].sound == sources[1].sound);
    CHECK(sources[0].path == "Effects\\Hit");
    CHECK(sources[1].path == "effects/hit");
  }

  TEST_CASE("Keeps distinct sounds when path repair collides") {
    pistoris::glb::Builder builder;
    const int root = builder.addNode("arx_cinematic__cinematic");
    builder.addRoot(root);
    const int illustration = addIllustration(builder, root, ImageSource::kEmbedded);
    addSoundKey(builder, illustration, 0, "effects/hit-.wav");
    addSoundKey(builder, illustration, 1, R"(Effects\Hit?.WAV)");
    addSoundKey(builder, illustration, 2, "effects/hit*.wav");
    addSoundKey(builder, illustration, 3, R"(EFFECTS\HIT-.WAV)");

    pistoris::Cinematic cinematic;
    std::vector<pistoris::CinematicSoundSourceReference> sources;
    REQUIRE(pistoris::Cinematic::importGlb(cinematic, write(builder), &sources) == ARX_OK);
    REQUIRE(cinematic.soundCount(pistoris::SoundKind::kEffect) == 3);
    std::array<ArxCinematicSoundView, 3> sounds{};
    REQUIRE(cinematic.copySoundViews(pistoris::SoundKind::kEffect, 0, sounds.size(), sounds.data()) == ARX_OK);
    CHECK((std::string_view(sounds[0].path.data, sounds[0].path.size) == "effects/hit-.wav"));
    CHECK((std::string_view(sounds[1].path.data, sounds[1].path.size) == "effects/hit-_1.wav"));
    CHECK((std::string_view(sounds[2].path.data, sounds[2].path.size) == "effects/hit-_2.wav"));
    REQUIRE(sources.size() == 4);
    CHECK(sources[0].sound == sources[3].sound);
    CHECK(sources[0].sound != sources[1].sound);
    CHECK(sources[1].sound != sources[2].sound);
  }

  TEST_CASE("Keeps effect and speech identities independent") {
    pistoris::glb::Builder builder;
    const int root = builder.addNode("arx_cinematic__cinematic");
    builder.addRoot(root);
    const int illustration = addIllustration(builder, root, ImageSource::kEmbedded);
    addSoundKey(builder, illustration, 0, "voices/line", pistoris::SoundKind::kEffect);
    addSoundKey(builder, illustration, 1, "voices/line", pistoris::SoundKind::kSpeech);

    pistoris::Cinematic cinematic;
    std::vector<pistoris::CinematicSoundSourceReference> sources;
    REQUIRE(pistoris::Cinematic::importGlb(cinematic, write(builder), &sources) == ARX_OK);
    CHECK(cinematic.soundCount(pistoris::SoundKind::kEffect) == 1);
    CHECK(cinematic.soundCount(pistoris::SoundKind::kSpeech) == 1);
    REQUIRE(sources.size() == 2);
    CHECK(sources[0].sound != sources[1].sound);
    CHECK(sources[0].path == sources[1].path);
  }

  TEST_CASE("Classifies malformed Cinematic documents at their hierarchy boundary") {
    pistoris::Cinematic cinematic;

    SUBCASE("no Cinematic") {
      pistoris::glb::Builder builder;
      builder.addRoot(builder.addNode("ordinary"));
      CHECK(pistoris::Cinematic::importGlb(cinematic, write(builder)) == ARX_GLB_NO_CINEMATIC);
    }

    SUBCASE("ambiguous Cinematic") {
      pistoris::glb::Builder builder;
      builder.addRoot(builder.addNode("arx_cinematic__first"));
      builder.addRoot(builder.addNode("arx_cinematic__second"));
      CHECK(pistoris::Cinematic::importGlb(cinematic, write(builder)) == ARX_GLB_AMBIGUOUS_CINEMATIC);
    }

    SUBCASE("bad root") {
      pistoris::glb::Builder builder;
      builder.addRoot(builder.addNode("arx_cinematic__FPS_bad__root"));
      CHECK(pistoris::Cinematic::importGlb(cinematic, write(builder)) == ARX_GLB_BAD_CINEMATIC_ROOT);
    }

    SUBCASE("bad illustration") {
      pistoris::glb::Builder builder;
      const int root = builder.addNode("arx_cinematic__cinematic");
      builder.addRoot(root);
      builder.addChild(root, builder.addNode("arx_illustration__0__illustration"));
      CHECK(pistoris::Cinematic::importGlb(cinematic, write(builder)) == ARX_GLB_BAD_CINEMATIC_ILLUSTRATION);
    }

    SUBCASE("bad image") {
      pistoris::glb::Builder builder;
      const int root = builder.addNode("arx_cinematic__cinematic");
      builder.addRoot(root);
      addIllustration(builder, root, ImageSource::kExternal);
      CHECK(pistoris::Cinematic::importGlb(cinematic, write(builder)) == ARX_GLB_BAD_CINEMATIC_IMAGE);
    }

    SUBCASE("bad key") {
      pistoris::glb::Builder builder;
      const int root = builder.addNode("arx_cinematic__cinematic");
      builder.addRoot(root);
      const int illustration = addIllustration(builder, root, ImageSource::kEmbedded);
      builder.addChild(illustration, builder.addNode("KEY_bad__key"));
      CHECK(pistoris::Cinematic::importGlb(cinematic, write(builder)) == ARX_GLB_BAD_CINEMATIC_KEY_NAME);
    }

    SUBCASE("bad helper") {
      pistoris::glb::Builder builder;
      const int root = builder.addNode("arx_cinematic__cinematic");
      builder.addRoot(root);
      const int illustration = addIllustration(builder, root, ImageSource::kEmbedded);
      const int key = builder.addNode("KEY_0__key");
      builder.setNodeRotation(key, pistoris::glb_cinematic::toGlbRotation(0.0f));
      builder.addChild(key, builder.addNode("PRESENTATION__presentation"));
      builder.addChild(illustration, key);
      addValidKey(builder, illustration, 1);
      CHECK(pistoris::Cinematic::importGlb(cinematic, write(builder)) == ARX_GLB_BAD_CINEMATIC_HELPER);
    }
  }
}
