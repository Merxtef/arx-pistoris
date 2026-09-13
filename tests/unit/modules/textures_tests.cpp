// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#include "doctest/doctest.h"

#include "arx_pistoris/base/indices.h"

#include "image_helpers.h"
#include "modules/textures.h"

#include <string>
#include <vector>

TEST_SUITE("textures") {
  TEST_CASE("Image paths preserve the physical extension separately from the identity") {
    const pistoris::Texture jpeg = pistoris::textures::fromImagePath("folder/item.pie.JPG");
    CHECK(jpeg.path == "folder/item.pie");
    CHECK(jpeg.external_image_extension == ".jpg");

    const pistoris::Texture terminated = pistoris::textures::fromImagePath("folder/item.pie.");
    CHECK(terminated.path == "folder/item.pie");
    CHECK(terminated.external_image_extension.empty());

    const pistoris::Texture extensionless = pistoris::textures::fromImagePath("folder/item");
    CHECK(extensionless.path == "folder/item");
    CHECK(extensionless.external_image_extension.empty());
  }

  TEST_CASE("Rebase assigns collision-free identities after the complete filename") {
    std::vector<pistoris::Texture> textures(3);
    textures[0].path = "first/item.pie";
    textures[1].path = "second/item.pie_1";
    textures[2].path = "third/item.pie";

    pistoris::TexturesData data{textures};
    pistoris::textures::PathRebaseInfo info;
    REQUIRE(pistoris::textures::rebasePaths(data, "GRAPH\\OBJ3D\\TEXTURES", &info) == pistoris::textures::Error::kNone);

    REQUIRE(data.textures.size() == 3);
    CHECK(data.textures[0].path == "graph/obj3d/textures/item.pie");
    CHECK(data.textures[1].path == "graph/obj3d/textures/item.pie_1");
    CHECK(data.textures[2].path == "graph/obj3d/textures/item.pie_2");
    REQUIRE(info.repairs.size() == 1);
    CHECK(info.repairs[0].original == "third/item.pie");
    CHECK(info.repairs[0].repaired == "graph/obj3d/textures/item.pie_2");
  }

  TEST_CASE("Rebase reserves natural names before assigning repaired names") {
    pistoris::TexturesData data;
    data.textures = {{"first/my??tex"}, {"second/my_tex"}, {"third/my_tex_1"}, {"fourth/CON"}, {"fifth/CON_1"}};

    pistoris::textures::PathRebaseInfo info;
    REQUIRE(pistoris::textures::rebasePaths(data, "textures", &info) == pistoris::textures::Error::kNone);

    REQUIRE(data.textures.size() == 5);
    CHECK(data.textures[0].path == "textures/my--tex");
    CHECK(data.textures[1].path == "textures/my_tex");
    CHECK(data.textures[2].path == "textures/my_tex_1");
    CHECK(data.textures[3].path == "textures/con-");
    CHECK(data.textures[4].path == "textures/con_1");
    CHECK(info.repairs.size() == 2);
  }

  TEST_CASE("Rebase repairs portable device names before their extension") {
    pistoris::TexturesData data;
    data.textures = {{"first/NUL.png"}, {"second/NUL_1.png"}};

    REQUIRE(pistoris::textures::rebasePaths(data, "textures") == pistoris::textures::Error::kNone);

    REQUIRE(data.textures.size() == 2);
    CHECK(data.textures[0].path == "textures/nul-.png");
    CHECK(data.textures[1].path == "textures/nul_1.png");
    CHECK(pistoris::textures::validPath(data.textures[0].path));
  }

  TEST_CASE("Rebase preserves game texture punctuation") {
    pistoris::TexturesData data;
    data.textures = {{"first/Wall [metal] (old)&new"}};

    REQUIRE(pistoris::textures::rebasePaths(data, "Custom Textures/(Set)&More") == pistoris::textures::Error::kNone);

    REQUIRE(data.textures.size() == 1);
    CHECK(data.textures[0].path == "custom textures/(set)&more/wall [metal] (old)&new");
  }

  TEST_CASE("Rebase accepts root and trailing directories") {
    pistoris::TexturesData rooted{{{"folder/texture"}}};
    REQUIRE(pistoris::textures::rebasePaths(rooted, {}) == pistoris::textures::Error::kNone);
    CHECK(rooted.textures[0].path == "texture");

    pistoris::TexturesData nested{{{"folder/texture"}}};
    REQUIRE(pistoris::textures::rebasePaths(nested, R"(Custom\Textures\)") == pistoris::textures::Error::kNone);
    CHECK(nested.textures[0].path == "custom/textures/texture");
  }

  TEST_CASE("Rebase reports structural directory repair once") {
    pistoris::TexturesData data{{{"first/a"}, {"second/b"}}};
    pistoris::textures::PathRebaseInfo info;
    REQUIRE(pistoris::textures::rebasePaths(data, "Custom?/Textures", &info) == pistoris::textures::Error::kNone);

    REQUIRE(info.repairs.size() == 1);
    CHECK(info.repairs[0].original == "Custom?/Textures");
    CHECK(info.repairs[0].repaired == "custom-/textures");
  }

  TEST_CASE("Invalid rebase directory is transactional") {
    pistoris::TexturesData data{{{"folder/texture"}}};
    pistoris::textures::PathRebaseInfo info;
    info.repairs.push_back({"old", "new"});

    CHECK(pistoris::textures::rebasePaths(data, "bad//directory", &info) == pistoris::textures::Error::kBadTexture);
    CHECK(data.textures[0].path == "folder/texture");
    REQUIRE(info.repairs.size() == 1);
    CHECK(info.repairs[0].original == "old");
  }

  TEST_CASE("Rebase keeps collision suffixes within the portable filename limit") {
    const std::string long_name(240, 'a');
    pistoris::TexturesData data;
    data.textures = {{"first/" + long_name}, {"second/" + long_name}};

    REQUIRE(pistoris::textures::rebasePaths(data, "textures") == pistoris::textures::Error::kNone);

    REQUIRE(data.textures.size() == 2);
    CHECK(data.textures[0].path == "textures/" + long_name);
    CHECK(data.textures[1].path.size() == std::string("textures/").size() + 240);
    CHECK(data.textures[1].path.ends_with("_1"));
  }

  TEST_CASE("Rebase preserves repeated underscores in texture directories") {
    pistoris::TexturesData data;
    data.textures = {{"texture"}};

    CHECK(pistoris::textures::rebasePaths(data, "BAD__DIRECTORY") == pistoris::textures::Error::kNone);
    CHECK(data.textures[0].path == "bad__directory/texture");
  }

  TEST_CASE("Rebase bounds names against the complete texture path") {
    const std::string component(200, 'd');
    const std::string folder = component + "/" + component + "/" + component + "/" + component;
    const std::string long_name(240, 'a');
    pistoris::TexturesData data;
    data.textures = {{"first/" + long_name}, {"second/" + long_name}};

    REQUIRE(pistoris::textures::rebasePaths(data, folder) == pistoris::textures::Error::kNone);

    REQUIRE(data.textures.size() == 2);
    CHECK(data.textures[0].path.size() <= pistoris::textures::kPathMax);
    CHECK(data.textures[1].path.size() <= pistoris::textures::kPathMax);
    CHECK(pistoris::textures::validPath(data.textures[0].path));
    CHECK(pistoris::textures::validPath(data.textures[1].path));
    CHECK(data.textures[1].path.ends_with("_1"));
  }

  TEST_CASE("Texture validation accepts only standalone external image extension hints") {
    pistoris::Texture texture("texture");
    texture.external_image_extension = ".jpg";
    CHECK(pistoris::textures::validateTexture(texture) == pistoris::textures::Error::kNone);

    texture.external_image_extension = "jpg";
    CHECK(pistoris::textures::validateTexture(texture) == pistoris::textures::Error::kBadImage);

    texture.external_image_extension = ".jpg";
    texture.encoded_image = makeTestBmp();
    CHECK(pistoris::textures::validateTexture(texture) == pistoris::textures::Error::kBadImage);
  }

  TEST_CASE("Texture repair silently canonicalizes case") {
    using namespace pistoris;

    TexturesData data;
    Texture texture{"GRAPH/OBJ3D/TEXTURES/WALL"};
    texture.external_image_extension = ".BMP";
    textures::PathRepairInfo info;
    REQUIRE(textures::repairPath(data, texture, kNoTexture, &info) == textures::Error::kNone);

    CHECK(texture.path == "graph/obj3d/textures/wall");
    CHECK(texture.external_image_extension == ".bmp");
    CHECK(info.repairs.empty());
    CHECK(textures::validateTexture(texture) == textures::Error::kNone);
  }

  TEST_CASE("Texture paths use portable resource path grammar") {
    using namespace pistoris;

    CHECK(textures::validPath("graph/obj3d/textures/wall-[metal].variant"));
    CHECK(textures::validPath("graph/obj3d/textures/wall [metal] (old)&new.variant"));
    CHECK(textures::validPath("_"));
    CHECK(textures::validPath("__"));
    CHECK_FALSE(textures::validPath("graph\\obj3d\\textures\\wall"));
    CHECK_FALSE(textures::validPath("graph/obj3d/textures/WALL"));
    CHECK(textures::validPath("graph/obj3d/textures/wall__metal"));
    CHECK_FALSE(textures::validPath("graph/../textures/wall"));
    CHECK_FALSE(textures::validPath("graph//textures/wall"));
    CHECK_FALSE(textures::validPath("graph/obj3d/textures/wall "));
    CHECK_FALSE(textures::validPath("graph/obj3d/textures/wall#old"));
    CHECK(textures::normalizePath("graph\\obj3d\\textures\\wall [metal] (old)&new.variant") ==
          "graph/obj3d/textures/wall [metal] (old)&new.variant");
    CHECK(textures::normalizePath("graph/obj3d/textures/wall??[metal].variant") ==
          "graph/obj3d/textures/wall--[metal].variant");
    CHECK(textures::normalizePath("graph/obj3d/textures/wall#old.variant") == "graph/obj3d/textures/wall-old.variant");
    CHECK(textures::normalizePath("graph\\obj3d//textures\\wall").empty());
  }

  TEST_CASE("Invalid texture path repair is transactional") {
    using namespace pistoris;

    TexturesData data{{Texture{"valid/path"}}};
    Texture candidate{""};
    textures::PathRepairInfo info;
    CHECK(textures::repairPath(data, candidate, kNoTexture, &info) == textures::Error::kBadTexture);
    CHECK(candidate.path.empty());
    CHECK(data.textures == std::vector<Texture>{{"valid/path"}});
    CHECK(info.repairs.empty());
  }

  TEST_CASE("Texture collision suffix follows the complete logical identity") {
    using namespace pistoris;

    std::vector<Texture> values = {{"texture/item.pie"}, {"texture/item.pie"}};
    REQUIRE(textures::repairPaths(values) == textures::Error::kNone);
    CHECK(values[0].path == "texture/item.pie");
    CHECK(values[1].path == "texture/item.pie_1");
  }
}
