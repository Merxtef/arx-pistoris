// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#include "modules/rooms.h"

#include "arx_pistoris/base/status.h"
#include "arx_pistoris/debug/level.hpp"
#include "arx_pistoris/debug/level/diagnostics.hpp"
#include "arx_pistoris/level.hpp"

#include "api/status_boundary.h"
#include "level/data.h"
#include "level/debug/access.h"
#include "level/debug/diagnostics.h"
#include "level/validation.h"
#include "utils/math/finite.h"

#include <utility>

namespace pistoris {
namespace {

static_assert(Level::RoomDistanceGenOptions{}.portal_side_offset == rooms::RoomDistanceOptions{}.portal_side_offset);
static_assert(Level::RoomDistanceGenOptions{}.sample_spacing == rooms::RoomDistanceOptions{}.sample_spacing);
static_assert(Level::RoomDistanceGenOptions{}.sample_height_offset ==
              rooms::RoomDistanceOptions{}.sample_height_offset);

bool resolveOptions(const Level::RoomDistanceGenOptions& options, rooms::RoomDistanceOptions& out) {
  out = {};
  if (!math::finite(options.portal_side_offset) || options.portal_side_offset <= 0.0f ||
      options.portal_side_offset > kMaxRoomDistancePortalOffset)
    return false;
  if (!math::finite(options.sample_spacing) || options.sample_spacing < kMinRoomDistanceSampleSpacing) return false;
  if (!math::finite(options.sample_height_offset) || options.sample_height_offset < kMinRoomDistanceSampleHeight)
    return false;

  out.portal_side_offset = options.portal_side_offset;
  out.sample_spacing = options.sample_spacing;
  out.sample_height_offset = options.sample_height_offset;

  const float min_link_distance = out.sample_spacing * kRoomDistanceMinLinkDistanceSpacingFactor;
  if (!math::finite(options.max_link_distance)) return false;
  if (options.max_link_distance == 0.0f) {
    out.max_link_distance = out.sample_spacing * kRoomDistanceDefaultLinkDistanceSpacingFactor;
  } else if (options.max_link_distance >= min_link_distance) {
    out.max_link_distance = options.max_link_distance;
  } else {
    return false;
  }
  return true;
}

ArxReturnCode generateRoomDistancesImpl(LevelModules& modules, LevelValidationState& validation,
                                        const Level::RoomDistanceGenOptions& options, RoomDistances& distances,
                                        rooms::RoomDistanceGenerationDiagnostics* diagnostics) {
  rooms::RoomDistanceOptions effective_options;
  if (!resolveOptions(options, effective_options)) return ARX_INVALID_OPTIONS;

  ArxReturnCode rc = level_validation::faces(modules, validation);
  if (rc != ARX_OK) return rc;
  rc = level_validation::rooms(modules, validation);
  if (rc != ARX_OK) return rc;
  rc = level_validation::faceRooms(modules, validation);
  if (rc != ARX_OK) return rc;
  rc = level_validation::portals(modules, validation);
  if (rc != ARX_OK) return rc;

  rc = level_validation::roomsError(
      rooms::generateRoomDistances(distances, modules.rooms, modules.geometry, effective_options, diagnostics));
  if (rc != ARX_OK) return rc;

  rc = level_validation::roomsError(rooms::validateRoomDistances(distances, modules.rooms));
  if (rc != ARX_OK) return rc;
  return ARX_OK;
}

void commitRoomDistances(LevelModules& modules, LevelValidationState& validation, RoomDistances&& distances) noexcept {
  rooms::replaceRoomDistances(modules.rooms, std::move(distances));
  level_validation::markValid(validation, LevelValidation::kRoomDistances);
}

}  // namespace

ArxReturnCode Level::generateRoomDistances() noexcept { return generateRoomDistances(RoomDistanceGenOptions{}); }

ArxReturnCode Level::generateRoomDistances(const RoomDistanceGenOptions& options) noexcept {
  return api_detail::statusBoundary([&]() -> ArxReturnCode {
    RoomDistances distances;
    const ArxReturnCode rc = generateRoomDistancesImpl(*data_, data_->validation, options, distances, nullptr);
    if (rc == ARX_OK) commitRoomDistances(*data_, data_->validation, std::move(distances));
    return rc;
  });
}

namespace level_debug {

ArxReturnCode generateRoomDistances(Level& level, RoomDistanceGenDiagnostics& diagnostics) noexcept {
  return generateRoomDistances(level, Level::RoomDistanceGenOptions{}, diagnostics);
}

ArxReturnCode generateRoomDistances(Level& level, const Level::RoomDistanceGenOptions& options,
                                    RoomDistanceGenDiagnostics& diagnostics) noexcept {
  return api_detail::statusBoundary([&]() -> ArxReturnCode {
    LevelModules& modules = LevelDebugAccess::modules(level);
    LevelValidationState& validation = LevelDebugAccess::validation(level);
    rooms::RoomDistanceGenerationDiagnostics internal;
    RoomDistances distances;
    const ArxReturnCode rc = generateRoomDistancesImpl(modules, validation, options, distances, &internal);
    RoomDistanceGenDiagnostics next;
    detail::copyDiagnostics(internal, next);
    if (rc == ARX_OK) commitRoomDistances(modules, validation, std::move(distances));
    diagnostics = std::move(next);
    return rc;
  });
}

}  // namespace level_debug
}  // namespace pistoris
