// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#include "doctest/doctest.h"

#include "utils/packed_index.h"

TEST_CASE("PackedSymmetricPairIndexUsesLowerTriangleOrder") {
  CHECK(pistoris::packed::symmetricPairCount(0) == 0);
  CHECK(pistoris::packed::symmetricPairCount(1) == 0);
  CHECK(pistoris::packed::symmetricPairCount(4) == 6);

  CHECK(pistoris::packed::symmetricPairIndex(0, 1) == 0);
  CHECK(pistoris::packed::symmetricPairIndex(1, 0) == 0);
  CHECK(pistoris::packed::symmetricPairIndex(0, 2) == 1);
  CHECK(pistoris::packed::symmetricPairIndex(2, 0) == 1);
  CHECK(pistoris::packed::symmetricPairIndex(1, 2) == 2);
  CHECK(pistoris::packed::symmetricPairIndex(0, 3) == 3);
  CHECK(pistoris::packed::symmetricPairIndex(1, 3) == 4);
  CHECK(pistoris::packed::symmetricPairIndex(2, 3) == 5);
}
