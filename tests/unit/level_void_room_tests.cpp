// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#include "doctest/doctest.h"

#include "arx_pistoris/base/indices.h"
#include "arx_pistoris/base/math.h"
#include "arx_pistoris/base/status.h"
#include "arx_pistoris/level.hpp"
#include "arx_pistoris/level/types.h"

#include "external/glb/accessor.h"
#include "external/glb/container.h"
#include "external/glb/level/names.h"
#include "image_helpers.h"
#include "level/data.h"
#include "level/debug/access.h"
#include "level/validation.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <string_view>

namespace {

using namespace pistoris;

Level mixedLevel() {
  Level level;
  constexpr std::array<float, 18> kPositions = {
      100,
      0,
      100,
      200,
      0,
      100,
      100,
      0,
      200,
      9000,
      1000,
      9000,
      9100,
      1000,
      9000,
      9000,
      1000,
      9100,
  };
  constexpr std::array<std::uint32_t, 6> kIndices = {0, 1, 2, 3, 4, 5};
  constexpr std::array<float, 12> kUvs{};
  constexpr std::array<float, 18> kNormals = {0, -1, 0, 0, -1, 0, 0, -1, 0, 0, -1, 0, 0, -1, 0, 0, -1, 0};
  constexpr std::array<TextureIndex, 2> kTextures = {kNoTexture, kNoTexture};
  constexpr std::array<float, 2> kTransvals{};
  constexpr std::array<float, 3> kColor = {0.25f, 0.5f, 0.75f};
  REQUIRE(level.replaceVertices(kPositions).code() == ARX_OK);
  REQUIRE(level.addRoom({{"main", 4}}).code() == ARX_OK);
  REQUIRE(level.replaceFaces(kIndices, kUvs, kNormals, kTextures, kTransvals, kColor).code() == ARX_OK);
  REQUIRE(level.replaceFaceRooms(std::array<RoomIndex, 2>{0, kNoRoom}).code() == ARX_OK);
  return level;
}

const LevelDerivedState& derived(const Level& level) {
  return level_debug::LevelDebugAccess::validation(level).derived;
}

void checkEffectiveRange(const Level& level, float min_x, float max_x) {
  REQUIRE(level.validate().code() == ARX_OK);
  REQUIRE(derived(level).effective_bounds.has_value());
  CHECK(derived(level).effective_bounds->min.x == doctest::Approx(min_x));
  CHECK(derived(level).effective_bounds->max.x == doctest::Approx(max_x));
}

}  // namespace

