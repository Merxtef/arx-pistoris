// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#include "doctest/doctest.h"

#include "arx_pistoris/arx_pistoris.h"
#include "arx_pistoris/cinematic/types.h"
#include "arx_pistoris/native/text.h"

#include <cstddef>
#include <cstdint>
#include <string_view>

namespace {

ArxStringView view(std::string_view value) { return {value.data(), value.size()}; }

ArxCinematicKeyframe keyframe(std::int32_t frame, ArxSoundHandle sound) {
  ArxCinematicKeyframe key{};
  key.frame = frame;
  key.illustration = 0;
  key.outgoing_speed = 1.0f;
  key.sound = sound;
  key.interpolation = ARX_CINEMATIC_INTERPOLATION_LINEAR;
  return key;
}

}  // namespace

TEST_SUITE("C Cinematic API") {
  TEST_CASE("Builds, bakes, and imports through opaque handles") {
    ArxCinematic* cinematic = nullptr;
    REQUIRE(arx_pistoris_cinematic_create(&cinematic) == ARX_OK);
    REQUIRE(arx_pistoris_cinematic_set_resource_path(cinematic, view("cinematic:test")) == ARX_OK);

    const ArxTextureView texture{view("graph/interface/illustrations/test"), {}, view(".bmp")};
    ArxTextureIndex texture_index = ARX_NO_TEXTURE;
    REQUIRE(arx_pistoris_cinematic_add_texture(cinematic, &texture, &texture_index) == ARX_OK);
    ArxCinematicIllustrationIndex illustration = ARX_INVALID_CINEMATIC_ILLUSTRATION;
    REQUIRE(arx_pistoris_cinematic_add_illustration(cinematic, {texture_index, 1}, &illustration) == ARX_OK);
    REQUIRE(arx_pistoris_cinematic_set_timeline(cinematic, 10, 25.0f) == ARX_OK);

    ArxSoundHandle sound = ARX_NO_SOUND_HANDLE;
    REQUIRE(arx_pistoris_cinematic_add_sound(cinematic, ARX_SOUND_EFFECT, view("cinematic/test"), &sound) == ARX_OK);
    std::size_t index = 99;
    ArxCinematicKeyframe first = keyframe(0, sound);
    ArxCinematicKeyframe last = keyframe(10, ARX_NO_SOUND_HANDLE);
    REQUIRE(arx_pistoris_cinematic_add_keyframe(cinematic, &first, &index) == ARX_OK);
    REQUIRE(arx_pistoris_cinematic_add_keyframe(cinematic, &last, &index) == ARX_OK);
    REQUIRE(arx_pistoris_cinematic_validate(cinematic) == ARX_OK);

    ArxNativeCinematicBakeOptions options{};
    ArxCin* native = nullptr;
    ArxNativeTextureFiles* illustration_files = nullptr;
    ArxCinematicSoundFiles* sound_files = nullptr;
    options.illustration_format = ARX_IMAGE_FORMAT_PNG;
    CHECK(arx_pistoris_cinematic_bake_native(cinematic, &options, &native, &illustration_files, &sound_files) ==
          ARX_INVALID_OPTIONS);
    CHECK(native == nullptr);
    options.illustration_format = ARX_IMAGE_FORMAT_UNKNOWN;
    REQUIRE(arx_pistoris_cinematic_bake_native(cinematic, &options, &native, &illustration_files, &sound_files) ==
            ARX_OK);
    std::size_t count = 99;
    REQUIRE(arx_pistoris_native_texture_files_count(illustration_files, &count) == ARX_OK);
    CHECK(count == 0);
    REQUIRE(arx_pistoris_cinematic_sound_files_count(sound_files, &count) == ARX_OK);
    CHECK(count == 0);

    std::uint8_t* bytes = nullptr;
    std::size_t byte_count = 0;
    REQUIRE(arx_pistoris_cin_write(native, &bytes, &byte_count) == ARX_OK);
    CHECK(bytes != nullptr);
    CHECK(byte_count != 0);

    ArxCinematic* imported = nullptr;
    ArxTextureSourcePaths* illustration_sources = nullptr;
    ArxCinematicSoundSourceReferences* sound_sources = nullptr;
    REQUIRE(arx_pistoris_cinematic_import_native(
                native, &imported, &illustration_sources, &sound_sources, ARX_NATIVE_TEXT_AUTO) == ARX_OK);
    REQUIRE(arx_pistoris_texture_source_paths_count(illustration_sources, &count) == ARX_OK);
    CHECK(count == 1);
    REQUIRE(arx_pistoris_cinematic_sound_source_references_count(sound_sources, &count) == ARX_OK);
    CHECK(count == 1);
    ArxCinematicSoundSourceReference reference{};
    REQUIRE(arx_pistoris_cinematic_sound_source_references_get(sound_sources, 0, &reference) == ARX_OK);
    CHECK(reference.sound != ARX_NO_SOUND_HANDLE);
    CHECK((std::string_view(reference.path.data, reference.path.size) == "cinematic/test"));

    arx_pistoris_cinematic_sound_source_references_destroy(sound_sources);
    arx_pistoris_texture_source_paths_destroy(illustration_sources);
    arx_pistoris_cinematic_destroy(imported);
    arx_pistoris_free_bytes(bytes);
    arx_pistoris_cinematic_sound_files_destroy(sound_files);
    arx_pistoris_native_texture_files_destroy(illustration_files);
    arx_pistoris_cin_destroy(native);
    arx_pistoris_cinematic_destroy(cinematic);
  }

  TEST_CASE("Rejects invalid handles and pointers") {
    CHECK(arx_pistoris_cinematic_create(nullptr) == ARX_INVALID_DATA_POINTER);
    CHECK(arx_pistoris_cinematic_validate(nullptr) == ARX_INVALID_HANDLE);
    ArxCinematic* cinematic = nullptr;
    REQUIRE(arx_pistoris_cinematic_create(&cinematic) == ARX_OK);
    CHECK(arx_pistoris_cinematic_resource_path(cinematic, nullptr) == ARX_INVALID_DATA_POINTER);
    CHECK(arx_pistoris_cinematic_sound_count(cinematic, static_cast<ArxSoundKind>(99), nullptr) ==
          ARX_INVALID_DATA_POINTER);
    std::size_t count = 0;
    CHECK(arx_pistoris_cinematic_sound_count(cinematic, static_cast<ArxSoundKind>(99), &count) == ARX_INVALID_OPTIONS);
    arx_pistoris_cinematic_destroy(cinematic);
  }
}
