// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#include "arx_pistoris/arx_math.hpp"
#include "arx_pistoris/indices.h"

#include "modules/geometry.h"
#include "utils/math/bounds.h"
#include "utils/math/geometry.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <optional>
#include <span>
#include <vector>

namespace pistoris::geometry {
namespace {

IndexedTriangle indexedTriangle(const SurfaceSupportTriangle& triangle) {
  return {triangle.vertices, triangleBounds(triangle.vertices)};
}

void appendSupportTriangle(const GeometryData& geometry, FaceIndex face_index,
                           std::vector<SurfaceSupportTriangle>& out) {
  if (face_index >= geometry.faces.size()) return;
  out.push_back({face_index, facePositions(geometry, geometry.faces[face_index])});
}

}  // namespace

SurfaceSupportIndex::SurfaceSupportIndex(const std::vector<SurfaceSupportTriangle>& triangles) {
  faces_.reserve(triangles.size());
  std::vector<IndexedTriangle> indexed;
  indexed.reserve(triangles.size());
  for (const SurfaceSupportTriangle& triangle : triangles) {
    SupportFace support;
    support.face = triangle.face;
    support.vertices = triangle.vertices;
    support.normal = math::normalizeFiniteOr(
        math::cross(support.vertices[1] - support.vertices[0], support.vertices[2] - support.vertices[0]), {});
    faces_.push_back(support);
    indexed.push_back(indexedTriangle(triangle));

    ArxAabb triangle_bounds = indexed.back().bounds;
    if (!has_bounds_) {
      bounds_ = triangle_bounds;
      has_bounds_ = true;
    } else {
      math::expand(bounds_, triangle_bounds.min);
      math::expand(bounds_, triangle_bounds.max);
    }
  }
  index_ = TriangleIndex(indexed);
}

bool SurfaceSupportIndex::empty() const { return faces_.empty(); }

bool SurfaceSupportIndex::hasBounds() const { return has_bounds_; }

const ArxAabb& SurfaceSupportIndex::bounds() const { return bounds_; }

std::vector<SurfaceSupportTriangle> SurfaceSupportIndex::triangles() const {
  std::vector<SurfaceSupportTriangle> out;
  out.reserve(faces_.size());
  for (const SupportFace& face : faces_) out.push_back({face.face, face.vertices});
  return out;
}

std::vector<SurfaceSupportHit> SurfaceSupportIndex::hitsAt(float x, float z) const {
  std::vector<SurfaceSupportHit> hits;
  for (std::uint32_t face_index : index_.candidatesForXz(x, z)) {
    const SupportFace& face = faces_[face_index];
    std::array<double, 3> weights{};
    if (!math::barycentricXz(face.vertices[0], face.vertices[1], face.vertices[2], x, z, weights)) continue;
    hits.push_back(
        {face.face, math::interpolate(face.vertices[0], face.vertices[1], face.vertices[2], weights), face.normal});
  }
  std::sort(hits.begin(), hits.end(), [](const SurfaceSupportHit& a, const SurfaceSupportHit& b) {
    return a.position.y < b.position.y;
  });
  return hits;
}

std::vector<SurfaceSupportHit> SurfaceSupportIndex::downwardHitsAt(float x, float z, float origin_y) const {
  std::vector<SurfaceSupportHit> hits;
  for (std::uint32_t face_index : index_.candidatesForXz(x, z)) {
    const SupportFace& face = faces_[face_index];
    std::array<double, 3> weights{};
    if (!math::barycentricXz(face.vertices[0], face.vertices[1], face.vertices[2], x, z, weights)) continue;
    ArxVector3 position = math::interpolate(face.vertices[0], face.vertices[1], face.vertices[2], weights);
    float distance = position.y - origin_y;
    if (distance < 0.0f) continue;
    hits.push_back({face.face, position, face.normal});
  }
  std::sort(hits.begin(), hits.end(), [origin_y](const SurfaceSupportHit& a, const SurfaceSupportHit& b) {
    return (a.position.y - origin_y) < (b.position.y - origin_y);
  });
  return hits;
}

std::optional<SurfaceSupportHit> SurfaceSupportIndex::closestDownwardHit(float x, float z, float origin_y) const {
  std::vector<SurfaceSupportHit> hits = downwardHitsAt(x, z, origin_y);
  if (hits.empty()) return std::nullopt;
  return hits.front();
}

std::optional<SurfaceSupportHit> SurfaceSupportIndex::closestHit(float x, float z, float reference_y,
                                                                 float max_delta) const {
  std::vector<SurfaceSupportHit> hits = hitsAt(x, z);
  if (hits.empty()) return std::nullopt;
  const SurfaceSupportHit* best = nullptr;
  float best_delta = std::numeric_limits<float>::max();
  for (const SurfaceSupportHit& hit : hits) {
    float delta = std::abs(hit.position.y - reference_y);
    if (delta >= best_delta) continue;
    best = &hit;
    best_delta = delta;
  }
  if (!best || best_delta > max_delta) return std::nullopt;
  return *best;
}

void mergeSurfaceSupportHits(std::vector<SurfaceSupportHit>& hits, float merge_distance) {
  std::sort(hits.begin(), hits.end(), [](const SurfaceSupportHit& a, const SurfaceSupportHit& b) {
    return a.position.y < b.position.y;
  });
  if (!std::isfinite(merge_distance) || merge_distance < 0.0f) return;
  const double merge_distance_squared = static_cast<double>(merge_distance) * merge_distance;
  hits.erase(std::unique(hits.begin(),
                         hits.end(),
                         [merge_distance_squared](const SurfaceSupportHit& a, const SurfaceSupportHit& b) {
                           return math::lengthSquared(a.position - b.position) <= merge_distance_squared;
                         }),
             hits.end());
}

SurfaceSupportIndex buildSurfaceSupportIndex(const GeometryData& geometry) {
  return buildSurfaceSupportIndex(geometry, FacePredicate{});
}

SurfaceSupportIndex buildSurfaceSupportIndex(const GeometryData& geometry, FacePredicate predicate) {
  std::vector<SurfaceSupportTriangle> triangles;
  triangles.reserve(geometry.faces.size());
  for (std::size_t face_index = 0; face_index < geometry.faces.size(); ++face_index) {
    const FaceIndex face = static_cast<FaceIndex>(face_index);
    if (predicate.function != nullptr && !predicate.function(face, predicate.user_data)) continue;
    appendSupportTriangle(geometry, face, triangles);
  }
  return SurfaceSupportIndex(triangles);
}

SurfaceSupportIndex buildSurfaceSupportIndex(const GeometryData& geometry, std::span<const FaceIndex> face_indices) {
  std::vector<SurfaceSupportTriangle> triangles;
  triangles.reserve(face_indices.size());
  for (FaceIndex face_index : face_indices) appendSupportTriangle(geometry, face_index, triangles);
  return SurfaceSupportIndex(triangles);
}

}  // namespace pistoris::geometry