TEST_SUITE("LevelVoidRooms") {
  TEST_CASE("Effective bounds follow cached room affiliations while geometry bounds retain void faces") {
    Level level = mixedLevel();
    checkEffectiveRange(level, 100, 200);
    REQUIRE(level.bounds().has_value());
    REQUIRE(level.referencedBounds().has_value());
    CHECK(level.bounds()->max.x == 9100);
    CHECK(level.referencedBounds()->max.x == 9100);

    REQUIRE(level.setFaceRoom(1, 0).code() == ARX_OK);
    checkEffectiveRange(level, 100, 9100);
    REQUIRE(level.setFaceRoom(1, kNoRoom).code() == ARX_OK);
    checkEffectiveRange(level, 100, 200);
    REQUIRE(level.replaceFaceRooms(std::array<RoomIndex, 2>{kNoRoom, 0}).code() == ARX_OK);
    checkEffectiveRange(level, 9000, 9100);

    ArxLevelFace face = level.faces()[0];
    face.room = 0;
    REQUIRE(level.setFace(0, face).code() == ARX_OK);
    checkEffectiveRange(level, 100, 9100);
    REQUIRE(level.removeRoom(0).code() == ARX_OK);
    CHECK(level.validate().code() == ARX_OK);
    CHECK_FALSE(derived(level).effective_bounds.has_value());
    CHECK(level.roomCount() == 0);
    CHECK(level.faceCount() == 2);
    CHECK(level.setFaceRoom(0, 0).code() == ARX_LEVEL_BAD_FACE_ROOM_INDEX);
  }

  TEST_CASE("GLB preserves void geometry without introducing a room or moving minimap geometry") {
    Level level = mixedLevel();
    const auto image = makeTestBmp();
    const ArxRect map_bounds{{25, 50}, {400, 500}};
    REQUIRE(level.setMinimap({image.data(), image.size()}, map_bounds).code() == ARX_OK);
    auto exported = level.exportGlb();
    REQUIRE(exported.code() == ARX_OK);
    glb::Asset asset;
    REQUIRE(glb::parse(*exported, asset) == ARX_OK);
    std::size_t void_groups = 0;
    std::size_t minimaps = 0;
    const cgltf_data& data = *asset.data();
    for (std::size_t index = 0; index < data.nodes_count; ++index) {
      const cgltf_node& node = data.nodes[index];
      const std::string_view name = node.name != nullptr ? node.name : "";
      if (name == glb_level::kExportVoidRoomRootName) ++void_groups;
      if (name != glb_level::kExportMinimapRootName) continue;
      ++minimaps;
      REQUIRE(node.mesh != nullptr);
      const cgltf_accessor* positions =
          cgltf_find_accessor(&node.mesh->primitives[0], cgltf_attribute_type_position, 0);
      REQUIRE(positions != nullptr);
      glb::AccessorCache cache(asset, 1);
      const glb::AccessorView* view = nullptr;
      REQUIRE(cache.get(positions, view) == ARX_OK);
      for (std::size_t corner = 0; corner < view->count; ++corner)
        CHECK(glb::readVec3(*view, corner).y == doctest::Approx(-10));
    }
    CHECK(void_groups == 1);
    CHECK(minimaps == 1);

    auto imported = Level::importGlb(*exported);
    REQUIRE(imported.code() == ARX_OK);
    CHECK(imported->roomCount() == 1);
    CHECK(imported->faceCount() == 2);
    std::size_t unassigned = 0;
    for (const auto face : imported->faces())
      if (face.room == kNoRoom) ++unassigned;
    CHECK(unassigned == 1);
    checkEffectiveRange(*imported, 100, 200);
    const auto minimap = imported->minimap();
    REQUIRE(minimap.encoded_image.size != 0);
    CHECK(minimap.world_xz_bounds.min.x == doctest::Approx(map_bounds.min.x));
    CHECK(minimap.world_xz_bounds.min.y == doctest::Approx(map_bounds.min.y));
    CHECK(minimap.world_xz_bounds.max.x == doctest::Approx(map_bounds.max.x));
    CHECK(minimap.world_xz_bounds.max.y == doctest::Approx(map_bounds.max.y));
  }

  TEST_CASE("Void-only GLB round trips through a neutral origin with no authored rooms") {
    Level level = mixedLevel();
    REQUIRE(level.validate().code() == ARX_OK);
    level.clearRooms();
    REQUIRE(level.validate().code() == ARX_OK);
    CHECK_FALSE(derived(level).effective_bounds.has_value());
    auto exported = level.exportGlb();
    REQUIRE(exported.code() == ARX_OK);
    auto imported = Level::importGlb(*exported);
    REQUIRE(imported.code() == ARX_OK);
    CHECK(imported->roomCount() == 0);
    CHECK(imported->roomDistanceCount() == 0);
    CHECK(imported->faceCount() == 2);
    for (const auto face : imported->faces()) CHECK(face.room == kNoRoom);
    CHECK_FALSE(derived(*imported).effective_bounds.has_value());
    REQUIRE(imported->referencedBounds().has_value());
    CHECK(imported->referencedBounds()->min.x == doctest::Approx(100));
    CHECK(imported->referencedBounds()->max.x == doctest::Approx(9100));
    CHECK(imported->weldVertices().code() == ARX_OK);
  }

  TEST_CASE("GLB placement translates a void-only level and its minimap by the same offset") {
    Level level = mixedLevel();
    level.clearRooms();
    const auto image = makeTestBmp();
    const ArxRect bounds{{25, 50}, {400, 500}};
    REQUIRE(level.setMinimap({image.data(), image.size()}, bounds).code() == ARX_OK);
    Level::GlbExportOptions export_options;
    export_options.arx_offset = {500, 0, 500};
    auto exported = level.exportGlb(export_options);
    REQUIRE(exported.code() == ARX_OK);
    ArxLevelGlbImportInfo info{};
    auto imported = Level::importGlb(*exported, Level::GlbImportOptions{}, &info);
    REQUIRE(imported.code() == ARX_OK);
    CHECK(info.applied_arx_offset.x == doctest::Approx(400));
    CHECK(info.applied_arx_offset.z == doctest::Approx(400));
    REQUIRE(imported->referencedBounds().has_value());
    CHECK(imported->referencedBounds()->min.x == doctest::Approx(0));
    CHECK(imported->referencedBounds()->min.z == doctest::Approx(0));
    const auto minimap = imported->minimap();
    REQUIRE(minimap.encoded_image.size != 0);
    CHECK(minimap.world_xz_bounds.min.x == doctest::Approx(bounds.min.x - 100));
    CHECK(minimap.world_xz_bounds.min.y == doctest::Approx(bounds.min.y - 100));
    CHECK(minimap.world_xz_bounds.max.x == doctest::Approx(bounds.max.x - 100));
    CHECK(minimap.world_xz_bounds.max.y == doctest::Approx(bounds.max.y - 100));
  }
}
