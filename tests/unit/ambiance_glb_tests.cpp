// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#include "doctest/doctest.h"

#include "arx_pistoris/ambiance.hpp"
#include "arx_pistoris/ambiance/types.h"
#include "arx_pistoris/base/indices.h"
#include "arx_pistoris/base/status.h"
#include "arx_pistoris/base/string_view.h"
#include "arx_pistoris/glb/location.hpp"
#include "arx_pistoris/model.hpp"
#include "arx_pistoris/native/amb.hpp"
#include "arx_pistoris/pistoris.hpp"
#include "arx_pistoris/sound.h"

#include "amb_helpers.h"
#include "external/glb/container.h"
#include "external/glb/writer.h"
#include "model_helpers.h"

#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace {

ArxStringView view(std::string_view value) { return {value.data(), value.size()}; }

ArxSoundView soundView(std::string_view path) { return {view(path), {nullptr, 0}}; }

pistoris::Ambiance importGlb(std::span<const std::uint8_t> encoded,
                             std::vector<pistoris::SoundSourceReference>* sources = nullptr) {
  auto result = pistoris::Ambiance::importGlb(encoded, {}, sources);
  REQUIRE(result);
  return std::move(*result);
}

pistoris::Ambiance importNative(const pistoris::amb::Data& native) {
  auto result = pistoris::Ambiance::importNative(native);
  REQUIRE(result);
  return std::move(*result);
}

std::vector<std::uint8_t> exportGlb(const pistoris::Ambiance& ambiance) {
  auto result = ambiance.exportGlb();
  REQUIRE(result);
  return std::move(*result);
}

const cgltf_node* findNode(const cgltf_data& data, std::string_view name) {
  for (std::size_t index = 0; index < data.nodes_count; ++index) {
    const cgltf_node& node = data.nodes[index];
    if (node.name != nullptr && node.name == name) return &node;
  }
  return nullptr;
}

struct WarningCapture {
  std::vector<std::string> messages;

  WarningCapture() {
    pistoris::setLogCallback(
        [](ArxLogLevel level, const char* message, void* userdata) {
          if (level == ARX_LOG_WARN && message != nullptr)
            static_cast<WarningCapture*>(userdata)->messages.emplace_back(message);
        },
        this);
  }

  ~WarningCapture() { pistoris::setLogCallback(nullptr, nullptr); }

  bool contains(std::string_view text) const {
    for (const std::string& message : messages)
      if (message.find(text) != std::string::npos) return true;
    return false;
  }
};

}  // namespace

