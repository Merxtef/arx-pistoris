// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#include "doctest/doctest.h"

#include "arx_pistoris/ftl_data.hpp"
#include "arx_pistoris/pistoris_types.h"
#include "arx_pistoris/tea_data.hpp"

#include "helpers.h"
#include "utils/math/xform.h"

#include <cstdlib>
#include <vector>

using pistoris::makeAffineXform;

static pistoris::ftl::Data makeFtlForXform() {
  auto d = makeData(3);
  d.faces.push_back(makeFace(0, 1, 2));

  pistoris::ftl::Vertex origin_v;
  origin_v.position = {0.0f, 0.0f, 0.0f};
  origin_v.normal   = {0.0f, 0.0f, 1.0f};
  d.vertices.push_back(origin_v);
  d.header.origin = 3;
  return d;
}

TEST_SUITE("xform") {
  TEST_CASE("XformIdentity") {
    auto x = makeAffineXform(0, 0, 0, 1, 1, 1, 0, 0, 0);
    CHECK(pistoris::isIdentityXform(x));
    auto d = makeFtlForXform();
    REQUIRE(pistoris::applyXformFtl(d, x) == ARX_OK);
    CHECK(d.vertices[0].position.x == 0.0f);
    CHECK(d.vertices[1].position.x == 1.0f);
    CHECK(d.vertices[2].position.x == 2.0f);
  }

  TEST_CASE("XformRotate180Y") {
    auto x = makeAffineXform(0, 180, 0, 1, 1, 1, 0, 0, 0);
    auto d = makeFtlForXform();
    REQUIRE(pistoris::applyXformFtl(d, x) == ARX_OK);
    CHECK(d.vertices[0].position.x == doctest::Approx(0.0f));
    CHECK(d.vertices[1].position.x == doctest::Approx(-1.0f));
    CHECK(d.vertices[2].position.x == doctest::Approx(-2.0f));

    CHECK(d.vertices[3].position.x == doctest::Approx(0.0f));
    CHECK(d.vertices[3].position.y == doctest::Approx(0.0f));
    CHECK(d.vertices[3].position.z == doctest::Approx(0.0f));
  }

  TEST_CASE("XformScaleUniform") {
    auto x = makeAffineXform(0, 0, 0, 2, 2, 2, 0, 0, 0);
    auto d = makeFtlForXform();
    REQUIRE(pistoris::applyXformFtl(d, x) == ARX_OK);
    CHECK(d.vertices[1].position.x == doctest::Approx(2.0f));
    CHECK(d.vertices[2].position.x == doctest::Approx(4.0f));
    CHECK(d.vertices[1].normal.z == doctest::Approx(1.0f));
  }

  TEST_CASE("XformOffsetAndHeaderOrigin") {
    auto x = makeAffineXform(0, 0, 0, 1, 1, 1, 5, 0, 0);
    auto d = makeFtlForXform();
    REQUIRE(pistoris::applyXformFtl(d, x) == ARX_OK);
    CHECK(d.vertices[0].position.x == doctest::Approx(5.0f));
    CHECK(d.vertices[1].position.x == doctest::Approx(6.0f));
    CHECK(d.vertices[2].position.x == doctest::Approx(7.0f));
    CHECK(d.vertices[3].position.x == doctest::Approx(0.0f));
  }

  TEST_CASE("XformHeaderOriginGetsLinearNotOffset") {
    auto d                 = makeData(3);
    d.vertices[0].position = {2.0f, 0.0f, 0.0f};
    d.faces.push_back(makeFace(0, 1, 2));
    d.header.origin = 0;

    auto x = makeAffineXform(0, 0, 0, 3, 3, 3, 5, 0, 0);
    REQUIRE(pistoris::applyXformFtl(d, x) == ARX_OK);
    CHECK(d.vertices[0].position.x == doctest::Approx(6.0f));
    CHECK(d.vertices[1].position.x == doctest::Approx(8.0f));
    CHECK(d.vertices[2].position.x == doctest::Approx(11.0f));
  }

  TEST_CASE("XformRejectsNegativeDet") {
    auto x = makeAffineXform(0, 0, 0, -1, 1, 1, 0, 0, 0);
    auto d = makeFtlForXform();
    CHECK(pistoris::applyXformFtl(d, x) == ARX_INVALID_XFORM);
  }

  TEST_CASE("XformNonUniformScaleAdjustsNormal") {
    auto x               = makeAffineXform(0, 0, 0, 2, 1, 1, 0, 0, 0);
    auto d               = makeFtlForXform();
    d.vertices[1].normal = {1.0f, 0.0f, 0.0f};
    REQUIRE(pistoris::applyXformFtl(d, x) == ARX_OK);
    CHECK(d.vertices[1].normal.x == doctest::Approx(1.0f));
    CHECK(d.vertices[1].normal.y == doctest::Approx(0.0f));
  }

  TEST_CASE("XformTeaGroupAnimTranslateGetsLinearNoOffset") {
    pistoris::tea::Data t;
    t.num_frames = 10;
    t.num_groups = 1;
    pistoris::tea::Keyframe kf;
    kf.num_frame = 5;
    pistoris::tea::GroupAnim ga;
    ga.quat      = {1.0f, 0.0f, 0.0f, 0.0f};
    ga.translate = {1.0f, 0.0f, 0.0f};
    ga.zoom      = {0.0f, 0.0f, 0.0f};
    kf.groups.push_back(ga);
    t.keyframes.push_back(kf);

    auto x = makeAffineXform(0, 0, 0, 2, 2, 2, 5, 0, 0);
    REQUIRE(pistoris::applyXformTea(t, x) == ARX_OK);
    CHECK(t.keyframes[0].groups[0].translate.x == doctest::Approx(2.0f));
    CHECK(t.keyframes[0].groups[0].zoom.x == doctest::Approx(0.0f));
  }

  TEST_CASE("XformTeaRootTranslateGetsBothLinearAndOffset") {
    pistoris::tea::Data t;
    t.num_frames = 10;
    t.num_groups = 0;
    pistoris::tea::Keyframe kf;
    kf.num_frame = 5;
    kf.translate = pistoris::ArxVector3{1.0f, 0.0f, 0.0f};
    t.keyframes.push_back(kf);

    auto x = makeAffineXform(0, 0, 0, 2, 2, 2, 5, 0, 0);
    REQUIRE(pistoris::applyXformTea(t, x) == ARX_OK);
    REQUIRE(t.keyframes[0].translate.has_value());
    CHECK(t.keyframes[0].translate->x == doctest::Approx(7.0f));
  }

  TEST_CASE("XformTeaQuatIdentityStaysIdentity") {
    pistoris::tea::Data t;
    t.num_frames = 10;
    t.num_groups = 1;
    pistoris::tea::Keyframe kf;
    kf.num_frame = 5;
    kf.quat      = pistoris::ArxQuat{1.0f, 0.0f, 0.0f, 0.0f};  // identity (w first)
    pistoris::tea::GroupAnim ga;
    ga.quat = {1.0f, 0.0f, 0.0f, 0.0f};
    kf.groups.push_back(ga);
    t.keyframes.push_back(kf);

    auto x = makeAffineXform(0, 180, 0, 1, 1, 1, 0, 0, 0);
    REQUIRE(pistoris::applyXformTea(t, x) == ARX_OK);
    REQUIRE(t.keyframes[0].quat.has_value());
    CHECK(t.keyframes[0].quat->w == doctest::Approx(1.0f).epsilon(1e-5f));
    CHECK(t.keyframes[0].quat->x == doctest::Approx(0.0f).epsilon(1e-5f));
    CHECK(t.keyframes[0].quat->y == doctest::Approx(0.0f).epsilon(1e-5f));
    CHECK(t.keyframes[0].quat->z == doctest::Approx(0.0f).epsilon(1e-5f));
    CHECK(t.keyframes[0].groups[0].quat.w == doctest::Approx(1.0f).epsilon(1e-5f));
    CHECK(t.keyframes[0].groups[0].quat.x == doctest::Approx(0.0f).epsilon(1e-5f));
    CHECK(t.keyframes[0].groups[0].quat.y == doctest::Approx(0.0f).epsilon(1e-5f));
    CHECK(t.keyframes[0].groups[0].quat.z == doctest::Approx(0.0f).epsilon(1e-5f));
  }

  TEST_CASE("XformTeaGroupQuatAxisIsRemapped") {
    pistoris::tea::Data t;
    t.num_frames = 10;
    t.num_groups = 1;
    pistoris::tea::Keyframe kf;
    kf.num_frame = 5;
    pistoris::tea::GroupAnim ga;
    ga.quat = {0.70710678f, 0.0f, 0.70710678f, 0.0f};  // 90 degrees around +Y
    kf.groups.push_back(ga);
    t.keyframes.push_back(kf);

    auto x = makeAffineXform(180, 0, 0, 1, 1, 1, 0, 0, 0);
    REQUIRE(pistoris::applyXformTea(t, x) == ARX_OK);
    const auto& q = t.keyframes[0].groups[0].quat;
    CHECK(q.w == doctest::Approx(0.70710678f).epsilon(1e-5f));
    CHECK(q.x == doctest::Approx(0.0f).epsilon(1e-5f));
    CHECK(q.y == doctest::Approx(-0.70710678f).epsilon(1e-5f));
    CHECK(q.z == doctest::Approx(0.0f).epsilon(1e-5f));
  }
}  // TEST_SUITE
