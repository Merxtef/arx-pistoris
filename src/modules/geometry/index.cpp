// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#include "arx_pistoris/base/math.hpp"

#include "modules/geometry.h"
#include "utils/math/bounds.h"
#include "utils/spatial/arx_level_grid_index.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <span>
#include <utility>
#include <vector>

namespace pistoris::geometry {
namespace {

void sortUnique(std::vector<std::uint32_t>& values) {
  std::sort(values.begin(), values.end());
  values.erase(std::unique(values.begin(), values.end()), values.end());
}

ArxAabb segmentBounds(const ArxVector3& a, const ArxVector3& b) {
  ArxAabb out;
  out.min = math::componentMin(a, b);
  out.max = math::componentMax(a, b);
  return out;
}

}  // namespace

TriangleIndex::TriangleIndex(std::span<const IndexedTriangle> triangles)
    : triangles_(triangles.begin(), triangles.end()) {
  buildIndex();
}

TriangleIndex::TriangleIndex(std::vector<IndexedTriangle>&& triangles) : triangles_(std::move(triangles)) {
  buildIndex();
}

void TriangleIndex::buildIndex() {
  for (std::size_t index = 0; index < triangles_.size(); ++index) {
    if (index > std::numeric_limits<std::uint32_t>::max()) break;
    const std::uint32_t triangle_index = static_cast<std::uint32_t>(index);
    grid_index_.add(triangle_index, triangles_[triangle_index].bounds);
  }
}

std::size_t TriangleIndex::size() const noexcept { return triangles_.size(); }

const IndexedTriangle& TriangleIndex::triangle(std::uint32_t index) const { return triangles_[index]; }

void TriangleIndex::visitCandidatesForXz(float x, float z, CandidateVisitor visitor, void* user_data) const {
  if (visitor == nullptr) return;
  struct Context {
    const TriangleIndex* index;
    CandidateVisitor visitor;
    void* user_data;
    float x;
    float z;
  } context{this, visitor, user_data, x, z};
  grid_index_.visitPointCandidates(
      x,
      z,
      [](std::uint32_t index, void* data) {
        const Context& context = *static_cast<const Context*>(data);
        if (math::containsXzInclusive(context.index->triangles_[index].bounds, context.x, context.z))
          context.visitor(index, context.user_data);
      },
      &context);
}

void TriangleIndex::findCandidatesForXz(std::vector<std::uint32_t>& out, float x, float z) const {
  out.clear();
  visitCandidatesForXz(
      x,
      z,
      [](std::uint32_t index, void* data) { static_cast<std::vector<std::uint32_t>*>(data)->push_back(index); },
      &out);
  sortUnique(out);
}

void TriangleIndex::findCandidatesForAabb(std::vector<std::uint32_t>& out, const ArxAabb& bounds) const {
  out.clear();
  grid_index_.findAabbCandidates(out, bounds);
  out.erase(
      std::remove_if(out.begin(),
                     out.end(),
                     [&](std::uint32_t index) { return !math::overlapsInclusive(triangles_[index].bounds, bounds); }),
      out.end());
}

void TriangleIndex::findCandidatesForSegment(std::vector<std::uint32_t>& out, const ArxVector3& a,
                                             const ArxVector3& b) const {
  findCandidatesForAabb(out, segmentBounds(a, b));
}

}  // namespace pistoris::geometry
