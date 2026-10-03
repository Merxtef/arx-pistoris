// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#include "arx_pistoris/base/indices.h"
#include "arx_pistoris/base/math.h"

#include "modules/geometry.h"
#include "modules/navigation.h"
#include "modules/navigation/internal.h"
#include "modules/navigation/surface/internal.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <optional>
#include <utility>
#include <vector>

namespace pistoris::navigation {
namespace {

constexpr float kFloorVertexMergeDistance = 1.0e-4f;

SurfaceDebugTriangle debugTriangle(const std::array<ArxVector3, 3>& vertices) { return {vertices}; }

}  // namespace

Error generateSurfaceFromFloor(NavSurface& out, const GeometryData& geometry, const NavSurfaceSourceOptions& options,
                               NavSurfaceGenerationDiagnostics* diagnostics) {
  if (!validNavSurfaceSourceOptions(options)) return Error::kInvalidOptions;
  if (diagnostics) *diagnostics = {};

  std::vector<FaceIndex> face_indices = navSurfaceSupportFaceIndices(geometry, options);
  if (face_indices.empty()) return Error::kEmptyResult;

  NavSurface surface;
  surface.triangles.reserve(face_indices.size());
  const std::size_t max_vertices = std::numeric_limits<NavSurfaceVertexIndex>::max();
  if (face_indices.size() <= max_vertices / 3U) surface.vertices.reserve(face_indices.size() * 3U);
  if (diagnostics) {
    diagnostics->support.reserve(face_indices.size());
    diagnostics->base.reserve(face_indices.size());
  }
  geometry::PositionIndex position_index(kFloorVertexMergeDistance, geometry::PositionWeldMetric::kAxisAligned);
  position_index.reservePositionCapacity(face_indices.size() * 3U);
  for (FaceIndex face_index : face_indices) {
    if (face_index >= geometry.faces.size()) return Error::kBadFaceVertex;
    const std::array<ArxVector3, 3> positions = geometry::facePositions(geometry, geometry.faces[face_index]);
    NavSurfaceTriangle triangle;
    for (std::size_t corner = 0; corner < positions.size(); ++corner) {
      ArxVector3 position = surface::navigationVertexPosition(positions[corner], options.clearance);
      std::optional<std::uint32_t> existing = position_index.find(position);
      if (existing) {
        triangle.vertices[corner] = *existing;
        continue;
      }
      if (surface.vertices.size() >= static_cast<std::size_t>(std::numeric_limits<NavSurfaceVertexIndex>::max()))
        return Error::kTooManySurfaceVertices;
      NavSurfaceVertexIndex vertex_index = static_cast<NavSurfaceVertexIndex>(surface.vertices.size());
      surface.vertices.push_back({position});
      if (!position_index.tryAdd(vertex_index, position)) return Error::kInvalidOptions;
      triangle.vertices[corner] = vertex_index;
    }
    if (triangle.vertices[0] == triangle.vertices[1] || triangle.vertices[0] == triangle.vertices[2] ||
        triangle.vertices[1] == triangle.vertices[2])
      return Error::kDegenerateSurfaceTriangle;
    surface.triangles.push_back(triangle);
    if (diagnostics) {
      diagnostics->support.push_back(debugTriangle(positions));
      diagnostics->base.push_back(debugTriangle({surface.vertices[triangle.vertices[0]].position,
                                                 surface.vertices[triangle.vertices[1]].position,
                                                 surface.vertices[triangle.vertices[2]].position}));
    }
  }

  Error error = validateSurface(surface);
  if (error != Error::kNone) return error;
  out = std::move(surface);
  return Error::kNone;
}

}  // namespace pistoris::navigation
