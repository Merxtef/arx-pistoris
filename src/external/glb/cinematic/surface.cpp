// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#include "surface.h"

#include "arx_pistoris/base/math.hpp"

#include "external/glb/accessor.h"
#include "utils/math/geometry_algorithms.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <limits>

namespace pistoris::glb_cinematic {
namespace {

constexpr double kEpsilon = 1.0e-8;
constexpr float kAffineTolerance = 1.0e-4f;
constexpr float kHeightTolerance = 1.0e-4f;

bool weights(const std::array<ArxVector2, 3>& triangle, const ArxVector2& point, std::array<double, 3>& out) noexcept {
  const double denominator = (static_cast<double>(triangle[1].y) - triangle[2].y) * (triangle[0].x - triangle[2].x) +
                             (static_cast<double>(triangle[2].x) - triangle[1].x) * (triangle[0].y - triangle[2].y);
  if (!std::isfinite(denominator) || std::abs(denominator) <= kEpsilon) return false;
  out[0] = ((static_cast<double>(triangle[1].y) - triangle[2].y) * (point.x - triangle[2].x) +
            (static_cast<double>(triangle[2].x) - triangle[1].x) * (point.y - triangle[2].y)) /
           denominator;
  out[1] = ((static_cast<double>(triangle[2].y) - triangle[0].y) * (point.x - triangle[2].x) +
            (static_cast<double>(triangle[0].x) - triangle[2].x) * (point.y - triangle[2].y)) /
           denominator;
  out[2] = 1.0 - out[0] - out[1];
  return std::isfinite(out[0]) && std::isfinite(out[1]) && std::isfinite(out[2]);
}

glb::Vec2 interpolate(const SurfaceTriangle& triangle, const std::array<double, 3>& values) noexcept {
  return {
      static_cast<float>(values[0] * triangle.texcoord[0].x + values[1] * triangle.texcoord[1].x +
                         values[2] * triangle.texcoord[2].x),
      static_cast<float>(values[0] * triangle.texcoord[0].y + values[1] * triangle.texcoord[1].y +
                         values[2] * triangle.texcoord[2].y),
  };
}

bool near(glb::Vec2 first, glb::Vec2 second) noexcept {
  return std::abs(first.x - second.x) <= kAffineTolerance && std::abs(first.y - second.y) <= kAffineTolerance;
}

double distanceSquared(const ArxVector2& first, const ArxVector2& second) noexcept {
  const double x = static_cast<double>(first.x) - second.x;
  const double y = static_cast<double>(first.y) - second.y;
  return x * x + y * y;
}

}  // namespace

void IllustrationSurface::addTriangle(SurfaceTriangle triangle) { triangles_.push_back(triangle); }

void IllustrationSurface::includePosition(const ArxVector2& position) noexcept {
  if (!has_position_) {
    bounds_.min = position;
    bounds_.max = position;
    has_position_ = true;
    return;
  }
  bounds_.min.x = std::min(bounds_.min.x, position.x);
  bounds_.min.y = std::min(bounds_.min.y, position.y);
  bounds_.max.x = std::max(bounds_.max.x, position.x);
  bounds_.max.y = std::max(bounds_.max.y, position.y);
}

void IllustrationSurface::noteHeight(float height) noexcept {
  height_sum_ += height;
  ++height_count_;
  if (!has_height_) {
    first_height_ = height;
    has_height_ = true;
  } else if (std::abs(height - first_height_) > kHeightTolerance) {
    varying_height_ = true;
  }
}

float IllustrationSurface::averageHeight() const noexcept {
  return height_count_ != 0 ? static_cast<float>(height_sum_ / static_cast<double>(height_count_)) : 0.0f;
}

void IllustrationSurface::noteMissingTexcoords() noexcept { missing_texcoords_ = true; }

bool IllustrationSurface::finalize() noexcept {
  if (!has_position_) return false;
  const bool collapsed_x = !(bounds_.min.x < bounds_.max.x);
  const bool collapsed_y = !(bounds_.min.y < bounds_.max.y);
  if (triangles_.empty()) {
    if (collapsed_x && collapsed_y) return false;
    collapsed_axis_ = collapsed_x || collapsed_y;
    mapping_mode_ = SurfaceMappingMode::kBounds;
    return true;
  }

  mapping_mode_ = SurfaceMappingMode::kAffineUv;
  const SurfaceTriangle& basis = triangles_.front();
  for (const SurfaceTriangle& triangle : triangles_) {
    for (std::size_t corner = 0; corner < triangle.position.size(); ++corner) {
      std::array<double, 3> affine{};
      if (!weights(basis.position, triangle.position[corner], affine) ||
          !near(interpolate(basis, affine), triangle.texcoord[corner])) {
        mapping_mode_ = SurfaceMappingMode::kPiecewiseUv;
        return true;
      }
    }
  }
  return true;
}

bool IllustrationSurface::sample(const ArxVector2& position, SurfaceSample& out) const noexcept {
  if (mapping_mode_ == SurfaceMappingMode::kBounds) {
    const float width = bounds_.max.x - bounds_.min.x;
    const float height = bounds_.max.y - bounds_.min.y;
    if (!(width > 0.0f) && !(height > 0.0f)) return false;
    out.texcoord = {
        width > 0.0f ? (position.x - bounds_.min.x) / width : 0.5f,
        height > 0.0f ? (position.y - bounds_.min.y) / height : 0.5f,
    };
    out.outside = position.x < bounds_.min.x || position.x > bounds_.max.x || position.y < bounds_.min.y ||
                  position.y > bounds_.max.y;
    return true;
  }

  if (mapping_mode_ == SurfaceMappingMode::kAffineUv) {
    std::array<double, 3> affine{};
    if (!weights(triangles_.front().position, position, affine)) return false;
    out.texcoord = interpolate(triangles_.front(), affine);
    return true;
  }

  bool found = false;
  glb::Vec2 selected{};
  for (const SurfaceTriangle& triangle : triangles_) {
    std::array<double, 3> affine{};
    if (!weights(triangle.position, position, affine)) continue;
    constexpr double kInsideTolerance = 1.0e-6;
    if (affine[0] < -kInsideTolerance || affine[1] < -kInsideTolerance || affine[2] < -kInsideTolerance) continue;
    const glb::Vec2 current = interpolate(triangle, affine);
    if (found && !near(selected, current)) out.ambiguous = true;
    if (!found) selected = current;
    found = true;
  }
  if (found) {
    out.texcoord = selected;
    return true;
  }

  double nearest_distance = std::numeric_limits<double>::infinity();
  const SurfaceTriangle* nearest = nullptr;
  for (const SurfaceTriangle& triangle : triangles_) {
    const std::array<ArxVector3, 3> vertices = {{
        {triangle.position[0].x, 0.0f, triangle.position[0].y},
        {triangle.position[1].x, 0.0f, triangle.position[1].y},
        {triangle.position[2].x, 0.0f, triangle.position[2].y},
    }};
    std::array<double, 3> closest{};
    if (!math::closestTriangleWeightsXz(vertices, {position.x, position.y}, closest)) continue;
    const ArxVector2 projected{
        static_cast<float>(closest[0] * triangle.position[0].x + closest[1] * triangle.position[1].x +
                           closest[2] * triangle.position[2].x),
        static_cast<float>(closest[0] * triangle.position[0].y + closest[1] * triangle.position[1].y +
                           closest[2] * triangle.position[2].y),
    };
    const double distance = distanceSquared(position, projected);
    if (distance < nearest_distance) {
      nearest_distance = distance;
      nearest = &triangle;
    }
  }
  if (nearest == nullptr) return false;
  std::array<double, 3> affine{};
  if (!weights(nearest->position, position, affine)) return false;
  out.texcoord = interpolate(*nearest, affine);
  out.outside = true;
  return true;
}

float IllustrationSurface::pixelsPerUnit(const ArxVector2& image_size) const noexcept {
  const float width = bounds_.max.x - bounds_.min.x;
  const float height = bounds_.max.y - bounds_.min.y;
  const bool vertical = std::isfinite(height) && height > 0.0f;
  const float extent = vertical ? height : width;
  if (!std::isfinite(extent) || extent <= 0.0f) return 0.0f;

  const float middle_x = 0.5f * bounds_.min.x + 0.5f * bounds_.max.x;
  const float middle_y = 0.5f * bounds_.min.y + 0.5f * bounds_.max.y;
  const ArxVector2 first = vertical ? ArxVector2{middle_x, bounds_.min.y} : ArxVector2{bounds_.min.x, middle_y};
  const ArxVector2 last = vertical ? ArxVector2{middle_x, bounds_.max.y} : ArxVector2{bounds_.max.x, middle_y};
  SurfaceSample start;
  SurfaceSample end;
  if (!sample(first, start) || !sample(last, end)) return 0.0f;
  const double dx = static_cast<double>(end.texcoord.x - start.texcoord.x) * image_size.x;
  const double dy = static_cast<double>(end.texcoord.y - start.texcoord.y) * image_size.y;
  const double scale = std::hypot(dx, dy) / extent;
  return std::isfinite(scale) && scale > 0.0 ? static_cast<float>(scale) : 0.0f;
}

}  // namespace pistoris::glb_cinematic
