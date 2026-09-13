// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#include "doctest/doctest.h"

#include "arx_pistoris/base/flags.h"
#include "arx_pistoris/base/indices.h"
#include "arx_pistoris/base/math.h"

#include "image_helpers.h"
#include "modules/geometry.h"
#include "modules/textures.h"

#include <limits>
#include <string>

using namespace pistoris;

namespace {

Face makeValidFace() {
  Face face;
  face.texture = 0;
  face.flags = kFaceBitStone;
  face.transval = 0.25f;

  face.corners[0].vertex = 0;
  face.corners[0].normal = {0.0f, 1.0f, 0.0f};
  face.corners[0].u = 0.0f;
  face.corners[0].v = 0.0f;

  face.corners[1].vertex = 1;
  face.corners[1].normal = face.corners[0].normal;
  face.corners[1].u = 1.0f;
  face.corners[1].v = 0.0f;

  face.corners[2].vertex = 2;
  face.corners[2].normal = face.corners[0].normal;
  face.corners[2].u = 0.0f;
  face.corners[2].v = 1.0f;

  return face;
}

GeometryData makeValidGeometry() {
  GeometryData geometry;
  geometry.vertices = {
      {{1.0f, 2.0f, 3.0f}},
      {{3.0f, 2.0f, 3.0f}},
      {{1.0f, 6.0f, 8.0f}},
      {{10.0f, 11.0f, 12.0f}},
  };
  geometry.faces.push_back(makeValidFace());
  return geometry;
}

void checkAabb(const ArxAabb& bounds, const ArxVector3& min, const ArxVector3& max) {
  CHECK(bounds.min.x == doctest::Approx(min.x));
  CHECK(bounds.min.y == doctest::Approx(min.y));
  CHECK(bounds.min.z == doctest::Approx(min.z));
  CHECK(bounds.max.x == doctest::Approx(max.x));
  CHECK(bounds.max.y == doctest::Approx(max.y));
  CHECK(bounds.max.z == doctest::Approx(max.z));
}

}  // namespace

