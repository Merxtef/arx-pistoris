// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#include "doctest/doctest.h"

#include "arx_pistoris/pistoris.hpp"

#include "support/corpus_files.h"
#include "support/native_equivalence.h"

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <ios>
#include <iterator>
#include <vector>

namespace fs = std::filesystem;

namespace {

std::vector<std::uint8_t> readBytes(const fs::path& path) {
  std::ifstream file(path, std::ios::binary);
  return {std::istreambuf_iterator<char>(file), {}};
}

template <class T, std::size_t N>
bool equivalentArray(const T (&lhs)[N], const T (&rhs)[N]) {
  for (std::size_t i = 0; i < N; ++i) {
    if (!(lhs[i] == rhs[i])) return false;
  }
  return true;
}

template <class T, std::size_t N, class Predicate>
bool equivalentArray(const T (&lhs)[N], const T (&rhs)[N], Predicate equivalent) {
  for (std::size_t i = 0; i < N; ++i) {
    if (!equivalent(lhs[i], rhs[i])) return false;
  }
  return true;
}

bool equivalent(const pistoris::fts::Header& lhs, const pistoris::fts::Header& rhs) {
  return equivalentArray(lhs.path, rhs.path) && lhs.count == rhs.count &&
         test_support::equivalent(lhs.version, rhs.version) && equivalentArray(lhs.pad, rhs.pad);
}

bool equivalent(const pistoris::fts::UniqueHeader3& lhs, const pistoris::fts::UniqueHeader3& rhs) {
  return equivalentArray(lhs.path, rhs.path) && equivalentArray(lhs.check, rhs.check);
}

bool equivalent(const pistoris::fts::SceneHeader& lhs, const pistoris::fts::SceneHeader& rhs) {
  return test_support::equivalent(lhs.version, rhs.version) && lhs.sizex == rhs.sizex && lhs.sizez == rhs.sizez &&
         lhs.num_textures == rhs.num_textures && lhs.num_polys == rhs.num_polys && lhs.num_anchors == rhs.num_anchors &&
         test_support::equivalent(lhs.playerpos, rhs.playerpos) &&
         test_support::equivalent(lhs.Mscenepos, rhs.Mscenepos) && lhs.num_portals == rhs.num_portals &&
         lhs.num_rooms == rhs.num_rooms;
}

bool equivalent(const pistoris::fts::Texture& lhs, const pistoris::fts::Texture& rhs) {
  return lhs.temp == rhs.temp && equivalentArray(lhs.fic, rhs.fic);
}

bool equivalent(const pistoris::fts::Vertex& lhs, const pistoris::fts::Vertex& rhs) {
  return test_support::equivalent(lhs.sy, rhs.sy) && test_support::equivalent(lhs.ssx, rhs.ssx) &&
         test_support::equivalent(lhs.ssz, rhs.ssz) && test_support::equivalent(lhs.stu, rhs.stu) &&
         test_support::equivalent(lhs.stv, rhs.stv);
}

bool equivalent(const pistoris::fts::Poly& lhs, const pistoris::fts::Poly& rhs) {
  const auto vector_equivalent = [](const auto& a, const auto& b) { return test_support::equivalent(a, b); };
  return equivalentArray(lhs.v, rhs.v, [](const auto& a, const auto& b) { return equivalent(a, b); }) &&
         lhs.tex == rhs.tex && test_support::equivalent(lhs.norm, rhs.norm) &&
         test_support::equivalent(lhs.norm2, rhs.norm2) && equivalentArray(lhs.nrml, rhs.nrml, vector_equivalent) &&
         test_support::equivalent(lhs.transval, rhs.transval) && test_support::equivalent(lhs.area, rhs.area) &&
         lhs.type == rhs.type && lhs.room == rhs.room && lhs.paddy == rhs.paddy;
}

bool equivalent(const pistoris::fts::AnchorData& lhs, const pistoris::fts::AnchorData& rhs) {
  return test_support::equivalent(lhs.pos, rhs.pos) && test_support::equivalent(lhs.radius, rhs.radius) &&
         test_support::equivalent(lhs.height, rhs.height) && lhs.num_linked == rhs.num_linked && lhs.flags == rhs.flags;
}

bool equivalent(const pistoris::fts::SavedTextureVertex& lhs, const pistoris::fts::SavedTextureVertex& rhs) {
  return test_support::equivalent(lhs.pos, rhs.pos) && test_support::equivalent(lhs.rhw, rhs.rhw) &&
         lhs.color == rhs.color && lhs.specular == rhs.specular && test_support::equivalent(lhs.tu, rhs.tu) &&
         test_support::equivalent(lhs.tv, rhs.tv);
}

bool equivalent(const pistoris::fts::SavePoly& lhs, const pistoris::fts::SavePoly& rhs) {
  const auto vertex_equivalent = [](const auto& a, const auto& b) { return equivalent(a, b); };
  const auto vector_equivalent = [](const auto& a, const auto& b) { return test_support::equivalent(a, b); };
  return lhs.type == rhs.type && test_support::equivalent(lhs.min, rhs.min) &&
         test_support::equivalent(lhs.max, rhs.max) && test_support::equivalent(lhs.norm, rhs.norm) &&
         test_support::equivalent(lhs.norm2, rhs.norm2) && equivalentArray(lhs.v, rhs.v, vertex_equivalent) &&
         equivalentArray(lhs.tv, rhs.tv, vertex_equivalent) && equivalentArray(lhs.nrml, rhs.nrml, vector_equivalent) &&
         lhs.tex == rhs.tex && test_support::equivalent(lhs.center, rhs.center) &&
         test_support::equivalent(lhs.transval, rhs.transval) && test_support::equivalent(lhs.area, rhs.area) &&
         lhs.room == rhs.room && lhs.misc == rhs.misc;
}

bool equivalent(const pistoris::fts::Portal& lhs, const pistoris::fts::Portal& rhs) {
  return equivalent(lhs.poly, rhs.poly) && lhs.room_1 == rhs.room_1 && lhs.room_2 == rhs.room_2 &&
         lhs.useportal == rhs.useportal && lhs.paddy == rhs.paddy;
}

bool equivalent(const pistoris::fts::RoomData& lhs, const pistoris::fts::RoomData& rhs) {
  return lhs.num_portals == rhs.num_portals && lhs.num_polys == rhs.num_polys && equivalentArray(lhs.padd, rhs.padd);
}

bool equivalent(const pistoris::fts::EpData& lhs, const pistoris::fts::EpData& rhs) {
  return lhs.px == rhs.px && lhs.py == rhs.py && lhs.idx == rhs.idx && lhs.padd == rhs.padd;
}

bool equivalent(const pistoris::fts::RoomDistData& lhs, const pistoris::fts::RoomDistData& rhs) {
  return test_support::equivalent(lhs.distance, rhs.distance) && test_support::equivalent(lhs.startpos, rhs.startpos) &&
         test_support::equivalent(lhs.endpos, rhs.endpos);
}

template <class T>
void checkEquivalentRange(const std::vector<T>& lhs, const std::vector<T>& rhs) {
  REQUIRE(lhs.size() == rhs.size());
  for (std::size_t element_index = 0; element_index < lhs.size(); ++element_index) {
    CAPTURE(element_index);
    CHECK(equivalent(lhs[element_index], rhs[element_index]));
  }
}

void checkEquivalent(const pistoris::Fts& lhs, const pistoris::Fts& rhs) {
  CHECK(equivalent(lhs.header, rhs.header));
  checkEquivalentRange(lhs.unique_headers, rhs.unique_headers);
  CHECK(equivalent(lhs.scene, rhs.scene));

  REQUIRE(lhs.textures.size() == rhs.textures.size());
  for (const auto& [id, texture] : lhs.textures) {
    CAPTURE(id);
    const auto found = rhs.textures.find(id);
    REQUIRE(found != rhs.textures.end());
    CHECK(equivalent(texture, found->second));
  }

  REQUIRE(lhs.cells.size() == rhs.cells.size());
  for (std::size_t cell_index = 0; cell_index < lhs.cells.size(); ++cell_index) {
    CAPTURE(cell_index);
    checkEquivalentRange(lhs.cells[cell_index].polygons, rhs.cells[cell_index].polygons);
    CHECK(lhs.cells[cell_index].anchor_ids == rhs.cells[cell_index].anchor_ids);
  }

  REQUIRE(lhs.anchors.size() == rhs.anchors.size());
  for (std::size_t anchor_index = 0; anchor_index < lhs.anchors.size(); ++anchor_index) {
    CAPTURE(anchor_index);
    CHECK(equivalent(lhs.anchors[anchor_index].data, rhs.anchors[anchor_index].data));
    CHECK(lhs.anchors[anchor_index].linked == rhs.anchors[anchor_index].linked);
  }

  checkEquivalentRange(lhs.portals, rhs.portals);

  REQUIRE(lhs.rooms.size() == rhs.rooms.size());
  for (std::size_t room_index = 0; room_index < lhs.rooms.size(); ++room_index) {
    CAPTURE(room_index);
    CHECK(equivalent(lhs.rooms[room_index].data, rhs.rooms[room_index].data));
    CHECK(lhs.rooms[room_index].portal_ids == rhs.rooms[room_index].portal_ids);
    checkEquivalentRange(lhs.rooms[room_index].polygons, rhs.rooms[room_index].polygons);
  }

  checkEquivalentRange(lhs.room_distances, rhs.room_distances);
}

}  // namespace

TEST_SUITE("fts_corpus") {
  TEST_CASE("ArxFtsParse") {
    for (const fs::path& path :
         test_support::discoverCorpusFiles({"data/fixtures/level/fts/native", "data/arx/fts"}, ".fts")) {
      CAPTURE(path.string());

      pistoris::Fts fts;
      REQUIRE(pistoris::readFts(readBytes(path), fts) == ARX_OK);
      CHECK(pistoris::validate(fts) == ARX_OK);
    }
  }

  TEST_CASE("ArxFtsWriteRoundtrip") {
    for (const fs::path& path :
         test_support::discoverCorpusFiles({"data/fixtures/level/fts/native", "data/arx/fts"}, ".fts")) {
      CAPTURE(path.string());

      pistoris::Fts source;
      REQUIRE(pistoris::readFts(readBytes(path), source) == ARX_OK);

      std::vector<std::uint8_t> written;
      REQUIRE(pistoris::writeFts(source, written) == ARX_OK);

      pistoris::Fts roundtrip;
      REQUIRE(pistoris::readFts(written, roundtrip) == ARX_OK);
      CHECK(pistoris::validate(roundtrip) == ARX_OK);
      checkEquivalent(source, roundtrip);
    }
  }
}
