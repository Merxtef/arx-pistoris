// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#include "doctest/doctest.h"

#include "arx_pistoris/arx_math.h"
#include "arx_pistoris/flags.h"
#include "arx_pistoris/indices.h"

#include "modules/geometry.h"
#include "modules/lights.h"

#include <cmath>
#include <cstdint>

using namespace pistoris;

namespace {

Face makeFace(std::uint32_t a, std::uint32_t b, std::uint32_t c, const ArxVector3& normal = {0.0f, -1.0f, 0.0f}) {
  return {{{{a, normal, 0.0f, 0.0f}, {b, normal, 0.0f, 0.0f}, {c, normal, 0.0f, 0.0f}}}, kNoTexture, 0, 0.0f};
}

Light makeLight(ArxVector3 position, ArxColor3 color = {1.0f, 1.0f, 1.0f}) {
  Light light;
  light.name = "light";
  light.position = position;
  light.color = color;
  light.fallstart = 1000.0f;
  light.fallend = 2000.0f;
  light.intensity = 1.0f;
  return light;
}

GeometryData makeTriangleGeometry() {
  GeometryData geometry;
  geometry.vertices = {{{0.0f, 0.0f, 0.0f}}, {{100.0f, 0.0f, 0.0f}}, {{0.0f, 0.0f, 100.0f}}};
  geometry.faces.push_back(makeFace(0, 1, 2));
  return geometry;
}

lights::Error generateStaticLighting(LightingData& lighting, const GeometryData& geometry,
                                     const lights::StaticLightingGenOptions& options = {},
                                     lights::StaticLightingDiagnostics* diagnostics = nullptr) {
  return lights::generateStaticLighting(lighting.corner_colors, geometry, lighting.lights, options, diagnostics);
}

void addShadowBlocker(GeometryData& geometry) {
  const std::uint32_t base = static_cast<std::uint32_t>(geometry.vertices.size());
  geometry.vertices.push_back({{-20.0f, -50.0f, -20.0f}});
  geometry.vertices.push_back({{20.0f, -50.0f, -20.0f}});
  geometry.vertices.push_back({{0.0f, -50.0f, 20.0f}});
  geometry.faces.push_back(makeFace(base + 0, base + 1, base + 2));
}

void addCornerTouchingBlocker(GeometryData& geometry) {
  const std::uint32_t base = static_cast<std::uint32_t>(geometry.vertices.size());
  geometry.vertices.push_back({{0.0f, 0.0f, 0.0f}});
  geometry.vertices.push_back({{20.0f, -50.0f, -20.0f}});
  geometry.vertices.push_back({{-20.0f, -50.0f, -20.0f}});
  geometry.faces.push_back(makeFace(base + 0, base + 1, base + 2));
}

}  // namespace

