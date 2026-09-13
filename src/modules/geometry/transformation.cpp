// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#include "arx_pistoris/base/math.hpp"

#include "modules/geometry.h"
#include "utils/math/finite.h"
#include "utils/math/mat3.h"

namespace pistoris::geometry {

Error validateScale(const GeometryData& geometry, float factor) noexcept {
  for (const Vertex& vertex : geometry.vertices)
    if (!math::finite(vertex.position * factor)) return Error::kBadVertex;
  return Error::kNone;
}

void applyScale(GeometryData& geometry, float factor) noexcept {
  for (Vertex& vertex : geometry.vertices) vertex.position = vertex.position * factor;
}

Error validateRotation(const GeometryData& geometry, const ArxMat3& rotation) noexcept {
  for (const Vertex& vertex : geometry.vertices)
    if (!math::finite(rotation * vertex.position)) return Error::kBadVertex;
  for (const Face& face : geometry.faces) {
    if (!math::finite(rotation * face.normal)) return Error::kBadFaceNormal;
    for (const Corner& corner : face.corners)
      if (!math::finite(rotation * corner.normal)) return Error::kBadCornerNormal;
  }
  return Error::kNone;
}

void applyRotation(GeometryData& geometry, const ArxMat3& rotation) noexcept {
  for (Vertex& vertex : geometry.vertices) vertex.position = rotation * vertex.position;
  for (Face& face : geometry.faces) {
    face.normal = rotation * face.normal;
    for (Corner& corner : face.corners) corner.normal = rotation * corner.normal;
  }
}

Error validateTranslation(const GeometryData& geometry, const ArxVector3& offset) noexcept {
  for (const Vertex& vertex : geometry.vertices)
    if (!math::finite(vertex.position + offset)) return Error::kBadVertex;
  return Error::kNone;
}

void applyTranslation(GeometryData& geometry, const ArxVector3& offset) noexcept {
  for (Vertex& vertex : geometry.vertices) vertex.position = vertex.position + offset;
}

}  // namespace pistoris::geometry
