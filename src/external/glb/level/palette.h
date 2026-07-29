// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#pragma once

#include "../writer.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <string_view>
#include <vector>

namespace pistoris::glb_level {

enum class PaletteItem : std::uint8_t {
  kPortal,
  kZone,
  kNavigationSurface,
  kNavigationSupport,
  kAnchor,
  kAnchorConnection,
  kGeometryContext,
  kDebugGeometry,
  kPortalCentroid,
  kStoredRoomDistance,
  kSurfaceGenerated,
  kSurfaceRepaired,
  kSurfacePruned,
  kAnchorRepaired,
  kAnchorRejected,
  kAnchorRepairSegment,
  kAnchorPruned,
  kInvalid,
  kNoSupport,
  kTooFar,
  kUnresolved,
  kResolved,
  kResolution,
  kMaxSteps,
  kEndMismatch,
  kRequested,
  kPortalAccessPoint,
  kSampledPoint,
  kPortalAccessSegment,
  kVisibilityEdge,
  kInRoomPortalPath,
  kRoomPairPath,
  kCount,
};

bool isReservedPaletteStem(std::string_view stem);

class Palette {
 public:
  explicit Palette(glb::Builder& builder);

  int material(PaletteItem item);
  int roomMaterial(std::size_t room);

 private:
  glb::Builder& builder_;
  std::array<int, static_cast<std::size_t>(PaletteItem::kCount)> materials_{};
  std::vector<int> room_materials_;
};

}  // namespace pistoris::glb_level
