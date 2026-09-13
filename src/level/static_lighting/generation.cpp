// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#include "arx_pistoris/base/status.h"
#include "arx_pistoris/debug/level.hpp"
#include "arx_pistoris/debug/level/diagnostics.hpp"
#include "arx_pistoris/level.hpp"

#include "api/status_boundary.h"
#include "level/data.h"
#include "level/debug/access.h"
#include "level/debug/diagnostics.h"
#include "level/validation.h"
#include "modules/lights.h"

#include <utility>
#include <vector>

namespace pistoris {
namespace {

static_assert(Level::StaticLightingGenOptions{}.ambient_color.r ==
              lights::StaticLightingGenerationOptions{}.ambient_color.r);
static_assert(Level::StaticLightingGenOptions{}.ambient_color.g ==
              lights::StaticLightingGenerationOptions{}.ambient_color.g);
static_assert(Level::StaticLightingGenOptions{}.ambient_color.b ==
              lights::StaticLightingGenerationOptions{}.ambient_color.b);
static_assert(Level::StaticLightingGenOptions{}.global_factor ==
              lights::StaticLightingGenerationOptions{}.global_factor);
static_assert(Level::StaticLightingGenOptions{}.use_normals == lights::StaticLightingGenerationOptions{}.use_normals);
static_assert(Level::StaticLightingGenOptions{}.use_shadows == lights::StaticLightingGenerationOptions{}.use_shadows);

lights::StaticLightingGenerationOptions toModuleOptions(const Level::StaticLightingGenOptions& options) {
  return {
      .ambient_color = options.ambient_color,
      .global_factor = options.global_factor,
      .use_normals = options.use_normals,
      .use_shadows = options.use_shadows,
  };
}

ArxReturnCode generateStaticLightingImpl(LevelModules& modules, LevelValidationState& validation,
                                         const Level::StaticLightingGenOptions& options,
                                         lights::StaticLightingDiagnostics* diagnostics) {
  ArxReturnCode rc = level_validation::faces(modules, validation);
  if (rc != ARX_OK) return rc;
  rc = level_validation::lightSources(modules, validation);
  if (rc != ARX_OK) return rc;

  std::vector<ArxColor3> corner_colors;
  rc = level_validation::lightingError(lights::generateStaticLighting(
      corner_colors, modules.geometry, modules.lighting.lights, toModuleOptions(options), diagnostics));
  if (rc != ARX_OK) return rc;
  rc = level_validation::lightingError(lights::validateCornerColors(corner_colors, modules.geometry.faces.size()));
  if (rc != ARX_OK) return rc;

  lights::replaceCornerColors(modules.lighting, std::move(corner_colors));
  level_validation::markValid(validation, LevelValidation::kCornerColors);
  return ARX_OK;
}

}  // namespace

ArxReturnCode Level::generateStaticLighting() noexcept { return generateStaticLighting(StaticLightingGenOptions{}); }

ArxReturnCode Level::generateStaticLighting(const StaticLightingGenOptions& options) noexcept {
  return api_detail::statusBoundary(
      [&]() -> ArxReturnCode { return generateStaticLightingImpl(*data_, data_->validation, options, nullptr); });
}

namespace level_debug {

ArxReturnCode generateStaticLighting(Level& level, StaticLightingDiagnostics& diagnostics) noexcept {
  return generateStaticLighting(level, Level::StaticLightingGenOptions{}, diagnostics);
}

ArxReturnCode generateStaticLighting(Level& level, const Level::StaticLightingGenOptions& options,
                                     StaticLightingDiagnostics& diagnostics) noexcept {
  return api_detail::statusBoundary([&]() -> ArxReturnCode {
    lights::StaticLightingDiagnostics internal;
    const ArxReturnCode rc = generateStaticLightingImpl(
        LevelDebugAccess::modules(level), LevelDebugAccess::validation(level), options, &internal);
    detail::copyDiagnostics(internal, diagnostics);
    return rc;
  });
}

}  // namespace level_debug
}  // namespace pistoris
