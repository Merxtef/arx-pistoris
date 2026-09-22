// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#include "doctest/doctest.h"

#include "arx_pistoris/base/flags.h"
#include "arx_pistoris/base/status.h"
#include "arx_pistoris/native/fts.hpp"
#include "arx_pistoris/runtime/types.h"

#include "helpers.h"
#include "native/fts.h"
#include "utils/cursor.h"
#include "utils/log.h"

#include <cstdint>
#include <cstring>
#include <limits>
#include <string>
#include <vector>

namespace {

struct LogCapture {
  std::vector<std::string> messages;

  LogCapture() {
    pistoris::log_fn = [](ArxLogLevel, const char* message, void* userdata) {
      static_cast<LogCapture*>(userdata)->messages.emplace_back(message);
    };
    pistoris::log_ud = this;
  }

  ~LogCapture() {
    pistoris::log_fn = nullptr;
    pistoris::log_ud = nullptr;
  }
};

void setTexturePath(pistoris::fts::Data& data, std::int32_t id, const char* path) {
  pistoris::fts::Texture& texture = data.textures[id];
  std::memcpy(texture.fic, path, std::strlen(path) + 1U);
  data.scene.num_textures = static_cast<std::int32_t>(data.textures.size());
}

}  // namespace

TEST_SUITE("fts") {
  static ArxReturnCode load(const std::vector<uint8_t>& buf, pistoris::fts::Data& d) {
    pistoris::ReadCursor c(buf.data(), buf.size());
    return pistoris::loadFts(&d, c);
  }

  TEST_CASE("FtsReadMinimal") {
    pistoris::fts::Data d;
    CHECK(load(makeMinimalFts(), d) == ARX_OK);
    CHECK(d.scene.sizex == 1);
    CHECK(d.scene.sizez == 1);
    CHECK(d.cells.size() == 1);
    CHECK(d.rooms.size() == 1);
  }

  TEST_CASE("FtsWriteReadRoundtrip") {
    pistoris::fts::Data in = makeMinimalFtsData();
    auto& texture = in.textures[24275104];
    std::memcpy(texture.fic, "graph/levels/l1", sizeof("graph/levels/l1"));
    in.scene.num_textures = 1;
    in.cells[0].polygons.resize(1);
    in.cells[0].polygons[0].tex = 0;
    in.cells[0].polygons[0].type = pistoris::kFaceBitDoublesided;
    in.scene.num_polys = 1;

    pistoris::WriteCursor wc;
    REQUIRE(pistoris::saveFts(&in, wc) == ARX_OK);
    std::vector<uint8_t> bytes = wc.take();

    pistoris::fts::Data out;
    REQUIRE(load(bytes, out) == ARX_OK);
    CHECK(out.textures.size() == 1);
    REQUIRE(out.textures.contains(24275104));
    CHECK(std::memcmp(out.textures.at(24275104).fic, texture.fic, sizeof(texture.fic)) == 0);
    REQUIRE(out.cells.size() == 1);
    REQUIRE(out.cells[0].polygons.size() == 1);
    CHECK(out.cells[0].polygons[0].tex == 0);
    CHECK(out.cells[0].polygons[0].type == pistoris::kFaceBitDoublesided);
  }

  TEST_CASE("FtsRejectsNegativeTextureIds") {
    pistoris::fts::Data in = makeMinimalFtsData();
    in.cells[0].polygons.resize(1);
    in.cells[0].polygons[0].tex = -1;
    in.scene.num_polys = 1;

    CHECK(pistoris::validateFts(&in) == ARX_FTS_BAD_POLYGON_TEXTURE_ID);
  }

  TEST_CASE("FtsAllowsTextureIdsThatAreNotTextureIndices") {
    pistoris::fts::Data in = makeMinimalFtsData();
    auto& texture = in.textures[24275104];
    std::memcpy(texture.fic, "graph/levels/l1", sizeof("graph/levels/l1"));
    in.scene.num_textures = 1;
    in.cells[0].polygons.resize(1);
    in.cells[0].polygons[0].tex = 24275104;
    in.scene.num_polys = 1;

    CHECK(pistoris::validateFts(&in) == ARX_OK);
  }

  TEST_CASE("FtsRejectsUnterminatedTexturePaths") {
    pistoris::fts::Data in = makeMinimalFtsData();
    in.textures[1] = {};
    in.scene.num_textures = 1;
    std::memset(in.textures.at(1).fic, 'x', sizeof(in.textures.at(1).fic));

    CHECK(pistoris::validateFts(&in) == ARX_FTS_BAD_TEXTURE_PATH);
  }

  TEST_CASE("FtsAllowsEmptyTexturePaths") {
    pistoris::fts::Data in = makeMinimalFtsData();
    in.textures[1] = {};
    in.scene.num_textures = 1;

    pistoris::WriteCursor wc;
    REQUIRE(pistoris::saveFts(&in, wc) == ARX_OK);

    pistoris::fts::Data out;
    REQUIRE(load(wc.take(), out) == ARX_OK);
    REQUIRE(out.textures.contains(1));
    CHECK(out.textures.at(1).fic[0] == '\0');
  }

  TEST_CASE("FtsAllowsAtMost254RealRooms") {
    pistoris::fts::Data in = makeMinimalFtsData();
    in.scene.num_rooms = static_cast<std::int32_t>(pistoris::kFtsMaxRooms);
    in.rooms.resize(pistoris::kFtsMaxRooms + 1U);
    in.room_distances.resize((pistoris::kFtsMaxRooms + 1U) * (pistoris::kFtsMaxRooms + 1U));

    CHECK(pistoris::validateFts(&in) == ARX_OK);

    ++in.scene.num_rooms;
    CHECK(pistoris::validateFts(&in) == ARX_FTS_BAD_ROOM_COUNT);
  }

  TEST_CASE("FtsAllowsAtMost32768PolygonsPerCell") {
    pistoris::fts::Data in = makeMinimalFtsData();
    in.cells[0].polygons.resize(pistoris::kFtsMaxCellPolygons);
    in.scene.num_polys = static_cast<std::int32_t>(pistoris::kFtsMaxCellPolygons);

    CHECK(pistoris::validateFts(&in) == ARX_OK);

    in.cells[0].polygons.emplace_back();
    ++in.scene.num_polys;
    CHECK(pistoris::validateFts(&in) == ARX_FTS_BAD_CELL_POLYGON_COUNT);
  }

  TEST_CASE("FtsLimitsRoomVerticesPerRuntimeTexture") {
    pistoris::fts::Data in = makeMinimalFtsData();
    setTexturePath(in, 1, "graph/obj3d/textures/wall.png");
    in.cells[0].polygons.resize(1);
    in.cells[0].polygons[0].tex = 1;
    in.scene.num_polys = 1;

    in.rooms[0].polygons.assign(21845, {0, 0, 0, 0});
    CHECK(pistoris::validateFts(&in) == ARX_OK);

    in.rooms[0].polygons.emplace_back();
    CHECK(pistoris::validateFts(&in) == ARX_FTS_BAD_ROOM_TEXTURE_VERTEX_COUNT);

    in.cells[0].polygons[0].type = pistoris::kFaceBitIgnore;
    CHECK(pistoris::validateFts(&in) == ARX_OK);

    in.cells[0].polygons[0].type = pistoris::kFaceBitHide;
    CHECK(pistoris::validateFts(&in) == ARX_OK);

    in.cells[0].polygons[0].type = pistoris::kFaceBitNodraw;
    in.cells[0].polygons[0].tex = 0;
    CHECK(pistoris::validateFts(&in) == ARX_OK);

    in.cells[0].polygons[0].tex = 1;
    CHECK(pistoris::validateFts(&in) == ARX_FTS_BAD_ROOM_TEXTURE_VERTEX_COUNT);

    in.textures.at(1).fic[0] = '\0';
    CHECK(pistoris::validateFts(&in) == ARX_OK);

    setTexturePath(in, 1, "graph/obj3d/textures/wall.png");
    CHECK(pistoris::validateFts(&in) == ARX_FTS_BAD_ROOM_TEXTURE_VERTEX_COUNT);

    in.cells[0].polygons[0].type = pistoris::kFaceBitQuad;
    in.rooms[0].polygons.resize(16383);
    CHECK(pistoris::validateFts(&in) == ARX_OK);

    in.rooms[0].polygons.emplace_back();
    CHECK(pistoris::validateFts(&in) == ARX_FTS_BAD_ROOM_TEXTURE_VERTEX_COUNT);
  }

  TEST_CASE("FtsGroupsRoomVerticesByCanonicalTextureResource") {
    pistoris::fts::Data in = makeMinimalFtsData();
    setTexturePath(in, 1, "graph/obj3d/textures/wall");
    setTexturePath(in, 2, "graph/obj3d/textures/wall_other");
    in.cells[0].polygons.resize(2);
    in.cells[0].polygons[0].tex = 1;
    in.cells[0].polygons[1].tex = 2;
    in.scene.num_polys = 2;

    in.rooms[0].polygons.assign(10923, {0, 0, 0, 0});
    in.rooms[0].polygons.insert(in.rooms[0].polygons.end(), 10923, {0, 0, 1, 0});
    CHECK(pistoris::validateFts(&in) == ARX_OK);

    setTexturePath(in, 2, "graph/obj3d/textures/wall");
    CHECK(pistoris::validateFts(&in) == ARX_FTS_BAD_ROOM_TEXTURE_VERTEX_COUNT);
  }

  TEST_CASE("FtsRoomDistancesUseNbRoomsPlusOne") {
    pistoris::fts::Data in = makeMinimalFtsData();
    in.scene.num_rooms = 1;
    in.rooms.resize(2);
    in.room_distances.resize(4);

    CHECK(pistoris::validateFts(&in) == ARX_OK);

    in.room_distances.resize(1);
    CHECK(pistoris::validateFts(&in) == ARX_FTS_BAD_ROOM_DISTANCE_COUNT);

    in.room_distances.resize(4);
    in.room_distances[0].distance = std::numeric_limits<float>::infinity();
    CHECK(pistoris::validateFts(&in) == ARX_FTS_BAD_ROOM_DISTANCE);

    in.room_distances[0].distance = -1.0f;
    in.room_distances[0].startpos.x = std::numeric_limits<float>::quiet_NaN();
    CHECK(pistoris::validateFts(&in) == ARX_OK);
  }

  TEST_CASE("FtsAnchorValidationRequiresFinitePositionsWithoutGeometryContainment") {
    pistoris::fts::Data in = makeMinimalFtsData();
    pistoris::fts::Poly poly{};
    poly.v[0].ssx = 0.0f;
    poly.v[0].ssz = 0.0f;
    poly.v[1].ssx = 1.0f;
    poly.v[1].ssz = 0.0f;
    poly.v[2].ssx = 0.0f;
    poly.v[2].ssz = 1.0f;
    poly.nrml[0] = poly.nrml[1] = poly.nrml[2] = {0.0f, -1.0f, 0.0f};
    in.cells[0].polygons.push_back(poly);
    in.scene.num_polys = 1;
    in.anchors.resize(1);
    in.scene.num_anchors = 1;

    in.anchors[0].data.pos.x = std::numeric_limits<float>::infinity();
    CHECK(pistoris::validateFts(&in) == ARX_FTS_BAD_ANCHOR_POSITION);

    in.anchors[0].data.pos.x = 0.0f;
    in.anchors[0].data.radius = -1.0f;
    CHECK(pistoris::validateFts(&in) == ARX_OK);

    in.anchors[0].data.radius = 1.0f;
    in.anchors[0].data.height = 1.0f;
    CHECK(pistoris::validateFts(&in) == ARX_OK);

    in.anchors[0].data.height = -1.0f;
    CHECK(pistoris::validateFts(&in) == ARX_OK);

    in.anchors[0].data.pos.x = 2.0f;
    CHECK(pistoris::validateFts(&in) == ARX_OK);
  }

  TEST_CASE("FtsValidationRejectsNullData") { CHECK(pistoris::validateFts(nullptr) == ARX_INVALID_DATA_POINTER); }

  TEST_CASE("FtsValidationReportsSpecificContainerErrors") {
    {
      pistoris::fts::Data in = makeMinimalFtsData();
      in.header.version = 0.0f;
      CHECK(pistoris::validateFts(&in) == ARX_FTS_BAD_VERSION);
    }
    {
      pistoris::fts::Data in = makeMinimalFtsData();
      in.scene.Mscenepos.x = std::numeric_limits<float>::infinity();
      CHECK(pistoris::validateFts(&in) == ARX_FTS_BAD_SCENE_OFFSET);
    }
    {
      pistoris::fts::Header header;
      header.count = static_cast<std::int32_t>(pistoris::kFtsMaxHeaderBlocks + 1U);
      std::vector<std::uint8_t> bytes;
      appendBytes(bytes, header);
      pistoris::fts::Data out;
      CHECK(load(bytes, out) == ARX_FTS_BAD_METADATA_COUNT);
    }
    {
      pistoris::fts::Data in = makeMinimalFtsData();
      in.scene.num_textures = 1;
      CHECK(pistoris::validateFts(&in) == ARX_FTS_BAD_TEXTURE_COUNT);
    }
    {
      pistoris::fts::Data in = makeMinimalFtsData();
      in.textures[0] = {};
      in.scene.num_textures = 1;
      CHECK(pistoris::validateFts(&in) == ARX_FTS_BAD_TEXTURE_ID);
    }
    {
      pistoris::fts::Data in = makeMinimalFtsData();
      in.scene.num_polys = 1;
      CHECK(pistoris::validateFts(&in) == ARX_FTS_BAD_POLYGON_COUNT);
    }
    {
      std::vector<std::uint8_t> bytes = makeMinimalFts();
      const std::size_t info_offset = sizeof(pistoris::fts::Header) + sizeof(pistoris::fts::SceneHeader);
      pistoris::fts::SceneInfo info;
      std::memcpy(&info, bytes.data() + info_offset, sizeof(info));
      info.nbianchors = -1;
      std::memcpy(bytes.data() + info_offset, &info, sizeof(info));
      pistoris::fts::Data out;
      CHECK(load(bytes, out) == ARX_FTS_BAD_CELL_ANCHOR_COUNT);
    }
    {
      pistoris::fts::Data in = makeMinimalFtsData();
      in.cells[0].polygons.resize(1);
      in.cells[0].polygons[0].type = pistoris::kFaceBitsAll | 0x80000000U;
      in.scene.num_polys = 1;
      CHECK(pistoris::validateFts(&in) == ARX_FTS_BAD_POLYGON_TYPE);
    }
    {
      pistoris::fts::Data in = makeMinimalFtsData();
      in.cells[0].polygons.resize(1);
      in.cells[0].polygons[0].v[0].ssx = -1.0f;
      in.scene.num_polys = 1;
      CHECK(pistoris::validateFts(&in) == ARX_FTS_BAD_POLYGON_POSITION);
    }
    {
      pistoris::fts::Data in = makeMinimalFtsData();
      in.cells[0].polygons.resize(1);
      in.cells[0].polygons[0].v[0].stu = std::numeric_limits<float>::quiet_NaN();
      in.scene.num_polys = 1;
      CHECK(pistoris::validateFts(&in) == ARX_FTS_BAD_POLYGON_UV);
    }
    {
      pistoris::fts::Data in = makeMinimalFtsData();
      in.cells[0].polygons.resize(1);
      in.cells[0].polygons[0].transval = std::numeric_limits<float>::infinity();
      in.scene.num_polys = 1;
      CHECK(pistoris::validateFts(&in) == ARX_FTS_BAD_POLYGON_TRANSVAL);
    }
    {
      pistoris::fts::Data in = makeMinimalFtsData();
      in.scene.num_anchors = 1;
      CHECK(pistoris::validateFts(&in) == ARX_FTS_BAD_ANCHOR_COUNT);
    }
    {
      pistoris::fts::Data in = makeMinimalFtsData();
      in.anchors.resize(1);
      in.anchors[0].linked.resize(static_cast<std::size_t>(std::numeric_limits<std::int16_t>::max()) + 1U);
      in.scene.num_anchors = 1;
      CHECK(pistoris::validateFts(&in) == ARX_FTS_BAD_ANCHOR_LINK_COUNT);
    }
    {
      pistoris::fts::Data in = makeMinimalFtsData();
      in.anchors.resize(1);
      in.anchors[0].linked.push_back(1);
      in.scene.num_anchors = 1;
      CHECK(pistoris::validateFts(&in) == ARX_FTS_BAD_ANCHOR_INDEX);
    }
    {
      pistoris::fts::Data in = makeMinimalFtsData();
      in.scene.num_portals = 1;
      CHECK(pistoris::validateFts(&in) == ARX_FTS_BAD_PORTAL_COUNT);
    }
    {
      pistoris::fts::Data in = makeMinimalFtsData();
      in.rooms[0].portal_ids.push_back(0);
      CHECK(pistoris::validateFts(&in) == ARX_FTS_BAD_ROOM_PORTAL_INDEX);
    }
    {
      pistoris::fts::Data in = makeMinimalFtsData();
      in.portals.resize(1);
      in.portals[0].room_1 = 1;
      in.portals[0].poly.v[0].pos = {0.0f, 0.0f, 0.0f};
      in.portals[0].poly.v[1].pos = {1.0f, 0.0f, 0.0f};
      in.portals[0].poly.v[2].pos = {0.0f, 1.0f, 0.0f};
      in.scene.num_portals = 1;
      CHECK(pistoris::validateFts(&in) == ARX_FTS_BAD_PORTAL_ROOM_INDEX);
    }
    {
      std::vector<std::uint8_t> bytes = makeMinimalFts();
      const std::size_t room_offset =
          sizeof(pistoris::fts::Header) + sizeof(pistoris::fts::SceneHeader) + sizeof(pistoris::fts::SceneInfo);
      pistoris::fts::RoomData room;
      std::memcpy(&room, bytes.data() + room_offset, sizeof(room));
      room.num_portals = -1;
      std::memcpy(bytes.data() + room_offset, &room, sizeof(room));
      pistoris::fts::Data out;
      CHECK(load(bytes, out) == ARX_FTS_BAD_ROOM_PORTAL_COUNT);
    }
    {
      std::vector<std::uint8_t> bytes = makeMinimalFts();
      const std::size_t room_offset =
          sizeof(pistoris::fts::Header) + sizeof(pistoris::fts::SceneHeader) + sizeof(pistoris::fts::SceneInfo);
      pistoris::fts::RoomData room;
      std::memcpy(&room, bytes.data() + room_offset, sizeof(room));
      room.num_polys = -1;
      std::memcpy(bytes.data() + room_offset, &room, sizeof(room));
      pistoris::fts::Data out;
      CHECK(load(bytes, out) == ARX_FTS_BAD_ROOM_POLYGON_COUNT);
    }
    {
      pistoris::fts::Data in = makeMinimalFtsData();
      in.rooms[0].polygons.push_back({});
      CHECK(pistoris::validateFts(&in) == ARX_FTS_BAD_ROOM_POLYGON_INDEX);
    }
  }

  TEST_CASE("FtsReadRejectsDuplicateTextureIds") {
    struct TextureRecord {
      std::int32_t id = 0;
      std::int32_t scratch = 0;
      char path[256] = {};
    };
    static_assert(sizeof(TextureRecord) == 264);

    pistoris::fts::Data in = makeMinimalFtsData();
    setTexturePath(in, 1, "graph/obj3d/textures/one.bmp");
    setTexturePath(in, 2, "graph/obj3d/textures/two.bmp");
    std::vector<std::uint8_t> bytes = makeFtsBytes(in);

    const std::size_t first_texture = sizeof(pistoris::fts::Header) + sizeof(pistoris::fts::SceneHeader);
    std::int32_t first_id = 0;
    std::memcpy(&first_id, bytes.data() + first_texture, sizeof(first_id));
    std::memcpy(bytes.data() + first_texture + sizeof(TextureRecord), &first_id, sizeof(first_id));

    pistoris::fts::Data out;
    CHECK(load(bytes, out) == ARX_FTS_DUPLICATE_TEXTURE_ID);
  }

  TEST_CASE("FtsRejectsInvalidPortalData") {
    pistoris::fts::Data in = makeMinimalFtsData();
    in.portals.resize(1);
    in.scene.num_portals = 1;
    in.portals[0].room_1 = 0;
    in.portals[0].room_2 = 0;
    in.portals[0].poly.v[0].pos = {0.0f, 0.0f, 0.0f};
    in.portals[0].poly.v[1].pos = {1.0f, 0.0f, 0.0f};
    in.portals[0].poly.v[2].pos = {0.0f, 1.0f, 0.0f};

    CHECK(pistoris::validateFts(&in) == ARX_OK);

    in.portals[0].poly.type = pistoris::kFaceBitStone;
    CHECK(pistoris::validateFts(&in) == ARX_FTS_BAD_PORTAL_TYPE);

    in.portals[0].poly.type = 0;
    in.portals[0].poly.v[1].pos = in.portals[0].poly.v[0].pos;
    CHECK(pistoris::validateFts(&in) == ARX_FTS_BAD_PORTAL_GEOMETRY);

    in.portals[0].poly.v[1].pos.x = std::numeric_limits<float>::infinity();
    CHECK(pistoris::validateFts(&in) == ARX_FTS_BAD_PORTAL_POSITION);

    in.portals[0].poly.type = pistoris::kFaceBitQuad;
    in.portals[0].poly.v[0].pos = {0.0f, 0.0f, 0.0f};
    in.portals[0].poly.v[1].pos = {1.0f, 0.0f, 0.0f};
    in.portals[0].poly.v[2].pos = {0.0f, 1.0f, 0.0f};
    in.portals[0].poly.v[3].pos = {1.0f, 1.0f, 0.0f};
    CHECK(pistoris::validateFts(&in) == ARX_OK);

    in.portals[0].poly.v[3].pos = in.portals[0].poly.v[1].pos;
    CHECK(pistoris::validateFts(&in) == ARX_FTS_BAD_PORTAL_GEOMETRY);

    in.portals[0].poly.v[3].pos = {0.0f, 1.0f, 0.0f};
    in.portals[0].poly.v[2].pos = {1.0f, 1.0f, 0.0f};
    CHECK(pistoris::validateFts(&in) == ARX_FTS_BAD_PORTAL_GEOMETRY);

    in.portals[0].poly.v[0].pos = {0.0f, 0.0f, 0.0f};
    in.portals[0].poly.v[1].pos = {200.0f, 0.0f, 0.0f};
    in.portals[0].poly.v[2].pos = {0.0f, 300.0f, 0.0f};
    in.portals[0].poly.v[3].pos = {200.0f, 300.0f, 3.0f};
    CHECK(pistoris::validateFts(&in) == ARX_OK);

    in.portals[0].poly.v[3].pos.z = 3.5f;
    CHECK(pistoris::validateFts(&in) == ARX_FTS_BAD_PORTAL_GEOMETRY);
  }

  TEST_CASE("FtsStaleUncompressedSizeStillParsesPlainPayload") {
    pistoris::fts::Data in = makeMinimalFtsData();
    in.header.uncompressedsize = 1234;
    std::vector<uint8_t> bytes = makeFtsBytes(in);

    pistoris::fts::Data out;
    CHECK(load(bytes, out) == ARX_OK);
  }

  TEST_CASE("FtsStaleUncompressedSizeDoesNotMaskPlainParseFailure") {
    pistoris::fts::Header header;
    header.uncompressedsize = 128;
    std::vector<uint8_t> bytes;
    appendBytes(bytes, header);
    bytes.insert(bytes.end(), 16, 0xFF);

    pistoris::fts::Data out;
    CHECK(load(bytes, out) == ARX_UNEXPECTED_EOF);
  }

  TEST_CASE("FtsStaleUncompressedSizeDoesNotMislabelValidationFailure") {
    pistoris::fts::Data in = makeMinimalFtsData();
    in.header.uncompressedsize = 128;
    in.portals.resize(1);
    in.scene.num_portals = 1;
    std::vector<uint8_t> bytes = makeFtsBytes(in);

    LogCapture logs;
    pistoris::fts::Data out;
    CHECK(load(bytes, out) == ARX_FTS_BAD_PORTAL_GEOMETRY);
    for (const std::string& message : logs.messages) CHECK(message.find("payload parse failed") == std::string::npos);
  }

  TEST_CASE("FtsBadCountsRejected") {
    pistoris::fts::Data in = makeMinimalFtsData();
    std::vector<uint8_t> bytes = makeFtsBytes(in);

    auto* scene = reinterpret_cast<pistoris::fts::SceneHeader*>(bytes.data() + sizeof(pistoris::fts::Header));
    scene->sizex = -1;

    pistoris::fts::Data out;
    CHECK(load(bytes, out) == ARX_FTS_BAD_GRID_SIZE);
  }

  TEST_CASE("FtsTruncatedReturnsEof") {
    std::vector<uint8_t> bytes = makeMinimalFts();
    bytes.pop_back();

    pistoris::fts::Data out;
    CHECK(load(bytes, out) == ARX_UNEXPECTED_EOF);
  }
}
