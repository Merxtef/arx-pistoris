// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#include "doctest/doctest.h"

#include "arx_pistoris/ftl_data.hpp"
#include "arx_pistoris/pistoris_types.h"

#include "api/api_helpers.h"
#include "helpers.h"
#include "utils/log.h"

#include <cstring>
#include <string>

TEST_SUITE("api_helpers") {
  TEST_CASE("OverwriteTexturePathsNoopsWithoutContainers") {
    auto d = makeData(3);
    d.faces.push_back(makeFace(0, 1, 2, pistoris::kFtlTextureNone));

    CHECK(pistoris::api::overwriteTexturePaths(d, "GRAPH\\NEW.BMP", "test") == ARX_OK);

    CHECK(d.texture_containers.empty());
    REQUIRE(d.faces.size() == 1);
    CHECK(d.faces[0].texture_id == pistoris::kFtlTextureNone);
  }

  TEST_CASE("OverwriteTexturePathsReplacesExistingContainersAndReassignsTexturedFaces") {
    auto d = makeData(3);
    pistoris::ftl::TextureContainer tc0{};
    pistoris::ftl::TextureContainer tc1{};
    std::memcpy(tc0.filename, "OLD0.BMP", 9);
    std::memcpy(tc1.filename, "OLD1.BMP", 9);
    d.texture_containers.push_back(tc0);
    d.texture_containers.push_back(tc1);
    d.faces.push_back(makeFace(0, 1, 2, 1));
    d.faces.push_back(makeFace(0, 1, 2, pistoris::kFtlTextureNone));

    CHECK(pistoris::api::overwriteTexturePaths(d, "GRAPH\\REPLACED.BMP", "test") == ARX_OK);

    REQUIRE(d.texture_containers.size() == 1);
    CHECK(std::string(d.texture_containers[0].filename) == "GRAPH\\REPLACED.BMP");
    REQUIRE(d.faces.size() == 2);
    CHECK(d.faces[0].texture_id == 0);
    CHECK(d.faces[1].texture_id == pistoris::kFtlTextureNone);
  }

  TEST_CASE("OverwriteTexturePathsTruncatesAndWarns") {
    auto d = makeData(1);
    pistoris::ftl::TextureContainer tc{};
    d.texture_containers.push_back(tc);
    bool warn_fired = false;
    pistoris::log_fn = [](ArxLogLevel level, const char*, void* ud) {
      if (level == ARX_LOG_WARN) *static_cast<bool*>(ud) = true;
    };
    pistoris::log_ud = &warn_fired;

    std::string long_path(300, 'A');
    CHECK(pistoris::api::overwriteTexturePaths(d, long_path, "test") == ARX_OK);
    pistoris::log_fn = nullptr;
    pistoris::log_ud = nullptr;

    REQUIRE(d.texture_containers.size() == 1);
    const char* fn = d.texture_containers[0].filename;
    CHECK(std::strlen(fn) == 255);
    CHECK(fn[255] == '\0');
    CHECK(warn_fired);
  }
}  // TEST_SUITE("api_helpers")
