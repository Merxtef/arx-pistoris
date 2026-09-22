// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#include "doctest/doctest.h"

#include "arx_pistoris/base/status.h"
#include "arx_pistoris/binary.hpp"
#include "arx_pistoris/paths.hpp"

#include "support/fixture_catalog.h"
#include "support/fixture_resources.h"

#include <cmath>
#include <cstdint>
#include <filesystem>
#include <string>
#include <unordered_set>
#include <vector>

namespace {

void checkFixturePath(const std::filesystem::path& path, bool required = true) {
  if (required) REQUIRE_FALSE(path.empty());
  if (path.empty()) return;
  CAPTURE(path.string());
  CHECK(std::filesystem::is_regular_file(path));
}

void checkGlbFixture(const test_support::GlbFixture& fixture, bool required = true) {
  checkFixturePath(fixture.path, required);
  if (fixture.empty()) return;
  CHECK(std::isfinite(fixture.arx_units_per_glb_unit));
  CHECK(fixture.arx_units_per_glb_unit > 0.0f);
}

template <class Fixture>
void checkUniqueFixtureIdentity(const std::vector<Fixture>& fixtures) {
  std::unordered_set<std::string> names;
  std::unordered_set<std::string> selectors;
  for (const Fixture& fixture : fixtures) {
    CAPTURE(fixture.name);
    CAPTURE(fixture.selector);
    CHECK_FALSE(fixture.name.empty());
    CHECK_FALSE(fixture.selector.empty());
    CHECK(names.insert(fixture.name).second);
    CHECK(selectors.insert(fixture.selector).second);
  }
}

}  // namespace

