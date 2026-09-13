// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#include "modules/navigation.h"

#include "arx_pistoris/base/status.h"
#include "arx_pistoris/debug/level.hpp"
#include "arx_pistoris/debug/level/diagnostics.hpp"
#include "arx_pistoris/level.hpp"

#include "api/status_boundary.h"
#include "level/data.h"
#include "level/debug/access.h"
#include "level/debug/diagnostics.h"
#include "level/validation.h"

#include <cassert>
#include <utility>
#include <vector>

namespace pistoris {
namespace {

static_assert(Level::NavSurfaceSourceOptions{}.clearance == navigation::NavSurfaceSourceOptions{}.clearance);
static_assert(Level::NavSurfaceSourceOptions{}.support_min_up_cos ==
              navigation::NavSurfaceSourceOptions{}.support_min_up_cos);
static_assert(Level::NavSurfaceSourceOptions{}.support_ignore_flags ==
              navigation::NavSurfaceSourceOptions{}.support_ignore_flags);
static_assert(Level::NavSurfaceGenOptions{}.radius == navigation::NavSurfaceGenerationOptions{}.radius);
static_assert(Level::NavSurfaceGenOptions{}.height == navigation::NavSurfaceGenerationOptions{}.height);
static_assert(Level::NavSurfaceGenOptions{}.max_step_up == navigation::NavSurfaceGenerationOptions{}.max_step_up);
static_assert(Level::NavSurfacePruneOptions{}.min_component_area_ratio ==
              navigation::NavSurfacePruneOptions{}.min_component_area_ratio);
static_assert(Level::NavSurfacePruneOptions{}.min_component_area ==
              navigation::NavSurfacePruneOptions{}.min_component_area);
static_assert(Level::AnchorGenOptions{}.sample_spacing == navigation::AnchorGenerationOptions{}.sample_spacing);
static_assert(Level::AnchorGenOptions{}.radius == navigation::AnchorGenerationOptions{}.radius);
static_assert(Level::AnchorGenOptions{}.height == navigation::AnchorGenerationOptions{}.height);
static_assert(Level::AnchorPruneOptions{}.min_component_anchor_ratio ==
              navigation::AnchorComponentPruneOptions{}.min_component_anchor_ratio);
static_assert(Level::AnchorPruneOptions{}.min_component_anchor_count ==
              navigation::AnchorComponentPruneOptions{}.min_component_anchor_count);
static_assert(Level::AnchorConnectionGenOptions{}.max_distance ==
              navigation::AnchorConnectionGenerationOptions{}.max_distance);
static_assert(Level::AnchorConnectionGenOptions{}.max_step_distance ==
              navigation::AnchorConnectionGenerationOptions{}.max_step_distance);
static_assert(Level::AnchorConnectionGenOptions{}.max_step_up ==
              navigation::AnchorConnectionGenerationOptions{}.max_step_up);
static_assert(Level::AnchorConnectionGenOptions{}.radius_scale ==
              navigation::AnchorConnectionGenerationOptions{}.radius_scale);
static_assert(Level::AnchorConnectionGenOptions{}.max_steps ==
              navigation::AnchorConnectionGenerationOptions{}.max_steps);

navigation::NavSurfaceSourceOptions toModuleOptions(const Level::NavSurfaceSourceOptions& options) {
  return {
      .clearance = options.clearance,
      .support_min_up_cos = options.support_min_up_cos,
      .support_ignore_flags = options.support_ignore_flags,
  };
}

navigation::NavSurfaceGenerationOptions toModuleOptions(const Level::NavSurfaceGenOptions& options) {
  navigation::NavSurfaceGenerationOptions result;
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

navigation::AnchorGenerationOptions toModuleOptions(const Level::AnchorGenOptions& options) {
  return {
      .sample_spacing = options.sample_spacing,
      .radius = options.radius,
      .height = options.height,
  };
}

navigation::AnchorConnectionGenerationOptions toModuleOptions(const Level::AnchorConnectionGenOptions& options) {
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
                                     const Level::NavSurfaceGenOptions& options, NavSurface& surface,
                                     navigation::NavSurfaceGenerationDiagnostics* diagnostics) {
  ArxReturnCode rc = level_validation::faces(modules, validation);
  if (rc != ARX_OK) return rc;

  rc = level_validation::navigationError(
      navigation::generateSurface(surface, modules.geometry, toModuleOptions(options), diagnostics));
  if (rc != ARX_OK) return rc;

  rc = level_validation::navigationError(navigation::validateSurface(surface));
  if (rc != ARX_OK) return rc;
  return ARX_OK;
}

ArxReturnCode generateNavSurfaceFromFloorPolygonsImpl(LevelModules& modules, LevelValidationState& validation,
                                                      const Level::NavSurfaceSourceOptions& options,
                                                      NavSurface& surface,
                                                      navigation::NavSurfaceGenerationDiagnostics* diagnostics) {
  ArxReturnCode rc = level_validation::faces(modules, validation);
  if (rc != ARX_OK) return rc;

  rc = level_validation::navigationError(
      navigation::generateSurfaceFromFloor(surface, modules.geometry, toModuleOptions(options), diagnostics));
  if (rc != ARX_OK) return rc;

  rc = level_validation::navigationError(navigation::validateSurface(surface));
  if (rc != ARX_OK) return rc;
  return ARX_OK;
}

ArxReturnCode pruneNavSurfaceComponentsImpl(LevelModules& modules, LevelValidationState& validation,
                                            const Level::NavSurfacePruneOptions& options,
                                            navigation::NavSurfaceComponentPrunePlan& plan,
                                            navigation::NavSurfacePruneDiagnostics* diagnostics) {
  ArxReturnCode rc = level_validation::navSurface(modules, validation);
  if (rc != ARX_OK) return rc;
  if (!modules.navigation.surface) return ARX_LEVEL_NAV_SURFACE_REQUIRED;

  return level_validation::navigationError(
      navigation::planSurfaceComponentPrune(plan, *modules.navigation.surface, toModuleOptions(options), diagnostics));
}

ArxReturnCode generateAnchorsImpl(LevelModules& modules, LevelValidationState& validation,
                                  const Level::AnchorGenOptions& options, std::vector<Anchor>& anchors,
                                  navigation::AnchorGenerationDiagnostics* diagnostics) {
  ArxReturnCode rc = level_validation::faces(modules, validation);
  if (rc != ARX_OK) return rc;
  if (!modules.navigation.surface) return ARX_LEVEL_NAV_SURFACE_REQUIRED;
  rc = level_validation::navSurface(modules, validation);
  if (rc != ARX_OK) return rc;
  const auto& surface = modules.navigation.surface;
  const auto& referenced_bounds = validation.derived.referenced_bounds;
  if (!surface.has_value() || !referenced_bounds.has_value()) return ARX_INTERNAL_ERROR;

  rc = level_validation::navigationError(navigation::generateAnchors(
      anchors, modules.geometry, *surface, *referenced_bounds, toModuleOptions(options), diagnostics));
  if (rc != ARX_OK) return rc;

  navigation::repairAnchorNames(anchors);
  rc = level_validation::navigationError(navigation::validateAnchorDefinitions(anchors));
  if (rc != ARX_OK) return rc;
  return ARX_OK;
}

ArxReturnCode generateAnchorConnectionsImpl(LevelModules& modules, LevelValidationState& validation,
                                            const Level::AnchorConnectionGenOptions& options,
                                            std::vector<AnchorConnection>& connections,
                                            navigation::AnchorConnectionGenerationDiagnostics* diagnostics) {
  ArxReturnCode rc = level_validation::faces(modules, validation);
  if (rc != ARX_OK) return rc;
  rc = level_validation::anchors(modules, validation);
  if (rc != ARX_OK) return rc;

  rc = level_validation::navigationError(navigation::generateAnchorConnections(
      connections, modules.geometry, modules.navigation.anchors, toModuleOptions(options), diagnostics));
  if (rc != ARX_OK) return rc;

  rc = level_validation::navigationError(navigation::validateConnections(modules.navigation.anchors, connections));
  if (rc != ARX_OK) return rc;
  return ARX_OK;
}

ArxReturnCode pruneAnchorComponentsImpl(LevelModules& modules, LevelValidationState& validation,
                                        const Level::AnchorPruneOptions& options,
                                        navigation::AnchorComponentPrunePlan& plan,
                                        navigation::AnchorComponentPruneDiagnostics* diagnostics) {
  ArxReturnCode rc = level_validation::anchorConnections(modules, validation);
  if (rc != ARX_OK) return rc;

  return level_validation::navigationError(navigation::planAnchorComponentPrune(
      plan, modules.navigation.anchors, modules.navigation.connections, toModuleOptions(options), diagnostics));
}

void commitNavSurface(LevelModules& modules, LevelValidationState& validation, NavSurface&& surface) noexcept {
  navigation::setSurface(modules.navigation, std::move(surface));
  level_validation::markValid(validation, LevelValidation::kNavSurface);
}

void commitNavSurfacePrune(LevelModules& modules, LevelValidationState& validation,
                           navigation::NavSurfaceComponentPrunePlan&& plan) noexcept {
  auto& surface = modules.navigation.surface;
  assert(surface.has_value());
  navigation::applySurfaceComponentPrune(*surface, std::move(plan));
  level_validation::markValid(validation, LevelValidation::kNavSurface);
}

void commitAnchors(LevelModules& modules, LevelValidationState& validation, std::vector<Anchor>&& anchors) noexcept {
  navigation::replaceAnchors(modules.navigation, std::move(anchors), {});
  level_validation::markValid(validation, LevelValidation::kAnchors | LevelValidation::kAnchorConnections);
}

void commitAnchorConnections(LevelModules& modules, LevelValidationState& validation,
                             std::vector<AnchorConnection>&& connections) noexcept {
  navigation::replaceConnections(modules.navigation, std::move(connections));
  level_validation::markValid(validation, LevelValidation::kAnchorConnections);
}

void commitAnchorPrune(LevelModules& modules, LevelValidationState& validation,
                       navigation::AnchorComponentPrunePlan&& plan) noexcept {
  navigation::applyAnchorComponentPrune(modules.navigation.anchors, modules.navigation.connections, std::move(plan));
  level_validation::invalidate(validation, LevelValidation::kAnchors);
  level_validation::markValid(validation, LevelValidation::kAnchors | LevelValidation::kAnchorConnections);
}

}  // namespace

ArxReturnCode Level::generateNavSurface() noexcept { return generateNavSurface(NavSurfaceGenOptions{}); }

ArxReturnCode Level::generateNavSurface(const NavSurfaceGenOptions& options) noexcept {
  return api_detail::statusBoundary([&]() -> ArxReturnCode {
    NavSurface surface;
    const ArxReturnCode rc = generateNavSurfaceImpl(*data_, data_->validation, options, surface, nullptr);
    if (rc == ARX_OK) commitNavSurface(*data_, data_->validation, std::move(surface));
    return rc;
  });
}

ArxReturnCode Level::setNavSurfaceFromFloor() noexcept { return setNavSurfaceFromFloor(NavSurfaceSourceOptions{}); }

ArxReturnCode Level::setNavSurfaceFromFloor(const NavSurfaceSourceOptions& options) noexcept {
  return api_detail::statusBoundary([&]() -> ArxReturnCode {
    NavSurface surface;
    const ArxReturnCode rc =
        generateNavSurfaceFromFloorPolygonsImpl(*data_, data_->validation, options, surface, nullptr);
    if (rc == ARX_OK) commitNavSurface(*data_, data_->validation, std::move(surface));
    return rc;
  });
}

ArxReturnCode Level::pruneNavSurfaceIslands() noexcept { return pruneNavSurfaceIslands(NavSurfacePruneOptions{}); }

ArxReturnCode Level::pruneNavSurfaceIslands(const NavSurfacePruneOptions& options) noexcept {
  return api_detail::statusBoundary([&]() -> ArxReturnCode {
    navigation::NavSurfaceComponentPrunePlan plan;
    const ArxReturnCode rc = pruneNavSurfaceComponentsImpl(*data_, data_->validation, options, plan, nullptr);
    if (rc == ARX_OK) commitNavSurfacePrune(*data_, data_->validation, std::move(plan));
    return rc;
  });
}

ArxReturnCode Level::generateAnchors() noexcept { return generateAnchors(AnchorGenOptions{}); }

ArxReturnCode Level::generateAnchors(const AnchorGenOptions& options) noexcept {
  return api_detail::statusBoundary([&]() -> ArxReturnCode {
    std::vector<Anchor> anchors;
    const ArxReturnCode rc = generateAnchorsImpl(*data_, data_->validation, options, anchors, nullptr);
    if (rc == ARX_OK) commitAnchors(*data_, data_->validation, std::move(anchors));
    return rc;
  });
}

ArxReturnCode Level::generateAnchorConnections() noexcept {
  return generateAnchorConnections(AnchorConnectionGenOptions{});
}

ArxReturnCode Level::generateAnchorConnections(const AnchorConnectionGenOptions& options) noexcept {
  return api_detail::statusBoundary([&]() -> ArxReturnCode {
    std::vector<AnchorConnection> connections;
    const ArxReturnCode rc = generateAnchorConnectionsImpl(*data_, data_->validation, options, connections, nullptr);
    if (rc == ARX_OK) commitAnchorConnections(*data_, data_->validation, std::move(connections));
    return rc;
  });
}

ArxReturnCode Level::pruneAnchorIslands() noexcept { return pruneAnchorIslands(AnchorPruneOptions{}); }

ArxReturnCode Level::pruneAnchorIslands(const AnchorPruneOptions& options) noexcept {
  return api_detail::statusBoundary([&]() -> ArxReturnCode {
    navigation::AnchorComponentPrunePlan plan;
    const ArxReturnCode rc = pruneAnchorComponentsImpl(*data_, data_->validation, options, plan, nullptr);
    if (rc == ARX_OK) commitAnchorPrune(*data_, data_->validation, std::move(plan));
    return rc;
  });
}

namespace level_debug {

ArxReturnCode generateNavSurface(Level& level, NavigationDiagnostics& diagnostics) noexcept {
  return generateNavSurface(level, Level::NavSurfaceGenOptions{}, diagnostics);
}

ArxReturnCode generateNavSurface(Level& level, const Level::NavSurfaceGenOptions& options,
                                 NavigationDiagnostics& diagnostics) noexcept {
  return api_detail::statusBoundary([&]() -> ArxReturnCode {
    LevelModules& modules = LevelDebugAccess::modules(level);
    LevelValidationState& validation = LevelDebugAccess::validation(level);
    navigation::NavSurfaceGenerationDiagnostics internal;
    NavSurface surface;
    const ArxReturnCode rc = generateNavSurfaceImpl(modules, validation, options, surface, &internal);
    NavSurfaceGenDiagnostics next;
    detail::copyDiagnostics(internal, next);
    if (rc == ARX_OK) commitNavSurface(modules, validation, std::move(surface));
    diagnostics.surface = std::move(next);
    return rc;
  });
}

ArxReturnCode setNavSurfaceFromFloor(Level& level, NavigationDiagnostics& diagnostics) noexcept {
  return setNavSurfaceFromFloor(level, Level::NavSurfaceSourceOptions{}, diagnostics);
}

ArxReturnCode setNavSurfaceFromFloor(Level& level, const Level::NavSurfaceSourceOptions& options,
                                     NavigationDiagnostics& diagnostics) noexcept {
  return api_detail::statusBoundary([&]() -> ArxReturnCode {
    LevelModules& modules = LevelDebugAccess::modules(level);
    LevelValidationState& validation = LevelDebugAccess::validation(level);
    navigation::NavSurfaceGenerationDiagnostics internal;
    NavSurface surface;
    const ArxReturnCode rc = generateNavSurfaceFromFloorPolygonsImpl(modules, validation, options, surface, &internal);
    NavSurfaceGenDiagnostics next;
    detail::copyDiagnostics(internal, next);
    if (rc == ARX_OK) commitNavSurface(modules, validation, std::move(surface));
    diagnostics.surface = std::move(next);
    return rc;
  });
}

ArxReturnCode pruneNavSurfaceIslands(Level& level, NavigationDiagnostics& diagnostics) noexcept {
  return pruneNavSurfaceIslands(level, Level::NavSurfacePruneOptions{}, diagnostics);
}

ArxReturnCode pruneNavSurfaceIslands(Level& level, const Level::NavSurfacePruneOptions& options,
                                     NavigationDiagnostics& diagnostics) noexcept {
  return api_detail::statusBoundary([&]() -> ArxReturnCode {
    LevelModules& modules = LevelDebugAccess::modules(level);
    LevelValidationState& validation = LevelDebugAccess::validation(level);
    navigation::NavSurfacePruneDiagnostics internal;
    navigation::NavSurfaceComponentPrunePlan plan;
    const ArxReturnCode rc = pruneNavSurfaceComponentsImpl(modules, validation, options, plan, &internal);
    NavSurfacePruneDiagnostics next;
    detail::copyDiagnostics(internal, next);
    if (rc == ARX_OK) commitNavSurfacePrune(modules, validation, std::move(plan));
    diagnostics.surface_pruning = std::move(next);
    return rc;
  });
}

ArxReturnCode generateAnchors(Level& level, NavigationDiagnostics& diagnostics) noexcept {
  return generateAnchors(level, Level::AnchorGenOptions{}, diagnostics);
}

ArxReturnCode generateAnchors(Level& level, const Level::AnchorGenOptions& options,
                              NavigationDiagnostics& diagnostics) noexcept {
  return api_detail::statusBoundary([&]() -> ArxReturnCode {
    LevelModules& modules = LevelDebugAccess::modules(level);
    LevelValidationState& validation = LevelDebugAccess::validation(level);
    navigation::AnchorGenerationDiagnostics internal;
    std::vector<Anchor> anchors;
    const ArxReturnCode rc = generateAnchorsImpl(modules, validation, options, anchors, &internal);
    AnchorGenDiagnostics next;
    detail::copyDiagnostics(internal, next);
    if (rc == ARX_OK) commitAnchors(modules, validation, std::move(anchors));
    diagnostics.anchors = std::move(next);
    return rc;
  });
}

ArxReturnCode generateAnchorConnections(Level& level, NavigationDiagnostics& diagnostics) noexcept {
  return generateAnchorConnections(level, Level::AnchorConnectionGenOptions{}, diagnostics);
}

ArxReturnCode generateAnchorConnections(Level& level, const Level::AnchorConnectionGenOptions& options,
                                        NavigationDiagnostics& diagnostics) noexcept {
  return api_detail::statusBoundary([&]() -> ArxReturnCode {
    LevelModules& modules = LevelDebugAccess::modules(level);
    LevelValidationState& validation = LevelDebugAccess::validation(level);
    navigation::AnchorConnectionGenerationDiagnostics internal;
    std::vector<AnchorConnection> connections;
    const ArxReturnCode rc = generateAnchorConnectionsImpl(modules, validation, options, connections, &internal);
    AnchorConnectionGenDiagnostics next;
    detail::copyDiagnostics(internal, next);
    if (rc == ARX_OK) commitAnchorConnections(modules, validation, std::move(connections));
    diagnostics.connections = std::move(next);
    return rc;
  });
}

ArxReturnCode pruneAnchorIslands(Level& level, NavigationDiagnostics& diagnostics) noexcept {
  return pruneAnchorIslands(level, Level::AnchorPruneOptions{}, diagnostics);
}

ArxReturnCode pruneAnchorIslands(Level& level, const Level::AnchorPruneOptions& options,
                                 NavigationDiagnostics& diagnostics) noexcept {
  return api_detail::statusBoundary([&]() -> ArxReturnCode {
    LevelModules& modules = LevelDebugAccess::modules(level);
    LevelValidationState& validation = LevelDebugAccess::validation(level);
    navigation::AnchorComponentPruneDiagnostics internal;
    navigation::AnchorComponentPrunePlan plan;
    const ArxReturnCode rc = pruneAnchorComponentsImpl(modules, validation, options, plan, &internal);
    AnchorComponentPruneDiagnostics next;
    detail::copyDiagnostics(internal, next);
    if (rc == ARX_OK) commitAnchorPrune(modules, validation, std::move(plan));
    diagnostics.anchor_pruning = std::move(next);
    return rc;
  });
}

}  // namespace level_debug
}  // namespace pistoris
