// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#include "arx_pistoris/base/indices.h"
#include "arx_pistoris/base/math.hpp"

#include "modules/rooms.h"
#include "modules/rooms/internal.h"

#include <cstddef>
#include <optional>
#include <utility>
#include <vector>

namespace pistoris::rooms {
namespace {

ArxVector3 toArxVector3(const Vec3<double>& value) {
  return {static_cast<float>(value.x), static_cast<float>(value.y), static_cast<float>(value.z)};
}

}  // namespace

Error flattenPortals(RoomsData& rooms, PortalFlattenStatistics* statistics) {
  Error error = validatePortals(rooms);
  if (error != Error::kNone) return error;

  std::vector<Portal> next = rooms.portals;
  PortalFlattenStatistics result;
  for (Portal& portal : next) {
    if (portal.shape == PortalShape::kTriangle) {
      ++result.triangles;
      continue;
    }

    const std::optional<PortalSurface> surface = projectPortalSurface(portal);
    if (!surface) return Error::kDegeneratePortal;
    const ArxVector3 projected = toArxVector3(surface->vertices[2]);
    if (portal.vertices[2] == projected) {
      ++result.already_planar_quads;
      continue;
    }

    portal.vertices[2] = projected;
    error = validatePortal(portal, rooms.definitions.size());
    if (error != Error::kNone) return error;
    ++result.flattened_quads;
  }

  if (result.flattened_quads != 0) {
    for (std::size_t index = 0; index < next.size(); ++index) {
      if (rooms.portals[index].vertices[2] == next[index].vertices[2]) continue;
      setPortal(rooms, static_cast<PortalIndex>(index), std::move(next[index]));
    }
  }
  if (statistics) *statistics = result;
  return Error::kNone;
}

}  // namespace pistoris::rooms