TEST_SUITE("fixture_catalog") {
  TEST_CASE("Level fixture set is present") { CHECK_FALSE(test_support::fixtureCatalog().levels.empty()); }

  TEST_CASE("Model fixture set is present") { CHECK_FALSE(test_support::fixtureCatalog().models.empty()); }

  TEST_CASE("Animation fixture set is present") { CHECK_FALSE(test_support::fixtureCatalog().animations.empty()); }

  TEST_CASE("Ambiance fixture set is present") { CHECK_FALSE(test_support::fixtureCatalog().ambiances.empty()); }

  TEST_CASE("Cinematic fixture set is present") { CHECK_FALSE(test_support::fixtureCatalog().cinematics.empty()); }

  TEST_CASE("Catalog paths exist") {
    const test_support::FixtureCatalog& catalog = test_support::fixtureCatalog();
    for (const test_support::LevelFixture& fixture : catalog.levels) {
      checkGlbFixture(fixture.glb);
      checkFixturePath(fixture.fts);
      checkFixturePath(fixture.llf);
      checkFixturePath(fixture.dlf);
    }
    bool has_model_glb = false;
    bool has_model_obj = false;
    for (const test_support::ModelFixture& fixture : catalog.models) {
      checkFixturePath(fixture.ftl);
      checkGlbFixture(fixture.glb, false);
      checkFixturePath(fixture.obj, false);
      const bool has_authoring_source = !fixture.glb.empty() || !fixture.obj.empty();
      CHECK(has_authoring_source);
      has_model_glb = has_model_glb || !fixture.glb.empty();
      has_model_obj = has_model_obj || !fixture.obj.empty();
    }
    CHECK(has_model_glb);
    CHECK(has_model_obj);
    for (const test_support::AnimationFixture& fixture : catalog.animations) checkFixturePath(fixture.tea);
    for (const test_support::AmbianceFixture& fixture : catalog.ambiances) {
      checkFixturePath(fixture.amb);
      checkGlbFixture(fixture.glb);
    }
    for (const test_support::CinematicFixture& fixture : catalog.cinematics) {
      checkFixturePath(fixture.glb);
      checkFixturePath(fixture.cin);
      std::unordered_set<std::string> languages;
      for (const std::string& language : fixture.audio.languages) {
        CHECK_FALSE(language.empty());
        CHECK(languages.insert(language).second);
      }
    }
    REQUIRE_FALSE(catalog.native_image_sidecars.empty());
    REQUIRE_FALSE(catalog.native_audio_sidecars.empty());
    std::unordered_set<std::string> native_sidecars;
    for (const std::filesystem::path& path : catalog.native_image_sidecars) {
      checkFixturePath(path);
      CHECK(test_support::isCommittedFixture(path));
      CHECK(native_sidecars.insert(path.generic_string()).second);
      CHECK(pistoris::binary::validateEncodedImage(test_support::readBytes(path)) == ARX_OK);
    }
    for (const std::filesystem::path& path : catalog.native_audio_sidecars) {
      checkFixturePath(path);
      CHECK(test_support::isCommittedFixture(path));
      CHECK(native_sidecars.insert(path.generic_string()).second);
      CHECK(pistoris::binary::validateEncodedAudio(test_support::readBytes(path)) == ARX_OK);
    }
    for (const test_support::JsonFixture& fixture : catalog.json) checkFixturePath(fixture.path);
  }

  TEST_CASE("Cataloged native aliases are byte-identical") {
    const test_support::FixtureCatalog& catalog = test_support::fixtureCatalog();
    std::unordered_set<std::string> alias_paths;
    for (const test_support::NativeAliasGroup& group : catalog.native_aliases) {
      CAPTURE(group.source.string());
      checkFixturePath(group.source);
      CHECK(test_support::isCommittedFixture(group.source));
      REQUIRE_FALSE(group.aliases.empty());
      const std::vector<std::uint8_t> source = test_support::readBytes(group.source);
      for (const std::filesystem::path& alias : group.aliases) {
        CAPTURE(alias.string());
        checkFixturePath(alias);
        CHECK(test_support::isCommittedFixture(alias));
        CHECK(alias != group.source);
        CHECK(alias_paths.insert(alias.generic_string()).second);
        CHECK(test_support::readBytes(alias) == source);
      }
    }
  }

  TEST_CASE("JSON fixture set covers every native carrier") {
    const std::unordered_set<std::string> expected_formats = {"amb", "dlf", "ftl", "fts", "llf", "tea"};
    std::unordered_set<std::string> formats;
    std::unordered_set<std::string> paths;
    for (const test_support::JsonFixture& fixture : test_support::fixtureCatalog().json) {
      CAPTURE(fixture.format);
      CAPTURE(fixture.path.string());
      CHECK(expected_formats.contains(fixture.format));
      formats.insert(fixture.format);
      CHECK(paths.insert(fixture.path.generic_string()).second);
    }
    CHECK(formats == expected_formats);
  }

  TEST_CASE("Catalog identities and references are coherent") {
    const test_support::FixtureCatalog& catalog = test_support::fixtureCatalog();
    checkUniqueFixtureIdentity(catalog.levels);
    checkUniqueFixtureIdentity(catalog.models);
    checkUniqueFixtureIdentity(catalog.animations);
    checkUniqueFixtureIdentity(catalog.ambiances);
    checkUniqueFixtureIdentity(catalog.cinematics);

    std::unordered_set<std::string> model_names;
    for (const test_support::ModelFixture& fixture : catalog.models) {
      model_names.insert(fixture.name);
      pistoris::paths::ModelPathView parsed;
      CHECK(pistoris::paths::modelFromSelector(fixture.selector, parsed));
    }
    for (const test_support::AnimationFixture& fixture : catalog.animations) {
      CAPTURE(fixture.name);
      pistoris::paths::AnimationPathView parsed;
      CHECK(pistoris::paths::animationFromSelector(fixture.selector, parsed));
      CHECK_FALSE(fixture.model.empty());
      CHECK(model_names.contains(fixture.model));
    }
    for (const test_support::AmbianceFixture& fixture : catalog.ambiances) {
      CAPTURE(fixture.name);
      pistoris::paths::AmbiancePathView parsed;
      CHECK(pistoris::paths::ambianceFromSelector(fixture.selector, parsed));
      const bool reference_exists = fixture.reference_model.empty() || model_names.contains(fixture.reference_model);
      CHECK(reference_exists);
    }
    for (const test_support::CinematicFixture& fixture : catalog.cinematics) {
      CAPTURE(fixture.name);
      pistoris::paths::CinematicPathView parsed;
      CHECK(pistoris::paths::cinematicFromSelector(fixture.selector, parsed));
    }

    for (const test_support::LevelFixture& fixture : catalog.levels) {
      std::uint32_t level = 0;
      CHECK(pistoris::paths::levelFromSelector(fixture.selector, level));
    }
  }
}
