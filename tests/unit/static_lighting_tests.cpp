// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#include "doctest/doctest.h"

#include "arx_pistoris/indices.h"
#include "arx_pistoris/pistoris.hpp"

#include "arx/conversion/level/api.h"
#include "level/data.h"
#include "level/validation.h"
#include "modules/geometry.h"
#include "modules/lights.h"

#include <cmath>
#include <cstddef>
#include <cstdint>
#include <utility>
#include <vector>

namespace {

using pistoris::ArxColor3;
using pistoris::ArxVector3;

pistoris::Face makeFace(std::uint32_t a, std::uint32_t b, std::uint32_t c,
                        const ArxVector3& normal = {0.0f, -1.0f, 0.0f}) {
  return {{{{a, normal, 0.0f, 0.0f}, {b, normal, 0.0f, 0.0f}, {c, normal, 0.0f, 0.0f}}}, pistoris::kNoTexture, 0, 0.0f};
}

pistoris::Light makeLight(ArxVector3 position, ArxColor3 color = {1.0f, 1.0f, 1.0f}) {
  pistoris::Light light;
  light.name = "light";
  light.position = position;
  light.color = color;
  light.fallstart = 1000.0f;
  light.fallend = 2000.0f;
  light.intensity = 1.0f;
  return light;
}

pistoris::LevelModules makeTriangleLevel() {
  pistoris::LevelModules level;
  level.geometry.vertices = {{{0.0f, 0.0f, 0.0f}}, {{100.0f, 0.0f, 0.0f}}, {{0.0f, 0.0f, 100.0f}}};
  level.geometry.faces.push_back(makeFace(0, 1, 2));
  level.rooms.face_rooms.push_back(0);
  return level;
}

pistoris::LevelModules makeBakeableTriangleLevel() {
  pistoris::LevelModules level = makeTriangleLevel();
  level.rooms.definitions.push_back({"room"});
  level.scene.player_spawn = pistoris::PlayerSpawn{{0.0f, 0.0f, 0.0f}, {}};
  return level;
}

void addShadowBlocker(pistoris::LevelModules& level) {
  const std::uint32_t base = static_cast<std::uint32_t>(level.geometry.vertices.size());
  level.geometry.vertices.push_back({{0.0f, -50.0f, 0.0f}});
  level.geometry.vertices.push_back({{20.0f, -50.0f, 0.0f}});
  level.geometry.vertices.push_back({{0.0f, -50.0f, 20.0f}});
  level.geometry.faces.push_back(makeFace(base + 0, base + 1, base + 2));
  level.rooms.face_rooms.push_back(0);
}

const pistoris::ArxColor3& bakedColor(const pistoris::LevelModules& level, std::size_t face, std::size_t corner) {
  return level.lighting.corner_colors[face * 3U + corner];
}

pistoris::lights::StaticLightingGenOptions toModuleOptions(const pistoris::Level::StaticLightingGenOptions& options) {
  return {
      .ambient_color = options.ambient_color,
      .global_factor = options.global_factor,
      .use_normals = options.use_normals,
      .use_shadows = options.use_shadows,
  };
}

ArxReturnCode generateStaticLighting(pistoris::LevelModules& level,
                                     const pistoris::Level::StaticLightingGenOptions& options = {},
                                     pistoris::lights::StaticLightingDiagnostics* diagnostics = nullptr) {
  ArxReturnCode rc = pistoris::level_validation::geometryError(pistoris::geometry::validate(level.geometry));
  if (rc != ARX_OK) return rc;
  rc = pistoris::level_validation::lightingError(pistoris::lights::validateLightSources(level.lighting));
  if (rc != ARX_OK) return rc;
  std::vector<pistoris::ArxColor3> corner_colors;
  rc = pistoris::level_validation::lightingError(pistoris::lights::generateStaticLighting(
      corner_colors, level.geometry, level.lighting.lights, toModuleOptions(options), diagnostics));
  if (rc != ARX_OK) return rc;
  level.lighting.corner_colors = std::move(corner_colors);
  return ARX_OK;
}

}  // namespace

TEST_CASE("StaticLightingGenerationUsesAmbientWhenThereAreNoStaticLights") {
  pistoris::LevelModules level = makeTriangleLevel();

  REQUIRE(generateStaticLighting(level) == ARX_OK);

  REQUIRE(level.lighting.corner_colors.size() == 3);
  for (const pistoris::ArxColor3& color : level.lighting.corner_colors) {
    CHECK(color.r == doctest::Approx(0.25f));
    CHECK(color.g == doctest::Approx(0.25f));
    CHECK(color.b == doctest::Approx(0.25f));
  }
}

