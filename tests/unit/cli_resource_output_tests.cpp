// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#include "doctest/doctest.h"

#include "io/path_location.h"
#include "resources/resource_output.h"

#include <array>
#include <cstdint>

namespace {

cli::PathLocation target(const char* path) { return {path, cli::PathAddress::kMountRelative}; }

}  // namespace

TEST_SUITE("CLI resource output") {
  TEST_CASE("Identical collisions select one payload") {
    constexpr std::array<std::uint8_t, 3> kData{1, 2, 3};
    cli::ResourceOutputPlan plan;
    const cli::ResourceAssetId first = plan.addAsset(cli::ResourceAssetKind::kAnimation, "anim:first");
    const cli::ResourceAssetId second = plan.addAsset(cli::ResourceAssetKind::kAnimation, "anim:second");
    plan.add(cli::ResourceFileKind::kAudio, target("sfx/step.wav"), kData.data(), kData.size(), first);
    plan.add(cli::ResourceFileKind::kAudio, target("SFX/STEP.WAV"), kData.data(), kData.size(), second);

    REQUIRE(plan.resolve(true, false));
    CHECK(plan.selectedCount() == 1);
  }

  TEST_CASE("Dry run reports differing collisions without selecting data") {
    constexpr std::array<std::uint8_t, 1> kFirst{1};
    constexpr std::array<std::uint8_t, 1> kSecond{2};
    cli::ResourceOutputPlan plan;
    const cli::ResourceAssetId first = plan.addAsset(cli::ResourceAssetKind::kAnimation, "anim:first");
    const cli::ResourceAssetId second = plan.addAsset(cli::ResourceAssetKind::kAnimation, "anim:second");
    const cli::ResourceAssetId third = plan.addAsset(cli::ResourceAssetKind::kAnimation, "anim:third");
    plan.add(cli::ResourceFileKind::kAudio, target("sfx/step.wav"), kFirst.data(), kFirst.size(), first);
    plan.add(cli::ResourceFileKind::kAudio, target("sfx/step.wav"), kFirst.data(), kFirst.size(), second);
    plan.add(cli::ResourceFileKind::kAudio, target("sfx/step.wav"), kSecond.data(), kSecond.size(), third);

    REQUIRE(plan.resolve(true, false));
    CHECK(plan.selectedCount() == 0);
  }

  TEST_CASE("Keep-first resolves differing collisions deterministically") {
    constexpr std::array<std::uint8_t, 1> kFirst{1};
    constexpr std::array<std::uint8_t, 1> kSecond{2};
    cli::ResourceOutputPlan plan;
    const cli::ResourceAssetId first = plan.addAsset(cli::ResourceAssetKind::kModel, "model:first");
    const cli::ResourceAssetId second = plan.addAsset(cli::ResourceAssetKind::kModel, "model:second");
    plan.add(cli::ResourceFileKind::kImage, target("textures/shared.png"), kFirst.data(), kFirst.size(), first);
    plan.add(cli::ResourceFileKind::kImage, target("textures/shared.png"), kSecond.data(), kSecond.size(), second);

    REQUIRE(plan.resolve(false, true));
    CHECK(plan.selectedCount() == 1);
  }

  TEST_CASE("Distinct targets remain independent") {
    constexpr std::array<std::uint8_t, 1> kData{1};
    cli::ResourceOutputPlan plan;
    const cli::ResourceAssetId asset = plan.addAsset(cli::ResourceAssetKind::kAmbiance, "ambiance:test");
    plan.add(cli::ResourceFileKind::kAudio, target("sfx/a.wav"), kData.data(), kData.size(), asset);
    plan.add(cli::ResourceFileKind::kAudio, target("sfx/b.wav"), kData.data(), kData.size(), asset);

    REQUIRE(plan.resolve(true, false));
    CHECK(plan.selectedCount() == 2);
  }

  TEST_CASE("Resource output cannot replace a reserved asset output") {
    constexpr std::array<std::uint8_t, 1> kData{1};
    cli::ResourceOutputPlan plan;
    const cli::ResourceAssetId asset = plan.addAsset(cli::ResourceAssetKind::kAnimation, "anim:test");
    plan.reserveOutput(target("asset.glb"));
    plan.add(cli::ResourceFileKind::kAudio, target("ASSET.GLB"), kData.data(), kData.size(), asset);

    CHECK_FALSE(plan.resolve(false, true));
    CHECK(plan.selectedCount() == 0);
  }

  TEST_CASE("Invalid asset references fail before collision resolution") {
    constexpr std::array<std::uint8_t, 1> kData{1};
    cli::ResourceOutputPlan plan;
    plan.add(cli::ResourceFileKind::kAudio, target("sfx/step.wav"), kData.data(), kData.size(), 0);

    CHECK_FALSE(plan.resolve(true, false));
    CHECK(plan.selectedCount() == 0);
  }

  TEST_CASE("Nonempty resources require payload data") {
    cli::ResourceOutputPlan plan;
    const cli::ResourceAssetId asset = plan.addAsset(cli::ResourceAssetKind::kAnimation, "anim:test");
    plan.add(cli::ResourceFileKind::kAudio, target("sfx/step.wav"), nullptr, 1, asset);

    CHECK_FALSE(plan.resolve(true, false));
    CHECK(plan.selectedCount() == 0);
  }
}
