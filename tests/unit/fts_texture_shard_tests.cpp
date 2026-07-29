// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#include "doctest/doctest.h"

#include "arx_pistoris/level.hpp"
#include "arx_pistoris/level/bake.hpp"
#include "arx_pistoris/pistoris_types.h"

#include "arx/conversion/level/fts/texture_shards.h"
#include "arx/conversion/level/internal.h"
#include "image_helpers.h"
#include "modules/geometry.h"

#include <cstddef>
#include <string>
#include <vector>

TEST_SUITE("FtsTextureShards") {
  TEST_CASE("AllocatorFirstFitsHolesAndReusesGlobalShardsAcrossRooms") {
    pistoris::arx_level_conversion::fts_bake::TextureShardAllocator allocator(2, 2, 15);

    CHECK(allocator.assign(0, 0, 4) == 0);
    CHECK(allocator.assign(0, 0, 4) == 0);
    CHECK(allocator.assign(0, 0, 4) == 0);
    CHECK(allocator.assign(0, 0, 4) == 1);
    CHECK(allocator.assign(0, 0, 3) == 0);

    for (std::size_t i = 0; i < 5; ++i) CHECK(allocator.assign(1, 0, 3) == 0);
    CHECK(allocator.assign(1, 0, 4) == 1);

    CHECK(allocator.assign(0, 1, 4) == 0);
    CHECK(allocator.assign(0, 1, 4) == 0);
    CHECK(allocator.assign(0, 1, 4) == 0);
    CHECK(allocator.assign(0, 1, 4) == 1);
  }

  TEST_CASE("ProjectedTextureFamiliesOwnCollisionFreeRoundtripSafeShardsAndSidecars") {
    std::vector<pistoris::Texture> textures(2);
    textures[0].path = "graph/obj3d/textures/l4_dwarf_[stone]_wall01.bmp";
    textures[0].encoded_image = makeTestBmp();
    textures[1].path = "graph/obj3d/textures/l4_dwarf_[stone]_wall01_1.bmp";
    pistoris::Level::NativeBakeOptions options;
    pistoris::arx_level_conversion::NativeBakeWarnings warnings;
    pistoris::arx_level_conversion::NativeTextureResources resources;
    REQUIRE(pistoris::arx_level_conversion::projectNativeTextures(textures, options, resources, warnings) == ARX_OK);

    REQUIRE(resources.families.size() == 2);
    REQUIRE(resources.families[0].shards.size() == 1);
    CHECK(resources.families[0].shards[0].resource_path == "graph/obj3d/textures/l4_dwarf_[stone]__wall01");
    CHECK(resources.families[1].shards[0].resource_path == "graph/obj3d/textures/l4_dwarf_[stone]_wall01_1");

    std::size_t shard = 0;
    REQUIRE(pistoris::arx_level_conversion::addNativeTextureShard(resources, 0, shard) == ARX_OK);
    REQUIRE(shard == 1);
    CHECK(resources.families[0].shards[1].resource_path == "graph/obj3d/textures/l4_dwarf_[stone]_wall01_2");

    std::vector<pistoris::NativeTextureFile> files;
    pistoris::arx_level_conversion::buildNativeTextureFiles(resources, files);
    REQUIRE(files.size() == 2);
    CHECK(files[0].source_texture == 0);
    CHECK(files[0].resource_path == "graph/obj3d/textures/l4_dwarf_[stone]__wall01.bmp");
    CHECK(files[1].source_texture == 0);
    CHECK(files[1].resource_path == "graph/obj3d/textures/l4_dwarf_[stone]_wall01_2.bmp");
    CHECK(files[0].encoded_image == files[1].encoded_image);
  }
}
