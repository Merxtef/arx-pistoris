// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest/doctest.h"

#include "arx_pistoris/base/status.h"
#include "arx_pistoris/base/string_view.h"
#include "arx_pistoris/cinematic.hpp"
#include "arx_pistoris/cinematic/sound.hpp"
#include "arx_pistoris/cinematic/types.h"
#include "arx_pistoris/native.hpp"
#include "arx_pistoris/paths.hpp"
#include "arx_pistoris/sound.hpp"

#include "io/path_location.h"
#include "io/policy.h"
#include "io/service.h"
#include "resources/cinematic_sound_io.h"
#include "resources/sound_io.h"
#include "support/fixture_catalog.h"
#include "support/fixture_resources.h"
#include "support/native_equivalence.h"

#include <filesystem>
#include <string>
#include <unordered_set>
#include <vector>

namespace {

std::string string(ArxStringView value) { return {value.data, value.size}; }

cli::SoundInput fixtureSoundInput(const test_support::CinematicFixture& fixture) {
  return {.use_format_sources = true,
          .source_base = {.path = std::filesystem::absolute(fixture.glb).parent_path().string(),
                          .address = cli::PathAddress::kAbsolute}};
}

}  // namespace

TEST_SUITE("cli_cinematic_corpus") {
  TEST_CASE("GLB fixture audio resolves to committed CIN") {
    for (const test_support::CinematicFixture& fixture : test_support::fixtureCatalog().cinematics) {
      CAPTURE(fixture.name);
      CAPTURE(fixture.glb.string());

      pistoris::Cinematic cinematic;
      std::vector<pistoris::CinematicSoundSourceReference> sound_sources;
      REQUIRE(pistoris::Cinematic::importGlb(cinematic, test_support::readBytes(fixture.glb), &sound_sources) ==
              ARX_OK);

      cli::IoService io(cli::OverwriteMode::kAlwaysYes, false, {});
      REQUIRE(cli::prepareCinematicSounds(
          cinematic, io, fixtureSoundInput(fixture), sound_sources, cli::CinematicSoundSourceFormat::kGlb, true));
      REQUIRE(cinematic.validate() == ARX_OK);
      CHECK(cinematic.soundEncodingCount() == fixture.audio.encodings);

      REQUIRE(cinematic.languageCount() == fixture.audio.languages.size());
      std::vector<ArxCinematicLanguageView> language_views(cinematic.languageCount());
      REQUIRE(cinematic.copyLanguages(0, language_views.size(), language_views.data()) == ARX_OK);
      std::unordered_set<std::string> languages;
      for (const ArxCinematicLanguageView& language : language_views) languages.insert(string(language.name));
      const std::unordered_set<std::string> expected_languages(fixture.audio.languages.begin(),
                                                               fixture.audio.languages.end());
      CHECK(languages == expected_languages);

      REQUIRE(cinematic.rebaseTexturePaths(pistoris::paths::cinematicIllustrationDirectory()) == ARX_OK);
      REQUIRE(cinematic.rebaseSoundPaths(pistoris::SoundKind::kEffect, {}) == ARX_OK);
      REQUIRE(cinematic.rebaseSoundPaths(pistoris::SoundKind::kSpeech, {}) == ARX_OK);
      pistoris::Cin actual;
      REQUIRE(cinematic.bakeNative(actual) == ARX_OK);
      pistoris::Cin expected;
      REQUIRE(pistoris::readCin(test_support::readBytes(fixture.cin), expected) == ARX_OK);
      test_support::checkEquivalent(actual, expected, {.comparison_epsilon = 1.0e-4f});
    }
  }
}
