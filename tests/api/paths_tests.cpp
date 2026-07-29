// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#include "doctest/doctest.h"

#include "arx_pistoris/arx_pistoris.h"
#include "arx_pistoris/paths/types.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <ostream>  // IWYU pragma: keep
#include <string_view>

namespace {

ArxStringView view(std::string_view value) { return {value.data(), value.size()}; }

std::string_view view(ArxStringView value) { return {value.data, value.size}; }

}  // namespace

TEST_SUITE("C path API") {
  TEST_CASE("Resource shorthand classification is fixed-width and prefix-only") {
    static_assert(sizeof(ArxResourceKind) == 1);
    ArxResourceKind kind = ARX_RESOURCE_KIND_NONE;
    REQUIRE(arx_pistoris_path_resource_shorthand_kind(view("MODEL:invalid"), &kind) == ARX_OK);
    CHECK(kind == ARX_RESOURCE_KIND_MODEL);
    REQUIRE(arx_pistoris_path_resource_shorthand_kind(view("file.ftl"), &kind) == ARX_OK);
    CHECK(kind == ARX_RESOURCE_KIND_NONE);
    CHECK(arx_pistoris_path_resource_shorthand_kind(view("level:1"), nullptr) == ARX_INVALID_DATA_POINTER);
  }

  TEST_CASE("Builders support size queries and preserve insufficient buffers") {
    std::size_t required = 0;
    REQUIRE(arx_pistoris_path_level_dlf(1, nullptr, 0, &required) == ARX_OK);
    CHECK(required == std::string_view("graph/levels/level1/level1.dlf").size());

    std::array<char, 4> insufficient = {'x', 'x', 'x', '\0'};
    CHECK(arx_pistoris_path_level_dlf(1, insufficient.data(), insufficient.size(), &required) == ARX_BUFFER_TOO_SMALL);
    CHECK(std::string_view(insufficient.data()) == "xxx");

    std::array<char, 64> output{};
    REQUIRE(arx_pistoris_path_level_dlf(1, output.data(), output.size(), &required) == ARX_OK);
    CHECK(std::string_view(output.data()) == "graph/levels/level1/level1.dlf");
  }

  TEST_CASE("Inverse helpers return views borrowed from the input") {
    constexpr std::string_view kPath = "game/graph/obj3d/interactive/items/armor/chest/chest.ftl";
    ArxModelPathView model{};
    REQUIRE(arx_pistoris_path_model_from_ftl(view(kPath), &model) == ARX_OK);
    CHECK(view(model.type) == "armor");
    CHECK(view(model.name) == "chest");
    CHECK(view(model.tweak).empty());
    CHECK(model.name.data >= kPath.data());
    CHECK(model.name.data + model.name.size <= kPath.data() + kPath.size());

    constexpr std::string_view kTweak = "game/graph/obj3d/interactive/npc/human_base/tweaks/skins/red.ftl";
    REQUIRE(arx_pistoris_path_model_from_ftl(view(kTweak), &model) == ARX_OK);
    CHECK(view(model.type) == "npc");
    CHECK(view(model.name) == "human_base");
    CHECK(view(model.tweak) == "skins/red");
  }

  TEST_CASE("Static type and search metadata use borrowed views") {
    CHECK(arx_pistoris_path_model_type_count() == 11);
    CHECK(arx_pistoris_path_animation_type_count() == 2);

    ArxStringView type{};
    REQUIRE(arx_pistoris_path_model_type(3, &type) == ARX_OK);
    CHECK(view(type) == "armor");
    CHECK(arx_pistoris_path_model_type(11, &type) == ARX_INDEX_OUT_OF_RANGE);
    CHECK(view(type).empty());

    ArxResourceSearchLocation location{};
    REQUIRE(arx_pistoris_path_model_search_location(view("armor"), &location) == ARX_OK);
    CHECK(view(location.base_path) == "game/graph/obj3d/interactive/items/armor");
    CHECK(location.max_discovery_depth == 3);
    REQUIRE(arx_pistoris_path_ambiance_search_location(&location) == ARX_OK);
    CHECK(view(location.base_path) == "sfx/ambiance");
    CHECK(location.max_discovery_depth == 8);
  }

  TEST_CASE("C shorthand helpers mirror the typed C++ resource API") {
    std::array<char, 128> output{};
    std::size_t size = 0;
    ArxModelPathView model{view("npc"), view("human_base"), view("red")};
    REQUIRE(arx_pistoris_path_model_shorthand(model, output.data(), output.size(), &size) == ARX_OK);
    CHECK(std::string_view(output.data()) == "model:npc:human_base:red");

    ArxModelPathView parsed{};
    REQUIRE(arx_pistoris_path_model_from_shorthand(view(output.data()), &parsed) == ARX_OK);
    CHECK(view(parsed.type) == "npc");
    CHECK(view(parsed.name) == "human_base");
    CHECK(view(parsed.tweak) == "red");

    REQUIRE(arx_pistoris_path_cinematic_shorthand({view("intro")}, output.data(), output.size(), &size) == ARX_OK);
    CHECK(std::string_view(output.data()) == "cinematic:intro");
    REQUIRE(arx_pistoris_path_ambiance_file({view("cave/water")}, output.data(), output.size(), &size) == ARX_OK);
    CHECK(std::string_view(output.data()) == "sfx/ambiance/cave/water.amb");
  }

  TEST_CASE("Invalid identifiers and pointers are reported") {
    std::uint32_t level = 42;
    CHECK(arx_pistoris_path_level_from_dlf(view("not/a/level"), &level) == ARX_INVALID_IDENTIFIER);
    CHECK(level == 0);
    CHECK(arx_pistoris_path_level_from_dlf(view("graph/levels/level1/level1.dlf"), nullptr) ==
          ARX_INVALID_DATA_POINTER);
    CHECK(arx_pistoris_path_level_dlf(1, nullptr, 1, nullptr) == ARX_INVALID_DATA_POINTER);
  }
}
