// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#include "doctest/doctest.h"

#include "utils/number_text.h"

#include <limits>

TEST_SUITE("Number text") {
  TEST_CASE("Finite floats require a complete locale-independent value") {
    float value = 0.0f;
    CHECK(pistoris::parseFiniteFloat("-1.25e2", value));
    CHECK(value == -125.0f);
    CHECK(pistoris::parseFiniteFloat(".5", value));
    CHECK(value == 0.5f);

    CHECK_FALSE(pistoris::parseFiniteFloat("", value));
    CHECK_FALSE(pistoris::parseFiniteFloat(" 1", value));
    CHECK_FALSE(pistoris::parseFiniteFloat("1 ", value));
    CHECK_FALSE(pistoris::parseFiniteFloat("1suffix", value));
    CHECK_FALSE(pistoris::parseFiniteFloat("+1", value));
  }

  TEST_CASE("Finite floats reject non-finite and out-of-range values") {
    float value = 0.0f;
    CHECK_FALSE(pistoris::parseFiniteFloat("inf", value));
    CHECK_FALSE(pistoris::parseFiniteFloat("-inf", value));
    CHECK_FALSE(pistoris::parseFiniteFloat("nan", value));
    CHECK_FALSE(pistoris::parseFiniteFloat("1e1000", value));
    CHECK_FALSE(pistoris::parseFiniteFloat("1e-1000", value));
    CHECK(value == 0.0f);

    CHECK(pistoris::parseFiniteFloat("3.4028234e38", value));
    CHECK(value <= std::numeric_limits<float>::max());
  }
}
