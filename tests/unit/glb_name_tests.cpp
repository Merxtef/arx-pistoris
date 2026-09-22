// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#include "doctest/doctest.h"

#include "external/glb/utils/names.h"

#include <ostream>  // IWYU pragma: keep
#include <span>
#include <string_view>

TEST_SUITE("GLB convention names") {
  TEST_CASE("Recognizes labels that resemble convention tokens") {
    CHECK(pistoris::glb::looksLikeConventionToken("FINAL_KEY"));
    CHECK(pistoris::glb::looksLikeConventionToken("DREAM.001"));
    CHECK(pistoris::glb::looksLikeConventionToken("X.001"));
    CHECK_FALSE(pistoris::glb::looksLikeConventionToken("RGB"));
    CHECK_FALSE(pistoris::glb::looksLikeConventionToken("OFF"));
    CHECK_FALSE(pistoris::glb::looksLikeConventionToken(".001"));
    CHECK_FALSE(pistoris::glb::looksLikeConventionToken("1234"));
    CHECK_FALSE(pistoris::glb::looksLikeConventionToken("label"));
  }

  TEST_CASE("Semantic tokens take precedence over recoverable labels") {
    auto parse = [](std::span<const std::string_view> tokens, bool& dream) {
      if (tokens.empty() || tokens.front() != "KEY") return false;
      if (tokens.size() == 1) return true;
      if (tokens.size() != 2) return false;
      if (tokens.back() == "DREAM") {
        dream = true;
        return true;
      }
      return tokens.back() == "SPEED_1";
    };

    bool dream = false;
    pistoris::glb::ParsedLabel label;
    REQUIRE(pistoris::glb::parseRecoverableLabel("KEY__DREAM", dream, &label, {{"DREAM"}, {"SPEED_"}}, parse));
    CHECK(dream);
    CHECK(label.presence == pistoris::glb::LabelPresence::kMissing);

    dream = false;
    REQUIRE(pistoris::glb::parseRecoverableLabel("KEY__DREAM.001", dream, &label, {{"DREAM"}, {"SPEED_"}}, parse));
    CHECK_FALSE(dream);
    CHECK(label.presence == pistoris::glb::LabelPresence::kPresent);
    CHECK(label.text == "DREAM.001");

    CHECK_FALSE(pistoris::glb::parseRecoverableLabel("KEY__SPEED_bad", dream, &label, {{"DREAM"}, {"SPEED_"}}, parse));

    bool marker = false;
    const auto parse_marker = [](std::span<const std::string_view> tokens, bool& parsed) {
      if (tokens.size() != 1 || tokens.front() != "MARKER") return false;
      parsed = true;
      return true;
    };
    REQUIRE(pistoris::glb::parseRecoverableLabel("MARKER__MARKER", marker, &label, {}, parse_marker));
    CHECK(marker);
    CHECK(label.presence == pistoris::glb::LabelPresence::kPresent);
    CHECK(label.text == "MARKER");
  }
}
