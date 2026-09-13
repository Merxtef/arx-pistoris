// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#include "doctest/doctest.h"

#include "arx_pistoris/base/indices.h"

#include "external/glb/utils/texture.h"
#include "modules/textures.h"

#include <string>
#include <vector>

TEST_SUITE("glb::textures") {
  TEST_CASE("External image identity is normalized without losing source spelling") {
    using namespace pistoris;

    char first_path[] = R"(Textures\Wall.PNG)";
    char second_path[] = "textures/wall.png";
    cgltf_image first{};
    first.uri = first_path;
    cgltf_image second{};
    second.uri = second_path;
    TexturesData textures;
    std::vector<std::string> sources;
    glb::TextureImporter importer(textures, &sources, "test");

    TextureIndex first_index = kNoTexture;
    TextureIndex second_index = kNoTexture;
    REQUIRE(importer.import({&first, "wall"}, first_index) == glb::TextureImportError::kNone);
    REQUIRE(importer.import({&second, "wall"}, second_index) == glb::TextureImportError::kNone);
    CHECK(first_index == second_index);
    REQUIRE(textures.textures.size() == 1);
    CHECK(textures.textures[0].path == "textures/wall");
    CHECK(textures.textures[0].external_image_extension == ".png");
    REQUIRE(sources.size() == 1);
    CHECK(sources[0] == first_path);
  }

  TEST_CASE("Material fallbacks share normalized identity") {
    using namespace pistoris;

    TexturesData textures;
    glb::TextureImporter importer(textures, nullptr, "test");
    TextureIndex first = kNoTexture;
    TextureIndex second = kNoTexture;
    REQUIRE(importer.import({nullptr, "Stone"}, first) == glb::TextureImportError::kNone);
    REQUIRE(importer.import({nullptr, "stone"}, second) == glb::TextureImportError::kNone);
    CHECK(first == second);
    REQUIRE(textures.textures.size() == 1);
    CHECK(textures.textures[0].path == "stone");
  }

  TEST_CASE("Embedded images and material fallbacks have no external source path") {
    using namespace pistoris;

    char embedded_uri[] =
        "data:image/png;base64,"
        "iVBORw0KGgoAAAANSUhEUgAAAAEAAAABCAQAAAC1HAwCAAAAC0lEQVR42mNk+A8AAQUBAScY42YAAAAASUVORK5CYII=";
    cgltf_image embedded{};
    embedded.uri = embedded_uri;
    TexturesData textures;
    std::vector<std::string> source_paths;
    glb::TextureImporter importer(textures, &source_paths, "test");
    TextureIndex embedded_index = kNoTexture;
    TextureIndex fallback_index = kNoTexture;
    REQUIRE(importer.import({&embedded, "embedded"}, embedded_index) == glb::TextureImportError::kNone);
    REQUIRE(importer.import({nullptr, "fallback"}, fallback_index) == glb::TextureImportError::kNone);
    REQUIRE(source_paths.size() == 2);
    CHECK(source_paths[embedded_index].empty());
    CHECK(source_paths[fallback_index].empty());
  }

  TEST_CASE("Collision suffixes stay within the logical texture path limit") {
    const std::string component(240, 'a');
    const std::string path =
        component + "/" + component + "/" + component + "/" + component + "/" + std::string(59, 'b');
    REQUIRE(path.size() == pistoris::textures::kPathMax);
    std::vector<pistoris::Texture> textures = {{path}, {path}};

    REQUIRE(pistoris::glb::makeTexturePathsUnique(textures, "test"));

    REQUIRE(textures.size() == 2);
    CHECK(textures[0].path == path);
    CHECK(textures[1].path != path);
    CHECK(textures[1].path.size() == pistoris::textures::kPathMax);
    CHECK(textures[1].path.ends_with("_1"));
    CHECK(pistoris::textures::validPath(textures[1].path));
  }
}
