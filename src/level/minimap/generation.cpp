// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#include "arx_pistoris/base/image.h"
#include "arx_pistoris/base/status.h"
#include "arx_pistoris/debug/level.hpp"
#include "arx_pistoris/debug/level/diagnostics.hpp"
#include "arx_pistoris/level.hpp"
#include "arx_pistoris/runtime/types.h"

#include "api/status_boundary.h"
#include "level/data.h"
#include "level/debug/access.h"
#include "level/debug/diagnostics.h"
#include "level/validation.h"
#include "modules/minimap.h"
#include "utils/log.h"

#include <utility>

namespace pistoris {
namespace {

static_assert(kLevelMinXZ == 0.0f);
static_assert(static_cast<float>(minimap::kGenerationWidth) * minimap::kArxUnitsPerPixel == kLevelMaxXZ - kLevelMinXZ);
static_assert(static_cast<float>(minimap::kGenerationHeight) * minimap::kArxUnitsPerPixel == kLevelMaxXZ - kLevelMinXZ);

bool validImageView(ArxEncodedImageView image) noexcept { return image.size == 0 || image.data != nullptr; }

minimap::Sampler toModuleSampler(const Level::MinimapSampler& sampler) noexcept {
  return {{sampler.image.data, sampler.image.size}, sampler.color};
}

minimap::GenerationOptions toModuleOptions(const Level::MinimapGenerationOptions& options) noexcept {
  return {
      .foreground = toModuleSampler(options.foreground),
      .background = toModuleSampler(options.background),
      .water = toModuleSampler(options.water),
      .lava = toModuleSampler(options.lava),
      .halo_color = options.halo_color,
      .halo_radius = options.halo_radius,
  };
}

ArxReturnCode generateMinimapImpl(LevelModules& modules, LevelValidationState& validation,
                                  const Level::MinimapGenerationOptions& options,
                                  minimap::GenerationDiagnostics* diagnostics) {
  if (!validImageView(options.foreground.image) || !validImageView(options.background.image) ||
      !validImageView(options.water.image) || !validImageView(options.lava.image)) {
    return ARX_INVALID_DATA_POINTER;
  }
  ArxReturnCode rc = level_validation::faces(modules, validation);
  if (rc != ARX_OK) return rc;
  if (!validation.derived.referenced_bounds) return ARX_LEVEL_NO_GEOMETRY;

  MinimapData generated;
  minimap::GenerationDiagnostics local_diagnostics;
  minimap::GenerationDiagnostics* generated_diagnostics = diagnostics != nullptr ? diagnostics : &local_diagnostics;
  rc = level_validation::minimapError(
      minimap::generate(generated, modules.geometry, toModuleOptions(options), generated_diagnostics));
  if (rc != ARX_OK) return rc;
  rc = level_validation::minimapError(minimap::validate(generated));
  if (rc != ARX_OK) return rc;

  minimap::setImage(modules.minimap, std::move(generated.encoded_image), generated.world_xz_bounds);
  level_validation::markValid(validation, LevelValidation::kMinimap);
  log(ARX_LOG_INFO,
      "Level minimap generated: {} sampled cells, {} skipped, {} foreground, {} water, {} lava, {} halo pixels",
      generated_diagnostics->sampled_cells,
      generated_diagnostics->skipped_cells,
      generated_diagnostics->foreground_pixels,
      generated_diagnostics->water_pixels,
      generated_diagnostics->lava_pixels,
      generated_diagnostics->halo_pixels);
  return ARX_OK;
}

}  // namespace

ArxReturnCode Level::generateMinimap() noexcept { return generateMinimap(MinimapGenerationOptions{}); }

ArxReturnCode Level::generateMinimap(const MinimapGenerationOptions& options) noexcept {
  return api_detail::statusBoundary(
      [&]() -> ArxReturnCode { return generateMinimapImpl(*data_, data_->validation, options, nullptr); });
}

namespace level_debug {

ArxReturnCode generateMinimap(Level& level, const Level::MinimapGenerationOptions& options,
                              MinimapGenerationDiagnostics& diagnostics) noexcept {
  return api_detail::statusBoundary([&]() -> ArxReturnCode {
    minimap::GenerationDiagnostics internal;
    const ArxReturnCode rc =
        generateMinimapImpl(LevelDebugAccess::modules(level), LevelDebugAccess::validation(level), options, &internal);
    detail::copyDiagnostics(internal, diagnostics);
    return rc;
  });
}

}  // namespace level_debug
}  // namespace pistoris
