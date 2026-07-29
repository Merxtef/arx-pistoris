// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#include "modules/navigation.h"

#include "arx_pistoris/debug/level.hpp"
#include "arx_pistoris/debug/level_diagnostics.hpp"
#include "arx_pistoris/level.hpp"
#include "arx_pistoris/pistoris_types.h"

#include "level/data.h"
#include "level/debug/access.h"
#include "level/debug/diagnostics.h"
#include "level/validation.h"

#include <utility>
#include <vector>

namespace pistoris {
namespace {

static_assert(Level::NavSurfaceSourceOptions{}.clearance == navigation::NavSurfaceSourceOptions{}.clearance);
static_assert(Level::NavSurfaceSourceOptions{}.support_min_up_cos ==
              navigation::NavSurfaceSourceOptions{}.support_min_up_cos);
static_assert(Level::NavSurfaceSourceOptions{}.support_ignore_flags ==
              navigation::NavSurfaceSourceOptions{}.support_ignore_flags);
static_assert(Level::NavSurfaceGenOptions{}.radius == navigation::NavSurfaceGenOptions{}.radius);
static_assert(Level::NavSurfaceGenOptions{}.height == navigation::NavSurfaceGenOptions{}.height);
static_assert(Level::NavSurfaceGenOptions{}.max_step_up == navigation::NavSurfaceGenOptions{}.max_step_up);
static_assert(Level::NavSurfacePruneOptions{}.min_component_area_ratio ==
              navigation::NavSurfacePruneOptions{}.min_component_area_ratio);
static_assert(Level::NavSurfacePruneOptions{}.min_component_area ==
              navigation::NavSurfacePruneOptions{}.min_component_area);
static_assert(Level::AnchorGenOptions{}.sample_spacing == navigation::AnchorGenOptions{}.sample_spacing);
static_assert(Level::AnchorGenOptions{}.radius == navigation::AnchorGenOptions{}.radius);
static_assert(Level::AnchorGenOptions{}.height == navigation::AnchorGenOptions{}.height);
static_assert(Level::AnchorPruneOptions{}.min_component_anchor_ratio ==
              navigation::AnchorComponentPruneOptions{}.min_component_anchor_ratio);
static_assert(Level::AnchorPruneOptions{}.min_component_anchor_count ==
              navigation::AnchorComponentPruneOptions{}.min_component_anchor_count);
static_assert(Level::AnchorConnectionGenOptions{}.max_distance ==
              navigation::AnchorConnectionGenOptions{}.max_distance);
static_assert(Level::AnchorConnectionGenOptions{}.max_step_distance ==
              navigation::AnchorConnectionGenOptions{}.max_step_distance);
static_assert(Level::AnchorConnectionGenOptions{}.max_step_up == navigation::AnchorConnectionGenOptions{}.max_step_up);
static_assert(Level::AnchorConnectionGenOptions{}.radius_scale ==
              navigation::AnchorConnectionGenOptions{}.radius_scale);
static_assert(Level::AnchorConnectionGenOptions{}.max_steps == navigation::AnchorConnectionGenOptions{}.max_steps);

navigation::NavSurfaceSourceOptions toModuleOptions(const Level::NavSurfaceSourceOptions& options) {
  return {
      .clearance = options.clearance,
      .support_min_up_cos = options.support_min_up_cos,
      .support_ignore_flags = options.support_ignore_flags,
  };
}

navigation::NavSurfaceGenOptions toModuleOptions(const Level::NavSurfaceGenOptions& options) {
  navigation::NavSurfaceGenOptions result;
  static_cast<navigation::NavSurfaceSourceOptions&>(result) =
      toModuleOptions(static_cast<const Level::NavSurfaceSourceOptions&>(options));
  result.radius = options.radius;
  result.height = options.height;
  result.max_step_up = options.max_step_up;
  return result;
}

navigation::NavSurfacePruneOptions toModuleOptions(const Level::NavSurfacePruneOptions& options) {
  return {
      .min_component_area_ratio = options.min_component_area_ratio,
      .min_component_area = options.min_component_area,
  };
}

navigation::AnchorGenOptions toModuleOptions(const Level::AnchorGenOptions& options) {
  return {
      .sample_spacing = options.sample_spacing,
      .radius = options.radius,
      .height = options.height,
  };
}

navigation::AnchorConnectionGenOptions toModuleOptions(const Level::AnchorConnectionGenOptions& options) {
  return {
      .max_distance = options.max_distance,
      .max_step_distance = options.max_step_distance,
      .max_step_up = options.max_step_up,
      .radius_scale = options.radius_scale,
      .max_steps = options.max_steps,
  };
}

navigation::AnchorComponentPruneOptions toModuleOptions(const Level::AnchorPruneOptions& options) {
  return {
      .min_component_anchor_ratio = options.min_component_anchor_ratio,
      .min_component_anchor_count = options.min_component_anchor_count,
  };
}

ArxReturnCode generateNavSurfaceImpl(LevelModules& modules, LevelValidationState& validation,
                                     const Level::NavSurfaceGenOptions& options,
                                     navigation::NavSurfaceGenDiagnostics* diagnostics) {
  ArxReturnCode rc = level_validation::faces(modules, validation);
  if (rc != ARX_OK) return rc;

  NavSurface surface;
  rc = level_validation::navigationError(
      navigation::generateSurface(surface, modules.geometry, toModuleOptions(options), diagnostics));
  if (rc != ARX_OK) return rc;

  modules.navigation.surface = std::move(surface);
  level_validation::markValid(validation, LevelValidation::kNavSurface);
  return ARX_OK;
}

ArxReturnCode generateNavSurfaceFromFloorPolygonsImpl(LevelModules& modules, LevelValidationState& validation,
                                                      const Level::NavSurfaceSourceOptions& options,
                                                      navigation::NavSurfaceGenDiagnostics* diagnostics) {
  ArxReturnCode rc = level_validation::faces(modules, validation);
  if (rc != ARX_OK) return rc;

  NavSurface surface;
  rc = level_validation::navigationError(
      navigation::generateSurfaceFromFloorPolygons(surface, modules.geometry, toModuleOptions(options), diagnostics));
  if (rc != ARX_OK) return rc;

  modules.navigation.surface = std::move(surface);
  level_validation::markValid(validation, LevelValidation::kNavSurface);
  return ARX_OK;
}

ArxReturnCode pruneNavSurfaceComponentsImpl(LevelModules& modules, LevelValidationState& validation,
                                            const Level::NavSurfacePruneOptions& options,
                                            navigation::NavSurfacePruneDiagnostics* diagnostics) {
  ArxReturnCode rc = level_validation::navSurface(modules, validation);
  if (rc != ARX_OK) return rc;
  if (!modules.navigation.surface) return ARX_LEVEL_NAV_SURFACE_REQUIRED;

  NavSurface surface = *modules.navigation.surface;
  rc = level_validation::navigationError(
      navigation::pruneSurfaceComponents(surface, toModuleOptions(options), diagnostics));
  if (rc != ARX_OK) return rc;
  if (surface.triangles.size() == modules.navigation.surface->triangles.size()) return ARX_OK;

  modules.navigation.surface = std::move(surface);
  level_validation::markValid(validation, LevelValidation::kNavSurface);
  return ARX_OK;
}

ArxReturnCode generateAnchorsImpl(LevelModules& modules, LevelValidationState& validation,
                                  const Level::AnchorGenOptions& options,
                                  navigation::AnchorGenDiagnostics* diagnostics) {
  ArxReturnCode rc = level_validation::faces(modules, validation);
  if (rc != ARX_OK) return rc;
  if (!modules.navigation.surface) return ARX_LEVEL_NAV_SURFACE_REQUIRED;
  rc = level_validation::navSurface(modules, validation);
  if (rc != ARX_OK) return rc;
  const auto& surface = modules.navigation.surface;
  const auto& referenced_bounds = validation.derived.referenced_bounds;
  if (!surface.has_value() || !referenced_bounds.has_value()) return ARX_INTERNAL_ERROR;

  std::vector<Anchor> anchors;
  rc = level_validation::navigationError(navigation::generateAnchors(
      anchors, modules.geometry, *surface, *referenced_bounds, toModuleOptions(options), diagnostics));
  if (rc != ARX_OK) return rc;

  modules.navigation.anchors = std::move(anchors);
  modules.navigation.connections.clear();
  level_validation::markValid(validation, LevelValidation::kAnchors | LevelValidation::kAnchorConnections);
  return ARX_OK;
}

ArxReturnCode generateAnchorConnectionsImpl(LevelModules& modules, LevelValidationState& validation,
                                            const Level::AnchorConnectionGenOptions& options,
                                            navigation::AnchorConnectionGenDiagnostics* diagnostics) {
  ArxReturnCode rc = level_validation::faces(modules, validation);
  if (rc != ARX_OK) return rc;
  rc = level_validation::anchors(modules, validation);
  if (rc != ARX_OK) return rc;

  std::vector<AnchorConnection> connections;
  rc = level_validation::navigationError(navigation::generateAnchorConnections(
      connections, modules.geometry, modules.navigation.anchors, toModuleOptions(options), diagnostics));
  if (rc != ARX_OK) return rc;

  modules.navigation.connections = std::move(connections);
  level_validation::markValid(validation, LevelValidation::kAnchorConnections);
  return ARX_OK;
}

ArxReturnCode pruneAnchorComponentsImpl(LevelModules& modules, LevelValidationState& validation,
                                        const Level::AnchorPruneOptions& options,
                                        navigation::AnchorComponentPruneDiagnostics* diagnostics) {
  ArxReturnCode rc = level_validation::anchorConnections(modules, validation);
  if (rc != ARX_OK) return rc;

  std::vector<Anchor> anchors = modules.navigation.anchors;
  std::vector<AnchorConnection> connections = modules.navigation.connections;
  rc = level_validation::navigationError(
      navigation::pruneAnchorComponents(anchors, connections, toModuleOptions(options), diagnostics));
  if (rc != ARX_OK) return rc;
  if (anchors.size() == modules.navigation.anchors.size()) return ARX_OK;

  modules.navigation.anchors = std::move(anchors);
  modules.navigation.connections = std::move(connections);
  level_validation::invalidate(validation, LevelValidation::kAnchors);
  level_validation::markValid(validation, LevelValidation::kAnchors | LevelValidation::kAnchorConnections);
  return ARX_OK;
}

}  // namespace

ArxReturnCode Level::generateNavSurface() { return generateNavSurface(NavSurfaceGenOptions{}); }

ArxReturnCode Level::generateNavSurface(const NavSurfaceGenOptions& options) {
  return generateNavSurfaceImpl(*data_, data_->validation, options, nullptr);
}

ArxReturnCode Level::setNavSurfaceFromFloor() { return setNavSurfaceFromFloor(NavSurfaceSourceOptions{}); }

ArxReturnCode Level::setNavSurfaceFromFloor(const NavSurfaceSourceOptions& options) {
  return generateNavSurfaceFromFloorPolygonsImpl(*data_, data_->validation, options, nullptr);
}

ArxReturnCode Level::pruneNavSurfaceIslands() { return pruneNavSurfaceIslands(NavSurfacePruneOptions{}); }

ArxReturnCode Level::pruneNavSurfaceIslands(const NavSurfacePruneOptions& options) {
  return pruneNavSurfaceComponentsImpl(*data_, data_->validation, options, nullptr);
}

ArxReturnCode Level::generateAnchors() { return generateAnchors(AnchorGenOptions{}); }

ArxReturnCode Level::generateAnchors(const AnchorGenOptions& options) {
  return generateAnchorsImpl(*data_, data_->validation, options, nullptr);
}

ArxReturnCode Level::generateAnchorConnections() { return generateAnchorConnections(AnchorConnectionGenOptions{}); }

ArxReturnCode Level::generateAnchorConnections(const AnchorConnectionGenOptions& options) {
  return generateAnchorConnectionsImpl(*data_, data_->validation, options, nullptr);
}

ArxReturnCode Level::pruneAnchorIslands() { return pruneAnchorIslands(AnchorPruneOptions{}); }

ArxReturnCode Level::pruneAnchorIslands(const AnchorPruneOptions& options) {
  return pruneAnchorComponentsImpl(*data_, data_->validation, options, nullptr);
}

namespace level_debug {

ArxReturnCode generateNavSurface(Level& level, NavigationDiagnostics& diagnostics) {
  return generateNavSurface(level, Level::NavSurfaceGenOptions{}, diagnostics);
}

ArxReturnCode generateNavSurface(Level& level, const Level::NavSurfaceGenOptions& options,
                                 NavigationDiagnostics& diagnostics) {
  navigation::NavSurfaceGenDiagnostics internal;
  const ArxReturnCode rc =
      generateNavSurfaceImpl(LevelDebugAccess::modules(level), LevelDebugAccess::validation(level), options, &internal);
  detail::copyDiagnostics(internal, diagnostics.surface);
  return rc;
}

ArxReturnCode setNavSurfaceFromFloor(Level& level, NavigationDiagnostics& diagnostics) {
  return setNavSurfaceFromFloor(level, Level::NavSurfaceSourceOptions{}, diagnostics);
}

ArxReturnCode setNavSurfaceFromFloor(Level& level, const Level::NavSurfaceSourceOptions& options,
                                     NavigationDiagnostics& diagnostics) {
  navigation::NavSurfaceGenDiagnostics internal;
  const ArxReturnCode rc = generateNavSurfaceFromFloorPolygonsImpl(
      LevelDebugAccess::modules(level), LevelDebugAccess::validation(level), options, &internal);
  detail::copyDiagnostics(internal, diagnostics.surface);
  return rc;
}

ArxReturnCode pruneNavSurfaceIslands(Level& level, NavigationDiagnostics& diagnostics) {
  return pruneNavSurfaceIslands(level, Level::NavSurfacePruneOptions{}, diagnostics);
}

ArxReturnCode pruneNavSurfaceIslands(Level& level, const Level::NavSurfacePruneOptions& options,
                                     NavigationDiagnostics& diagnostics) {
  navigation::NavSurfacePruneDiagnostics internal;
  const ArxReturnCode rc = pruneNavSurfaceComponentsImpl(
      LevelDebugAccess::modules(level), LevelDebugAccess::validation(level), options, &internal);
  detail::copyDiagnostics(internal, diagnostics.surface_pruning);
  return rc;
}

ArxReturnCode generateAnchors(Level& level, NavigationDiagnostics& diagnostics) {
  return generateAnchors(level, Level::AnchorGenOptions{}, diagnostics);
}

ArxReturnCode generateAnchors(Level& level, const Level::AnchorGenOptions& options,
                              NavigationDiagnostics& diagnostics) {
  navigation::AnchorGenDiagnostics internal;
  const ArxReturnCode rc =
      generateAnchorsImpl(LevelDebugAccess::modules(level), LevelDebugAccess::validation(level), options, &internal);
  detail::copyDiagnostics(internal, diagnostics.anchors);
  return rc;
}

ArxReturnCode generateAnchorConnections(Level& level, NavigationDiagnostics& diagnostics) {
  return generateAnchorConnections(level, Level::AnchorConnectionGenOptions{}, diagnostics);
}

ArxReturnCode generateAnchorConnections(Level& level, const Level::AnchorConnectionGenOptions& options,
                                        NavigationDiagnostics& diagnostics) {
  navigation::AnchorConnectionGenDiagnostics internal;
  const ArxReturnCode rc = generateAnchorConnectionsImpl(
      LevelDebugAccess::modules(level), LevelDebugAccess::validation(level), options, &internal);
  detail::copyDiagnostics(internal, diagnostics.connections);
  return rc;
}

ArxReturnCode pruneAnchorIslands(Level& level, NavigationDiagnostics& diagnostics) {
  return pruneAnchorIslands(level, Level::AnchorPruneOptions{}, diagnostics);
}

ArxReturnCode pruneAnchorIslands(Level& level, const Level::AnchorPruneOptions& options,
                                 NavigationDiagnostics& diagnostics) {
  navigation::AnchorComponentPruneDiagnostics internal;
  const ArxReturnCode rc = pruneAnchorComponentsImpl(
      LevelDebugAccess::modules(level), LevelDebugAccess::validation(level), options, &internal);
  detail::copyDiagnostics(internal, diagnostics.anchor_pruning);
  return rc;
}

}  // namespace level_debug
}  // namespace pistoris
