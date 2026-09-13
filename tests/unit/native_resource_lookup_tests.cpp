// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#include "doctest/doctest.h"

#include "arx_pistoris/base/status.h"
#include "arx_pistoris/native/amb.hpp"
#include "arx_pistoris/native/dlf.hpp"
#include "arx_pistoris/native/ftl.hpp"
#include "arx_pistoris/native/fts.hpp"
#include "arx_pistoris/native/tea.hpp"
#include "arx_pistoris/runtime/types.h"

#include "amb_helpers.h"
#include "helpers.h"
#include "native/amb.h"
#include "native/dlf.h"
#include "native/ftl.h"
#include "native/fts.h"
#include "native/resource_lookup.h"
#include "native/tea.h"
#include "utils/cursor.h"
#include "utils/log.h"

#include <array>
#include <cstring>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace {

struct LogCapture {
  std::vector<std::string> warnings;

  LogCapture() {
    pistoris::log_fn = [](ArxLogLevel level, const char* message, void* userdata) {
      if (level == ARX_LOG_WARN && message) static_cast<LogCapture*>(userdata)->warnings.emplace_back(message);
    };
    pistoris::log_ud = this;
  }

  ~LogCapture() {
    pistoris::log_fn = nullptr;
    pistoris::log_ud = nullptr;
  }

  bool contains(std::string_view text) const {
    for (const std::string& warning : warnings)
      if (warning.find(text) != std::string::npos) return true;
    return false;
  }
};

pistoris::tea::Data makeTeaWithSample(std::string_view path) {
  pistoris::tea::Data data;
  data.keyframes.resize(1);
  pistoris::tea::Sample& sample = data.keyframes.front().sample.emplace();
  std::memcpy(sample.name, path.data(), path.size());
  return data;
}

pistoris::dlf::Data makeDlfWithPaths(std::string scene, std::string entity, std::string ambiance) {
  pistoris::dlf::Data data;
  data.scene_path = std::move(scene);
  if (!entity.empty()) data.entities.push_back({std::move(entity)});
  if (!ambiance.empty()) {
    pistoris::dlf::Zone zone;
    zone.name = "zone";
    zone.points.resize(3);
    zone.height = 1;
    zone.ambiance = {std::move(ambiance), 100.0f};
    data.zones.push_back(std::move(zone));
  }
  return data;
}

void writeAll(const pistoris::ftl::Data& ftl, const pistoris::fts::Data& fts, const pistoris::amb::Data& amb,
              const pistoris::tea::Data& tea, const pistoris::dlf::Data& dlf) {
  pistoris::WriteCursor cursor;
  REQUIRE(pistoris::saveFtl(&ftl, cursor) == ARX_OK);
  cursor = {};
  REQUIRE(pistoris::saveFts(&fts, cursor) == ARX_OK);
  cursor = {};
  REQUIRE(pistoris::saveAmb(&amb, cursor) == ARX_OK);
  cursor = {};
  REQUIRE(pistoris::saveTea(&tea, cursor) == ARX_OK);
  cursor = {};
  REQUIRE(pistoris::saveDlf(&dlf, nullptr, {}, cursor) == ARX_OK);
}

}  // namespace

TEST_SUITE("native resource lookup") {
  TEST_CASE("RecognizesLibertatisDefaultLooseRoots") {
    constexpr std::array<std::string_view, 7> kRoots = {
        "editor", "game", "graph", "localisation", "misc", "sfx", "speech"};
    for (std::string_view root : kRoots) {
      CHECK(pistoris::resolvesThroughDefaultLooseRoot({}, root));
      CHECK(pistoris::resolvesThroughDefaultLooseRoot({}, std::string(root) + "/path/file.ext"));
    }

    CHECK(pistoris::resolvesThroughDefaultLooseRoot({}, R"(GrApH\\obj3d//file.ftl)"));
    CHECK(pistoris::resolvesThroughDefaultLooseRoot("sfx", "./ambiance/../spell.wav"));
    CHECK(pistoris::resolvesThroughDefaultLooseRoot("sfx", "../misc/spell.wav"));
    CHECK(pistoris::resolvesThroughDefaultLooseRoot("sfx/ambiance", "../../graph/spell.wav"));
    CHECK_FALSE(pistoris::resolvesThroughDefaultLooseRoot("sfx", "../custom/spell.wav"));
    CHECK_FALSE(pistoris::resolvesThroughDefaultLooseRoot("sfx", "../../graph/spell.wav"));
    CHECK_FALSE(pistoris::resolvesThroughDefaultLooseRoot({}, "custom/file.ext"));
    CHECK_FALSE(pistoris::resolvesThroughDefaultLooseRoot({}, {}));
  }

  TEST_CASE("NativeWritersKeepCanonicalResourcePathsQuiet") {
    pistoris::ftl::Data ftl = makeData();
    std::strcpy(ftl.texture_containers.emplace_back().filename, "graph/obj3d/texture.bmp");

    pistoris::fts::Data fts = makeMinimalFtsData();
    std::strcpy(fts.textures[1].fic, "graph/levels/texture.bmp");
    fts.scene.num_textures = 1;

    pistoris::amb::Data amb = makeAmbData();
    pistoris::tea::Data tea = makeTeaWithSample("footstep.wav");
    pistoris::dlf::Data dlf =
        makeDlfWithPaths("graph/levels/level1/", "graph/obj3d/interactive/items/key/key", "test.amb");

    LogCapture logs;
    writeAll(ftl, fts, amb, tea, dlf);
    CHECK(logs.warnings.empty());
  }

  TEST_CASE("NativeWritersWarnForResourcesOutsideDefaultLooseRoots") {
    pistoris::ftl::Data ftl = makeData();
    std::strcpy(ftl.texture_containers.emplace_back().filename, "custom/model.bmp");

    pistoris::fts::Data fts = makeMinimalFtsData();
    std::strcpy(fts.textures[7].fic, "custom/level.bmp");
    fts.scene.num_textures = 1;

    pistoris::amb::Data amb = makeAmbData();
    amb.tracks.front().sample_path = "custom/ambiance.wav";
    pistoris::tea::Data tea = makeTeaWithSample("../custom/step.wav");
    pistoris::dlf::Data dlf =
        makeDlfWithPaths("../custom/level/", "obj3d/interactive/item", "../../custom/ambiance.amb");

    LogCapture logs;
    writeAll(ftl, fts, amb, tea, dlf);
    CHECK(logs.contains("FTL saving: texture[0]"));
    CHECK(logs.contains("FTS saving: texture 7"));
    CHECK(logs.contains("AMB saving: track[0]"));
    CHECK(logs.contains("TEA saving: keyframe[0]"));
    CHECK(logs.contains("DLF saving: scene path"));
    CHECK(logs.contains("DLF saving: entity[0]"));
    CHECK(logs.contains("DLF saving: zone[0] ambiance path"));
  }
}
