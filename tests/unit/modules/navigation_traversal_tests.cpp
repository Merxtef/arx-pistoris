// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#include "doctest/doctest.h"

#include "arx_pistoris/base/indices.h"
#include "arx_pistoris/base/math.h"

#include "modules/geometry.h"
#include "modules/navigation/traversal.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <vector>

namespace {

pistoris::GeometryData makeFloor() {
  pistoris::GeometryData result;
  result.vertices = {
      {{0.0f, 0.0f, 0.0f}},
      {{200.0f, 0.0f, 0.0f}},
      {{200.0f, 0.0f, 200.0f}},
      {{0.0f, 0.0f, 200.0f}},
  };
  for (const std::array<pistoris::VertexIndex, 3> indices : {
           std::array<pistoris::VertexIndex, 3>{0, 1, 2},
           std::array<pistoris::VertexIndex, 3>{0, 2, 3},
       }) {
    pistoris::Face face;
    face.normal = {0.0f, -1.0f, 0.0f};
    for (std::size_t corner = 0; corner < 3; ++corner) {
      face.corners[corner].vertex = indices[corner];
      face.corners[corner].normal = face.normal;
    }
    result.faces.push_back(face);
  }
  return result;
}

}  // namespace

TEST_SUITE("navigation::traversal") {
  TEST_CASE("Traversal reports success and matches the convenience query") {
    const pistoris::GeometryData geometry = makeFloor();
    const pistoris::navigation::NavigationCollisionScene scene(geometry);
    const pistoris::navigation::CylinderTraversalOptions options;
    std::vector<std::uint32_t> scratch;

    CHECK(scene.traversalStatus({50.0f, 0.0f, 50.0f}, {150.0f, 0.0f, 50.0f}, 10.0f, -80.0f, options, scratch) ==
          pistoris::navigation::CylinderTraversalStatus::kTraversable);
    CHECK(scene.traversable({50.0f, 0.0f, 50.0f}, {150.0f, 0.0f, 50.0f}, 10.0f, -80.0f, options, scratch));
  }

  TEST_CASE("Traversal distinguishes unsupported start and step") {
    const pistoris::GeometryData geometry = makeFloor();
    const pistoris::navigation::NavigationCollisionScene scene(geometry);
    pistoris::navigation::CylinderTraversalOptions options;
    options.max_step_distance = 250.0f;
    std::vector<std::uint32_t> scratch;
    pistoris::navigation::CylinderTraversalFailure failure;

    CHECK(scene.traversalStatus(
              {250.0f, 0.0f, 50.0f}, {150.0f, 0.0f, 50.0f}, 10.0f, -80.0f, options, scratch, &failure) ==
          pistoris::navigation::CylinderTraversalStatus::kStartNoSupport);
    CHECK(failure.attempt == pistoris::navigation::CylinderTraversalAttempt::kStart);
    CHECK(failure.step_index == 0);

    CHECK(
        scene.traversalStatus({50.0f, 0.0f, 50.0f}, {300.0f, 0.0f, 50.0f}, 10.0f, -80.0f, options, scratch, &failure) ==
        pistoris::navigation::CylinderTraversalStatus::kStepNoSupport);
    CHECK(failure.attempt != pistoris::navigation::CylinderTraversalAttempt::kStart);
    CHECK(failure.step_index > 0);
  }
}
