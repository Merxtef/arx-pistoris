// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#include "doctest/doctest.h"

#include "utils/identifier.h"
#include "utils/portable_filename.h"
#include "utils/resource_path.h"
#include "utils/unique_value.h"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <unordered_set>
#include <vector>

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

TEST_SUITE("Semantic identifiers") {
  TEST_CASE("Identifiers reject ambiguous separators and accept inert hyphens") {
    using namespace pistoris;

    CHECK(isIdentifier("room_12"));
    CHECK_FALSE(isIdentifier("_room"));
    CHECK_FALSE(isIdentifier("room_"));
    CHECK_FALSE(isIdentifier("room__12"));
    CHECK(isIdentifier("room-name"));
    CHECK_FALSE(isIdentifier("room name (old)&new"));
    CHECK(isIdentifier("room name (old)&new",
                       {.allow_spaces = true, .allow_parentheses = true, .allow_ampersands = true}));
    CHECK(isIdentifier("", {.allow_empty = true}));
    CHECK_FALSE(isIdentifier(""));
  }

  TEST_CASE("Normalization repairs one identifier without assigning a collision suffix") {
    using namespace pistoris;

    const IdentifierNormalization normalized = normalizeIdentifier("_North ?? Hall__A_");
    CHECK(normalized.value == "North-Hall_A");
    CHECK(normalized.repair != IdentifierRepair::kNone);

    const IdentifierNormalization path =
        normalizeIdentifier("graph\\obj3d//textures\\item.pie",
                            {.allow_path_separators = true, .allow_brackets = true, .allow_dots = true});
    CHECK(path.value == "graph/obj3d/textures/item.pie");
  }

  TEST_CASE("In-place repair preserves canonical strings") {
    using namespace pistoris;

    std::string canonical = "action_point";
    const char* storage = canonical.data();
    CHECK(repairIdentifier(canonical) == IdentifierRepair::kNone);
    CHECK(canonical.data() == storage);

    std::string repaired = "_Action ?? Point_";
    CHECK(repairIdentifier(repaired, {.letter_case = IdentifierCase::kLower}) != IdentifierRepair::kNone);
    CHECK(repaired == "action-point");
  }

  TEST_CASE("Uniquifier normalizes names before reserving natural suffixes") {
    using namespace pistoris;

    std::vector<std::string> values = {"a", "a", "a_1", "_bad__name_", ""};
    std::vector<IdentifierRepair> repairs(values.size());
    IdentifierUniquifier names;
    names.reserve(values.size());
    for (std::string& value : values) names.add(value);

    const IdentifierRepairSummary summary = names.apply(repairs);

    CHECK(values == std::vector<std::string>{"a", "a_2", "a_1", "bad_name", "unnamed"});
    CHECK(summary.changed == 3);
    CHECK(summary.normalized == 2);
    CHECK(summary.deduplicated == 1);
    CHECK(repairs[0] == IdentifierRepair::kNone);
    CHECK(repairs[1] == IdentifierRepair::kDuplicate);
    CHECK(repairs[3] != IdentifierRepair::kNone);
    CHECK(repairs[4] != IdentifierRepair::kNone);

    CHECK(names.apply().changed == 0);
    CHECK(values == std::vector<std::string>{"a", "a_2", "a_1", "bad_name", "unnamed"});
  }

  TEST_CASE("Occupied identifiers remain fixed and constrain suffix length") {
    using namespace pistoris;

    std::string candidate = "aaaaaaaa";
    IdentifierUniquifier names({.max_length = 8});
    names.reserve(1, 2);
    names.occupy("aaaaaaaa");
    names.occupy(std::string("aaaaaa_1"));
    names.add(candidate);

    const IdentifierRepairSummary summary = names.apply();

    CHECK(candidate == "aaaaaa_2");
    CHECK(candidate.size() == 8);
    CHECK(summary.changed == 1);
    CHECK(summary.normalized == 0);
    CHECK(summary.deduplicated == 1);
  }

  TEST_CASE("Path identifiers normalize separators and retain texture brackets") {
    using namespace pistoris;

    std::string path = "graph\\obj3d//textures\\wall_[metal]__01";
    IdentifierUniquifier names({.allow_path_separators = true, .allow_brackets = true});
    names.add(path);

    CHECK(names.apply().changed == 1);
    CHECK(path == "graph/obj3d/textures/wall_[metal]_01");
    CHECK(isIdentifier(path, {.allow_path_separators = true, .allow_brackets = true}));
  }

  TEST_CASE("Case-insensitive identity preserves spelling and suffixes collisions") {
    using namespace pistoris;

    std::vector<std::string> values = {"Zone", "zone", "ZONE_1"};
    IdentifierUniquifier names({.ascii_case_insensitive = true});
    for (std::string& value : values) names.add(value);

    CHECK(names.apply().deduplicated == 1);
    CHECK(values == std::vector<std::string>{"Zone", "zone_2", "ZONE_1"});
  }

  TEST_CASE("Allowed empty identifiers do not participate in uniqueness") {
    using namespace pistoris;

    std::vector<std::string> values = {"", "", "named"};
    IdentifierUniquifier names({.allow_empty = true});
    for (std::string& value : values) names.add(value);

    CHECK(names.apply().changed == 0);
    CHECK(values == std::vector<std::string>{"", "", "named"});
  }

  TEST_CASE("Bounded identifiers use compact deterministic fallbacks") {
    using namespace pistoris;

    std::string candidate = "a";
    IdentifierUniquifier names({.max_length = 3});
    names.occupy("a");
    for (std::size_t ordinal = 1; ordinal <= 9; ++ordinal) names.occupy("a_" + std::to_string(ordinal));
    names.add(candidate);

    const IdentifierRepairSummary summary = names.apply();

    CHECK_FALSE(summary.exhausted);
    CHECK(candidate == "0");
    CHECK(summary.deduplicated == 1);
  }

  TEST_CASE("Exhausted bounded identifiers remain unchanged") {
    using namespace pistoris;

    std::string candidate = "a";
    IdentifierUniquifier names({.max_length = 1});
    for (char value : std::string_view("0123456789abcdefghijklmnopqrstuvwxyz")) names.occupy({&value, 1});
    names.add(candidate);

    const IdentifierRepairSummary summary = names.apply();

    CHECK(summary.exhausted);
    CHECK(candidate == "a");
    CHECK(summary.changed == 0);
  }

  TEST_CASE("Value uniquifier clears the caller change mask") {
    using namespace pistoris;

    std::vector<std::string> values = {"a", "b"};
    std::vector<std::uint8_t> changed(values.size(), 1);
    ValueUniquifier<ResourcePathIdentityHash, ResourcePathIdentityEqual> names;
    const ValueUniquifierResult result = names.apply(
        std::span(values),
        [](std::string_view value, std::size_t ordinal) -> std::optional<std::string> {
          return std::string(value) + "_" + std::to_string(ordinal);
        },
        changed);

    CHECK(result.error == ValueUniquifierError::kNone);
    CHECK(result.changed == 0);
    CHECK(changed == std::vector<std::uint8_t>{0, 0});
  }
}
