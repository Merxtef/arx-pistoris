// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#include "doctest/doctest.h"

#include "arx_pistoris/arx_pistoris.h"
#include "arx_pistoris/cinematic/types.h"

#include "image_helpers.h"

#include <cstddef>
#include <cstdint>
#include <string_view>
#include <vector>

namespace {

ArxStringView view(std::string_view value) { return {value.data(), value.size()}; }

}  // namespace

TEST_SUITE("C Cinematic GLB API") {
  TEST_CASE("Exports and imports through optional conversion collections") {
    ArxCinematic* cinematic = nullptr;
    REQUIRE(arx_pistoris_cinematic_create(&cinematic) == ARX_OK);

    const std::vector<std::uint8_t> image = makeSolidTestBmp(8, 4);
    const ArxTextureView texture{view("story/c_api"), {image.data(), image.size()}, {}};
    ArxTextureIndex texture_index = ARX_NO_TEXTURE;
    REQUIRE(arx_pistoris_cinematic_add_texture(cinematic, &texture, &texture_index) == ARX_OK);
    ArxCinematicIllustrationIndex illustration = ARX_INVALID_CINEMATIC_ILLUSTRATION;
    REQUIRE(arx_pistoris_cinematic_add_illustration(cinematic, {texture_index, 1}, &illustration) == ARX_OK);
    REQUIRE(arx_pistoris_cinematic_set_timeline(cinematic, 2, 25.0f) == ARX_OK);

    ArxCinematicKeyframe first{};
    first.frame = 0;
    first.illustration = illustration;
    first.outgoing_speed = 1.0f;
    first.interpolation = ARX_CINEMATIC_INTERPOLATION_LINEAR;
    ArxCinematicKeyframe last = first;
    last.frame = 2;
    std::size_t index = 0;
    REQUIRE(arx_pistoris_cinematic_add_keyframe(cinematic, &first, &index) == ARX_OK);
    REQUIRE(arx_pistoris_cinematic_add_keyframe(cinematic, &last, &index) == ARX_OK);

    std::uint8_t* glb = nullptr;
    std::size_t glb_size = 0;
    ArxCinematicSoundFiles* files = nullptr;
    REQUIRE(arx_pistoris_cinematic_export_glb(cinematic, &glb, &glb_size, &files) == ARX_OK);
    CHECK(glb != nullptr);
    CHECK(glb_size != 0);
    std::size_t count = 99;
    REQUIRE(arx_pistoris_cinematic_sound_files_count(files, &count) == ARX_OK);
    CHECK(count == 0);

    ArxCinematic* imported = nullptr;
    ArxCinematicSoundSourceReferences* sources = nullptr;
    REQUIRE(arx_pistoris_cinematic_import_glb(glb, glb_size, &imported, &sources) == ARX_OK);
    REQUIRE(arx_pistoris_cinematic_keyframe_count(imported, &count) == ARX_OK);
    CHECK(count == 2);
    REQUIRE(arx_pistoris_cinematic_sound_source_references_count(sources, &count) == ARX_OK);
    CHECK(count == 0);

    arx_pistoris_cinematic_sound_source_references_destroy(sources);
    arx_pistoris_cinematic_destroy(imported);
    arx_pistoris_cinematic_sound_files_destroy(files);
    arx_pistoris_free_bytes(glb);
    arx_pistoris_cinematic_destroy(cinematic);
  }

  TEST_CASE("Rejects invalid conversion pointers") {
    ArxCinematic* cinematic = nullptr;
    REQUIRE(arx_pistoris_cinematic_create(&cinematic) == ARX_OK);
    std::uint8_t* data = nullptr;
    std::size_t size = 0;
    CHECK(arx_pistoris_cinematic_export_glb(nullptr, &data, &size, nullptr) == ARX_INVALID_HANDLE);
    CHECK(arx_pistoris_cinematic_export_glb(cinematic, nullptr, &size, nullptr) == ARX_INVALID_DATA_POINTER);
    CHECK(arx_pistoris_cinematic_import_glb(nullptr, 0, &cinematic, nullptr) == ARX_INVALID_DATA_POINTER);
    arx_pistoris_cinematic_destroy(cinematic);
  }
}
