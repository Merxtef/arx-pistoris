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
#include <utility>
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
    std::vector<std::string> sources;
    auto import_result = pistoris::Model::importNative(native, &sources);
    REQUIRE(import_result);
    pistoris::Model model = std::move(*import_result);
    REQUIRE(sources.size() == 1);
    CHECK(sources[0] == kUtf8);

    auto bake_result = model.bakeNativeBundle({.include_texture_files = false});
    REQUIRE(bake_result);
    CHECK(std::string_view(bake_result->ftl.texture_containers[0].filename) == kUtf8);
    auto latin1_bake_result =
        model.bakeNativeBundle({.include_texture_files = false, .text_mode = pistoris::NativeTextMode::kLatin1});
    REQUIRE(latin1_bake_result);
    CHECK(std::string_view(latin1_bake_result->ftl.texture_containers[0].filename) == raw);

    const std::vector<std::uint8_t> image = makeTestBmp();
    REQUIRE(model.setTextureImage(0, {image.data(), image.size()}));
    auto file_bake_result = model.bakeNativeBundle({.text_mode = pistoris::NativeTextMode::kLatin1});
    REQUIRE(file_bake_result);
    CHECK(std::string_view(file_bake_result->ftl.texture_containers[0].filename) == raw);
    REQUIRE(file_bake_result->texture_files.size() == 1);
    CHECK(file_bake_result->texture_files[0].resource_path == std::string(kUtf8) + ".bmp");

    auto no_file_bake_result =
        model.bakeNativeBundle({.include_texture_files = false, .text_mode = pistoris::NativeTextMode::kLatin1});
    REQUIRE(no_file_bake_result);
    CHECK(std::string_view(no_file_bake_result->ftl.texture_containers[0].filename) == raw);
    CHECK(no_file_bake_result->texture_files.empty());
  }

  TEST_CASE("Animation and Ambiance native sound paths decode and bake as UTF-8 by default") {
    pistoris::Tea tea = makeAnimationTea();
    const std::string sample = latin1Path("custom/caf", "");
    constexpr std::string_view kUtf8Sample = "custom/caf\xc3\xa9";
    std::memcpy(tea.keyframes[1].sample->name, sample.data(), sample.size());
    tea.keyframes[1].sample->name[sample.size()] = '\0';
    std::vector<pistoris::SoundSourceReference> animation_sources;
    auto animation_result = pistoris::Animation::importNative(tea, &animation_sources);
    REQUIRE(animation_result);
    pistoris::Animation animation = std::move(*animation_result);
    REQUIRE(animation_sources.size() == 1);
    CHECK(animation_sources[0].path == kUtf8Sample);
    auto animation_bake = animation.bakeNativeBundle({.include_sound_files = false});
    REQUIRE(animation_bake);
    REQUIRE(animation_bake->tea.keyframes[1].sample.has_value());
    CHECK(std::string_view(animation_bake->tea.keyframes[1].sample->name) == kUtf8Sample);
    auto latin1_animation_bake =
        animation.bakeNativeBundle({.include_sound_files = false, .text_mode = pistoris::NativeTextMode::kLatin1});
    REQUIRE(latin1_animation_bake);
    REQUIRE(latin1_animation_bake->tea.keyframes[1].sample.has_value());
    CHECK(std::string_view(latin1_animation_bake->tea.keyframes[1].sample->name) == sample);

    pistoris::Amb amb = makeAmbData();
    const std::string track = latin1Path("sfx/ambiance/caf", ".wav");
    constexpr std::string_view kUtf8Track = "sfx/ambiance/caf\xc3\xa9.wav";
    amb.tracks[0].sample_path = track;
    std::vector<pistoris::SoundSourceReference> ambiance_sources;
    auto ambiance_result = pistoris::Ambiance::importNative(amb, &ambiance_sources);
    REQUIRE(ambiance_result);
    pistoris::Ambiance ambiance = std::move(*ambiance_result);
    REQUIRE(ambiance_sources.size() == 1);
    CHECK(ambiance_sources[0].path == kUtf8Track);
    auto ambiance_bake = ambiance.bakeNativeBundle({.include_sound_files = false});
    REQUIRE(ambiance_bake);
    CHECK(ambiance_bake->amb.tracks[0].sample_path == kUtf8Track);
    auto latin1_ambiance_bake =
        ambiance.bakeNativeBundle({.include_sound_files = false, .text_mode = pistoris::NativeTextMode::kLatin1});
    REQUIRE(latin1_ambiance_bake);
    CHECK(latin1_ambiance_bake->amb.tracks[0].sample_path == track);
  }

  TEST_CASE("Cinematic native references decode and bake as UTF-8 by default") {
    pistoris::Cin native = makeCinData();
    const std::string image = latin1Path("graph/interface/illustrations/caf", "");
    const std::string sound = latin1Path("cinematic/caf", "");
    constexpr std::string_view kUtf8Image = "graph/interface/illustrations/caf\xc3\xa9";
    constexpr std::string_view kUtf8Sound = "cinematic/caf\xc3\xa9";
    native.bitmaps[0].path = image;
    native.sounds[0].path = sound;
    std::vector<std::string> illustration_sources;
    std::vector<pistoris::CinematicSoundSourceReference> sound_sources;
    auto import_result = pistoris::Cinematic::importNative(native, &illustration_sources, &sound_sources);
    REQUIRE(import_result);
    pistoris::Cinematic cinematic = std::move(*import_result);
    REQUIRE(illustration_sources.size() == 1);
    CHECK(illustration_sources[0] == kUtf8Image);
    REQUIRE(sound_sources.size() == 1);
    CHECK(sound_sources[0].path == kUtf8Sound);

    auto bake_result = cinematic.bakeNativeBundle({.include_illustration_files = false, .include_sound_files = false});
    REQUIRE(bake_result);
    CHECK(bake_result->cin.bitmaps[0].path == kUtf8Image);
    CHECK(bake_result->cin.sounds[0].path == kUtf8Sound);
    auto latin1_bake_result = cinematic.bakeNativeBundle({.include_illustration_files = false,
                                                          .include_sound_files = false,
                                                          .text_mode = pistoris::NativeTextMode::kLatin1});
    REQUIRE(latin1_bake_result);
    CHECK(latin1_bake_result->cin.bitmaps[0].path == image);
    CHECK(latin1_bake_result->cin.sounds[0].path == sound);
  }

  TEST_CASE("Level native texture references decode and bake as UTF-8 by default") {
    pistoris::Fts native = makeTriangleFtsData();
    native.scene.num_textures = 1;
    native.cells[0].polygons[0].tex = 1;
    const std::string raw = latin1Path("graph/obj3d/textures/caf", "");
    constexpr std::string_view kUtf8 = "graph/obj3d/textures/caf\xc3\xa9";
    std::memcpy(native.textures[1].fic, raw.data(), raw.size());
    native.textures[1].fic[raw.size()] = '\0';
    std::vector<std::string> sources;
    auto import_result = pistoris::Level::importNative(native, nullptr, nullptr, &sources);
    REQUIRE(import_result);
    pistoris::Level level = std::move(*import_result);
    REQUIRE(sources.size() == 1);
    CHECK(sources[0] == kUtf8);
    pistoris::Level::NativeBakeOptions options;
    options.level_name = "level1";
    options.include_texture_files = false;
    auto bake_result = level.bakeNativeBundle(options);
    REQUIRE(bake_result);
    REQUIRE(bake_result->fts.textures.size() == 1);
    CHECK(std::string_view(bake_result->fts.textures.at(1).fic) == kUtf8);

    const std::vector<std::uint8_t> image = makeTestBmp();
    REQUIRE(level.setTextureImage(0, {image.data(), image.size()}));
    options.include_texture_files = true;
    options.text_mode = pistoris::NativeTextMode::kLatin1;
    auto file_bake_result = level.bakeNativeBundle(options);
    REQUIRE(file_bake_result);
    CHECK(std::string_view(file_bake_result->fts.textures.at(1).fic) == raw);
    REQUIRE(file_bake_result->texture_files.size() == 1);
    CHECK(file_bake_result->texture_files[0].resource_path == std::string(kUtf8) + ".bmp");

    options.include_texture_files = false;
    auto no_file_bake_result = level.bakeNativeBundle(options);
    REQUIRE(no_file_bake_result);
    CHECK(std::string_view(no_file_bake_result->fts.textures.at(1).fic) == raw);
    CHECK(no_file_bake_result->texture_files.empty());
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

    auto model_result = pistoris::Model::importNative(makeSemanticModelFtl());
    REQUIRE(model_result);
    pistoris::Model model = std::move(*model_result);
    REQUIRE(model.setTexture(0, texture));
    auto model_bake =
        model.bakeNativeBundle({.include_texture_files = false, .text_mode = pistoris::NativeTextMode::kLatin1});
    REQUIRE(model_bake);
    CHECK(std::string_view(model_bake->ftl.texture_containers[0].filename) == latin1_path);
    CHECK(
        model.bakeNativeBundle({.include_texture_files = false, .text_mode = pistoris::NativeTextMode::kUtf8}).code() ==
        ARX_MODEL_BAD_TEXTURE_PATH);

    pistoris::Fts fts = makeTriangleFtsData();
    fts.scene.num_textures = 1;
    fts.cells[0].polygons[0].tex = 1;
    REQUIRE(pistoris::copyFixedString("texture", fts.textures[1].fic, false));
    auto level_result = pistoris::Level::importNative(fts);
    REQUIRE(level_result);
    pistoris::Level level = std::move(*level_result);
    REQUIRE(level.setTexture(0, texture));
    pistoris::Level::NativeBakeOptions level_options;
    level_options.level_name = "level1";
    level_options.include_texture_files = false;
    level_options.text_mode = pistoris::NativeTextMode::kLatin1;
    auto level_bake = level.bakeNativeBundle(level_options);
    REQUIRE(level_bake);
    CHECK(std::string_view(level_bake->fts.textures.at(1).fic) == latin1_path);
    level_options.text_mode = pistoris::NativeTextMode::kUtf8;
    CHECK(level.bakeNativeBundle(level_options).code() == ARX_FTS_BAD_TEXTURE_PATH);
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
    CHECK(pistoris::Level::importNative(fts, nullptr, &native, nullptr, pistoris::NativeTextMode::kUtf8).code() ==
          ARX_LEVEL_BAD_ENTITY_CLASS_PATH);

    auto level_result =
        pistoris::Level::importNative(fts, nullptr, &native, nullptr, pistoris::NativeTextMode::kLatin1);
    REQUIRE(level_result);
    pistoris::Level level = std::move(*level_result);

    pistoris::Level::DlfBakeOptions options;
    options.level_name = "level1";
    options.text_mode = pistoris::NativeTextMode::kUtf8;
    auto bake_result = level.bakeDlf(options);
    REQUIRE(bake_result);
    REQUIRE(bake_result->entities.size() == 1);
    CHECK(pistoris::fixedStringView(bake_result->entities[0].class_path) == kUtf8Entity);
    REQUIRE(bake_result->zones.size() == 1);
    CHECK(pistoris::fixedStringView(bake_result->zones[0].name) == "zone_caf-");
    REQUIRE(bake_result->zones[0].ambiance.has_value());
    CHECK(pistoris::fixedStringView(bake_result->zones[0].ambiance->name) == kUtf8Ambiance);
    REQUIRE(bake_result->paths.size() == 1);
    CHECK(pistoris::fixedStringView(bake_result->paths[0].name) == "path_caf-");

    options.text_mode = pistoris::NativeTextMode::kLatin1;
    auto latin1_bake_result = level.bakeDlf(options);
    REQUIRE(latin1_bake_result);
    CHECK(pistoris::fixedStringView(latin1_bake_result->entities[0].class_path) == raw_entity);
    CHECK(pistoris::fixedStringView(latin1_bake_result->zones[0].name) == "zone_caf-");
    CHECK(pistoris::fixedStringView(latin1_bake_result->zones[0].ambiance->name) == raw_ambiance);
    CHECK(pistoris::fixedStringView(latin1_bake_result->paths[0].name) == "path_caf-");
  }
}
