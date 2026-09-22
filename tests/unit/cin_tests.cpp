// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#include "doctest/doctest.h"

#include "arx_pistoris/base/status.h"
#include "arx_pistoris/native/cin.hpp"

#include "cin_helpers.h"
#include "native/cin.h"
#include "utils/cursor.h"

#include <cstdint>
#include <cstring>
#include <limits>
#include <vector>

TEST_SUITE("cin") {
  TEST_CASE("Version 1.75 discards its inactive sound field") {
    const std::vector<std::uint8_t> bytes = makeCinBytes(makeCinData(), pistoris::kCinVersion175);
    pistoris::Cin result;
    pistoris::ReadCursor cursor(bytes.data(), bytes.size());
    REQUIRE(pistoris::loadCin(&result, cursor) == ARX_OK);
    REQUIRE(result.keyframes.size() == 2);
    CHECK(result.keyframes.front().sound == -1);
    CHECK(result.keyframes.back().sound == -1);
  }

  TEST_CASE("Version 1.76 selects only sound slot 3") {
    std::vector<std::uint8_t> bytes = makeCinBytes();
    const std::size_t slots = bytes.size() - 2U * 180U + 116U;
    const std::int32_t selected = -1;
    const std::int32_t ignored = 0;
    std::memcpy(bytes.data() + slots, &ignored, sizeof(ignored));
    std::memcpy(bytes.data() + slots + 3U * sizeof(std::int32_t), &selected, sizeof(selected));

    pistoris::Cin result;
    pistoris::ReadCursor cursor(bytes.data(), bytes.size());
    REQUIRE(pistoris::loadCin(&result, cursor) == ARX_OK);
    CHECK(result.keyframes.front().sound == -1);
  }

  TEST_CASE("Physical keys become the effective ordered timeline") {
    pistoris::Cin source = makeCinData();
    pistoris::cin::Keyframe late = makeCinKey(10);
    pistoris::cin::Keyframe discarded = makeCinKey(-1);
    pistoris::cin::Keyframe replaced = makeCinKey(0);
    replaced.camera_position.x = 1.0f;
    pistoris::cin::Keyframe retained = makeCinKey(0);
    retained.camera_position.x = 2.0f;
    retained.interpolation = 99;
    retained.crossfade = 128;
    source.keyframes = {late, discarded, replaced, retained};

    const std::vector<std::uint8_t> bytes = makeCinBytes(source);
    pistoris::Cin result;
    pistoris::ReadCursor cursor(bytes.data(), bytes.size());
    REQUIRE(pistoris::loadCin(&result, cursor) == ARX_OK);
    REQUIRE(result.keyframes.size() == 2);
    CHECK(result.keyframes[0].frame == 0);
    CHECK(result.keyframes[0].camera_position.x == 2.0f);
    CHECK(result.keyframes[0].interpolation == 1);
    CHECK(result.keyframes[0].crossfade == 0);
    CHECK(result.keyframes[1].frame == 10);
  }

  TEST_CASE("Terminal key may precede the declared end frame") {
    pistoris::Cin source = makeCinData();
    source.end_frame = 11;
    CHECK(pistoris::validateCin(&source) == ARX_OK);

    pistoris::WriteCursor cursor;
    REQUIRE(pistoris::saveCin(&source, cursor) == ARX_OK);
    const std::vector<std::uint8_t> bytes = cursor.take();
    std::int32_t version = 0;
    std::memcpy(&version, bytes.data() + 4U, sizeof(version));
    CHECK(version == pistoris::kCinVersion);
  }

  TEST_CASE("Inactive effect payloads are canonicalized on read and write") {
    pistoris::Cin source = makeCinData();
    const float nan = std::numeric_limits<float>::quiet_NaN();
    source.keyframes[0].flash_decay = nan;
    source.keyframes[0].light.position.x = nan;
    source.keyframes[0].light.intensity = nan;
    REQUIRE(pistoris::validateCin(&source) == ARX_OK);

    const std::vector<std::uint8_t> raw = makeCinBytes(source);
    pistoris::ReadCursor raw_cursor(raw.data(), raw.size());
    pistoris::Cin loaded;
    REQUIRE(pistoris::loadCin(&loaded, raw_cursor) == ARX_OK);
    CHECK(loaded.keyframes[0].flash_decay == 0.0f);
    CHECK(loaded.keyframes[0].light.position.x == 0.0f);
    CHECK(loaded.keyframes[0].light.intensity == -1.0f);

    pistoris::WriteCursor written;
    REQUIRE(pistoris::saveCin(&source, written) == ARX_OK);
    const std::vector<std::uint8_t> encoded = written.take();
    pistoris::ReadCursor encoded_cursor(encoded.data(), encoded.size());
    pistoris::Cin saved;
    REQUIRE(pistoris::loadCin(&saved, encoded_cursor) == ARX_OK);
    CHECK(saved.keyframes[0].flash_decay == 0.0f);
    CHECK(saved.keyframes[0].light.position.x == 0.0f);
    CHECK(saved.keyframes[0].light.intensity == -1.0f);
    CHECK(encoded == makeCinBytes(loaded));
  }

  TEST_CASE("Explicit light-off ignores its unused native light payload") {
    pistoris::Cin source = makeCinData();
    source.keyframes[0].effects = 1U << 24U;
    source.keyframes[0].light.intensity = -1.0f;
    source.keyframes[0].light.position.x = std::numeric_limits<float>::quiet_NaN();
    REQUIRE(pistoris::validateCin(&source) == ARX_OK);

    const std::vector<std::uint8_t> raw = makeCinBytes(source);
    pistoris::ReadCursor cursor(raw.data(), raw.size());
    pistoris::Cin loaded;
    REQUIRE(pistoris::loadCin(&loaded, cursor) == ARX_OK);
    CHECK(loaded.keyframes[0].effects == source.keyframes[0].effects);
    CHECK(loaded.keyframes[0].light.intensity == -1.0f);
    CHECK(loaded.keyframes[0].light.position.x == 0.0f);
  }

  TEST_CASE("Active effects, grid transforms, and stored speed remain finite-validated") {
    pistoris::Cin source = makeCinData();
    const float nan = std::numeric_limits<float>::quiet_NaN();
    SUBCASE("flash decay with active flash") {
      source.keyframes[0].effects = 1U << 16U;
      source.keyframes[0].flash_decay = nan;
    }
    SUBCASE("light intensity with active light cue") {
      source.keyframes[0].effects = 1U << 24U;
      source.keyframes[0].light.intensity = nan;
    }
    SUBCASE("light geometry with active light cue") {
      source.keyframes[0].effects = 1U << 24U;
      source.keyframes[0].light.intensity = 1.0f;
      source.keyframes[0].light.position.x = nan;
    }
    SUBCASE("illustration grid position") { source.keyframes[0].bitmap_position.x = nan; }
    SUBCASE("illustration grid roll") { source.keyframes[0].bitmap_roll = nan; }
    SUBCASE("terminal outgoing speed") { source.keyframes[1].outgoing_speed = nan; }
    CHECK(pistoris::validateCin(&source) == ARX_CIN_BAD_KEY_TRANSFORM);
  }

  TEST_CASE("Rejects renderer grid and derived timing limits") {
    pistoris::Cin source = makeCinData();
    source.bitmaps[0].subdivision_scale = 200;
    CHECK(pistoris::validateCin(&source) == ARX_CIN_BAD_BITMAP_SCALE);

    source.bitmaps[0].subdivision_scale = 64;
    source.keyframes[0].effects = 1U << 8U;
    CHECK(pistoris::validateCin(&source) == ARX_CIN_BAD_BITMAP_SCALE);

    source.bitmaps[0].subdivision_scale = 2;
    source.keyframes[0].effects = 0;
    source.fps = 1.0e30f;
    source.keyframes[0].outgoing_speed = 1.0e30f;
    CHECK(pistoris::validateCin(&source) == ARX_CIN_BAD_KEY_TIMING);
    source.fps = 25.0f;
    source.keyframes[0].outgoing_speed = 1.0f;
    CHECK(pistoris::validateCin(&source) == ARX_OK);
  }

  TEST_CASE("Rejects a Dream crossfade into an oversized next grid") {
    pistoris::Cin source = makeCinData();
    source.bitmaps.push_back({64, "graph/interface/illustrations/next"});
    source.keyframes[0].effects = 1U << 8U;
    source.keyframes[0].crossfade = 1;
    source.keyframes[1].bitmap = 1;
    CHECK(pistoris::validateCin(&source) == ARX_CIN_BAD_BITMAP_SCALE);

    source.keyframes[0].crossfade = 0;
    CHECK(pistoris::validateCin(&source) == ARX_OK);
  }
}
