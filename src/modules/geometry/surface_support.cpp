// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#include "arx_pistoris/base/indices.h"
#include "arx_pistoris/base/math.hpp"

#include "modules/geometry.h"
#include "utils/math/bounds.h"
#include "utils/math/geometry_algorithms.h"

#include <algorithm>
#include <array>
#include <cassert>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <memory>
#include <numeric>
#include <optional>
#include <span>
#include <utility>
#include <vector>

namespace pistoris::geometry {
namespace {

IndexedTriangle indexedTriangle(const SurfaceSupportTriangle& triangle) {
  return {triangle.vertices, triangleBounds(triangle.vertices)};
}

bool supportHitLess(const SurfaceSupportHit& left, const SurfaceSupportHit& right) noexcept {
  if (left.position.y != right.position.y) return left.position.y < right.position.y;
  return left.face < right.face;
}

}  // namespace

SurfaceSupportIndexBuilder::SurfaceSupportIndexBuilder(std::size_t capacity) { records_.reserve(capacity); }

void SurfaceSupportIndexBuilder::addTriangle(FaceIndex face, const std::array<ArxVector3, 3>& vertices) {
  records_.push_back({face,
                      math::normalizeFiniteOr(math::cross(vertices[1] - vertices[0], vertices[2] - vertices[0]), {}),
                      indexedTriangle({face, vertices})});
}

SurfaceSupportIndex SurfaceSupportIndexBuilder::build() && {
  auto storage = std::make_shared<SurfaceSupportIndex::Storage>();
  storage->records = std::move(records_);
  SurfaceSupportIndex out;
  out.storage_ = std::move(storage);
  out.triangle_indices_.resize(out.storage_->records.size());
  std::iota(out.triangle_indices_.begin(), out.triangle_indices_.end(), std::uint32_t{0});
  for (std::uint32_t index : out.triangle_indices_) out.addTriangleToIndex(index);
  return out;
}

void SurfaceSupportIndex::addTriangleToIndex(std::uint32_t index) {
  const ArxAabb& triangle_bounds = storage_->records[index].triangle.bounds;
  grid_index_.add(index, triangle_bounds);
  if (!has_bounds_) {
    bounds_ = triangle_bounds;
    has_bounds_ = true;
  } else {
    math::expand(bounds_, triangle_bounds.min);
    math::expand(bounds_, triangle_bounds.max);
  }
}

bool SurfaceSupportIndex::empty() const { return triangle_indices_.empty(); }

std::size_t SurfaceSupportIndex::size() const noexcept { return triangle_indices_.size(); }

bool SurfaceSupportIndex::hasBounds() const { return has_bounds_; }

const ArxAabb& SurfaceSupportIndex::bounds() const { return bounds_; }

SurfaceSupportTriangle SurfaceSupportIndex::triangle(std::size_t index) const {
  const Record& record = storage_->records[triangle_indices_[index]];
  return {record.face, record.triangle.vertices};
}

SurfaceSupportIndex SurfaceSupportIndex::subset(std::span<const std::uint32_t> triangle_indices) const {
  SurfaceSupportIndex out;
  out.storage_ = storage_;
  out.triangle_indices_.reserve(triangle_indices.size());
  for (std::size_t position = 0; position < triangle_indices.size(); ++position) {
    const std::uint32_t local_index = triangle_indices[position];
    assert(local_index < triangle_indices_.size());
    assert(position == 0 || triangle_indices[position - 1U] < local_index);
    const std::uint32_t storage_index = triangle_indices_[local_index];
    out.triangle_indices_.push_back(storage_index);
    out.addTriangleToIndex(storage_index);
  }
  return out;
}

bool SurfaceSupportIndex::mayHaveHitsInAabb(const ArxAabb& bounds) const {
  return grid_index_.hasAabbCandidates(bounds);
}

std::optional<SurfaceSupportHit> SurfaceSupportIndex::hitAt(std::uint32_t index, float x, float z) const {
  const Record& record = storage_->records[index];
  const std::array<ArxVector3, 3>& vertices = record.triangle.vertices;
  std::array<double, 3> weights{};
  if (!math::barycentricXz(vertices[0], vertices[1], vertices[2], x, z, weights)) return std::nullopt;
  return SurfaceSupportHit{
      record.face, math::interpolate(vertices[0], vertices[1], vertices[2], weights), record.normal};
}

void SurfaceSupportIndex::visitCandidatesForXz(float x, float z, TriangleIndex::CandidateVisitor visitor,
                                               void* user_data) const {
  if (visitor == nullptr) return;
  struct Context {
    const SurfaceSupportIndex* index = nullptr;
    TriangleIndex::CandidateVisitor visitor = nullptr;
    void* user_data = nullptr;
    float x = 0.0f;
    float z = 0.0f;
  } context{this, visitor, user_data, x, z};
  grid_index_.visitPointCandidates(
      x,
      z,
      [](std::uint32_t index, void* raw) {
        const auto& context = *static_cast<const Context*>(raw);
        if (math::containsXzInclusive(context.index->storage_->records[index].triangle.bounds, context.x, context.z))
          context.visitor(index, context.user_data);
      },
      &context);
}

void SurfaceSupportIndex::visitHitsAt(float x, float z, HitVisitor visitor, void* user_data) const {
  if (visitor == nullptr) return;
  struct Context {
    const SurfaceSupportIndex* index = nullptr;
    HitVisitor visitor = nullptr;
    void* user_data = nullptr;
    float x = 0.0f;
    float z = 0.0f;
  } context{this, visitor, user_data, x, z};
  visitCandidatesForXz(
      x,
      z,
      [](std::uint32_t index, void* raw) {
        auto& context = *static_cast<Context*>(raw);
        if (std::optional<SurfaceSupportHit> hit = context.index->hitAt(index, context.x, context.z))
          context.visitor(*hit, context.user_data);
      },
      &context);
}

void SurfaceSupportIndex::findHitsAt(std::vector<SurfaceSupportHit>& out, float x, float z) const {
  out.clear();
  visitHitsAt(
      x,
      z,
      [](const SurfaceSupportHit& hit, void* raw) {
        auto& hits = *static_cast<std::vector<SurfaceSupportHit>*>(raw);
        hits.push_back(hit);
      },
      &out);
  std::sort(out.begin(), out.end(), supportHitLess);
}

void SurfaceSupportIndex::findDownwardHitsAt(std::vector<SurfaceSupportHit>& out, float x, float z,
                                             float origin_y) const {
  findHitsAt(out, x, z);
  out.erase(std::remove_if(out.begin(),
                           out.end(),
                           [origin_y](const SurfaceSupportHit& hit) { return hit.position.y - origin_y < 0.0f; }),
            out.end());
}

std::optional<SurfaceSupportHit> SurfaceSupportIndex::closestDownwardHit(float x, float z, float origin_y) const {
  struct Context {
    const SurfaceSupportIndex* index = nullptr;
    std::optional<SurfaceSupportHit> best;
    float x = 0.0f;
    float z = 0.0f;
    float origin_y = 0.0f;
  } context{this, std::nullopt, x, z, origin_y};
  visitCandidatesForXz(
      x,
      z,
      [](std::uint32_t index, void* raw) {
        auto& context = *static_cast<Context*>(raw);
        std::optional<SurfaceSupportHit> hit = context.index->hitAt(index, context.x, context.z);
        if (!hit || hit->position.y < context.origin_y) return;
        if (!context.best || hit->position.y < context.best->position.y) context.best = *hit;
      },
      &context);
  return context.best;
}

std::optional<SurfaceSupportHit> SurfaceSupportIndex::closestHit(float x, float z, float reference_y,
                                                                 float max_delta) const {
  struct Context {
    const SurfaceSupportIndex* index = nullptr;
    std::optional<SurfaceSupportHit> best;
    float x = 0.0f;
    float z = 0.0f;
    float reference_y = 0.0f;
    float best_delta = std::numeric_limits<float>::max();
  } context{this, std::nullopt, x, z, reference_y};
  visitCandidatesForXz(
      x,
      z,
      [](std::uint32_t index, void* raw) {
        auto& context = *static_cast<Context*>(raw);
        std::optional<SurfaceSupportHit> hit = context.index->hitAt(index, context.x, context.z);
        if (!hit) return;
        const float delta = std::abs(hit->position.y - context.reference_y);
        if (delta >= context.best_delta) return;
        context.best = *hit;
        context.best_delta = delta;
      },
      &context);
  if (!context.best || context.best_delta > max_delta) return std::nullopt;
  return context.best;
}

void mergeSurfaceSupportHits(std::vector<SurfaceSupportHit>& hits, float merge_distance) {
  std::sort(hits.begin(), hits.end(), supportHitLess);
  mergeSortedSurfaceSupportHits(hits, merge_distance);
}

void mergeSortedSurfaceSupportHits(std::vector<SurfaceSupportHit>& hits, float merge_distance) {
  assert(std::is_sorted(hits.begin(), hits.end(), supportHitLess));
  if (!std::isfinite(merge_distance) || merge_distance < 0.0f) return;
  const double merge_distance_squared = static_cast<double>(merge_distance) * merge_distance;
  std::size_t retained = hits.empty() ? 0 : 1;
  for (std::size_t index = 1; index < hits.size(); ++index) {
    if (math::lengthSquared(hits[retained - 1U].position - hits[index].position) <= merge_distance_squared) continue;
    if (retained != index) hits[retained] = hits[index];
    ++retained;
  }
  hits.resize(retained);
}

SurfaceSupportIndex buildSurfaceSupportIndex(const GeometryData& geometry) {
  SurfaceSupportIndexBuilder builder(geometry.faces.size());
  for (std::size_t index = 0; index < geometry.faces.size(); ++index) {
    const FaceIndex face = static_cast<FaceIndex>(index);
    builder.addTriangle(face, facePositions(geometry, geometry.faces[index]));
  }
  return std::move(builder).build();
}

SurfaceSupportIndex buildSurfaceSupportIndex(const GeometryData& geometry, FacePredicate predicate,
                                             std::vector<FaceIndex>& face_scratch) {
  if (predicate.function == nullptr) return buildSurfaceSupportIndex(geometry);
  face_scratch.clear();
  face_scratch.reserve(geometry.faces.size());
  for (std::size_t face_index = 0; face_index < geometry.faces.size(); ++face_index) {
    const FaceIndex face = static_cast<FaceIndex>(face_index);
    if (!predicate.function(face, predicate.user_data)) continue;
    face_scratch.push_back(face);
  }
  SurfaceSupportIndexBuilder builder(face_scratch.size());
  for (FaceIndex face : face_scratch) builder.addTriangle(face, facePositions(geometry, geometry.faces[face]));
  return std::move(builder).build();
}

SurfaceSupportIndex buildSurfaceSupportIndex(const GeometryData& geometry, std::span<const FaceIndex> face_indices) {
  const std::size_t valid_count = static_cast<std::size_t>(std::count_if(
      face_indices.begin(), face_indices.end(), [&](FaceIndex face) { return face < geometry.faces.size(); }));
  SurfaceSupportIndexBuilder builder(valid_count);
  for (FaceIndex face : face_indices) {
    if (face >= geometry.faces.size()) continue;
    builder.addTriangle(face, facePositions(geometry, geometry.faces[face]));
  }
  return std::move(builder).build();
}

}  // namespace pistoris::geometry
