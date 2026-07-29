// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#include "doctest/doctest.h"

#include "utils/spatial/hash_grid.h"

#include <unordered_set>

TEST_SUITE("spatial::hash_grid") {
  TEST_CASE("Signed cell coordinates have distinct defined keys") {
    std::unordered_set<pistoris::spatial::CellKey> keys;
    for (int x = -1; x <= 1; ++x)
      for (int z = -1; z <= 1; ++z) keys.insert(pistoris::spatial::cellKey(x, z));

    CHECK(keys.size() == 9);
    CHECK(pistoris::spatial::cellKey(-0.1f, 0.0f, 100.0f) == pistoris::spatial::cellKey(-1, 0));
    CHECK(pistoris::spatial::cellKey(0.0f, -0.1f, 100.0f) == pistoris::spatial::cellKey(0, -1));
  }
}
