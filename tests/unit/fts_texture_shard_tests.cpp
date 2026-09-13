// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#include "doctest/doctest.h"

#include "arx_pistoris/base/status.h"
#include "arx_pistoris/level.hpp"
#include "arx_pistoris/texture.hpp"

#include "image_helpers.h"
#include "level/native/fts/texture_shards.h"
#include "level/native/internal.h"
#include "modules/textures.h"

#include <cstddef>
#include <string>
#include <vector>

TEST_SUITE("FtsTextureShards") {
  TEST_CASE("AllocatorFirstFitsHolesAndReusesGlobalShardsAcrossRooms") {
    pistoris::level_native::fts_bake::TextureShardAllocator allocator(2, 2, 15);

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
    pistoris::TexturesData textures;
    textures.textures.resize(2);
    textures.textures[0].path = "graph/obj3d/textures/l4_dwarf_[stone]_wall01";
    textures.textures[0].encoded_image = makeTestBmp();
    textures.textures[1].path = "graph/obj3d/textures/l4_dwarf_[stone]_wall01_1";
    pistoris::Level::NativeBakeOptions options;
    pistoris::level_native::NativeBakeWarnings warnings;
    pistoris::level_native::NativeTextureResources resources;
    REQUIRE(pistoris::level_native::projectNativeTextures(textures, options, resources, warnings) == ARX_OK);

    REQUIRE(resources.families.size() == 2);
    REQUIRE(resources.families[0].shards.size() == 1);
    CHECK(resources.families[0].shards[0].resource_path == "graph/obj3d/textures/l4_dwarf_[stone]_wall01");
    CHECK(resources.families[1].shards[0].resource_path == "graph/obj3d/textures/l4_dwarf_[stone]_wall01_1");

    std::size_t shard = 0;
    REQUIRE(pistoris::level_native::addNativeTextureShard(resources, 0, shard) == ARX_OK);
    REQUIRE(shard == 1);
    CHECK(resources.families[0].shards[1].resource_path == "graph/obj3d/textures/l4_dwarf_[stone]_wall01_2");

    std::vector<pistoris::NativeTextureFile> files;
    pistoris::level_native::buildNativeTextureFiles(resources, files);
    REQUIRE(files.size() == 2);
    CHECK(files[0].source_texture == 0);
    CHECK(files[0].resource_path == "graph/obj3d/textures/l4_dwarf_[stone]_wall01.bmp");
    CHECK(files[1].source_texture == 0);
    CHECK(files[1].resource_path == "graph/obj3d/textures/l4_dwarf_[stone]_wall01_2.bmp");
    CHECK(files[0].encoded_image == files[1].encoded_image);
  }
}
