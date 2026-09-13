// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#include "doctest/doctest.h"

#include "arx_pistoris/base/flags.h"
#include "arx_pistoris/base/indices.h"
#include "arx_pistoris/base/math.h"

#include "modules/geometry.h"
#include "modules/lights.h"

#include <array>
#include <limits>
#include <string>
#include <utility>

using namespace pistoris;

namespace {

Light makeLight() {
  Light light;
  light.name = "light";
  light.position = {1.0f, 2.0f, 3.0f};
  light.color = {0.25f, 0.5f, 0.75f};
  light.fallstart = 1.0f;
  light.fallend = 10.0f;
  light.intensity = 2.0f;
  return light;
}

LightingData makeLighting() {
  LightingData lighting;
  lighting.lights.push_back(makeLight());
  return lighting;
}

GeometryData makeGeometry() {
  GeometryData geometry;
  geometry.vertices = {{{0.0f, 0.0f, 0.0f}}, {{100.0f, 0.0f, 0.0f}}, {{0.0f, 0.0f, 100.0f}}};
  constexpr ArxVector3 kNormal = {0.0f, -1.0f, 0.0f};
  Face face;
  face.corners[0].vertex = 0;
  face.corners[0].normal = kNormal;
  face.corners[1].vertex = 1;
  face.corners[1].normal = kNormal;
  face.corners[2].vertex = 2;
  face.corners[2].normal = kNormal;
  geometry.faces.push_back(face);
  return geometry;
}

}  // namespace

