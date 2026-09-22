// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#include "doctest/doctest.h"

#include "arx_pistoris/base/status.h"

#include "external/glb/container.h"
#include "external/glb/writer.h"

#include <cstdint>
#include <string>
#include <vector>

TEST_SUITE("GLB writer") {
  TEST_CASE("Escapes JSON strings without changing their values") {
    const std::string name = std::string("node\\quote\"\ncontrol") + '\x01';
    pistoris::glb::Builder builder;
    builder.addRoot(builder.addNode(name));

    std::vector<std::uint8_t> encoded;
    REQUIRE(builder.write(encoded) == ARX_OK);

    pistoris::glb::Asset asset;
    REQUIRE(pistoris::glb::parse(encoded, asset) == ARX_OK);
    REQUIRE(asset.data()->nodes_count == 1);
    REQUIRE(asset.data()->nodes[0].name != nullptr);
    CHECK(std::string(asset.data()->nodes[0].name) == name);
  }

  TEST_CASE("Writes perspective cameras attached to nodes") {
    pistoris::glb::Builder builder;
    const int node = builder.addNode("view");
    const int camera = builder.addPerspectiveCamera(
        "camera", {.vertical_fov = 0.8f, .aspect_ratio = 4.0f / 3.0f, .znear = 0.01f, .zfar = {}});
    builder.setNodeCamera(node, camera);
    builder.addRoot(node);

    std::vector<std::uint8_t> encoded;
    REQUIRE(builder.write(encoded) == ARX_OK);
    pistoris::glb::Asset asset;
    REQUIRE(pistoris::glb::parse(encoded, asset) == ARX_OK);
    REQUIRE(asset.data()->cameras_count == 1);
    REQUIRE(asset.data()->nodes_count == 1);
    const auto& perspective = asset.data()->cameras[0].data.perspective;
    CHECK(asset.data()->nodes[0].camera == &asset.data()->cameras[0]);
    CHECK(asset.data()->cameras[0].type == cgltf_camera_type_perspective);
    CHECK(perspective.has_aspect_ratio);
    CHECK(perspective.aspect_ratio == doctest::Approx(4.0f / 3.0f));
    CHECK(perspective.yfov == doctest::Approx(0.8f));
    CHECK(perspective.znear == doctest::Approx(0.01f));
    CHECK_FALSE(perspective.has_zfar);
  }

  TEST_CASE("Rejects invalid perspective camera options") {
    pistoris::glb::Builder builder;
    const int node = builder.addNode("view");
    builder.setNodeCamera(
        node,
        builder.addPerspectiveCamera("camera",
                                     {.vertical_fov = 0.0f, .aspect_ratio = 4.0f / 3.0f, .znear = 0.01f, .zfar = {}}));
    builder.addRoot(node);
    std::vector<std::uint8_t> encoded;
    CHECK(builder.write(encoded) == ARX_GLB_BAD_FORMAT);
  }
}