TEST_CASE("StaticLightingGenerationAppliesGlobalFactorAndSkipsSemidynamicLights") {
  pistoris::LevelModules level = makeTriangleLevel();
  level.lighting.lights.push_back(makeLight({0.0f, -100.0f, 0.0f}));
  level.lighting.lights.push_back(makeLight({0.0f, -100.0f, 0.0f}, {0.0f, 1.0f, 0.0f}));
  level.lighting.lights.back().name = "semidynamic";
  level.lighting.lights.back().flags = pistoris::kLightFlagSemidynamic;
  pistoris::lights::StaticLightingDiagnostics diagnostics;

  REQUIRE(generateStaticLighting(level, {.use_normals = false}, &diagnostics) == ARX_OK);

  REQUIRE(level.lighting.corner_colors.size() == 3);
  for (const pistoris::ArxColor3& color : level.lighting.corner_colors) {
    CHECK(color.r == doctest::Approx(0.85f));
    CHECK(color.g == doctest::Approx(0.85f));
    CHECK(color.b == doctest::Approx(0.85f));
  }
  CHECK(diagnostics.generated_corners == 3);
  CHECK(diagnostics.skipped_lights == 1);
}

TEST_CASE("StaticLightingGenerationAppliesLinearFalloffAndStopsAtFallend") {
  pistoris::LevelModules level = makeTriangleLevel();
  level.lighting.lights.push_back(makeLight({0.0f, -100.0f, 0.0f}));
  level.lighting.lights[0].fallstart = 0.0f;
  level.lighting.lights[0].fallend = 200.0f;

  REQUIRE(
      generateStaticLighting(
          level,
          {.ambient_color = {0.0f, 0.0f, 0.0f}, .global_factor = 1.0f, .use_normals = false, .use_shadows = false}) ==
      ARX_OK);

  CHECK(bakedColor(level, 0, 0).r == doctest::Approx(0.5f));
  CHECK(bakedColor(level, 0, 1).r == doctest::Approx(1.0f - std::sqrt(20000.0f) / 200.0f));
  CHECK(bakedColor(level, 0, 2).r == doctest::Approx(1.0f - std::sqrt(20000.0f) / 200.0f));

  pistoris::LevelModules outside = makeTriangleLevel();
  outside.lighting.lights.push_back(makeLight({0.0f, -100.0f, 0.0f}));
  outside.lighting.lights[0].fallstart = 0.0f;
  outside.lighting.lights[0].fallend = 100.0f;

  REQUIRE(
      generateStaticLighting(
          outside,
          {.ambient_color = {0.1f, 0.2f, 0.3f}, .global_factor = 1.0f, .use_normals = false, .use_shadows = false}) ==
      ARX_OK);
  CHECK(bakedColor(outside, 0, 0).r == doctest::Approx(0.1f));
  CHECK(bakedColor(outside, 0, 1).g == doctest::Approx(0.2f));
  CHECK(bakedColor(outside, 0, 2).b == doctest::Approx(0.3f));
}

TEST_CASE("StaticLightingGenerationUsesNormalResponseWhenEnabled") {
  pistoris::LevelModules level = makeTriangleLevel();
  level.lighting.lights.push_back(makeLight({0.0f, -100.0f, 0.0f}));

  REQUIRE(generateStaticLighting(level) == ARX_OK);

  CHECK(bakedColor(level, 0, 0).r == doctest::Approx(0.85f));
  CHECK(bakedColor(level, 0, 1).r < 0.85f);
  CHECK(bakedColor(level, 0, 1).r > 0.25f);
}

TEST_CASE("StaticLightingGenerationKeepsBackfacesAmbientUnlessNormalsAreDisabled") {
  pistoris::LevelModules with_normals = makeTriangleLevel();
  with_normals.geometry.faces[0] = makeFace(0, 1, 2, {0.0f, 1.0f, 0.0f});
  with_normals.lighting.lights.push_back(makeLight({0.0f, -100.0f, 0.0f}));

  REQUIRE(generateStaticLighting(with_normals) == ARX_OK);
  CHECK(bakedColor(with_normals, 0, 0).r == doctest::Approx(0.25f));

  pistoris::LevelModules without_normals = makeTriangleLevel();
  without_normals.geometry.faces[0] = makeFace(0, 1, 2, {0.0f, 1.0f, 0.0f});
  without_normals.lighting.lights.push_back(makeLight({0.0f, -100.0f, 0.0f}));

  REQUIRE(generateStaticLighting(without_normals, {.use_normals = false}) == ARX_OK);
  CHECK(bakedColor(without_normals, 0, 0).r == doctest::Approx(0.85f));
}