TEST_SUITE("Ambiance GLB conversion") {
  TEST_CASE("Rejects invalid coordinate units") {
    const pistoris::Ambiance ambiance = importNative(makeAmbData());
    pistoris::Ambiance::GlbExportOptions export_options;
    export_options.arx_units_per_glb_unit = 1001.0f;
    CHECK(ambiance.exportGlb(export_options).code() == ARX_INVALID_OPTIONS);

    const std::vector<std::uint8_t> encoded = exportGlb(ambiance);
    pistoris::Ambiance::GlbImportOptions import_options;
    import_options.arx_units_per_glb_unit = 0.0f;
    CHECK(pistoris::Ambiance::importGlb(encoded, import_options).code() == ARX_INVALID_OPTIONS);
  }

  TEST_CASE("Repairs logical sound paths without losing source spellings") {
    pistoris::glb::Builder builder;
    const int root = builder.addNode("arx_ambiance__MASTER_0__root");
    builder.addRoot(root);
    builder.addChild(root, builder.addNode(R"(TRACK_0__SFX\Hit?.WAV__first)"));
    builder.addChild(root, builder.addNode("TRACK_1__sfx/hit*.wav__second"));
    builder.addChild(root, builder.addNode(R"(TRACK_2__sfx\hit?.wav__third)"));

    std::vector<std::uint8_t> encoded;
    REQUIRE(builder.write(encoded) == ARX_OK);
    std::vector<pistoris::SoundSourceReference> sources;
    const pistoris::Ambiance ambiance = importGlb(encoded, &sources);
    CHECK(ambiance.trackCount() == 3);
    CHECK(ambiance.soundCount() == 2);
    REQUIRE(sources.size() == 3);
    CHECK(sources[0].path == R"(SFX\Hit?.WAV)");
    CHECK(sources[1].path == "sfx/hit*.wav");
    CHECK(sources[2].path == R"(sfx\hit?.wav)");

    REQUIRE(ambiance.sounds().size() == 2);
    CHECK((std::string_view(ambiance.sounds()[0].path.data, ambiance.sounds()[0].path.size) == "sfx/hit-.wav"));
    CHECK((std::string_view(ambiance.sounds()[1].path.data, ambiance.sounds()[1].path.size) == "sfx/hit-_1.wav"));
  }

  TEST_CASE("Sorts ordinals and positions keyless tracks relative to the Ambiance root") {
    pistoris::glb::Builder builder;
    const int root = builder.addNode("arx_ambiance__MASTER_7__root");
    builder.setNodeTranslation(root, {1.0f, 2.0f, 3.0f});
    builder.setNodeRotation(root, {0.70710678f, 0.0f, 0.70710678f, 0.0f});
    builder.addRoot(root);
    builder.setRootTransform("outer", {10.0f, 20.0f, 30.0f}, {0.70710678f, 0.70710678f, 0.0f, 0.0f}, 3.0f);

    const int track_seven = builder.addNode("TRACK_7__sfx/wind__wide.wav__wind");
    builder.setNodeTranslation(track_seven, {2.0f, 3.0f, 4.0f});
    builder.addChild(root, track_seven);

    const int track_two = builder.addNode(R"(TRACK_2__sfx\pan.wav__pan)");
    const int key = builder.addNode("KEY_4__key");
    const int pan = builder.addNode("PAN__pan");
    builder.setNodeTranslation(key, {0.5f, 0.0f, 0.5f});
    builder.addChild(key, pan);
    builder.addChild(track_two, key);
    builder.addChild(root, track_two);

    std::vector<std::uint8_t> encoded;
    REQUIRE(builder.write(encoded) == ARX_OK);
    std::vector<pistoris::SoundSourceReference> sources;
    const pistoris::Ambiance ambiance = importGlb(encoded, &sources);
    CHECK(ambiance.trackCount() == 2);
    CHECK(ambiance.masterTrack() == 1);
    REQUIRE(sources.size() == 2);
    CHECK(sources[0].path == R"(sfx\pan.wav)");

    const auto tracks = ambiance.tracks();
    const auto sounds = ambiance.sounds();
    REQUIRE(tracks.size() == 2);
    REQUIRE(sounds.size() == 2);
    CHECK((std::string_view(sounds[tracks[0].sound].path.data, sounds[tracks[0].sound].path.size) == "sfx/pan.wav"));
    CHECK((std::string_view(sounds[tracks[1].sound].path.data, sounds[tracks[1].sound].path.size) ==
           "sfx/wind__wide.wav"));

    auto positioned_keys = ambiance.positionedKeys(1);
    REQUIRE(positioned_keys);
    REQUIRE(positioned_keys->size() == 1);
    const ArxAmbiancePositionedKey positioned = (*positioned_keys)[0];
    CHECK(positioned.x.first == doctest::Approx(20.0f));
    CHECK(positioned.y.first == doctest::Approx(-30.0f));
    CHECK(positioned.z.first == doctest::Approx(-40.0f));
  }

  TEST_CASE("Maps panning to the Model-facing GLB axes") {
    pistoris::amb::Track track;
    track.sample_path = "sfx/ambiance/pan.wav";
    track.flags = pistoris::amb::kTrackMaster;
    constexpr std::array<float, 3> kPanValues = {-1.0f, 0.0f, 1.0f};
    for (float pan : kPanValues) {
      pistoris::amb::Key key;
      key.volume = makeAmbSetting(1.0f);
      key.pitch = makeAmbSetting(1.0f);
      key.pan = makeAmbSetting(pan);
      track.keys.push_back(key);
    }

    pistoris::amb::Data native{{std::move(track)}};
    const pistoris::Ambiance ambiance = importNative(native);

    const std::vector<std::uint8_t> encoded = exportGlb(ambiance);
    pistoris::glb::Asset asset;
    REQUIRE(pistoris::glb::parse(encoded, asset) == ARX_OK);

    constexpr std::array<std::string_view, 3> kKeyNames = {"KEY_000__key_0", "KEY_001__key_1", "KEY_002__key_2"};
    constexpr std::array<float, 3> kExpectedX = {5.0f, 0.0f, -5.0f};
    constexpr std::array<float, 3> kExpectedZ = {0.0f, 5.0f, 0.0f};
    for (std::size_t index = 0; index < kKeyNames.size(); ++index) {
      const cgltf_node* key = findNode(*asset.data(), kKeyNames[index]);
      REQUIRE(key != nullptr);
      const cgltf_node* pan = nullptr;
      for (std::size_t child = 0; child < key->children_count; ++child)
        if (key->children[child] != nullptr && key->children[child]->name != nullptr &&
            std::string_view(key->children[child]->name) == "PAN__pan")
          pan = key->children[child];
      REQUIRE(pan != nullptr);
      REQUIRE(key->has_translation);
      CHECK(key->translation[0] == doctest::Approx(kExpectedX[index]));
      CHECK(key->translation[1] == doctest::Approx(0.0f));
      CHECK(key->translation[2] == doctest::Approx(kExpectedZ[index]));
      CHECK_FALSE(pan->has_translation);
      CHECK_FALSE(pan->has_rotation);
      CHECK_FALSE(pan->has_scale);
    }

    const pistoris::Ambiance imported = importGlb(encoded);
    auto imported_keys = imported.pannedKeys(0);
    REQUIRE(imported_keys);
    REQUIRE(imported_keys->size() == kPanValues.size());
    for (std::size_t index = 0; index < imported_keys->size(); ++index)
      CHECK((*imported_keys)[index].pan.first == doctest::Approx(kPanValues[index]));
  }

  TEST_CASE("Maps a back-arc PAN point to the corresponding front-arc value") {
    pistoris::glb::Builder builder;
    const int root = builder.addNode("arx_ambiance__root");
    const int track = builder.addNode("TRACK_0__sfx/pan.wav__track");
    const int key = builder.addNode("KEY_0__key");
    const int pan = builder.addNode("PAN__pan");
    builder.setNodeTranslation(key, {-0.5f, 0.0f, -0.5f});
    builder.addChild(key, pan);
    builder.addChild(track, key);
    builder.addChild(root, track);
    builder.addRoot(root);

    std::vector<std::uint8_t> encoded;
    REQUIRE(builder.write(encoded) == ARX_OK);
    WarningCapture warnings;
    const pistoris::Ambiance ambiance = importGlb(encoded);
    auto imported_keys = ambiance.pannedKeys(0);
    REQUIRE(imported_keys);
    REQUIRE(imported_keys->size() == 1);
    const ArxAmbiancePannedKey imported = (*imported_keys)[0];
    CHECK(imported.pan.first == doctest::Approx(std::sqrt(0.5f)));
    CHECK(warnings.contains("is behind the listener and mapped to the front arc"));
  }

  TEST_CASE("Uses KEY position, preserves signed ranges, and normalizes random ranges") {
    pistoris::glb::Builder builder;
    const int root = builder.addNode("arx_ambiance__root");
    const int track = builder.addNode("TRACK_0__sfx/position.wav__track");
    const int key = builder.addNode("KEY_0__key");
    builder.setNodeTranslation(key, {2.0f, 1.0f, -3.0f});
    const int x = builder.addNode("X__RANGE_-0.5__x");
    const int y = builder.addNode("Y__RANGE_-0.25__RANDOM_STEP__y");
    builder.setNodeTranslation(x, {7.0f, 8.0f, 9.0f});
    builder.addChild(key, x);
    builder.addChild(key, y);
    builder.addChild(track, key);
    builder.addChild(root, track);
    builder.addRoot(root);

    std::vector<std::uint8_t> encoded;
    REQUIRE(builder.write(encoded) == ARX_OK);
    WarningCapture warnings;
    const pistoris::Ambiance ambiance = importGlb(encoded);
    auto imported_keys = ambiance.positionedKeys(0);
    REQUIRE(imported_keys);
    REQUIRE(imported_keys->size() == 1);
    const ArxAmbiancePositionedKey imported = (*imported_keys)[0];
    CHECK(imported.x.first == doctest::Approx(25.0f));
    CHECK(imported.x.second == doctest::Approx(15.0f));
    CHECK(imported.y.first == doctest::Approx(-12.5f));
    CHECK(imported.y.second == doctest::Approx(-7.5f));
    CHECK(imported.y.mode == ARX_AMBIANCE_AUTOMATION_RANDOM_STEP);
    CHECK(imported.z.first == doctest::Approx(30.0f));
    REQUIRE(warnings.messages.size() == 1);
    CHECK(warnings.contains("has a negative RANGE; using its absolute value"));
  }

  TEST_CASE("Collapses automation ranges that cannot be represented in GLB") {
    ArxAmbiancePositionedKey key{};
    key.volume = {0.0f, std::numeric_limits<float>::denorm_min(), 0, ARX_AMBIANCE_AUTOMATION_STEP};
    key.pitch = {1.0f, 1.0f, 0, ARX_AMBIANCE_AUTOMATION_CONSTANT};
    key.x = {0.0f, 0.0f, 0, ARX_AMBIANCE_AUTOMATION_CONSTANT};
    key.y = {0.0f, 0.0f, 0, ARX_AMBIANCE_AUTOMATION_CONSTANT};
    key.z = {0.0f, 0.0f, 0, ARX_AMBIANCE_AUTOMATION_CONSTANT};
    constexpr std::string_view kSamplePath = "sfx/quiet.wav";
    pistoris::Ambiance ambiance;
    auto sound_result = ambiance.addSound(soundView(kSamplePath));
    REQUIRE(sound_result);
    const pistoris::SoundIndex sound = *sound_result;
    const ArxAmbiancePositionedTrackInput input{sound, &key, 1};

    REQUIRE(ambiance.addPositionedTrack(input));

    WarningCapture warnings;
    const std::vector<std::uint8_t> encoded = exportGlb(ambiance);
    const std::string_view bytes(reinterpret_cast<const char*>(encoded.data()), encoded.size());
    CHECK(bytes.find("VOLUME__VAL_0__volume") != std::string_view::npos);
    CHECK(bytes.find("VOLUME__VAL_0__RANGE_") == std::string_view::npos);
    CHECK(warnings.contains("VOLUME automation for 'sfx/quiet.wav' is too small"));

    const pistoris::Ambiance imported = importGlb(encoded);
    auto copied_keys = imported.positionedKeys(0);
    REQUIRE(copied_keys);
    REQUIRE(copied_keys->size() == 1);
    const ArxAmbiancePositionedKey copied = (*copied_keys)[0];
    CHECK(copied.volume.mode == ARX_AMBIANCE_AUTOMATION_CONSTANT);
    CHECK(copied.volume.first == doctest::Approx(0.0f));
    CHECK(copied.volume.second == doctest::Approx(0.0f));
  }

  TEST_CASE("Recovers self-delimiting helpers without labels") {
    pistoris::glb::Builder builder;
    const int root = builder.addNode("arx_ambiance__MASTER_0");
    const int track = builder.addNode("TRACK_0__sfx/test.wav__track");
    const int key = builder.addNode("KEY_0__PLAY_COUNT_2");
    builder.addChild(key, builder.addNode("VOLUME__VAL_0.5"));
    builder.addChild(track, key);
    builder.addChild(root, track);
    builder.addRoot(root);

    std::vector<std::uint8_t> encoded;
    REQUIRE(builder.write(encoded) == ARX_OK);
    WarningCapture warnings;
    const pistoris::Ambiance ambiance = importGlb(encoded);
    CHECK(ambiance.masterTrack() == 0);
    CHECK(warnings.contains("arx_ambiance__MASTER_0' has no final label"));
    CHECK(warnings.contains("KEY_0__PLAY_COUNT_2' has no final label"));
    CHECK(warnings.contains("VOLUME__VAL_0.5' has no final label"));
  }

  TEST_CASE("Keeps path-bearing track labels mandatory") {
    pistoris::glb::Builder builder;
    const int root = builder.addNode("arx_ambiance__root");
    builder.addChild(root, builder.addNode("TRACK_0__sfx/test.wav"));
    builder.addRoot(root);

    std::vector<std::uint8_t> encoded;
    REQUIRE(builder.write(encoded) == ARX_OK);
    CHECK(pistoris::Ambiance::importGlb(encoded).code() == ARX_GLB_BAD_AMBIANCE_TRACK);
  }

  TEST_CASE("Exports a reference Model preview and aligns the root to view_attach") {
    pistoris::amb::Data native = makeAmbData();
    native.tracks.front().sample_path = "sfx/ambiance/test.wav";
    const pistoris::Ambiance ambiance = importNative(native);
    auto model_result = pistoris::Model::importNative(makeSemanticModelFtl());
    REQUIRE(model_result);
    const pistoris::Model& model = *model_result;

    const pistoris::Ambiance::GlbExportOptions options;
    auto encoded_result = ambiance.exportGlb(options, &model);
    REQUIRE(encoded_result);
    const std::vector<std::uint8_t>& encoded = *encoded_result;

    pistoris::glb::Asset asset;
    REQUIRE(pistoris::glb::parse(encoded, asset) == ARX_OK);
    const cgltf_node* root = findNode(*asset.data(), "arx_ambiance__ambiance");
    const cgltf_node* reference = findNode(*asset.data(), "reference_model");
    REQUIRE(root != nullptr);
    REQUIRE(reference != nullptr);
    REQUIRE(findNode(*asset.data(), "TRACK_000__sfx/ambiance/test.wav__track_0") != nullptr);
    CHECK(reference->mesh != nullptr);
    CHECK(root->parent == nullptr);
    CHECK(reference->parent == nullptr);
    REQUIRE(root->has_translation);
    CHECK(root->translation[0] == doctest::Approx(0.1f));
    CHECK(root->translation[1] == doctest::Approx(0.0f));
    CHECK(root->translation[2] == doctest::Approx(0.0f));

    const pistoris::Ambiance imported = importGlb(encoded);
    CHECK(imported.trackCount() == ambiance.trackCount());
    CHECK(imported.masterTrack() == ambiance.masterTrack());
  }

  TEST_CASE("Rejects malformed recognized automation helpers") {
    pistoris::glb::Builder builder;
    const int root = builder.addNode("arx_ambiance__root");
    const int track = builder.addNode("TRACK_0__sfx/test.wav__track");
    const int key = builder.addNode("KEY_0__key");
    const int automation = builder.addNode("VOLUME__volume");
    builder.addChild(key, automation);
    builder.addChild(track, key);
    builder.addChild(root, track);
    builder.addRoot(root);

    std::vector<std::uint8_t> encoded;
    REQUIRE(builder.write(encoded) == ARX_OK);
    const pistoris::GlbResult<pistoris::Ambiance> result = pistoris::Ambiance::importGlb(encoded);
    REQUIRE_FALSE(result);
    CHECK(result.code() == ARX_GLB_BAD_AMBIANCE_AUTOMATION);
    REQUIRE(result.error() != nullptr);
    REQUIRE(result.error()->location().has_value());
    CHECK(result.error()->location()->element == pistoris::GlbElement::kNode);
    CHECK(result.error()->location()->index == static_cast<std::size_t>(automation));
    CHECK(result.error()->location()->label == "VOLUME__volume");
    CHECK(result.error()->location()->property == "automation");
  }

  TEST_CASE("Returns focused hierarchy and naming errors") {
    SUBCASE("Missing root") {
      pistoris::glb::Builder builder;
      builder.addRoot(builder.addNode("ordinary"));
      std::vector<std::uint8_t> encoded;
      REQUIRE(builder.write(encoded) == ARX_OK);
      CHECK(pistoris::Ambiance::importGlb(encoded).code() == ARX_GLB_NO_AMBIANCE);
    }

    SUBCASE("Ambiguous roots") {
      pistoris::glb::Builder builder;
      builder.addRoot(builder.addNode("arx_ambiance__first"));
      builder.addRoot(builder.addNode("arx_ambiance__second"));
      std::vector<std::uint8_t> encoded;
      REQUIRE(builder.write(encoded) == ARX_OK);
      CHECK(pistoris::Ambiance::importGlb(encoded).code() == ARX_GLB_AMBIGUOUS_AMBIANCE);
    }

    SUBCASE("Nested root") {
      pistoris::glb::Builder builder;
      const int root = builder.addNode("arx_ambiance__root");
      const int nested = builder.addNode("arx_ambiance__nested");
      builder.addChild(root, nested);
      builder.addRoot(root);
      std::vector<std::uint8_t> encoded;
      REQUIRE(builder.write(encoded) == ARX_OK);

      const pistoris::GlbResult<pistoris::Ambiance> result = pistoris::Ambiance::importGlb(encoded);
      REQUIRE_FALSE(result);
      CHECK(result.code() == ARX_GLB_BAD_AMBIANCE_ROOT);
      REQUIRE(result.error() != nullptr);
      REQUIRE(result.error()->location().has_value());
      CHECK(result.error()->location()->element == pistoris::GlbElement::kNode);
      CHECK(result.error()->location()->index == static_cast<std::size_t>(nested));
      CHECK(result.error()->location()->label == "arx_ambiance__nested");
    }

    SUBCASE("Missing root label") {
      pistoris::glb::Builder builder;
      const int root = builder.addNode("arx_ambiance");
      builder.addChild(root, builder.addNode("TRACK_0__sfx/test.wav__track"));
      builder.addRoot(root);
      std::vector<std::uint8_t> encoded;
      REQUIRE(builder.write(encoded) == ARX_OK);
      WarningCapture warnings;
      [[maybe_unused]] const pistoris::Ambiance ambiance = importGlb(encoded);
      CHECK(warnings.contains("arx_ambiance' has no final label"));
    }

    SUBCASE("Malformed root option") {
      pistoris::glb::Builder builder;
      const int root = builder.addNode("arx_ambiance__MASTER_bad__root");
      builder.addChild(root, builder.addNode("TRACK_0__sfx/test.wav__track"));
      builder.addRoot(root);
      std::vector<std::uint8_t> encoded;
      REQUIRE(builder.write(encoded) == ARX_OK);
      CHECK(pistoris::Ambiance::importGlb(encoded).code() == ARX_GLB_BAD_AMBIANCE_ROOT);
    }

    SUBCASE("Malformed track") {
      pistoris::glb::Builder builder;
      const int root = builder.addNode("arx_ambiance__root");
      builder.addChild(root, builder.addNode("TRACK_bad__sfx/test.wav__track"));
      builder.addRoot(root);
      std::vector<std::uint8_t> encoded;
      REQUIRE(builder.write(encoded) == ARX_OK);
      const pistoris::GlbResult<pistoris::Ambiance> result = pistoris::Ambiance::importGlb(encoded);
      REQUIRE_FALSE(result);
      CHECK(result.code() == ARX_GLB_BAD_AMBIANCE_TRACK);
      REQUIRE(result.error() != nullptr);
      REQUIRE(result.error()->location().has_value());
      CHECK(result.error()->location()->element == pistoris::GlbElement::kNode);
      CHECK(result.error()->location()->label == "TRACK_bad__sfx/test.wav__track");
      CHECK(result.error()->location()->property.empty());
      CHECK_FALSE(result.error()->detail().empty());
    }

    SUBCASE("Malformed key") {
      pistoris::glb::Builder builder;
      const int root = builder.addNode("arx_ambiance__root");
      const int track = builder.addNode("TRACK_0__sfx/test.wav__track");
      builder.addChild(track, builder.addNode("KEY_bad__key"));
      builder.addChild(root, track);
      builder.addRoot(root);
      std::vector<std::uint8_t> encoded;
      REQUIRE(builder.write(encoded) == ARX_OK);
      CHECK(pistoris::Ambiance::importGlb(encoded).code() == ARX_GLB_BAD_AMBIANCE_KEY);
    }
  }
}
