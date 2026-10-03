// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#include "arx_pistoris/base/indices.h"
#include "arx_pistoris/base/math.h"

#include "modules/geometry.h"
#include "modules/geometry/internal.h"
#include "utils/math/finite.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <optional>
#include <vector>

namespace pistoris::geometry {
namespace {

// Exact candidate checks preserve correctness when extreme cells collapse at this limit
constexpr double kPositionKeyLimit = 0x1p62;

std::int64_t positionKeyComponent(float coordinate, double scale) noexcept {
  double cell = std::floor(static_cast<double>(coordinate) * scale);
  return static_cast<std::int64_t>(std::clamp(cell, -kPositionKeyLimit, kPositionKeyLimit));
}

std::array<PositionKey, 27> nearbyKeys(PositionKey key) {
  std::array<PositionKey, 27> keys{};
  std::size_t index = 0;
  for (std::int64_t z = -1; z <= 1; ++z) {
    for (std::int64_t y = -1; y <= 1; ++y) {
      for (std::int64_t x = -1; x <= 1; ++x) keys[index++] = {key.x + x, key.y + y, key.z + z};
    }
  }
  return keys;
}

}  // namespace

std::size_t PositionKeyHash::operator()(const PositionKey& key) const {
  std::size_t seed = std::hash<std::int64_t>{}(key.x);
  seed ^= std::hash<std::int64_t>{}(key.y) + 0x9e3779b9U + (seed << 6U) + (seed >> 2U);
  seed ^= std::hash<std::int64_t>{}(key.z) + 0x9e3779b9U + (seed << 6U) + (seed >> 2U);
  return seed;
}

PositionIndex::PositionIndex(float radius, PositionWeldMetric metric) : radius_(radius), metric_(metric) {}

void PositionIndex::reservePositionCapacity(std::size_t capacity) { by_position_.reserve(capacity); }

PositionKey PositionIndex::key(const ArxVector3& position) const {
  double scale = 1.0 / static_cast<double>(radius_);
  return {positionKeyComponent(position.x, scale),
          positionKeyComponent(position.y, scale),
          positionKeyComponent(position.z, scale)};
}

bool PositionIndex::valid() const noexcept {
  return radius_ > 0.0f && std::isfinite(radius_) && validPositionWeldMetric(metric_);
}

bool PositionIndex::samePosition(const ArxVector3& a, const ArxVector3& b) const noexcept {
  switch (metric_) {
    case PositionWeldMetric::kEuclidean: {
      double dx = static_cast<double>(a.x) - static_cast<double>(b.x);
      double dy = static_cast<double>(a.y) - static_cast<double>(b.y);
      double dz = static_cast<double>(a.z) - static_cast<double>(b.z);
      double radius = static_cast<double>(radius_);
      return dx * dx + dy * dy + dz * dz <= radius * radius;
    }
    case PositionWeldMetric::kAxisAligned:
      return std::abs(a.x - b.x) <= radius_ && std::abs(a.y - b.y) <= radius_ && std::abs(a.z - b.z) <= radius_;
  }
  return false;
}

bool PositionIndex::tryAdd(std::uint32_t index, const ArxVector3& position) {
  if (!valid() || !math::finite(position)) return false;
  by_position_[key(position)].push_back({index, position});
  return true;
}

void PositionIndex::findCandidates(std::vector<std::uint32_t>& out, const ArxVector3& position) const {
  out.clear();
  if (!valid() || !math::finite(position)) return;
  for (PositionKey nearby : nearbyKeys(key(position))) {
    auto it = by_position_.find(nearby);
    if (it == by_position_.end()) continue;
    for (const Entry& entry : it->second)
      if (samePosition(entry.position, position)) out.push_back(entry.index);
  }
  std::sort(out.begin(), out.end());
  out.erase(std::unique(out.begin(), out.end()), out.end());
}

std::optional<std::uint32_t> PositionIndex::find(const ArxVector3& position) const {
  std::optional<std::uint32_t> best;
  if (!valid() || !math::finite(position)) return best;
  for (PositionKey nearby : nearbyKeys(key(position))) {
    auto it = by_position_.find(nearby);
    if (it == by_position_.end()) continue;
    for (const Entry& entry : it->second) {
      if (!samePosition(entry.position, position)) continue;
      if (!best.has_value() || entry.index < *best) best = entry.index;
    }
  }
  return best;
}

VertexIndex addOrFindVertex(GeometryData& geometry, PositionIndex& index, const ArxVector3& position) {
  if (index.enabled()) {
    if (std::optional<std::uint32_t> existing = index.find(position)) return static_cast<VertexIndex>(*existing);
  }
  const Vertex vertex{.position = position};
  if (geometry.vertices.size() >= static_cast<std::size_t>(kInvalidVertexIndex) ||
      validateVertex(vertex) != Error::kNone)
    return kInvalidVertexIndex;
  const VertexIndex vertex_index = addVertex(geometry, vertex);
  try {
    if (index.enabled() && !index.tryAdd(vertex_index, position)) {
      geometry.vertices.pop_back();
      return kInvalidVertexIndex;
    }
  } catch (...) {
    geometry.vertices.pop_back();
    throw;
  }
  return vertex_index;
}

}  // namespace pistoris::geometry
