// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#include "normals.h"

#include "arx_pistoris/base/math.hpp"
#include "arx_pistoris/base/status.h"

#include "modules/geometry.h"
#include "topology.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <limits>
#include <span>
#include <tuple>
#include <utility>
#include <vector>

namespace pistoris::glb_level {
namespace {

struct NormalCluster {
  std::vector<ArxVector3> directions;
  ArxVector3 representative = {};
};

ArxVector3 normalizeNormal(const ArxVector3& value) {
  return math::normalizeFiniteOr(value, {0.0f, 1.0f, 0.0f}, std::numeric_limits<float>::epsilon());
}
bool sameNormal(const ArxVector3& a, const ArxVector3& b) {
  return std::abs(a.x - b.x) <= kLevelGlbEpsilon && std::abs(a.y - b.y) <= kLevelGlbEpsilon &&
         std::abs(a.z - b.z) <= kLevelGlbEpsilon;
}
bool normalLess(const ArxVector3& a, const ArxVector3& b) { return std::tie(a.x, a.y, a.z) < std::tie(b.x, b.y, b.z); }
float clusterMinDot(const NormalCluster& a, const NormalCluster& b) {
  float min_dot = 1.0f;
  for (const ArxVector3& lhs : a.directions)
    for (const ArxVector3& rhs : b.directions) min_dot = std::min(min_dot, math::dotf(lhs, rhs));
  return min_dot;
}
ArxVector3 clusterRepresentative(const NormalCluster& cluster) {
  ArxVector3 sum{};
  for (const ArxVector3& direction : cluster.directions) {
    sum.x += direction.x;
    sum.y += direction.y;
    sum.z += direction.z;
  }
  return normalizeNormal(sum);
}

ArxVector3 combinedRepresentative(std::span<const NormalCluster> clusters) {
  if (clusters.empty()) return {0.0f, 1.0f, 0.0f};
  if (clusters.size() == 1) return clusters.front().representative;
  ArxVector3 sum{};
  for (const NormalCluster& cluster : clusters) {
    sum.x += cluster.representative.x;
    sum.y += cluster.representative.y;
    sum.z += cluster.representative.z;
  }
  if (math::lengthf(sum) > kLevelGlbEpsilon) return normalizeNormal(sum);
  return clusters.front().representative;
}

}  // namespace

ArxReturnCode analyzeLevelNormals(std::span<const Vertex> vertices, std::span<const Face> faces,
                                  float normal_weld_degrees, LevelNormalAnalysis& out) {
  if (!std::isfinite(normal_weld_degrees) || normal_weld_degrees < 0.0f || normal_weld_degrees >= 90.0f)
    return ARX_GLB_BAD_FORMAT;
  constexpr float kPi = 3.14159265358979323846f;
  float min_dot = normal_weld_degrees == 0.0f ? 1.0f : std::cos(normal_weld_degrees * kPi / 180.0f);

  struct CornerSample {
    ArxVector3 direction = {};
  };

  LevelNormalAnalysis tmp;
  tmp.vertices.resize(vertices.size());
  std::vector<std::vector<CornerSample>> samples(vertices.size());

  for (std::size_t face_index = 0; face_index < faces.size(); ++face_index) {
    const Face& face = faces[face_index];
    std::array<ArxVector3, 3> positions{};
    for (std::size_t corner_index = 0; corner_index < 3; ++corner_index) {
      const Corner& corner = face.corners[corner_index];
      if (corner.vertex >= vertices.size()) return ARX_GLB_BAD_FORMAT;
      positions[corner_index] = vertices[corner.vertex].position;
    }
    ArxVector3 face_normal = normalizeNormal(math::cross(positions[1] - positions[0], positions[2] - positions[0]));
    for (std::size_t corner_index = 0; corner_index < 3; ++corner_index) {
      const Corner& corner = face.corners[corner_index];
      ArxVector3 direction = corner.normal;
      if (math::lengthf(direction) <= kLevelGlbEpsilon) direction = face_normal;
      direction = normalizeNormal(direction);
      samples[corner.vertex].push_back({direction});
    }
  }

  for (std::size_t vertex_index = 0; vertex_index < samples.size(); ++vertex_index) {
    auto& vertex_samples = samples[vertex_index];
    std::vector<ArxVector3> unique_directions;
    unique_directions.reserve(vertex_samples.size());
    for (const CornerSample& sample : vertex_samples) unique_directions.push_back(sample.direction);
    std::sort(unique_directions.begin(), unique_directions.end(), normalLess);
    unique_directions.erase(std::unique(unique_directions.begin(), unique_directions.end(), sameNormal),
                            unique_directions.end());

    std::vector<NormalCluster> clusters;
    clusters.reserve(unique_directions.size());
    for (const ArxVector3& direction : unique_directions) clusters.push_back({{direction}, direction});

    while (true) {
      std::size_t best_a = clusters.size();
      std::size_t best_b = clusters.size();
      float best_dot = -1.0f;
      for (std::size_t a = 0; a < clusters.size(); ++a) {
        for (std::size_t b = a + 1; b < clusters.size(); ++b) {
          float pair_min_dot = clusterMinDot(clusters[a], clusters[b]);
          if (pair_min_dot < min_dot) continue;
          if (best_a == clusters.size() || pair_min_dot > best_dot) {
            best_a = a;
            best_b = b;
            best_dot = pair_min_dot;
          }
        }
      }
      if (best_a == clusters.size()) break;
      auto& destination = clusters[best_a].directions;
      const auto& source = clusters[best_b].directions;
      destination.insert(destination.end(), source.begin(), source.end());
      std::sort(destination.begin(), destination.end(), normalLess);
      clusters.erase(clusters.begin() + static_cast<std::ptrdiff_t>(best_b));
    }

    for (NormalCluster& cluster : clusters) cluster.representative = clusterRepresentative(cluster);
    tmp.vertices[vertex_index] = {combinedRepresentative(clusters), clusters.size()};
  }

  out = std::move(tmp);
  return ARX_OK;
}

}  // namespace pistoris::glb_level
