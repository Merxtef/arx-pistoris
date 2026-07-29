// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#include "doctest/doctest.h"

#include "utils/unique_name.h"

#include <string>
#include <unordered_set>

TEST_SUITE("Unique portable names") {
  TEST_CASE("Portable names are preserved and unsafe characters are collapsed") {
    CHECK(pistoris::makeUniquePortableName("my-texture.png") == "my-texture.png");
    CHECK(pistoris::makeUniquePortableName("wall_[metal].png") == "wall_[metal].png");
    CHECK(pistoris::makeUniquePortableName("my??__texture.png") == "my_texture.png");
    CHECK(pistoris::isPortableName("wall_[metal].png"));
    CHECK_FALSE(pistoris::isPortableName("my__texture.png"));
    CHECK_FALSE(pistoris::isPortableName("my?texture.png"));
  }

  TEST_CASE("Portable device names are always occupied") {
    CHECK(pistoris::isPortableReservedName("CON"));
    CHECK(pistoris::isPortableReservedName("nul.png"));
    CHECK_FALSE(pistoris::isPortableName("COM1.texture"));
    CHECK(pistoris::makeUniquePortableName("con") == "con_1");
    CHECK(pistoris::makeUniquePortableName("NUL.png") == "NUL_1.png");
  }

  TEST_CASE("Portable suffixes preserve extensions and skip occupied names") {
    const std::unordered_set<std::string> unavailable = {
        "my_texture.png", "my_texture_1.png", "my_texture_2.png", "patrol_"};
    CHECK(pistoris::makeUniquePortableName("my??texture.png", unavailable) == "my_texture_3.png");
    CHECK(pistoris::makeUniquePortableName("patrol_", unavailable) == "patrol_1");
  }
}