TEST_SUITE("lights::validation") {
  TEST_CASE("Accepts valid lights") {
    LightingData lighting = makeLighting();
    CHECK(lights::validateLight(lighting.lights[0]) == lights::Error::kNone);
    CHECK(lights::validateLights(lighting) == lights::Error::kNone);
    CHECK(lights::validate(lighting) == lights::Error::kNone);
  }

  TEST_CASE("Accepts zero falloff range") {
    LightingData lighting = makeLighting();
    lighting.lights[0].fallstart = 0.0f;
    lighting.lights[0].fallend = 0.0f;
    lighting.lights[0].effect_size = 1.0f;

    CHECK(lights::validate(lighting) == lights::Error::kNone);
  }

  TEST_CASE("Rejects bad names") {
    LightingData lighting;
    lighting.lights.push_back({});
    CHECK(lights::validate(lighting) == lights::Error::kBadLightName);

    lighting = makeLighting();
    lighting.lights[0].name = "light__one";
    CHECK(lights::validateLight(lighting.lights[0]) == lights::Error::kBadLightName);
    CHECK(lights::validate(lighting) == lights::Error::kBadLightName);

    lighting = makeLighting();
    lighting.lights[0].name = std::string("light\0one", 9);
    CHECK(lights::validateLight(lighting.lights[0]) == lights::Error::kBadLightName);
    CHECK(lights::validate(lighting) == lights::Error::kBadLightName);
  }

  TEST_CASE("Rejects and repairs duplicate light names") {
    LightingData lighting = makeLighting();
    lighting.lights.push_back(lighting.lights.front());

    CHECK(lights::validateLights(lighting) == lights::Error::kDuplicateLightName);
    CHECK(lights::repairLightNames(lighting.lights) == 1);
    CHECK(lighting.lights[0].name == "light");
    CHECK(lighting.lights[1].name == "light_1");
    CHECK(lights::validateLights(lighting) == lights::Error::kNone);
  }

  TEST_CASE("Light candidates validate before publishing") {
    LightingData lighting;
    Light candidate = makeLight();
    REQUIRE(lights::validateLightCount(1) == lights::Error::kNone);
    lights::repairLightName(lighting, candidate);
    REQUIRE(lights::validateLight(candidate) == lights::Error::kNone);
    LightIndex index = lights::addLight(lighting, std::move(candidate));
    REQUIRE(index == 0);

    Light invalid = makeLight();
    invalid.intensity = -1.0f;
    CHECK(lights::validateLight(invalid) == lights::Error::kBadLightIntensity);
    CHECK(lighting.lights[index].intensity == doctest::Approx(2.0f));

    candidate = makeLight();
    lights::repairLightName(lighting, candidate);
    CHECK(candidate.name == "light_1");
    CHECK(lights::validateLight(candidate) == lights::Error::kNone);
    lights::removeLight(lighting, 0);
    CHECK(lighting.lights.empty());
  }

  TEST_CASE("Rejects bad positions") {
    LightingData lighting = makeLighting();
    lighting.lights[0].position.x = std::numeric_limits<float>::infinity();
    CHECK(lights::validate(lighting) == lights::Error::kBadLightPosition);
  }

  TEST_CASE("Rejects bad colors") {
    LightingData lighting = makeLighting();
    lighting.lights[0].color.r = -0.01f;
    CHECK(lights::validate(lighting) == lights::Error::kBadLightColor);

    lighting = makeLighting();
    lighting.lights[0].color.b = 1.01f;
    CHECK(lights::validate(lighting) == lights::Error::kBadLightColor);

    lighting = makeLighting();
    lighting.lights[0].color.g = std::numeric_limits<float>::quiet_NaN();
    CHECK(lights::validate(lighting) == lights::Error::kBadLightColor);
  }

  TEST_CASE("Rejects bad falloff") {
    LightingData lighting = makeLighting();
    lighting.lights[0].fallstart = -1.0f;
    CHECK(lights::validate(lighting) == lights::Error::kBadLightFalloff);

    lighting = makeLighting();
    lighting.lights[0].fallstart = 2.0f;
    lighting.lights[0].fallend = 1.0f;
    CHECK(lights::validate(lighting) == lights::Error::kBadLightFalloff);

    lighting = makeLighting();
    lighting.lights[0].fallstart = 1.0f;
    lighting.lights[0].fallend = 1.0f;
    CHECK(lights::validate(lighting) == lights::Error::kBadLightFalloff);

    lighting = makeLighting();
    lighting.lights[0].fallend = std::numeric_limits<float>::infinity();
    CHECK(lights::validate(lighting) == lights::Error::kBadLightFalloff);
  }

  TEST_CASE("Rejects bad intensity") {
    LightingData lighting = makeLighting();
    lighting.lights[0].intensity = -1.0f;
    CHECK(lights::validate(lighting) == lights::Error::kBadLightIntensity);

    lighting = makeLighting();
    lighting.lights[0].intensity = std::numeric_limits<float>::infinity();
    CHECK(lights::validate(lighting) == lights::Error::kBadLightIntensity);
  }

  TEST_CASE("Rejects bad effects") {
    LightingData lighting = makeLighting();
    lighting.lights[0].effect_speed = std::numeric_limits<float>::quiet_NaN();
    CHECK(lights::validate(lighting) == lights::Error::kBadLightEffect);

    lighting = makeLighting();
    lighting.lights[0].flicker.g = std::numeric_limits<float>::infinity();
    CHECK(lights::validate(lighting) == lights::Error::kBadLightEffect);
  }

  TEST_CASE("Rejects bad flags") {
    LightingData lighting = makeLighting();
    lighting.lights[0].flags = kLightFlagsAll | 0x1000U;
    CHECK(lights::validate(lighting) == lights::Error::kBadLightFlags);
  }

  TEST_CASE("Validate returns first light error") {
    LightingData lighting = makeLighting();
    lighting.lights.push_back(makeLight());
    lighting.lights[0].intensity = -1.0f;
    lighting.lights[1].name.clear();

    CHECK(lights::validate(lighting) == lights::Error::kBadLightIntensity);
  }

  TEST_CASE("Validates corner colors") {
    LightingData lighting = makeLighting();
    GeometryData geometry = makeGeometry();
    lighting.corner_colors = {{0.1f, 0.2f, 0.3f}, {0.4f, 0.5f, 0.6f}, {0.7f, 0.8f, 0.9f}};

    CHECK(lights::expectedCornerColorCount(geometry) == 3);
    CHECK(lights::cornerColorIndex(static_cast<FaceIndex>(2), 1) == 7);
    CHECK(lights::hasCornerColors(lighting));
    ArxColor3 color = lights::cornerColorOr(lighting, static_cast<FaceIndex>(0), 1, {});
    CHECK(color.r == doctest::Approx(0.4f));
    CHECK(color.g == doctest::Approx(0.5f));
    CHECK(color.b == doctest::Approx(0.6f));
    CHECK(lights::validateCornerColors(lighting) == lights::Error::kNone);
    CHECK(lights::validateCornerColors(lighting, geometry) == lights::Error::kNone);
    CHECK(lights::validate(lighting) == lights::Error::kNone);
    CHECK(lights::validate(lighting, geometry) == lights::Error::kNone);
  }

  TEST_CASE("Compacts corner colors in place") {
    LightingData lighting;
    lighting.corner_colors = {
        {0.1f, 0.0f, 0.0f},
        {0.2f, 0.0f, 0.0f},
        {0.3f, 0.0f, 0.0f},
        {0.4f, 0.0f, 0.0f},
        {0.5f, 0.0f, 0.0f},
        {0.6f, 0.0f, 0.0f},
        {0.7f, 0.0f, 0.0f},
        {0.8f, 0.0f, 0.0f},
        {0.9f, 0.0f, 0.0f},
    };
    const ArxColor3* storage = lighting.corner_colors.data();
    const std::array<FaceIndex, 3> remap = {kInvalidFaceIndex, 0, 1};

    lights::remapCornerColors(lighting, remap);

    REQUIRE(lighting.corner_colors.size() == 6);
    CHECK(lighting.corner_colors[0].r == doctest::Approx(0.4f));
    CHECK(lighting.corner_colors[5].r == doctest::Approx(0.9f));
    CHECK(lighting.corner_colors.data() == storage);
  }

  TEST_CASE("Falls back for invalid corner color face ids and corner indexes") {
    LightingData lighting = makeLighting();
    lighting.corner_colors = {{0.1f, 0.2f, 0.3f}, {0.4f, 0.5f, 0.6f}, {0.7f, 0.8f, 0.9f}};
    ArxColor3 fallback = {0.9f, 0.8f, 0.7f};

    ArxColor3 color = lights::cornerColorOr(lighting, kInvalidFaceIndex, 0, fallback);

    CHECK(color.r == doctest::Approx(fallback.r));
    CHECK(color.g == doctest::Approx(fallback.g));
    CHECK(color.b == doctest::Approx(fallback.b));
    CHECK(lights::cornerColorIndex(0, 3) == lights::kInvalidCornerColorIndex);

    color = lights::cornerColorOr(lighting, 0, 3, fallback);
    CHECK(color.r == doctest::Approx(fallback.r));
    CHECK(color.g == doctest::Approx(fallback.g));
    CHECK(color.b == doctest::Approx(fallback.b));
  }

  TEST_CASE("Rejects bad corner colors") {
    LightingData lighting = makeLighting();
    GeometryData geometry = makeGeometry();
    lighting.corner_colors = {{0.1f, 0.2f, 0.3f}, {0.4f, 1.1f, 0.6f}, {0.7f, 0.8f, 0.9f}};

    CHECK(lights::validateLights(lighting) == lights::Error::kNone);
    CHECK(lights::validateCornerColors(lighting) == lights::Error::kBadCornerColor);
    CHECK(lights::validate(lighting) == lights::Error::kBadCornerColor);
    CHECK(lights::validate(lighting, geometry) == lights::Error::kBadCornerColor);
  }

  TEST_CASE("Rejects stale corner color counts") {
    LightingData lighting = makeLighting();
    GeometryData geometry = makeGeometry();
    lighting.corner_colors = {{0.1f, 0.2f, 0.3f}};

    ArxColor3 fallback = {0.9f, 0.8f, 0.7f};
    ArxColor3 color = lights::cornerColorOr(lighting, static_cast<FaceIndex>(0), 1, fallback);
    CHECK(color.r == doctest::Approx(fallback.r));
    CHECK(color.g == doctest::Approx(fallback.g));
    CHECK(color.b == doctest::Approx(fallback.b));
    CHECK(lights::validateCornerColors(lighting) == lights::Error::kNone);
    CHECK(lights::validateCornerColors(lighting, geometry) == lights::Error::kBadCornerColorCount);
    CHECK(lights::validate(lighting) == lights::Error::kNone);
    CHECK(lights::validate(lighting, geometry) == lights::Error::kBadCornerColorCount);
  }

  TEST_CASE("Validates corner colors independently from light sources") {
    LightingData lighting = makeLighting();
    lighting.lights[0].name.clear();
    lighting.corner_colors = {{0.1f, 0.2f, 0.3f}};

    CHECK(lights::validateLights(lighting) == lights::Error::kBadLightName);
    CHECK(lights::validateCornerColors(lighting) == lights::Error::kNone);
  }
}
