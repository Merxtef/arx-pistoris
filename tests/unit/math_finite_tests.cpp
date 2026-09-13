// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#include "doctest/doctest.h"

#include "arx_pistoris/base/math.h"

#include "utils/math/finite.h"

#include <limits>

using namespace pistoris;

TEST_SUITE("math::finite") {
  TEST_CASE("Scalar") {
    CHECK(math::finite(1.0f));
    CHECK(math::finite(1.0));
    CHECK_FALSE(math::finite(std::numeric_limits<float>::infinity()));
    CHECK_FALSE(math::finite(std::numeric_limits<double>::quiet_NaN()));
  }

  TEST_CASE("ArxTypes") {
    CHECK(math::finite(ArxVector2{1.0f, 2.0f}));
    CHECK(math::finite(ArxVector3{1.0f, 2.0f, 3.0f}));
    CHECK(math::finite(ArxAngle{1.0f, 2.0f, 3.0f}));
    CHECK(math::finite(ArxColor3{0.1f, 0.2f, 0.3f}));

    CHECK_FALSE(math::finite(ArxVector3{1.0f, std::numeric_limits<float>::infinity(), 3.0f}));
  }
}
