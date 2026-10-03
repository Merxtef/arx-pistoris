// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#include "doctest/doctest.h"

#include "arx_pistoris/animation.hpp"
#include "arx_pistoris/animation/bake.hpp"
#include "arx_pistoris/native.hpp"
#include "arx_pistoris/sound.hpp"

#include "support/animation_equivalence.h"
#include "support/corpus_checks.h"
#include "support/corpus_files.h"
#include "support/fixture_resources.h"

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <optional>
#include <span>
#include <utility>
#include <vector>

TEST_SUITE("animation_corpus") {
  TEST_CASE("Native TEA converts through Animation") {
    std::size_t fixture_hydrations = 0;
    for (const std::filesystem::path& path : test_support::nativeCorpusFiles(test_support::NativeCorpusFormat::kTea)) {
      CAPTURE(path.string());

      std::vector<std::uint8_t> source_bytes;
      if (!test_support::readCorpusBytes(path, source_bytes)) continue;
      auto native_result = pistoris::readTea(source_bytes);
      if (!test_support::checkCorpusStatus(path, "read TEA", native_result)) continue;
      pistoris::Tea native = std::move(*native_result);
      std::vector<pistoris::SoundSourceReference> sound_sources;
      auto imported = pistoris::Animation::importNative(native, &sound_sources);
      if (!test_support::checkCorpusStatus(path, "import TEA into Animation", imported)) continue;
      pistoris::Animation animation = std::move(*imported);
      if (!test_support::checkCorpusStatus(path, "validate Animation", animation.validate())) continue;
      const std::optional<test_support::HydrationResult> hydration = test_support::hydrateSounds(
          animation, test_support::nativeMount(path), sound_sources, test_support::SoundSourceLayout::kNativeAnimation);
      if (!hydration) continue;
      if (test_support::isCommittedFixture(path)) fixture_hydrations += hydration->hydrated;

      auto baked_result = animation.bakeNativeBundle({.include_sound_files = true});
      if (!test_support::checkCorpusStatus(path, "bake Animation to native bundle", baked_result)) continue;
      pistoris::NativeAnimationBundle baked = std::move(*baked_result);
      if (!test_support::validateSoundFiles(animation, std::span<const pistoris::SoundFile>(baked.sound_files)))
        continue;

      std::vector<pistoris::SoundSourceReference> roundtrip_sources;
      auto roundtrip_result = pistoris::Animation::importNative(baked.tea, &roundtrip_sources);
      if (!test_support::checkCorpusStatus(path, "import baked TEA into Animation", roundtrip_result)) continue;
      pistoris::Animation roundtrip = std::move(*roundtrip_result);
      const std::optional<test_support::HydrationResult> roundtrip_hydration =
          test_support::hydrateSoundsFromFiles(roundtrip,
                                               roundtrip_sources,
                                               std::span<const pistoris::SoundFile>(baked.sound_files),
                                               test_support::SoundSourceLayout::kNativeAnimation);
      if (!roundtrip_hydration) continue;
      if (!test_support::checkCorpusStatus(path, "validate roundtrip Animation", roundtrip.validate())) continue;
      CHECK(roundtrip_hydration->hydrated >= hydration->hydrated);
      test_support::AnimationEquivalenceOptions equivalence;
      equivalence.sounds.compare_paths = false;
      equivalence.sounds.compare_encoded_audio = false;
      test_support::checkAnimationsEquivalent(animation, roundtrip, equivalence);
    }
    CHECK(fixture_hydrations > 0);
  }
}