TEST_CASE("StaticLightingGenerationBlocksShadowCastingLightAndHonorsNoCasted") {
  pistoris::LevelModules shadowed = makeTriangleLevel();
  addShadowBlocker(shadowed);
  shadowed.lighting.lights.push_back(makeLight({0.0f, -100.0f, 0.0f}));

  REQUIRE(generateStaticLighting(shadowed, {.use_normals = false}) == ARX_OK);
  CHECK(bakedColor(shadowed, 0, 0).r == doctest::Approx(0.25f));

  pistoris::LevelModules no_casted = makeTriangleLevel();
  addShadowBlocker(no_casted);
  no_casted.lighting.lights.push_back(makeLight({0.0f, -100.0f, 0.0f}));
  no_casted.lighting.lights.back().flags = pistoris::kLightFlagNoCasted;

  REQUIRE(generateStaticLighting(no_casted, {.use_normals = false}) == ARX_OK);
  CHECK(bakedColor(no_casted, 0, 0).r == doctest::Approx(0.85f));
}

TEST_CASE("StaticLightingGenerationReplacesExistingColorsAndClampsGeneratedOutput") {
  pistoris::LevelModules level = makeTriangleLevel();
  level.lighting.corner_colors = {{0.9f, 0.1f, 0.4f}, {0.9f, 0.1f, 0.4f}, {0.9f, 0.1f, 0.4f}};
  level.lighting.lights.push_back(makeLight({0.0f, -100.0f, 0.0f}, {1.0f, 0.5f, 0.25f}));
  level.lighting.lights[0].intensity = 10.0f;

  REQUIRE(
      generateStaticLighting(
          level,
          {.ambient_color = {0.0f, 0.0f, 0.0f}, .global_factor = 1.0f, .use_normals = false, .use_shadows = false}) ==
      ARX_OK);

  CHECK(bakedColor(level, 0, 0).r == doctest::Approx(1.0f));
  CHECK(bakedColor(level, 0, 0).g == doctest::Approx(1.0f));
  CHECK(bakedColor(level, 0, 0).b == doctest::Approx(1.0f));
}

TEST_CASE("StaticLightingGenerationProducesColorsConsumedByNativeBake") {
  pistoris::LevelModules level = makeBakeableTriangleLevel();
  level.lighting.lights.push_back(makeLight({0.0f, -100.0f, 0.0f}, {1.0f, 0.0f, 0.0f}));

  REQUIRE(generateStaticLighting(level, {.use_normals = false, .use_shadows = false}) == ARX_OK);

  pistoris::NativeLevelBundle bundle;
  REQUIRE(pistoris::arx_level_conversion::bakeNativeLevelBundle(
              level, {.level_name = "output", .texture_folder = ""}, bundle) == ARX_OK);

  REQUIRE(bundle.llf.colors.size() == 3);
  CHECK(bundle.llf.colors[0].r == doctest::Approx(bakedColor(level, 0, 0).r));
  CHECK(bundle.llf.colors[0].g == doctest::Approx(bakedColor(level, 0, 0).g));
  CHECK(bundle.llf.colors[0].b == doctest::Approx(bakedColor(level, 0, 0).b));
}

TEST_CASE("StaticLightingGenerationRejectsInvalidOptions") {
  pistoris::LevelModules level = makeTriangleLevel();

  CHECK(generateStaticLighting(level, {.ambient_color = {-0.1f, 0.25f, 0.25f}}) == ARX_INVALID_OPTIONS);
  CHECK(generateStaticLighting(level, {.global_factor = -1.0f}) == ARX_INVALID_OPTIONS);
}

TEST_CASE("StaticLightingGenerationRejectsNonzeroEqualFalloff") {
  pistoris::LevelModules level = makeTriangleLevel();
  level.lighting.lights.push_back(makeLight({0.0f, -100.0f, 0.0f}));
  level.lighting.lights[0].fallstart = 10.0f;
  level.lighting.lights[0].fallend = 10.0f;

  CHECK(generateStaticLighting(level) == ARX_LEVEL_BAD_LIGHT_FALLOFF);
}
