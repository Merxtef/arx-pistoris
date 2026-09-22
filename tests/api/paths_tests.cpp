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
  TEST_CASE("Resource selector classification is fixed-width and prefix-only") {
    static_assert(sizeof(ArxResourceKind) == 1);
    ArxResourceKind kind = ARX_RESOURCE_KIND_NONE;
    REQUIRE(arx_pistoris_path_resource_selector_kind(view("MODEL:invalid"), &kind) == ARX_OK);
    CHECK(kind == ARX_RESOURCE_KIND_MODEL);
    REQUIRE(arx_pistoris_path_resource_selector_kind(view("file.ftl"), &kind) == ARX_OK);
    CHECK(kind == ARX_RESOURCE_KIND_NONE);
    CHECK(arx_pistoris_path_resource_selector_kind(view("level:1"), nullptr) == ARX_INVALID_DATA_POINTER);
  }

  TEST_CASE("Portable resource components are available through the C path API") {
    uint32_t portable = 0;
    REQUIRE(arx_pistoris_path_is_portable_resource_path_component(view("My texture_(stone)&[wet]__01.png"),
                                                                  &portable) == ARX_OK);
    CHECK(portable == 1);
    REQUIRE(arx_pistoris_path_is_portable_resource_path_component(view("my#texture.png"), &portable) == ARX_OK);
    CHECK(portable == 0);
    CHECK(arx_pistoris_path_is_portable_resource_path_component(view("texture.png"), nullptr) ==
          ARX_INVALID_DATA_POINTER);
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

  TEST_CASE("Level image paths follow runtime map sharing") {
    uint32_t resource_level = 99;
    REQUIRE(arx_pistoris_path_minimap_resource_level(14, &resource_level) == ARX_OK);
    CHECK(resource_level == 1);
    CHECK(arx_pistoris_path_minimap_resource_level(14, nullptr) == ARX_INVALID_DATA_POINTER);

    std::array<char, 64> output{};
    std::size_t size = 0;
    REQUIRE(arx_pistoris_path_level_minimap(14, output.data(), output.size(), &size) == ARX_OK);
    CHECK(std::string_view(output.data()) == "graph/levels/level1/map");
    REQUIRE(arx_pistoris_path_level_loading_screen(14, output.data(), output.size(), &size) == ARX_OK);
    CHECK(std::string_view(output.data()) == "graph/levels/level14/loading");

    ArxStringView offsets{};
    REQUIRE(arx_pistoris_path_minimap_offsets_file(&offsets) == ARX_OK);
    CHECK(view(offsets) == "graph/levels/mini_offsets.ini");
    CHECK(arx_pistoris_path_minimap_offsets_file(nullptr) == ARX_INVALID_DATA_POINTER);
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
    CHECK(arx_pistoris_path_model_selector_type_count() == 14);
    CHECK(arx_pistoris_path_animation_selector_type_count() == 2);

    ArxStringView type{};
    REQUIRE(arx_pistoris_path_texture_directory(&type) == ARX_OK);
    CHECK(view(type) == "graph/obj3d/textures");
    CHECK(arx_pistoris_path_texture_directory(nullptr) == ARX_INVALID_DATA_POINTER);
    REQUIRE(arx_pistoris_path_sound_directory(&type) == ARX_OK);
    CHECK(view(type) == "sfx");
    CHECK(arx_pistoris_path_sound_directory(nullptr) == ARX_INVALID_DATA_POINTER);
    REQUIRE(arx_pistoris_path_ambiance_sound_directory(&type) == ARX_OK);
    CHECK(view(type) == "sfx/ambiance");
    CHECK(arx_pistoris_path_ambiance_sound_directory(nullptr) == ARX_INVALID_DATA_POINTER);
    REQUIRE(arx_pistoris_path_cinematic_illustration_directory(&type) == ARX_OK);
    CHECK(view(type) == "graph/interface/illustrations");
    CHECK(arx_pistoris_path_cinematic_illustration_directory(nullptr) == ARX_INVALID_DATA_POINTER);
    REQUIRE(arx_pistoris_path_model_selector_type(3, &type) == ARX_OK);
    CHECK(view(type) == "armor");
    REQUIRE(arx_pistoris_path_model_selector_type(11, &type) == ARX_OK);
    CHECK(view(type) == "ui-runes");
    CHECK(arx_pistoris_path_model_selector_type(14, &type) == ARX_INDEX_OUT_OF_RANGE);
    CHECK(view(type).empty());

    ArxResourceSearchLocation location{};
    REQUIRE(arx_pistoris_path_model_search_location(view("armor"), &location) == ARX_OK);
    CHECK(view(location.base_path) == "game/graph/obj3d/interactive/items/armor");
    CHECK(location.max_discovery_depth == 3);
    REQUIRE(arx_pistoris_path_model_search_location(view("ui-menus"), &location) == ARX_OK);
    CHECK(view(location.base_path) == "game/graph/interface/menus");
    CHECK(location.max_discovery_depth == 1);
    REQUIRE(arx_pistoris_path_ambiance_search_location(&location) == ARX_OK);
    CHECK(view(location.base_path) == "sfx/ambiance");
    CHECK(location.max_discovery_depth == 8);
  }

  TEST_CASE("C selector helpers mirror the typed C++ resource API") {
    std::array<char, 128> output{};
    std::size_t size = 0;
    ArxModelPathView model{view("npc"), view("human_base"), view("red")};
    REQUIRE(arx_pistoris_path_model_selector(model, output.data(), output.size(), &size) == ARX_OK);
    CHECK(std::string_view(output.data()) == "model:npc:human_base:red");

    ArxModelPathView parsed{};
    REQUIRE(arx_pistoris_path_model_from_selector(view(output.data()), &parsed) == ARX_OK);
    CHECK(view(parsed.type) == "npc");
    CHECK(view(parsed.name) == "human_base");
    CHECK(view(parsed.tweak) == "red");

    REQUIRE(arx_pistoris_path_entity_class_from_model(model, output.data(), output.size(), &size) == ARX_OK);
    CHECK(std::string_view(output.data()) == "graph/obj3d/interactive/npc/human_base/tweaks/red");
    REQUIRE(arx_pistoris_path_base_entity_class_from_model(model, output.data(), output.size(), &size) == ARX_OK);
    CHECK(std::string_view(output.data()) == "graph/obj3d/interactive/npc/human_base/human_base");

    REQUIRE(arx_pistoris_path_entity_class_from_ftl(
                view("game/myitemsnew/my_item.ftl"), output.data(), output.size(), &size) == ARX_OK);
    CHECK(std::string_view(output.data()) == "myitemsnew/my_item");
    REQUIRE(arx_pistoris_path_ftl_from_entity_class(view("myitemsnew/my_item"), output.data(), output.size(), &size) ==
            ARX_OK);
    CHECK(std::string_view(output.data()) == "game/myitemsnew/my_item.ftl");

    ArxEntityClassKind class_kind = ARX_ENTITY_CLASS_KIND_UNKNOWN;
    REQUIRE(arx_pistoris_path_entity_class_kind(view("graph/obj3d/interactive/items/weapons/sword/sword"),
                                                &class_kind) == ARX_OK);
    CHECK(class_kind == ARX_ENTITY_CLASS_KIND_ITEM);
    REQUIRE(arx_pistoris_path_item_icon_from_entity_class(
                view("graph/obj3d/interactive/items/weapons/sword/sword"), output.data(), output.size(), &size) ==
            ARX_OK);
    CHECK(std::string_view(output.data()) == "graph/obj3d/interactive/items/weapons/sword/sword[icon]");
    REQUIRE(arx_pistoris_path_item_icon_from_entity_class(
                view("graph/obj3d/interactive/npc/human_base/human_base"), output.data(), output.size(), &size) ==
            ARX_OK);
    CHECK(std::string_view(output.data()).empty());
    CHECK(arx_pistoris_path_entity_class_kind(view("../items/sword"), &class_kind) == ARX_INVALID_IDENTIFIER);

    ArxModelPathView ui_model{view("ui-runes"), view("aam"), {}};
    REQUIRE(arx_pistoris_path_model_ftl(ui_model, output.data(), output.size(), &size) == ARX_OK);
    CHECK(std::string_view(output.data()) == "game/graph/interface/book/runes/aam.ftl");
    REQUIRE(arx_pistoris_path_model_from_ftl(view(output.data()), &parsed) == ARX_OK);
    CHECK(view(parsed.type) == "ui-runes");
    CHECK(view(parsed.name) == "aam");
    CHECK(view(parsed.tweak).empty());

    REQUIRE(arx_pistoris_path_cinematic_cin({view("intro")}, output.data(), output.size(), &size) == ARX_OK);
    CHECK(std::string_view(output.data()) == "graph/interface/illustrations/intro.cin");
    ArxCinematicPathView cinematic;
    REQUIRE(arx_pistoris_path_cinematic_from_cin(view(output.data()), &cinematic) == ARX_OK);
    CHECK(view(cinematic.name) == "intro");
    REQUIRE(arx_pistoris_path_cinematic_selector(cinematic, output.data(), output.size(), &size) == ARX_OK);
    CHECK(std::string_view(output.data()) == "cinematic:intro");
    REQUIRE(arx_pistoris_path_ambiance_amb({view("cave/water")}, output.data(), output.size(), &size) == ARX_OK);
    CHECK(std::string_view(output.data()) == "sfx/ambiance/cave/water.amb");
    ArxAmbiancePathView ambiance;
    REQUIRE(arx_pistoris_path_ambiance_from_amb(view(output.data()), &ambiance) == ARX_OK);
    CHECK(view(ambiance.name) == "cave/water");
    REQUIRE(arx_pistoris_path_amb_from_zone_ambiance(ambiance.name, output.data(), output.size(), &size) == ARX_OK);
    CHECK(std::string_view(output.data()) == "sfx/ambiance/cave/water.amb");
    CHECK(arx_pistoris_path_ambiance_amb({view("cave/water#deep")}, output.data(), output.size(), &size) ==
          ARX_INVALID_IDENTIFIER);
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
