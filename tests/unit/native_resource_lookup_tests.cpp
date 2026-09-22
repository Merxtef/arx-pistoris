// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#include "doctest/doctest.h"

#include "arx_pistoris/base/status.h"
#include "arx_pistoris/native/amb.hpp"
#include "arx_pistoris/native/cin.hpp"
#include "arx_pistoris/native/dlf.hpp"
#include "arx_pistoris/native/ftl.hpp"
#include "arx_pistoris/native/fts.hpp"
#include "arx_pistoris/native/tea.hpp"
#include "arx_pistoris/runtime/types.h"

#include "amb_helpers.h"
#include "cin_helpers.h"
#include "helpers.h"
#include "native/amb.h"
#include "native/cin.h"
#include "native/cin_resource_path.h"
#include "native/dlf.h"
#include "native/fixed_string.h"
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

pistoris::dlf::Data makeDlfWithPaths(const std::string& scene, const std::string& entity, const std::string& ambiance) {
  pistoris::dlf::Data data;
  REQUIRE(pistoris::copyFixedString(scene, data.scene_path));
  if (!entity.empty()) {
    data.entities.emplace_back();
    REQUIRE(pistoris::copyFixedString(entity, data.entities.back().class_path));
  }
  if (!ambiance.empty()) {
    pistoris::dlf::Zone zone;
    REQUIRE(pistoris::copyFixedString("zone", zone.name));
    zone.points.resize(3);
    zone.height = 1;
    zone.ambiance.emplace();
    REQUIRE(pistoris::copyFixedString(ambiance, zone.ambiance->name));
    data.zones.push_back(std::move(zone));
  }
  return data;
}

void writeAll(const pistoris::ftl::Data& ftl, const pistoris::fts::Data& fts, const pistoris::amb::Data& amb,
              const pistoris::tea::Data& tea, const pistoris::dlf::Data& dlf, const pistoris::Cin& cin) {
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
  cursor = {};
  REQUIRE(pistoris::saveCin(&cin, cursor) == ARX_OK);
}

}  // namespace

TEST_SUITE("native resource lookup") {
  TEST_CASE("CIN paths follow runtime lookup") {
    std::string path;
    REQUIRE(pistoris::decodeCinIllustrationPath(R"(C:\project\Arx\graph/interface/image.bmp)", path));
    CHECK(path == "graph/interface/image");
    REQUIRE(pistoris::decodeCinIllustrationPath(R"(C:/project/Arx/graph/interface/image.bmp)", path));
    CHECK(path == "c:/project/arx/graph/interface/image");
    REQUIRE(pistoris::decodeCinIllustrationPath(R"(prefixarx\graph/interface/image.bmp)", path));
    CHECK(path == "graph/interface/image");
    REQUIRE(pistoris::encodeCinIllustrationPath("graph/interface/image", path));
    CHECK(path == R"(arx\graph/interface/image.)");

    pistoris::cin::Sound sound;
    REQUIRE(pistoris::decodeCinSoundPath("my.sound.", sound));
    CHECK(sound.path == "my.sound");
    CHECK_FALSE(sound.speech);
    REQUIRE(pistoris::decodeCinSoundPath("my.sound.ogg", sound));
    CHECK(sound.path == "my.sound");
    CHECK_FALSE(sound.speech);
    REQUIRE(pistoris::decodeCinSoundPath("my.sound.wav", sound));
    CHECK(sound.path == "my.sound");
    CHECK_FALSE(sound.speech);
    REQUIRE(pistoris::decodeCinSoundPath(R"(C:\game\sfx\speech\English\hero/line.take.)", sound));
    CHECK(sound.path == "hero/line.take");
    CHECK(sound.speech);
    REQUIRE(pistoris::encodeCinSoundPath({"hero/line.take", true}, path));
    CHECK(path == R"(speech\\hero/line.take.)");
  }

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
    std::strcpy(ftl.texture_containers.emplace_back().filename, "graph/obj3d/texture");

    pistoris::fts::Data fts = makeMinimalFtsData();
    std::strcpy(fts.textures[1].fic, "graph/levels/texture");
    fts.scene.num_textures = 1;

    pistoris::amb::Data amb = makeAmbData();
    pistoris::tea::Data tea = makeTeaWithSample("sfx/footstep");
    pistoris::dlf::Data dlf = makeDlfWithPaths("graph/levels/level1", "graph/obj3d/interactive/items/key/key", "test");
    pistoris::Cin cin = makeCinData();
    cin.bitmaps.front().path = "graph/interface/illustrations/test";
    cin.sounds.push_back({"hero/line", true});

    LogCapture logs;
    writeAll(ftl, fts, amb, tea, dlf, cin);
    CHECK(logs.warnings.empty());
  }

  TEST_CASE("NativeWritersWarnForResourcesOutsideDefaultLooseRoots") {
    pistoris::ftl::Data ftl = makeData();
    std::strcpy(ftl.texture_containers.emplace_back().filename, "custom/model");

    pistoris::fts::Data fts = makeMinimalFtsData();
    std::strcpy(fts.textures[7].fic, "custom/level");
    fts.scene.num_textures = 1;

    pistoris::amb::Data amb = makeAmbData();
    amb.tracks.front().sample_path = "custom/ambiance.wav";
    pistoris::tea::Data tea = makeTeaWithSample("../custom/step");
    pistoris::dlf::Data dlf = makeDlfWithPaths("../custom/level", "obj3d/interactive/item", "../../custom/ambiance");
    pistoris::Cin cin = makeCinData();
    cin.bitmaps.front().path = "custom/illustration";
    cin.sounds.front().path = "../../custom/cinematic";

    LogCapture logs;
    writeAll(ftl, fts, amb, tea, dlf, cin);
    CHECK(logs.contains("FTL saving: texture[0]"));
    CHECK(logs.contains("FTS saving: texture 7"));
    CHECK(logs.contains("AMB saving: track[0]"));
    CHECK(logs.contains("TEA saving: keyframe[0]"));
    CHECK(logs.contains("DLF saving: scene path"));
    CHECK(logs.contains("DLF saving: entity[0]"));
    CHECK(logs.contains("DLF saving: zone[0] ambiance path"));
    CHECK(logs.contains("CIN saving: illustration[0]"));
    CHECK(logs.contains("CIN saving: sound[0]"));
  }
}
