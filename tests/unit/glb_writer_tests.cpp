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
}
