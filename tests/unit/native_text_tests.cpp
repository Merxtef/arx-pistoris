// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#include "doctest/doctest.h"

#include "arx_pistoris/ambiance.hpp"
#include "arx_pistoris/ambiance/bake.hpp"
#include "arx_pistoris/animation.hpp"
#include "arx_pistoris/animation/bake.hpp"
#include "arx_pistoris/base/status.h"
#include "arx_pistoris/cinematic.hpp"
#include "arx_pistoris/cinematic/bake.hpp"
#include "arx_pistoris/level.hpp"
#include "arx_pistoris/level/bake.hpp"
#include "arx_pistoris/model.hpp"
#include "arx_pistoris/model/bake.hpp"
#include "arx_pistoris/native/amb.hpp"
#include "arx_pistoris/native/cin.hpp"
#include "arx_pistoris/native/dlf.hpp"
#include "arx_pistoris/native/ftl.hpp"
#include "arx_pistoris/native/fts.hpp"
#include "arx_pistoris/native/tea.hpp"
#include "arx_pistoris/native/text.hpp"
#include "arx_pistoris/texture.h"

#include "amb_helpers.h"
#include "animation_helpers.h"
#include "cin_helpers.h"
#include "helpers.h"
#include "image_helpers.h"
#include "model_helpers.h"
#include "native/fixed_string.h"
#include "utils/native_text.h"
#include "utils/utf8.h"

#include <cstdint>
#include <cstring>
#include <ostream>  // IWYU pragma: keep
#include <string>
#include <string_view>
#include <vector>

namespace {

std::string latin1Path(std::string_view prefix, std::string_view suffix) {
  std::string result(prefix);
  result.push_back(static_cast<char>(0xe9));
  result += suffix;
  return result;
}

}  // namespace

