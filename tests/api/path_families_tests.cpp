// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#include "doctest/doctest.h"

#include "arx_pistoris/base/status.h"
#include "arx_pistoris/base/string_view.h"
#include "arx_pistoris/paths.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <ostream>  // IWYU pragma: keep
#include <string_view>

namespace {

ArxStringView pathView(std::string_view value) { return {value.data(), value.size()}; }

std::string_view pathView(ArxStringView value) { return {value.data, value.size}; }

}  // namespace

TEST_SUITE("C path API") {
  TEST_CASE("Portable filenames and zone ambiance paths expose their normalized forms") {
    std::uint32_t portable = 0;
    REQUIRE(arx_pistoris_path_is_portable_filename(pathView("wall_[metal].png"), &portable) == ARX_OK);
    CHECK(portable == 1);
    REQUIRE(arx_pistoris_path_is_portable_filename(pathView("folder/wall.png"), &portable) == ARX_OK);
    CHECK(portable == 0);

    std::array<char, 128> output{};
    std::size_t size = 0;
    REQUIRE(arx_pistoris_path_sanitize_portable_filename(
                pathView("new??__texture.png"), output.data(), output.size(), &size) == ARX_OK);
    CHECK(std::string_view(output.data()) == "new_texture.png");
    REQUIRE(arx_pistoris_path_normalize_zone_ambiance(
                pathView(R"(Cave\Water.v2.AMB)"), output.data(), output.size(), &size) == ARX_OK);
    CHECK(std::string_view(output.data()) == "cave/water.v2");
  }

  TEST_CASE("Level paths cover FTS, LLF, selector, and DLF scene projections") {
    std::array<char, 128> output{};
    std::size_t size = 0;
    std::uint32_t level = 0;

    REQUIRE(arx_pistoris_path_level_fts(7, output.data(), output.size(), &size) == ARX_OK);
    CHECK(std::string_view(output.data()) == "game/graph/levels/level7/fast.fts");
    REQUIRE(arx_pistoris_path_level_from_fts(pathView(output.data()), &level) == ARX_OK);
    CHECK(level == 7);

    REQUIRE(arx_pistoris_path_level_llf(7, output.data(), output.size(), &size) == ARX_OK);
    CHECK(std::string_view(output.data()) == "graph/levels/level7/level7.llf");
    REQUIRE(arx_pistoris_path_level_from_llf(pathView(output.data()), &level) == ARX_OK);
    CHECK(level == 7);

    REQUIRE(arx_pistoris_path_level_selector(7, output.data(), output.size(), &size) == ARX_OK);
    CHECK(std::string_view(output.data()) == "level:7");
    REQUIRE(arx_pistoris_path_level_from_selector(pathView(output.data()), &level) == ARX_OK);
    CHECK(level == 7);

    REQUIRE(arx_pistoris_path_dlf_scene_from_level_name(pathView("custom.v2"), output.data(), output.size(), &size) ==
            ARX_OK);
    CHECK(std::string_view(output.data()) == "graph/levels/custom.v2");
    REQUIRE(arx_pistoris_path_fts_from_dlf_scene(
                pathView("graph/levels/custom.v2"), output.data(), output.size(), &size) == ARX_OK);
    CHECK(std::string_view(output.data()) == "game/graph/levels/custom.v2/fast.fts");

    ArxResourceSearchLocation location{};
    REQUIRE(arx_pistoris_path_level_search_location(&location) == ARX_OK);
    CHECK(pathView(location.base_path) == "graph/levels");
  }

  TEST_CASE("Model class paths and Animation paths expose typed projections") {
    std::array<char, 160> output{};
    std::size_t size = 0;

    ArxModelPathView model{};
    REQUIRE(arx_pistoris_path_model_from_entity_class(pathView("graph/obj3d/interactive/npc/human_base/tweaks/red"),
                                                      &model) == ARX_OK);
    CHECK(pathView(model.type) == "npc");
    CHECK(pathView(model.name) == "human_base");
    CHECK(pathView(model.tweak) == "red");

    ArxStringView type{};
    REQUIRE(arx_pistoris_path_animation_selector_type(0, &type) == ARX_OK);
    CHECK(pathView(type) == "npc");
    REQUIRE(arx_pistoris_path_animation_directory(pathView("armor"), output.data(), output.size(), &size) == ARX_OK);
    CHECK(std::string_view(output.data()) == "graph/obj3d/anims/fix_inter");

    ArxAnimationPathView animation{pathView("npc"), pathView("walk")};
    REQUIRE(arx_pistoris_path_animation_tea(animation, output.data(), output.size(), &size) == ARX_OK);
    CHECK(std::string_view(output.data()) == "graph/obj3d/anims/npc/walk.tea");
    ArxAnimationPathView parsed{};
    REQUIRE(arx_pistoris_path_animation_from_tea(pathView(output.data()), &parsed) == ARX_OK);
    CHECK(pathView(parsed.type) == "npc");
    CHECK(pathView(parsed.name) == "walk");

    REQUIRE(arx_pistoris_path_animation_selector(animation, output.data(), output.size(), &size) == ARX_OK);
    CHECK(std::string_view(output.data()) == "anim:npc:walk");
    REQUIRE(arx_pistoris_path_animation_from_selector(pathView(output.data()), &parsed) == ARX_OK);
    CHECK(pathView(parsed.type) == "npc");
    CHECK(pathView(parsed.name) == "walk");

    ArxResourceSearchLocation location{};
    REQUIRE(arx_pistoris_path_animation_search_location(pathView("npc"), &location) == ARX_OK);
    CHECK(pathView(location.base_path) == "graph/obj3d/anims/npc");
  }

  TEST_CASE("Cinematic selectors expose typed names and search metadata") {
    ArxCinematicPathView cinematic{};
    REQUIRE(arx_pistoris_path_cinematic_from_selector(pathView("cinematic:intro"), &cinematic) == ARX_OK);
    CHECK(pathView(cinematic.name) == "intro");

    ArxResourceSearchLocation location{};
    REQUIRE(arx_pistoris_path_cinematic_search_location(&location) == ARX_OK);
    CHECK(pathView(location.base_path) == "graph/interface/illustrations");
  }

  TEST_CASE("Ambiance selectors roundtrip typed names") {
    std::array<char, 160> output{};
    std::size_t size = 0;

    ArxAmbiancePathView ambiance{pathView("cave/water")};
    REQUIRE(arx_pistoris_path_ambiance_selector(ambiance, output.data(), output.size(), &size) == ARX_OK);
    CHECK(std::string_view(output.data()) == "ambiance:cave/water");
    ArxAmbiancePathView parsed{};
    REQUIRE(arx_pistoris_path_ambiance_from_selector(pathView(output.data()), &parsed) == ARX_OK);
    CHECK(pathView(parsed.name) == "cave/water");
  }
}
