// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#include "doctest/doctest.h"

#include "arx_pistoris/glb.hpp"

#include "nlohmann/json.hpp"
#include "support/fixture_catalog.h"

#include <limits>
#include <stdexcept>

TEST_SUITE("Fixture catalog") {
  TEST_CASE("GLB unit ratios follow the conversion contract") {
    nlohmann::json entry = {
        {"glb", {{"path", "unused.glb"}, {"arx_units_per_glb_unit", 1.0f}}},
    };

    for (float units : {pistoris::glb::kMinArxUnitsPerUnit, pistoris::glb::kMaxArxUnitsPerUnit}) {
      entry["glb"]["arx_units_per_glb_unit"] = units;
      const test_support::GlbFixture fixture = test_support::fixtureGlb(entry);
      CHECK(fixture.arx_units_per_glb_unit == units);
    }

    for (float units :
         {0.0f, 1001.0f, std::numeric_limits<float>::infinity(), std::numeric_limits<float>::quiet_NaN()}) {
      entry["glb"]["arx_units_per_glb_unit"] = units;
      CHECK_THROWS_AS(test_support::fixtureGlb(entry), std::runtime_error);
    }
  }
}