TEST_SUITE("geometry::validation") {
  TEST_CASE("Texture identities are unique under game path comparison") {
    pistoris::TexturesData textures;
    textures.textures = {{"graph/textures/wall"}, {"graph/textures/wall"}};

    CHECK(pistoris::textures::validate(textures.textures) == pistoris::textures::Error::kDuplicateTexture);
  }

  TEST_CASE("Accepts valid geometry and reports derived bounds") {
    GeometryData geometry = makeValidGeometry();
    GeometryDerived derived;

    CHECK(geometry::validate(geometry, 1, &derived) == geometry::Error::kNone);
    checkAabb(derived.bounds, {1.0f, 2.0f, 3.0f}, {10.0f, 11.0f, 12.0f});
    checkAabb(derived.referenced_bounds, {1.0f, 2.0f, 3.0f}, {3.0f, 6.0f, 8.0f});
    CHECK(geometry::validate(geometry, 1) == geometry::Error::kNone);
  }

  TEST_CASE("Rejects geometry without faces") {
    GeometryData geometry = makeValidGeometry();
    geometry.faces.clear();

    CHECK(geometry::validate(geometry, 1) == geometry::Error::kNoGeometry);
  }

  TEST_CASE("Full validation reports vertex errors before textures and faces") {
    GeometryData geometry = makeValidGeometry();
    geometry.faces.clear();
    geometry.vertices[0].position.x = std::numeric_limits<float>::infinity();

    CHECK(geometry::validate(geometry, 1) == geometry::Error::kBadVertex);
  }

  TEST_CASE("Granular vertex validation reports bounds") {
    GeometryData geometry = makeValidGeometry();
    ArxAabb bounds;

    CHECK(geometry::validateVertices(geometry.vertices, &bounds) == geometry::Error::kNone);
    checkAabb(bounds, {1.0f, 2.0f, 3.0f}, {10.0f, 11.0f, 12.0f});
    CHECK(geometry::validateVertices({}) == geometry::Error::kNoGeometry);

    geometry.vertices[0].position.x = std::numeric_limits<float>::infinity();
    CHECK(geometry::validateVertices(geometry.vertices) == geometry::Error::kBadVertex);
  }

  TEST_CASE("Geometry accepts finite positions independent of Level bounds") {
    constexpr float kArxWorldMax = 16000.0f;
    CHECK(geometry::validateVertex({{0.0f, -100.0f, kArxWorldMax}}) == geometry::Error::kNone);
    CHECK(geometry::validateVertex({{kArxWorldMax, 100.0f, 0.0f}}) == geometry::Error::kNone);
    CHECK(geometry::validateVertex({{-0.01f, 0.0f, 0.0f}}) == geometry::Error::kNone);
    CHECK(geometry::validateVertex({{kArxWorldMax + 0.01f, 0.0f, 0.0f}}) == geometry::Error::kNone);
    CHECK(geometry::validateVertex({{0.0f, 0.0f, -0.01f}}) == geometry::Error::kNone);
    CHECK(geometry::validateVertex({{0.0f, 0.0f, kArxWorldMax + 0.01f}}) == geometry::Error::kNone);
  }

  TEST_CASE("Granular texture validation accepts repeated underscores but rejects empty paths") {
    TexturesData textures_data{{Texture{"graph/obj3d/textures/stone"}}};

    CHECK(textures::validateTexture(textures_data.textures[0]) == textures::Error::kNone);
    CHECK(textures::validate(textures_data.textures) == textures::Error::kNone);
    textures_data.textures[0].path.clear();
    CHECK(textures::validateTexture(textures_data.textures[0]) == textures::Error::kBadTexture);
    CHECK(textures::validate(textures_data.textures) == textures::Error::kBadTexture);

    textures_data.textures[0].path = "graph/obj3d/textures/stone__moss";
    CHECK(textures::validateTexture(textures_data.textures[0]) == textures::Error::kNone);
    CHECK(textures::validate(textures_data.textures) == textures::Error::kNone);

    textures_data.textures[0].path = std::string("graph/obj3d/textures/stone\0moss", 31);
    CHECK(textures::validateTexture(textures_data.textures[0]) == textures::Error::kBadTexture);
    CHECK(textures::validate(textures_data.textures) == textures::Error::kBadTexture);

    textures_data.textures[0].path = "graph/obj3d/textures/stone.bmp";
    textures_data.textures[0].encoded_image = {1, 2, 3};
    CHECK(textures::validateTexture(textures_data.textures[0]) == textures::Error::kBadImage);
    CHECK(textures::validate(textures_data.textures) == textures::Error::kBadImage);
    textures_data.textures[0].encoded_image = makeTestBmp();
    CHECK(textures::validateTexture(textures_data.textures[0]) == textures::Error::kNone);
    CHECK(textures::validate(textures_data.textures) == textures::Error::kNone);
  }

  TEST_CASE("Texture identities must be canonical before uniqueness validation") {
    TexturesData textures_data;
    textures_data.textures = {{"graph/obj3d/textures/stone"}, {"GRAPH/OBJ3D/TEXTURES/STONE"}};
    CHECK(textures::validate(textures_data.textures) == textures::Error::kBadTexture);
  }

  TEST_CASE("Granular face validation uses counts and reports referenced bounds") {
    GeometryData geometry = makeValidGeometry();
    ArxAabb referenced_bounds;

    CHECK(geometry::validateFaceReferences(geometry.faces[0], geometry.vertices.size(), 1) == geometry::Error::kNone);
    CHECK(geometry::validateFaces(geometry.faces, geometry.vertices, 1, &referenced_bounds) == geometry::Error::kNone);
    checkAabb(referenced_bounds, {1.0f, 2.0f, 3.0f}, {3.0f, 6.0f, 8.0f});

    CHECK(geometry::validateFaces(geometry.faces, geometry.vertices, 0) == geometry::Error::kBadFaceTexture);

    geometry.faces[0].texture = kNoTexture;
    CHECK(geometry::validateFaces(geometry.faces, geometry.vertices, 0) == geometry::Error::kNone);

    geometry.vertices[0].position.x = std::numeric_limits<float>::infinity();
    CHECK(geometry::validateFaces(geometry.faces, geometry.vertices, 0) == geometry::Error::kBadVertex);
  }

  TEST_CASE("Rejects bad vertices") {
    GeometryData geometry = makeValidGeometry();
    geometry.vertices[0].position.x = std::numeric_limits<float>::infinity();

    CHECK(geometry::validate(geometry, 1) == geometry::Error::kBadVertex);
  }

  TEST_CASE("Rejects bad face texture references") {
    GeometryData geometry = makeValidGeometry();
    geometry.faces[0].texture = -2;
    CHECK(geometry::validate(geometry, 1) == geometry::Error::kBadFaceTexture);

    geometry = makeValidGeometry();
    geometry.faces[0].texture = 1;
    CHECK(geometry::validate(geometry, 1) == geometry::Error::kBadFaceTexture);
  }

  TEST_CASE("Rejects bad face flags and transval") {
    GeometryData geometry = makeValidGeometry();
    geometry.faces[0].flags = kFaceBitsAll | (1U << 28U);
    CHECK(geometry::validate(geometry, 1) == geometry::Error::kBadFaceType);

    geometry = makeValidGeometry();
    geometry.faces[0].flags = kFaceBitQuad;
    CHECK(geometry::validate(geometry, 1) == geometry::Error::kBadFaceType);

    geometry = makeValidGeometry();
    geometry.faces[0].transval = std::numeric_limits<float>::infinity();
    CHECK(geometry::validate(geometry, 1) == geometry::Error::kBadFaceTransval);
  }

  TEST_CASE("Rejects bad face vertices") {
    GeometryData geometry = makeValidGeometry();
    geometry.faces[0].corners[2].vertex = 1;
    CHECK(geometry::validate(geometry, 1) == geometry::Error::kBadFaceVertex);

    geometry = makeValidGeometry();
    geometry.faces[0].corners[2].vertex = 99;
    CHECK(geometry::validate(geometry, 1) == geometry::Error::kBadFaceVertex);
  }

  TEST_CASE("Rejects bad corner attributes") {
    GeometryData geometry = makeValidGeometry();
    geometry.faces[0].corners[0].normal.x = std::numeric_limits<float>::infinity();
    CHECK(geometry::validate(geometry, 1) == geometry::Error::kBadCornerNormal);

    geometry = makeValidGeometry();
    geometry.faces[0].corners[0].normal = {0.0f, 2.0f, 0.0f};
    CHECK(geometry::validate(geometry, 1) == geometry::Error::kBadCornerNormal);

    geometry = makeValidGeometry();
    geometry.faces[0].corners[0].u = std::numeric_limits<float>::infinity();
    CHECK(geometry::validate(geometry, 1) == geometry::Error::kBadFaceUv);
  }

  TEST_CASE("Rejects a nonfinite stored face normal") {
    GeometryData geometry = makeValidGeometry();
    geometry.faces[0].normal.x = std::numeric_limits<float>::infinity();
    CHECK(geometry::validate(geometry, 1) == geometry::Error::kBadFaceNormal);
  }

  TEST_CASE("Rejects degenerate faces") {
    GeometryData geometry = makeValidGeometry();
    geometry.vertices[2].position = {7.0f, 2.0f, 3.0f};

    CHECK(geometry::validate(geometry, 1) == geometry::Error::kDegenerateFace);
  }
}
