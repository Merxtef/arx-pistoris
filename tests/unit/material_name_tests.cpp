// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#include "doctest/doctest.h"

#include "arx_pistoris/base/flags.h"

#include "external/material_name.h"

#include <array>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

TEST_SUITE("Material names") {
  TEST_CASE("Encoding uses the canonical face flag order") {
    using namespace pistoris;

    constexpr FaceType kFlags = kFaceBitNoShadow | kFaceBitDoublesided | kFaceBitTrans | kFaceBitWater | kFaceBitGlow |
                                kFaceBitIgnore | kFaceBitQuad | kFaceBitTiled | kFaceBitMetal | kFaceBitHide |
                                kFaceBitStone | kFaceBitWood | kFaceBitGravel | kFaceBitEarth | kFaceBitNocol |
                                kFaceBitLava | kFaceBitClimb | kFaceBitFall | kFaceBitNopath | kFaceBitNodraw |
                                kFaceBitPrecisePath | kFaceBitNoClimb | kFaceBitAngular | kFaceBitAngularIdx0 |
                                kFaceBitAngularIdx1 | kFaceBitAngularIdx2 | kFaceBitAngularIdx3 | kFaceBitLateMip;
    const std::string encoded = material_names::encode("stone", kFlags, -0.25f);
    CHECK(encoded ==
          "stone__NO_SHADOW__DOUBLESIDED__TRANS__WATER__GLOW__IGNORE__QUAD__TILED__METAL__HIDE__STONE__WOOD__"
          "GRAVEL__EARTH__NOCOL__LAVA__CLIMB__FALL__NOPATH__NODRAW__PRECISE_PATH__NO_CLIMB__ANGULAR__"
          "ANGULAR_IDX0__ANGULAR_IDX1__ANGULAR_IDX2__ANGULAR_IDX3__LATE_MIP__TRANSVAL_-0.25");

    material_names::Decoded decoded;
    material_names::DecodeInfo info;
    REQUIRE(material_names::decode(encoded, false, decoded, &info) == material_names::DecodeError::kNone);
    CHECK((decoded.fallback_stem == "stone"));
    CHECK(decoded.flags == kFlags);
    REQUIRE(decoded.transval.has_value());
    CHECK(*decoded.transval == -0.25f);
    CHECK(info.duplicate_flags == 0);
    CHECK(info.unknown_tokens == 0);
  }

  TEST_CASE("Decoding distinguishes malformed and repeated transparency values") {
    using namespace pistoris;

    material_names::Decoded decoded;
    CHECK(material_names::decode("", false, decoded) == material_names::DecodeError::kBadName);
    CHECK(material_names::decode("stone__", false, decoded) == material_names::DecodeError::kBadName);
    CHECK(material_names::decode("stone__TRANS__TRANSVAL_bad", false, decoded) ==
          material_names::DecodeError::kBadTransval);
    CHECK(material_names::decode("stone__TRANS__TRANSVAL_inf", false, decoded) ==
          material_names::DecodeError::kBadTransval);
    CHECK(material_names::decode("stone__TRANS__TRANSVAL_nan", false, decoded) ==
          material_names::DecodeError::kBadTransval);
    CHECK(material_names::decode("stone__TRANS__TRANSVAL_1__TRANSVAL_2", false, decoded) ==
          material_names::DecodeError::kDuplicateTransval);
  }

  TEST_CASE("Duplicate flags report and unknown tokens follow the selected policy") {
    using namespace pistoris;

    material_names::Decoded decoded;
    material_names::DecodeInfo info;
    REQUIRE(material_names::decode("stone__TRANS__TRANS", false, decoded, &info) == material_names::DecodeError::kNone);
    CHECK(decoded.flags == kFaceBitTrans);
    CHECK(info.duplicate_flags == 1);

    CHECK(material_names::decode("stone__CUSTOM", false, decoded) == material_names::DecodeError::kUnknownToken);
    REQUIRE(material_names::decode("stone__CUSTOM__CUSTOM_2", true, decoded, &info) ==
            material_names::DecodeError::kNone);
    CHECK(info.unknown_tokens == 2);
    CHECK((info.first_unknown_token == "CUSTOM"));
  }

  TEST_CASE("Fallback stems cannot end at the token delimiter") {
    bool normalized = false;
    CHECK(pistoris::material_names::fallbackStem("graph/test_", &normalized) == "test");
    CHECK(normalized);
    CHECK(pistoris::material_names::fallbackStem("graph/my__texture", &normalized) == "my_texture");
    CHECK(normalized);
  }

  TEST_CASE("Fallback stem batches reserve natural and reserved names") {
    using namespace pistoris;

    const std::vector<Texture> textures = {{"folder/no_tex"}, {"other/no_tex"}, {"folder/no_tex_1"}};
    const std::array<std::uint8_t, 3> referenced = {1, 1, 1};
    constexpr std::array<std::string_view, 1> kReserved = {"no_tex"};
    const std::vector<std::string> stems = material_names::fallbackStems(textures, referenced, kReserved, "test");
    CHECK(stems == std::vector<std::string>{"no_tex_2", "no_tex_3", "no_tex_1"});
  }
}
