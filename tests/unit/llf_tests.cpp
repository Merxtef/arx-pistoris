// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#include "doctest/doctest.h"

#include "arx_pistoris/flags.h"
#include "arx_pistoris/native/llf.hpp"
#include "arx_pistoris/pistoris_types.h"

#include "arx/llf.h"
#include "utils/cursor.h"

#include <cstdint>
#include <cstring>
#include <limits>
#include <vector>

namespace {

pistoris::llf::Light makeLight() {
  pistoris::llf::Light light;
  light.position = {1.0f, 2.0f, 3.0f};
  light.color = {0.25f, 0.5f, 0.75f};
  light.fallstart = 100.0f;
  light.fallend = 200.0f;
  light.intensity = 1.5f;
  light.flicker = {0.1f, 0.2f, 0.3f};
  light.effect_radius = 4.0f;
  light.effect_frequency = 0.25f;
  light.effect_size = 2.0f;
  light.effect_speed = 3.0f;
  light.flare_size = 80.0f;
  light.flags = pistoris::kLightFlagSemidynamic | pistoris::kLightFlagSpawnFire;
  return light;
}

pistoris::llf::Data makeData() {
  pistoris::llf::Data data;
  data.version = 9.25f;
  data.lights.push_back(makeLight());
  data.colors = {
      {1.0f, 0.5f, 0.0f},
      {0.0f, 0.25f, 1.0f},
  };
  return data;
}

ArxReturnCode load(const std::vector<std::uint8_t>& bytes, pistoris::llf::Data& out) {
  pistoris::ReadCursor cursor(bytes.data(), bytes.size());
  return pistoris::loadLlf(&out, cursor);
}

std::vector<std::uint8_t> save(const pistoris::llf::Data& data) {
  pistoris::WriteCursor cursor;
  REQUIRE(pistoris::saveLlf(&data, {}, cursor) == ARX_OK);
  return cursor.take();
}

template <typename T>
void overwrite(std::vector<std::uint8_t>& bytes, std::size_t offset, const T& value) {
  REQUIRE(offset + sizeof(T) <= bytes.size());
  std::memcpy(bytes.data() + offset, &value, sizeof(T));
}

}  // namespace

TEST_SUITE("llf") {
  TEST_CASE("LlfWriteReadRoundtrip") {
    pistoris::llf::Data source = makeData();
    std::vector<std::uint8_t> bytes = save(source);
    CHECK(bytes.size() == 7464 + 296 + 16 + 8);
    CHECK(std::memcmp(bytes.data() + 4, "DANAE_LLH_FILE\0\0", 16) == 0);

    pistoris::llf::Data loaded;
    REQUIRE(load(bytes, loaded) == ARX_OK);
    CHECK(loaded.version == source.version);
    REQUIRE(loaded.lights.size() == 1);
    CHECK(loaded.lights[0].position.x == source.lights[0].position.x);
    CHECK(loaded.lights[0].color.g == source.lights[0].color.g);
    CHECK(loaded.lights[0].fallstart == source.lights[0].fallstart);
    CHECK(loaded.lights[0].fallend == source.lights[0].fallend);
    CHECK(loaded.lights[0].flags == source.lights[0].flags);
    REQUIRE(loaded.colors.size() == 2);
    CHECK(loaded.colors[0].r == doctest::Approx(1.0f));
    CHECK(loaded.colors[0].g == doctest::Approx(128.0f / 255.0f));
    CHECK(loaded.colors[0].b == doctest::Approx(0.0f));
  }

  TEST_CASE("LlfUsesLittleEndianBgraAndIgnoresAlpha") {
    pistoris::llf::Data source;
    source.colors.push_back({1.0f, 0.5f, 0.0f});
    std::vector<std::uint8_t> bytes = save(source);
    constexpr std::size_t kColorOffset = 7464 + 16;
    REQUIRE(bytes.size() == kColorOffset + 4);
    CHECK(bytes[kColorOffset + 0] == 0);
    CHECK(bytes[kColorOffset + 1] == 128);
    CHECK(bytes[kColorOffset + 2] == 255);
    CHECK(bytes[kColorOffset + 3] == 255);

    bytes[kColorOffset + 3] = 7;
    pistoris::llf::Data loaded;
    REQUIRE(load(bytes, loaded) == ARX_OK);
    REQUIRE(loaded.colors.size() == 1);
    CHECK(loaded.colors[0].r == doctest::Approx(1.0f));
    CHECK(loaded.colors[0].g == doctest::Approx(128.0f / 255.0f));
    CHECK(loaded.colors[0].b == doctest::Approx(0.0f));
  }

  TEST_CASE("LlfReadRejectsContainerErrors") {
    pistoris::llf::Data data;
    CHECK(load({}, data) == ARX_UNEXPECTED_EOF);

    std::vector<std::uint8_t> bytes = save(makeData());
    bytes[4] = 'X';
    CHECK(load(bytes, data) == ARX_INVALID_IDENTIFIER);

    bytes = save(makeData());
    std::int32_t bad_count = -1;
    overwrite(bytes, 280, bad_count);
    CHECK(load(bytes, data) == ARX_LLF_BAD_LIGHT_COUNT);

    bytes = save(makeData());
    constexpr std::size_t kColorCountOffset = 7464 + 296;
    overwrite(bytes, kColorCountOffset, bad_count);
    CHECK(load(bytes, data) == ARX_LLF_BAD_BAKED_COLOR_COUNT);

    bytes = save(makeData());
    bytes.pop_back();
    CHECK(load(bytes, data) == ARX_UNEXPECTED_EOF);
  }

  TEST_CASE("LlfValidation") {
    pistoris::llf::Data data = makeData();
    CHECK(pistoris::validateLlf(nullptr) == ARX_INVALID_DATA_POINTER);
    CHECK(pistoris::validateLlf(&data) == ARX_OK);

    data.lights[0].position.x = std::numeric_limits<float>::infinity();
    CHECK(pistoris::validateLlf(&data) == ARX_LLF_BAD_LIGHT_POSITION);
    data = makeData();

    data.lights[0].color.r = 1.1f;
    CHECK(pistoris::validateLlf(&data) == ARX_LLF_BAD_LIGHT_COLOR);
    data = makeData();

    data.lights[0].fallend = data.lights[0].fallstart - 1.0f;
    CHECK(pistoris::validateLlf(&data) == ARX_LLF_BAD_LIGHT_FALLOFF);
    data = makeData();

    data.lights[0].fallend = data.lights[0].fallstart;
    CHECK(pistoris::validateLlf(&data) == ARX_OK);
    data = makeData();

    data.lights[0].fallstart = 0.0f;
    data.lights[0].fallend = 0.0f;
    CHECK(pistoris::validateLlf(&data) == ARX_OK);
    data = makeData();

    data.lights[0].intensity = -1.0f;
    CHECK(pistoris::validateLlf(&data) == ARX_LLF_BAD_LIGHT_INTENSITY);
    data = makeData();

    data.lights[0].effect_speed = std::numeric_limits<float>::quiet_NaN();
    CHECK(pistoris::validateLlf(&data) == ARX_LLF_BAD_LIGHT_EFFECT);
    data = makeData();

    data.lights[0].flags = pistoris::kLightFlagsAll | 0x1000U;
    CHECK(pistoris::validateLlf(&data) == ARX_LLF_BAD_LIGHT_FLAGS);
    data = makeData();

    data.colors[0].b = -0.01f;
    CHECK(pistoris::validateLlf(&data) == ARX_LLF_BAD_BAKED_COLOR);
  }
}
