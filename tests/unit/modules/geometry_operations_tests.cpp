// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#include "doctest/doctest.h"

#include "arx_pistoris/base/indices.h"
#include "arx_pistoris/base/math.h"

#include "modules/geometry.h"
#include "modules/textures.h"

#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <span>
#include <utility>
#include <vector>

using namespace pistoris;

namespace {

Face makeFace(VertexIndex a, VertexIndex b, VertexIndex c) {
  Face face;
  face.corners[0].vertex = a;
  face.corners[1].vertex = b;
  face.corners[2].vertex = c;
  return face;
}

}  // namespace

TEST_SUITE("geometry::operations") {
  TEST_CASE("Adds vertices, faces, and textures") {
    GeometryData geometry;
    REQUIRE(geometry::validateVertexAppend(geometry, 3) == geometry::Error::kNone);
    VertexIndex vertex = geometry::addVertex(geometry, {{0.0f, 0.0f, 0.0f}});
    CHECK(vertex == 0);
    vertex = geometry::addVertex(geometry, {{1.0f, 0.0f, 0.0f}});
    CHECK(vertex == 1);
    vertex = geometry::addVertex(geometry, {{0.0f, 0.0f, 1.0f}});
    CHECK(vertex == 2);
    CHECK(geometry::vertexCapacityForAppend(geometry, 2, 100) == 5);

    Face face = makeFace(0, 1, 2);
    for (Corner& corner : face.corners) corner.normal = {0.0f, 1.0f, 0.0f};
    REQUIRE(geometry::validateFaces(std::span<const Face>(&face, 1), geometry.vertices, 0) == geometry::Error::kNone);
    FaceIndex face_index = geometry::addFace(geometry, face);
    CHECK(face_index == 0);
    CHECK(geometry.faces.size() == 1);

    TexturesData texture_data;
    Texture candidate{"graph/obj3d/textures/stone.bmp"};
    REQUIRE(textures::repairPath(texture_data, candidate, kNoTexture) == textures::Error::kNone);
    REQUIRE(textures::validateTexture(candidate) == textures::Error::kNone);
    TextureIndex texture = textures::addTexture(texture_data, std::move(candidate));
    CHECK(texture == 0);
    REQUIRE(texture_data.textures.size() == 1);
    CHECK(texture_data.textures[0].path == "graph/obj3d/textures/stone.bmp");
  }

  TEST_CASE("Repairs duplicate texture identity before mutation") {
    TexturesData textures_data;
    Texture first{"graph/obj3d/textures/stone.bmp"};
    REQUIRE(textures::repairPath(textures_data, first, kNoTexture) == textures::Error::kNone);
    REQUIRE(textures::validateTexture(first) == textures::Error::kNone);
    CHECK(textures::addTexture(textures_data, std::move(first)) == 0);

    Texture second{"GRAPH/OBJ3D/TEXTURES/STONE.BMP"};
    REQUIRE(textures::repairPath(textures_data, second, kNoTexture) == textures::Error::kNone);
    REQUIRE(textures::validateTexture(second) == textures::Error::kNone);
    CHECK(textures::addTexture(textures_data, std::move(second)) == 1);
    REQUIRE(textures_data.textures.size() == 2);
    CHECK(textures_data.textures[0].path == "graph/obj3d/textures/stone.bmp");
    CHECK(textures_data.textures[1].path == "graph/obj3d/textures/stone.bmp_1");
  }

  TEST_CASE("Candidate validation does not mutate storage") {
    GeometryData geometry;
    geometry.vertices = {{{0.0f, 0.0f, 0.0f}}};
    TexturesData texture_data{{Texture{"graph/obj3d/textures/stone.bmp"}}};

    const Vertex invalid_vertex{{std::numeric_limits<float>::infinity(), 0.0f, 0.0f}};
    CHECK(geometry::validateVertex(invalid_vertex) == geometry::Error::kBadVertex);
    const Texture invalid_texture{""};
    CHECK(textures::validateTexture(invalid_texture) == textures::Error::kBadTexture);
    CHECK(geometry.vertices[0].position.x == doctest::Approx(0.0f));
    CHECK(texture_data.textures[0].path == "graph/obj3d/textures/stone.bmp");
  }

  TEST_CASE("Compacts vertices in table order and remaps faces") {
    GeometryData geometry;
    geometry.vertices = {
        {{0.0f, 0.0f, 0.0f}},
        {{1.0f, 0.0f, 0.0f}},
        {{2.0f, 0.0f, 0.0f}},
        {{3.0f, 0.0f, 0.0f}},
    };
    geometry.faces.push_back(makeFace(2, 0, 3));

    geometry::VertexIndexRemap remap;
    CHECK(geometry::compactVertices(geometry, &remap) == 1);
    REQUIRE(geometry.vertices.size() == 3);
    CHECK(remap == geometry::VertexIndexRemap{0, kInvalidVertexIndex, 1, 2});
    CHECK(geometry.vertices[0].position.x == doctest::Approx(0.0f));
    CHECK(geometry.vertices[1].position.x == doctest::Approx(2.0f));
    CHECK(geometry.vertices[2].position.x == doctest::Approx(3.0f));
    CHECK(geometry.faces[0].corners[0].vertex == 1);
    CHECK(geometry.faces[0].corners[1].vertex == 0);
    CHECK(geometry.faces[0].corners[2].vertex == 2);
  }

  TEST_CASE("Compact omits identity remap when all vertices are retained") {
    GeometryData geometry;
    geometry.vertices = {
        {{0.0f, 0.0f, 0.0f}},
        {{1.0f, 0.0f, 0.0f}},
        {{2.0f, 0.0f, 0.0f}},
    };
    geometry.faces.push_back(makeFace(2, 0, 1));

    geometry::VertexIndexRemap remap = {7};
    CHECK(geometry::compactVertices(geometry, &remap) == 0);
    CHECK(remap.empty());
    CHECK(geometry.faces[0].corners[0].vertex == 2);
    CHECK(geometry.faces[0].corners[1].vertex == 0);
    CHECK(geometry.faces[0].corners[2].vertex == 1);
  }

  TEST_CASE("Compacts unused textures in table order and remaps faces") {
    GeometryData geometry;
    TexturesData texture_data;
    texture_data.textures = {"graph/obj3d/textures/unused.bmp",
                             "graph/obj3d/textures/first.bmp",
                             "graph/obj3d/textures/unused2.bmp",
                             "graph/obj3d/textures/second.bmp"};
    texture_data.textures[0].encoded_image = {1};
    texture_data.textures[1].encoded_image = {2};
    texture_data.textures[2].encoded_image = {3};
    texture_data.textures[3].encoded_image = {4};
    Face first = makeFace(0, 1, 2);
    Face second = makeFace(0, 1, 2);
    Face no_tex = makeFace(0, 1, 2);
    first.texture = 3;
    second.texture = 1;
    no_tex.texture = kNoTexture;
    geometry.faces = {first, second, no_tex};

    std::vector<std::uint8_t> used;
    REQUIRE(geometry::collectTextureUsage(geometry, texture_data.textures.size(), used) == geometry::Error::kNone);
    std::vector<TextureIndex> remap;
    std::size_t removed = 0;
    REQUIRE(textures::compact(texture_data, used, remap, removed) == textures::Error::kNone);
    geometry::remapTextureReferences(geometry, remap);
    CHECK(removed == 2);
    REQUIRE(texture_data.textures.size() == 2);
    CHECK(texture_data.textures[0].path == "graph/obj3d/textures/first.bmp");
    CHECK(texture_data.textures[0].encoded_image == std::vector<std::uint8_t>{2});
    CHECK(texture_data.textures[1].path == "graph/obj3d/textures/second.bmp");
    CHECK(texture_data.textures[1].encoded_image == std::vector<std::uint8_t>{4});
    CHECK(geometry.faces[0].texture == 1);
    CHECK(geometry.faces[1].texture == 0);
    CHECK(geometry.faces[2].texture == kNoTexture);
  }

  TEST_CASE("Finds nearby vertices through position index") {
    GeometryData geometry;
    geometry::PositionIndex index(0.1f);

    CHECK(geometry::addOrFindVertex(geometry, index, {1.0f, 2.0f, 3.0f}) == 0);
    CHECK(geometry::addOrFindVertex(geometry, index, {1.05f, 2.05f, 3.05f}) == 0);
    CHECK(geometry.vertices.size() == 1);

    CHECK(geometry::addOrFindVertex(geometry, index, {1.2f, 2.0f, 3.0f}) == 1);
    CHECK(geometry.vertices.size() == 2);
  }

  TEST_CASE("Position index safely saturates extreme grid coordinates") {
    constexpr float kExtreme = 1.0e15f;
    geometry::PositionIndex index(1.0e-4f);
    REQUIRE(index.tryAdd(7, {kExtreme, kExtreme, kExtreme}));
    REQUIRE(index.tryAdd(9, {-kExtreme, -kExtreme, -kExtreme}));

    CHECK(index.find({kExtreme, kExtreme, kExtreme}) == 7);
    CHECK(index.find({-kExtreme, -kExtreme, -kExtreme}) == 9);
    CHECK_FALSE(index.find({kExtreme * 2.0f, kExtreme, kExtreme}).has_value());
  }

  TEST_CASE("Position index ignores invalid radius") {
    GeometryData geometry;
    geometry::PositionIndex index(0.0f);

    CHECK(geometry::addOrFindVertex(geometry, index, {1.0f, 2.0f, 3.0f}) == 0);
    CHECK(geometry::addOrFindVertex(geometry, index, {1.0f, 2.0f, 3.0f}) == 1);
    CHECK(geometry.vertices.size() == 2);
  }

  TEST_CASE("Position index supports Euclidean radius") {
    geometry::PositionIndex index(0.1f, geometry::PositionWeldMetric::kEuclidean);
    REQUIRE(index.tryAdd(7, {0.0f, 0.0f, 0.0f}));

    CHECK(index.find({0.05f, 0.05f, 0.05f}) == 7);
    CHECK(!index.find({0.09f, 0.09f, 0.09f}).has_value());
  }

  TEST_CASE("Welds vertices using axis-aligned metric") {
    GeometryData geometry;
    geometry.vertices = {
        {{0.0f, 0.0f, 0.0f}},
        {{0.09f, 0.09f, 0.09f}},
    };

    geometry::GeometryRemap remap;
    CHECK(geometry::weldVertices(geometry,
                                 {.radius = 0.1f, .metric = geometry::PositionWeldMetric::kAxisAligned},
                                 &remap) == geometry::Error::kNone);

    REQUIRE(geometry.vertices.size() == 1);
    CHECK(remap.vertices == geometry::VertexIndexRemap{0, 0});
    CHECK(geometry.vertices[0].position.x == doctest::Approx(0.0f));
    CHECK(geometry.vertices[0].position.y == doctest::Approx(0.0f));
    CHECK(geometry.vertices[0].position.z == doctest::Approx(0.0f));
  }

  TEST_CASE("Welds through best representative without chain collapse") {
    GeometryData geometry;
    geometry.vertices = {
        {{0.0f, 0.0f, 0.0f}},
        {{0.09f, 0.0f, 0.0f}},
        {{0.18f, 0.0f, 0.0f}},
        {{0.27f, 0.0f, 0.0f}},
    };

    CHECK(geometry::weldVertices(geometry, {.radius = 0.1f}) == geometry::Error::kNone);

    REQUIRE(geometry.vertices.size() == 2);
    CHECK(geometry.vertices[0].position.x == doctest::Approx(0.09f));
    CHECK(geometry.vertices[1].position.x == doctest::Approx(0.27f));
  }

  TEST_CASE("Weld remaps faces without changing face order") {
    GeometryData geometry;
    geometry.vertices = {
        {{0.0f, 0.0f, 0.0f}},
        {{1.0f, 0.0f, 0.0f}},
        {{0.0f, 1.0f, 0.0f}},
        {{0.01f, 0.0f, 0.0f}},
    };
    geometry.faces.push_back(makeFace(0, 1, 2));
    geometry.faces.push_back(makeFace(3, 1, 2));
    geometry.faces[0].normal = {1.0f, 0.0f, 0.0f};
    geometry.faces[1].normal = {1.0f, 0.0f, 0.0f};

    CHECK(geometry::weldVertices(geometry, {.radius = 0.1f}) == geometry::Error::kNone);

    REQUIRE(geometry.vertices.size() == 3);
    REQUIRE(geometry.faces.size() == 2);
    CHECK(geometry.faces[0].corners[0].vertex == 0);
    CHECK(geometry.faces[1].corners[0].vertex == 0);
    CHECK(geometry.faces[1].corners[1].vertex == 1);
    CHECK(geometry.faces[1].corners[2].vertex == 2);
    CHECK(geometry.faces[0].normal == geometry::faceNormalOr(geometry, geometry.faces[0], {}));
    CHECK(geometry.faces[1].normal == geometry::faceNormalOr(geometry, geometry.faces[1], {}));
  }

  TEST_CASE("Weld rejects degenerate face collapse without mutation") {
    GeometryData geometry;
    geometry.vertices = {
        {{0.0f, 0.0f, 0.0f}},
        {{0.01f, 0.0f, 0.0f}},
        {{1.0f, 0.0f, 0.0f}},
    };
    geometry.faces.push_back(makeFace(0, 1, 2));

    geometry::GeometryRemap remap{{7}, {7}};
    CHECK(geometry::weldVertices(geometry, {.radius = 0.1f}, &remap) == geometry::Error::kDegenerateFace);

    REQUIRE(geometry.vertices.size() == 3);
    REQUIRE(geometry.faces.size() == 1);
    CHECK(remap.vertices.empty());
    CHECK(remap.faces.empty());
    CHECK(geometry.faces[0].corners[0].vertex == 0);
    CHECK(geometry.faces[0].corners[1].vertex == 1);
    CHECK(geometry.faces[0].corners[2].vertex == 2);
  }

  TEST_CASE("Weld can discard collapsed faces") {
    GeometryData geometry;
    geometry.vertices = {
        {{0.0f, 0.0f, 0.0f}},
        {{0.01f, 0.0f, 0.0f}},
        {{1.0f, 0.0f, 0.0f}},
    };
    geometry.faces.push_back(makeFace(0, 1, 2));

    geometry::GeometryRemap remap;
    CHECK(geometry::weldVertices(geometry,
                                 {.radius = 0.1f, .degenerate_faces = geometry::DegenerateFacePolicy::kDiscard},
                                 &remap) == geometry::Error::kNone);

    CHECK(geometry.vertices.size() == 2);
    CHECK(geometry.faces.empty());
    CHECK(remap.vertices == geometry::VertexIndexRemap{0, 0, 1});
    CHECK(remap.faces == geometry::FaceIndexRemap{kInvalidFaceIndex});
  }

  TEST_CASE("Weld rejects geometric degeneration with distinct remapped vertices") {
    GeometryData geometry;
    geometry.vertices = {
        {{1.0f, 0.0f, 0.0f}},
        {{0.0f, 0.0f, 0.0f}},
        {{2.0f, 0.0f, 0.0f}},
        {{1.0f, 0.01f, 0.0f}},
    };
    geometry.faces.push_back(makeFace(1, 3, 2));

    CHECK(geometry::weldVertices(geometry, {.radius = 0.02f}) == geometry::Error::kDegenerateFace);
    REQUIRE(geometry.vertices.size() == 4);
    REQUIRE(geometry.faces.size() == 1);
    CHECK(geometry.faces[0].corners[1].vertex == 3);
  }

  TEST_CASE("Weld discards geometric degeneration and remaps retained faces") {
    GeometryData geometry;
    geometry.vertices = {
        {{1.0f, 0.0f, 0.0f}},
        {{0.0f, 0.0f, 0.0f}},
        {{2.0f, 0.0f, 0.0f}},
        {{1.0f, 0.01f, 0.0f}},
        {{0.0f, 1.0f, 0.0f}},
    };
    geometry.faces.push_back(makeFace(1, 3, 2));
    geometry.faces.push_back(makeFace(1, 4, 2));
    const Vertex* vertex_storage = geometry.vertices.data();
    const Face* face_storage = geometry.faces.data();

    geometry::GeometryRemap remap;
    CHECK(geometry::weldVertices(geometry,
                                 {.radius = 0.02f, .degenerate_faces = geometry::DegenerateFacePolicy::kDiscard},
                                 &remap) == geometry::Error::kNone);

    REQUIRE(geometry.vertices.size() == 4);
    REQUIRE(geometry.faces.size() == 1);
    CHECK(remap.vertices == geometry::VertexIndexRemap{0, 1, 2, 0, 3});
    CHECK(remap.faces == geometry::FaceIndexRemap{kInvalidFaceIndex, 0});
    CHECK(geometry.faces[0].corners[0].vertex == 1);
    CHECK(geometry.faces[0].corners[1].vertex == 3);
    CHECK(geometry.faces[0].corners[2].vertex == 2);
    CHECK(geometry.vertices.data() == vertex_storage);
    CHECK(geometry.faces.data() == face_storage);
  }

  TEST_CASE("Weld handles large sparse inputs without changing topology") {
    GeometryData geometry;
    constexpr std::size_t kVertexCount = 4096;
    geometry.vertices.reserve(kVertexCount);
    for (std::size_t i = 0; i < kVertexCount; ++i) {
      geometry.vertices.push_back({{static_cast<float>(i), 0.0f, 0.0f}});
    }

    geometry::GeometryRemap remap;
    CHECK(geometry::weldVertices(geometry, {}, &remap) == geometry::Error::kNone);
    CHECK(geometry.vertices.size() == kVertexCount);
    CHECK(remap.vertices.empty());
    CHECK(remap.faces.empty());
  }

  TEST_CASE("Weld rejects invalid options without mutation") {
    GeometryData geometry;
    geometry.vertices = {
        {{0.0f, 0.0f, 0.0f}},
        {{0.01f, 0.0f, 0.0f}},
    };

    CHECK(geometry::weldVertices(geometry, {.radius = 0.0f}) == geometry::Error::kInvalidOptions);
    CHECK(geometry.vertices.size() == 2);
  }

  TEST_CASE("Weld rejects non-finite segment vertices without mutation") {
    GeometryData geometry;
    geometry.vertices = {
        {{0.0f, 0.0f, 0.0f}},
        {{std::numeric_limits<float>::infinity(), 0.0f, 0.0f}},
    };

    geometry::GeometryRemap remap{{7}, {7}};
    CHECK(geometry::weldVertices(geometry, {}, &remap) == geometry::Error::kBadVertex);
    REQUIRE(geometry.vertices.size() == 2);
    CHECK(std::isinf(geometry.vertices[1].position.x));
    CHECK(remap.vertices.empty());
    CHECK(remap.faces.empty());
  }

  TEST_CASE("Segmented weld keeps coincident vertices in separate segments") {
    GeometryData geometry;
    geometry.vertices = {
        {{0.0f, 0.0f, 0.0f}},
        {{0.0f, 0.0f, 0.0f}},
    };
    const std::array<VertexIndex, 1> first_vertices = {0};
    const std::array<VertexIndex, 1> second_vertices = {1};
    const std::array<geometry::VertexWeldSegment, 2> segments = {geometry::VertexWeldSegment{first_vertices},
                                                                 geometry::VertexWeldSegment{second_vertices}};

    geometry::GeometryRemap remap;
    CHECK(geometry::weldVerticesSegmented(geometry, {.segments = segments, .protected_vertices = {}}, {}, &remap) ==
          geometry::Error::kNone);
    CHECK(geometry.vertices.size() == 2);
    CHECK(remap.vertices.empty());
  }

  TEST_CASE("Segmented weld rejects unprotected overlap transactionally") {
    GeometryData geometry;
    geometry.vertices = {
        {{0.0f, 0.0f, 0.0f}},
        {{1.0f, 0.0f, 0.0f}},
        {{2.0f, 0.0f, 0.0f}},
    };
    const std::array<VertexIndex, 2> first_vertices = {0, 1};
    const std::array<VertexIndex, 2> second_vertices = {1, 2};
    const std::array<geometry::VertexWeldSegment, 2> segments = {geometry::VertexWeldSegment{first_vertices},
                                                                 geometry::VertexWeldSegment{second_vertices}};

    geometry::GeometryRemap remap{{7}, {7}};
    CHECK(geometry::weldVerticesSegmented(geometry, {.segments = segments, .protected_vertices = {}}, {}, &remap) ==
          geometry::Error::kOverlappingVertexWeldSegments);
    CHECK(geometry.vertices.size() == 3);
    CHECK(remap.vertices.empty());
    CHECK(remap.faces.empty());
  }

  TEST_CASE("Protected vertices may belong to multiple weld segments") {
    GeometryData geometry;
    geometry.vertices = {
        {{0.0f, 0.0f, 0.0f}},
        {{1.0f, 0.0f, 0.0f}},
        {{2.0f, 0.0f, 0.0f}},
    };
    const std::array<VertexIndex, 2> first_vertices = {0, 1};
    const std::array<VertexIndex, 2> second_vertices = {1, 2};
    const std::array<VertexIndex, 1> protected_vertices = {1};
    const std::array<geometry::VertexWeldSegment, 2> segments = {geometry::VertexWeldSegment{first_vertices},
                                                                 geometry::VertexWeldSegment{second_vertices}};

    CHECK(geometry::weldVerticesSegmented(geometry, {.segments = segments, .protected_vertices = protected_vertices}) ==
          geometry::Error::kNone);
    CHECK(geometry.vertices.size() == 3);
  }

  TEST_CASE("Segmented weld retains protected positions and returns their compacted indices") {
    GeometryData geometry;
    geometry.vertices = {
        {{0.0f, 0.0f, 0.0f}},
        {{10.0f, 0.0f, 0.0f}},
        {{0.05f, 0.0f, 0.0f}},
    };
    const std::array<VertexIndex, 2> member_vertices = {0, 2};
    const std::array<VertexIndex, 1> protected_vertices = {2};
    const std::array<geometry::VertexWeldSegment, 1> segments = {geometry::VertexWeldSegment{member_vertices}};

    geometry::GeometryRemap remap;
    CHECK(geometry::weldVerticesSegmented(
              geometry, {.segments = segments, .protected_vertices = protected_vertices}, {.radius = 0.1f}, &remap) ==
          geometry::Error::kNone);
    REQUIRE(geometry.vertices.size() == 2);
    CHECK(geometry.vertices[0].position.x == doctest::Approx(10.0f));
    CHECK(geometry.vertices[1].position.x == doctest::Approx(0.05f));
    CHECK(remap.vertices == geometry::VertexIndexRemap{1, 0, 1});
  }

  TEST_CASE("Segmented weld chooses the closest protected representative") {
    GeometryData geometry;
    geometry.vertices = {
        {{0.05f, 0.0f, 0.0f}},
        {{0.0f, 0.0f, 0.0f}},
        {{0.08f, 0.0f, 0.0f}},
        {{0.14f, 0.0f, 0.0f}},
    };
    const std::array<VertexIndex, 4> member_vertices = {0, 1, 2, 3};
    const std::array<VertexIndex, 2> protected_vertices = {1, 2};
    const std::array<geometry::VertexWeldSegment, 1> segments = {geometry::VertexWeldSegment{member_vertices}};

    geometry::GeometryRemap remap;
    CHECK(geometry::weldVerticesSegmented(
              geometry, {.segments = segments, .protected_vertices = protected_vertices}, {.radius = 0.1f}, &remap) ==
          geometry::Error::kNone);
    REQUIRE(geometry.vertices.size() == 2);
    CHECK(geometry.vertices[0].position.x == doctest::Approx(0.0f));
    CHECK(geometry.vertices[1].position.x == doctest::Approx(0.08f));
    CHECK(remap.vertices == geometry::VertexIndexRemap{1, 0, 1, 1});
  }

  TEST_CASE("Segmented weld can preserve faces while welding unrelated vertices") {
    GeometryData geometry;
    geometry.vertices = {
        {{0.0f, 0.0f, 0.0f}},
        {{0.01f, 0.01f, 0.0f}},
        {{1.0f, 0.0f, 0.0f}},
        {{2.0f, 0.0f, 0.0f}},
        {{2.01f, 0.0f, 0.0f}},
    };
    geometry.faces.push_back(makeFace(0, 1, 2));
    const std::array<VertexIndex, 5> member_vertices = {0, 1, 2, 3, 4};
    const std::array<geometry::VertexWeldSegment, 1> segments = {geometry::VertexWeldSegment{member_vertices}};

    geometry::GeometryRemap remap;
    CHECK(
        geometry::weldVerticesSegmented(geometry,
                                        {.segments = segments, .protected_vertices = {}},
                                        {.radius = 0.1f, .degenerate_faces = geometry::DegenerateFacePolicy::kPreserve},
                                        &remap) == geometry::Error::kNone);
    REQUIRE(geometry.vertices.size() == 4);
    REQUIRE(geometry.faces.size() == 1);
    CHECK(geometry.faces[0].corners[0].vertex != geometry.faces[0].corners[1].vertex);
    CHECK(remap.vertices == geometry::VertexIndexRemap{0, 1, 2, 3, 3});
  }

  TEST_CASE("Segmented weld propagates face preservation across shared vertices") {
    GeometryData geometry;
    geometry.vertices = {
        {{0.0f, 0.0f, 0.0f}},
        {{0.1f, 0.1f, 0.0f}},
        {{30.0f, 0.1f, 0.0f}},
        {{30.0f, 0.0f, 0.0f}},
        {{10.0f, 0.1f, 0.0f}},
        {{20.0f, 0.1f, 0.0f}},
        {{20.0f, 0.0f, 0.0f}},
    };
    geometry.faces.push_back(makeFace(0, 4, 5));
    geometry.faces.push_back(makeFace(0, 2, 6));
    const std::array<VertexIndex, 4> member_vertices = {0, 1, 2, 3};
    const std::array<VertexIndex, 2> protected_vertices = {1, 3};
    const std::array<geometry::VertexWeldSegment, 1> segments = {geometry::VertexWeldSegment{member_vertices}};

    geometry::GeometryRemap remap;
    REQUIRE(
        geometry::weldVerticesSegmented(geometry,
                                        {.segments = segments, .protected_vertices = protected_vertices},
                                        {.radius = 0.2f, .degenerate_faces = geometry::DegenerateFacePolicy::kPreserve},
                                        &remap) == geometry::Error::kNone);

    REQUIRE(geometry.vertices.size() == 7);
    REQUIRE(geometry.faces.size() == 2);
    CHECK(remap.vertices.empty());
    CHECK(remap.faces.empty());
    for (const Face& face : geometry.faces) {
      const std::array<ArxVector3, 3> positions = geometry::facePositions(geometry, face);
      CHECK_FALSE(geometry::degenerateTriangle(positions[0], positions[1], positions[2]));
    }
  }
}