TEST_SUITE("lights::static_generation") {
  TEST_CASE("Uses ambient when there are no static lights") {
    LightingData lighting;
    GeometryData geometry = makeTriangleGeometry();

    REQUIRE(generateStaticLighting(lighting, geometry) == lights::Error::kNone);

    REQUIRE(lighting.corner_colors.size() == 3);
    for (const ArxColor3& color : lighting.corner_colors) {
      CHECK(color.r == doctest::Approx(0.25f));
      CHECK(color.g == doctest::Approx(0.25f));
      CHECK(color.b == doctest::Approx(0.25f));
    }
  }

  TEST_CASE("Applies lights and reports diagnostics") {
    LightingData lighting;
    GeometryData geometry = makeTriangleGeometry();
    lighting.lights.push_back(makeLight({0.0f, -100.0f, 0.0f}));
    lighting.lights.push_back(makeLight({0.0f, -100.0f, 0.0f}, {0.0f, 1.0f, 0.0f}));
    lighting.lights.back().flags = kLightFlagSemidynamic;
    lights::StaticLightingDiagnostics diagnostics;

    REQUIRE(generateStaticLighting(lighting, geometry, {.use_normals = false}, &diagnostics) == lights::Error::kNone);

    REQUIRE(lighting.corner_colors.size() == 3);
    for (const ArxColor3& color : lighting.corner_colors) {
      CHECK(color.r == doctest::Approx(0.85f));
      CHECK(color.g == doctest::Approx(0.85f));
      CHECK(color.b == doctest::Approx(0.85f));
    }
    CHECK(diagnostics.generated_corners == 3);
    CHECK(diagnostics.skipped_lights == 1);
    CHECK(diagnostics.contributing_light_corners == 3);
  }

  TEST_CASE("Applies linear falloff") {
    LightingData lighting;
    GeometryData geometry = makeTriangleGeometry();
    lighting.lights.push_back(makeLight({0.0f, -100.0f, 0.0f}));
    lighting.lights[0].fallstart = 0.0f;
    lighting.lights[0].fallend = 200.0f;

    REQUIRE(
        generateStaticLighting(
            lighting,
            geometry,
            {.ambient_color = {0.0f, 0.0f, 0.0f}, .global_factor = 1.0f, .use_normals = false, .use_shadows = false}) ==
        lights::Error::kNone);

    CHECK(lighting.corner_colors[0].r == doctest::Approx(0.5f));
    CHECK(lighting.corner_colors[1].r == doctest::Approx(1.0f - std::sqrt(20000.0f) / 200.0f));
    CHECK(lighting.corner_colors[2].r == doctest::Approx(1.0f - std::sqrt(20000.0f) / 200.0f));
  }

  TEST_CASE("Blocks shadow rays and honors no casted lights") {
    LightingData lighting;
    lighting.lights.push_back(makeLight({0.0f, -100.0f, 0.0f}));

    GeometryData shadowed = makeTriangleGeometry();
    addShadowBlocker(shadowed);
    REQUIRE(generateStaticLighting(lighting, shadowed, {.use_normals = false}) == lights::Error::kNone);
    CHECK(lighting.corner_colors[0].r == doctest::Approx(0.25f));

    GeometryData no_casted = makeTriangleGeometry();
    addShadowBlocker(no_casted);
    lighting.lights[0].flags = kLightFlagNoCasted;
    REQUIRE(generateStaticLighting(lighting, no_casted, {.use_normals = false}) == lights::Error::kNone);
    CHECK(lighting.corner_colors[0].r == doctest::Approx(0.85f));
  }

  TEST_CASE("Ignores shadow intersections at shaded corner endpoint") {
    LightingData lighting;
    lighting.lights.push_back(makeLight({0.0f, -100.0f, 0.0f}));

    GeometryData geometry = makeTriangleGeometry();
    addCornerTouchingBlocker(geometry);

    REQUIRE(generateStaticLighting(lighting, geometry, {.use_normals = false}) == lights::Error::kNone);

    CHECK(lighting.corner_colors[0].r == doctest::Approx(0.85f));
  }

  TEST_CASE("Clamps generated output to ambient and one") {
    LightingData lighting;
    GeometryData geometry = makeTriangleGeometry();
    lighting.lights.push_back(makeLight({0.0f, -100.0f, 0.0f}, {1.0f, 0.5f, 0.25f}));
    lighting.lights[0].intensity = 10.0f;

    REQUIRE(
        generateStaticLighting(
            lighting,
            geometry,
            {.ambient_color = {0.0f, 0.0f, 0.0f}, .global_factor = 1.0f, .use_normals = false, .use_shadows = false}) ==
        lights::Error::kNone);

    CHECK(lighting.corner_colors[0].r == doctest::Approx(1.0f));
    CHECK(lighting.corner_colors[0].g == doctest::Approx(1.0f));
    CHECK(lighting.corner_colors[0].b == doctest::Approx(1.0f));
  }

  TEST_CASE("Rejects invalid inputs") {
    LightingData lighting;
    GeometryData geometry = makeTriangleGeometry();

    CHECK(generateStaticLighting(lighting, geometry, {.ambient_color = {-0.1f, 0.25f, 0.25f}}) ==
          lights::Error::kInvalidOptions);
    CHECK(generateStaticLighting(lighting, geometry, {.global_factor = -1.0f}) == lights::Error::kInvalidOptions);
  }
}