TEST_SUITE("native text") {
  TEST_CASE("Auto decodes Latin-1 and never splits UTF-8 prefixes") {
    const std::string raw = latin1Path("caf", "/sound");
    std::string decoded;
    CHECK_FALSE(pistoris::utf8::valid(raw));
    REQUIRE(pistoris::native_text::decode(raw, pistoris::NativeTextMode::kAuto, decoded));
    CHECK(decoded == "caf\xc3\xa9/sound");
    const std::string ambiguous = "\xc3\xa9";
    REQUIRE(pistoris::native_text::decode(ambiguous, pistoris::NativeTextMode::kAuto, decoded));
    CHECK(decoded == ambiguous);
    REQUIRE(pistoris::native_text::decode(ambiguous, pistoris::NativeTextMode::kLatin1, decoded));
    CHECK(decoded == "\xc3\x83\xc2\xa9");
    CHECK_FALSE(pistoris::native_text::decode(raw, pistoris::NativeTextMode::kUtf8, decoded));
    CHECK(decoded == "\xc3\x83\xc2\xa9");
    CHECK(pistoris::utf8::prefixSize("caf\xc3\xa9", 4) == 3);
    CHECK(pistoris::utf8::prefixSize("caf\xc3\xa9", 5) == 5);
    REQUIRE(pistoris::native_text::encode("caf\xc3\xa9", pistoris::NativeTextMode::kLatin1, decoded));
    CHECK(decoded == latin1Path("caf", ""));
    const std::string unchanged = decoded;
    CHECK_FALSE(pistoris::native_text::encode("\xe2\x82\xac", pistoris::NativeTextMode::kLatin1, decoded));
    CHECK(decoded == unchanged);
  }

  TEST_CASE("Fixed native text encoding validates before truncating") {
    char complete[5]{};
    REQUIRE(pistoris::native_text::encodeTruncated("ab\xc3\xa9x", pistoris::NativeTextMode::kUtf8, complete));
    CHECK(std::string_view(complete) == "ab\xc3\xa9");

    char split[4]{};
    REQUIRE(pistoris::native_text::encodeTruncated("ab\xc3\xa9x", pistoris::NativeTextMode::kUtf8, split));
    CHECK(std::string_view(split) == "ab");

    char latin1[4] = {'o', 'l', 'd', '\0'};
    CHECK_FALSE(pistoris::native_text::encodeTruncated("abcd\xe2\x82\xac", pistoris::NativeTextMode::kLatin1, latin1));
    CHECK(std::string_view(latin1) == "old");
    REQUIRE(pistoris::native_text::encodeTruncated("abc\xc2\xa1x", pistoris::NativeTextMode::kLatin1, latin1));
    CHECK(std::string_view(latin1) == "abc");

    constexpr char kEmbeddedNul[] = {'a', '\0', 'b'};
    CHECK_FALSE(pistoris::native_text::encodeTruncated(
        std::string_view(kEmbeddedNul, sizeof(kEmbeddedNul)), pistoris::NativeTextMode::kUtf8, split));

    char exact[4]{};
    REQUIRE(pistoris::native_text::encodeFixed("abc", pistoris::NativeTextMode::kUtf8, exact));
    CHECK_FALSE(pistoris::native_text::encodeFixed("abcd", pistoris::NativeTextMode::kUtf8, exact));
    CHECK(std::string_view(exact) == "abc");
  }

  TEST_CASE("Model native references decode to UTF-8 and bake in the requested encoding") {
    pistoris::Ftl native = makeSemanticModelFtl();
    const std::string raw = latin1Path("graph/obj3d/textures/caf", "");
    constexpr std::string_view kUtf8 = "graph/obj3d/textures/caf\xc3\xa9";
    setFtlName(raw, native.texture_containers[0].filename, sizeof(native.texture_containers[0].filename));
    pistoris::Model model;
    std::vector<std::string> sources;
    REQUIRE(pistoris::Model::importNative(model, native, &sources) == ARX_OK);
    REQUIRE(sources.size() == 1);
    CHECK(sources[0] == kUtf8);

    pistoris::NativeModelBundle baked;
    REQUIRE(model.bakeNativeBundle({.include_texture_files = false}, baked) == ARX_OK);
    CHECK(std::string_view(baked.ftl.texture_containers[0].filename) == kUtf8);
    REQUIRE(model.bakeNativeBundle({.include_texture_files = false, .text_mode = pistoris::NativeTextMode::kLatin1},
                                   baked) == ARX_OK);
    CHECK(std::string_view(baked.ftl.texture_containers[0].filename) == raw);

    const std::vector<std::uint8_t> image = makeTestBmp();
    REQUIRE(model.setTextureImage(0, {image.data(), image.size()}) == ARX_OK);
    REQUIRE(model.bakeNativeBundle({.text_mode = pistoris::NativeTextMode::kLatin1}, baked) == ARX_OK);
    CHECK(std::string_view(baked.ftl.texture_containers[0].filename) == raw);
    REQUIRE(baked.texture_files.size() == 1);
    CHECK(baked.texture_files[0].resource_path == std::string(kUtf8) + ".bmp");

    REQUIRE(model.bakeNativeBundle({.include_texture_files = false, .text_mode = pistoris::NativeTextMode::kLatin1},
                                   baked) == ARX_OK);
    CHECK(std::string_view(baked.ftl.texture_containers[0].filename) == raw);
    CHECK(baked.texture_files.empty());
  }

  TEST_CASE("Animation and Ambiance native sound paths decode and bake as UTF-8 by default") {
    pistoris::Tea tea = makeAnimationTea();
    const std::string sample = latin1Path("custom/caf", "");
    constexpr std::string_view kUtf8Sample = "custom/caf\xc3\xa9";
    std::memcpy(tea.keyframes[1].sample->name, sample.data(), sample.size());
    tea.keyframes[1].sample->name[sample.size()] = '\0';
    pistoris::Animation animation;
    std::vector<pistoris::SoundSourceReference> animation_sources;
    REQUIRE(pistoris::Animation::importNative(animation, tea, &animation_sources) == ARX_OK);
    REQUIRE(animation_sources.size() == 1);
    CHECK(animation_sources[0].path == kUtf8Sample);
    pistoris::NativeAnimationBundle baked_animation;
    REQUIRE(animation.bakeNativeBundle({.include_sound_files = false}, baked_animation) == ARX_OK);
    REQUIRE(baked_animation.tea.keyframes[1].sample.has_value());
    CHECK(std::string_view(baked_animation.tea.keyframes[1].sample->name) == kUtf8Sample);
    REQUIRE(animation.bakeNativeBundle({.include_sound_files = false, .text_mode = pistoris::NativeTextMode::kLatin1},
                                       baked_animation) == ARX_OK);
    REQUIRE(baked_animation.tea.keyframes[1].sample.has_value());
    CHECK(std::string_view(baked_animation.tea.keyframes[1].sample->name) == sample);

    pistoris::Amb amb = makeAmbData();
    const std::string track = latin1Path("sfx/ambiance/caf", ".wav");
    constexpr std::string_view kUtf8Track = "sfx/ambiance/caf\xc3\xa9.wav";
    amb.tracks[0].sample_path = track;
    pistoris::Ambiance ambiance;
    std::vector<pistoris::SoundSourceReference> ambiance_sources;
    REQUIRE(pistoris::Ambiance::importNative(ambiance, amb, &ambiance_sources) == ARX_OK);
    REQUIRE(ambiance_sources.size() == 1);
    CHECK(ambiance_sources[0].path == kUtf8Track);
    pistoris::NativeAmbianceBundle baked_ambiance;
    REQUIRE(ambiance.bakeNativeBundle({.include_sound_files = false}, baked_ambiance) == ARX_OK);
    CHECK(baked_ambiance.amb.tracks[0].sample_path == kUtf8Track);
    REQUIRE(ambiance.bakeNativeBundle({.include_sound_files = false, .text_mode = pistoris::NativeTextMode::kLatin1},
                                      baked_ambiance) == ARX_OK);
    CHECK(baked_ambiance.amb.tracks[0].sample_path == track);
  }

  TEST_CASE("Cinematic native references decode and bake as UTF-8 by default") {
    pistoris::Cin native = makeCinData();
    const std::string image = latin1Path("graph/interface/illustrations/caf", "");
    const std::string sound = latin1Path("cinematic/caf", "");
    constexpr std::string_view kUtf8Image = "graph/interface/illustrations/caf\xc3\xa9";
    constexpr std::string_view kUtf8Sound = "cinematic/caf\xc3\xa9";
    native.bitmaps[0].path = image;
    native.sounds[0].path = sound;
    pistoris::Cinematic cinematic;
    std::vector<std::string> illustration_sources;
    std::vector<pistoris::CinematicSoundSourceReference> sound_sources;
    REQUIRE(pistoris::Cinematic::importNative(cinematic, native, &illustration_sources, &sound_sources) == ARX_OK);
    REQUIRE(illustration_sources.size() == 1);
    CHECK(illustration_sources[0] == kUtf8Image);
    REQUIRE(sound_sources.size() == 1);
    CHECK(sound_sources[0].path == kUtf8Sound);

    pistoris::NativeCinematicBundle baked;
    REQUIRE(cinematic.bakeNativeBundle({.include_illustration_files = false, .include_sound_files = false}, baked) ==
            ARX_OK);
    CHECK(baked.cin.bitmaps[0].path == kUtf8Image);
    CHECK(baked.cin.sounds[0].path == kUtf8Sound);
    REQUIRE(cinematic.bakeNativeBundle({.include_illustration_files = false,
                                        .include_sound_files = false,
                                        .text_mode = pistoris::NativeTextMode::kLatin1},
                                       baked) == ARX_OK);
    CHECK(baked.cin.bitmaps[0].path == image);
    CHECK(baked.cin.sounds[0].path == sound);
  }

  TEST_CASE("Level native texture references decode and bake as UTF-8 by default") {
    pistoris::Fts native = makeTriangleFtsData();
    native.scene.num_textures = 1;
    native.cells[0].polygons[0].tex = 1;
    const std::string raw = latin1Path("graph/obj3d/textures/caf", "");
    constexpr std::string_view kUtf8 = "graph/obj3d/textures/caf\xc3\xa9";
    std::memcpy(native.textures[1].fic, raw.data(), raw.size());
    native.textures[1].fic[raw.size()] = '\0';
    pistoris::Level level;
    std::vector<std::string> sources;
    REQUIRE(pistoris::Level::importNative(level, native, nullptr, nullptr, &sources) == ARX_OK);
    REQUIRE(sources.size() == 1);
    CHECK(sources[0] == kUtf8);
    pistoris::NativeLevelBundle baked;
    pistoris::Level::NativeBakeOptions options;
    options.level_name = "level1";
    options.include_texture_files = false;
    REQUIRE(level.bakeNativeBundle(options, baked) == ARX_OK);
    REQUIRE(baked.fts.textures.size() == 1);
    CHECK(std::string_view(baked.fts.textures.at(1).fic) == kUtf8);

    const std::vector<std::uint8_t> image = makeTestBmp();
    REQUIRE(level.setTextureImage(0, {image.data(), image.size()}) == ARX_OK);
    options.include_texture_files = true;
    options.text_mode = pistoris::NativeTextMode::kLatin1;
    REQUIRE(level.bakeNativeBundle(options, baked) == ARX_OK);
    CHECK(std::string_view(baked.fts.textures.at(1).fic) == raw);
    REQUIRE(baked.texture_files.size() == 1);
    CHECK(baked.texture_files[0].resource_path == std::string(kUtf8) + ".bmp");

    options.include_texture_files = false;
    REQUIRE(level.bakeNativeBundle(options, baked) == ARX_OK);
    CHECK(std::string_view(baked.fts.textures.at(1).fic) == raw);
    CHECK(baked.texture_files.empty());
  }

  TEST_CASE("Native texture limits apply after text encoding") {
    std::string component;
    for (std::size_t index = 0; index < 60; ++index) component += "\xc3\xaa";
    const std::string utf8_path = "g/" + component + "/" + component + "/" + component + "/" + component;
    std::string latin1_path;
    REQUIRE(pistoris::native_text::encode(utf8_path, pistoris::NativeTextMode::kLatin1, latin1_path));
    REQUIRE(utf8_path.size() > sizeof(pistoris::ftl::TextureContainer::filename));
    REQUIRE(latin1_path.size() + 2U <= sizeof(pistoris::ftl::TextureContainer::filename));

    ArxTextureView texture{};
    texture.path = {utf8_path.data(), utf8_path.size()};

    pistoris::Model model;
    REQUIRE(pistoris::Model::importNative(model, makeSemanticModelFtl()) == ARX_OK);
    REQUIRE(model.setTexture(0, texture) == ARX_OK);
    pistoris::NativeModelBundle model_bundle;
    REQUIRE(model.bakeNativeBundle({.include_texture_files = false, .text_mode = pistoris::NativeTextMode::kLatin1},
                                   model_bundle) == ARX_OK);
    CHECK(std::string_view(model_bundle.ftl.texture_containers[0].filename) == latin1_path);
    CHECK(model.bakeNativeBundle({.include_texture_files = false, .text_mode = pistoris::NativeTextMode::kUtf8},
                                 model_bundle) == ARX_MODEL_BAD_TEXTURE_PATH);

    pistoris::Fts fts = makeTriangleFtsData();
    fts.scene.num_textures = 1;
    fts.cells[0].polygons[0].tex = 1;
    REQUIRE(pistoris::copyFixedString("texture", fts.textures[1].fic, false));
    pistoris::Level level;
    REQUIRE(pistoris::Level::importNative(level, fts) == ARX_OK);
    REQUIRE(level.setTexture(0, texture) == ARX_OK);
    pistoris::Level::NativeBakeOptions level_options;
    level_options.level_name = "level1";
    level_options.include_texture_files = false;
    level_options.text_mode = pistoris::NativeTextMode::kLatin1;
    pistoris::NativeLevelBundle level_bundle;
    REQUIRE(level.bakeNativeBundle(level_options, level_bundle) == ARX_OK);
    CHECK(std::string_view(level_bundle.fts.textures.at(1).fic) == latin1_path);
    level_options.text_mode = pistoris::NativeTextMode::kUtf8;
    CHECK(level.bakeNativeBundle(level_options, level_bundle) == ARX_FTS_BAD_TEXTURE_PATH);
  }

  TEST_CASE("Level DLF text decodes to UTF-8 and bakes in the requested encoding") {
    pistoris::Dlf native;
    REQUIRE(pistoris::copyFixedString("graph/levels/level1", native.scene_path, false));

    const std::string raw_entity = latin1Path("graph/obj3d/interactive/items/caf", "");
    constexpr std::string_view kUtf8Entity = "graph/obj3d/interactive/items/caf\xc3\xa9";
    native.entities.emplace_back();
    REQUIRE(pistoris::copyFixedString(raw_entity, native.entities.back().class_path, false));

    const std::string raw_zone = latin1Path("zone_caf", "");
    const std::string raw_ambiance = latin1Path("ambiance/caf", "");
    constexpr std::string_view kUtf8Ambiance = "ambiance/caf\xc3\xa9";
    pistoris::dlf::Zone zone;
    REQUIRE(pistoris::copyFixedString(raw_zone, zone.name, false));
    zone.points = {{0.0f, 0.0f, 0.0f}, {100.0f, 0.0f, 0.0f}, {100.0f, 0.0f, 100.0f}};
    zone.height = -1;
    zone.ambiance.emplace();
    REQUIRE(pistoris::copyFixedString(raw_ambiance, zone.ambiance->name, false));
    native.zones.push_back(zone);

    const std::string raw_path = latin1Path("path_caf", "");
    pistoris::dlf::Path path;
    REQUIRE(pistoris::copyFixedString(raw_path, path.name, false));
    path.nodes.push_back({});
    native.paths.push_back(path);

    const pistoris::Fts fts = makeTriangleFtsData();
    pistoris::Level utf8_only;
    CHECK(pistoris::Level::importNative(utf8_only, fts, nullptr, &native, nullptr, pistoris::NativeTextMode::kUtf8) ==
          ARX_LEVEL_BAD_ENTITY_CLASS_PATH);

    pistoris::Level level;
    REQUIRE(pistoris::Level::importNative(level, fts, nullptr, &native, nullptr, pistoris::NativeTextMode::kLatin1) ==
            ARX_OK);

    pistoris::Level::DlfBakeOptions options;
    options.level_name = "level1";
    options.text_mode = pistoris::NativeTextMode::kUtf8;
    pistoris::Dlf baked;
    REQUIRE(level.bakeDlf(options, baked) == ARX_OK);
    REQUIRE(baked.entities.size() == 1);
    CHECK(pistoris::fixedStringView(baked.entities[0].class_path) == kUtf8Entity);
    REQUIRE(baked.zones.size() == 1);
    CHECK(pistoris::fixedStringView(baked.zones[0].name) == "zone_caf-");
    REQUIRE(baked.zones[0].ambiance.has_value());
    CHECK(pistoris::fixedStringView(baked.zones[0].ambiance->name) == kUtf8Ambiance);
    REQUIRE(baked.paths.size() == 1);
    CHECK(pistoris::fixedStringView(baked.paths[0].name) == "path_caf-");

    options.text_mode = pistoris::NativeTextMode::kLatin1;
    REQUIRE(level.bakeDlf(options, baked) == ARX_OK);
    CHECK(pistoris::fixedStringView(baked.entities[0].class_path) == raw_entity);
    CHECK(pistoris::fixedStringView(baked.zones[0].name) == "zone_caf-");
    CHECK(pistoris::fixedStringView(baked.zones[0].ambiance->name) == raw_ambiance);
    CHECK(pistoris::fixedStringView(baked.paths[0].name) == "path_caf-");
  }
}
