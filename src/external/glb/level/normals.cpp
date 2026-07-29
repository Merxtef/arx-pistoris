// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#include "normals.h"

#include "arx_pistoris/arx_math.h"
#include "arx_pistoris/level/types.h"
#include "arx_pistoris/pistoris_types.h"

#include "topology.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <span>
#include <tuple>
#include <utility>
#include <vector>

namespace pistoris::glb_level {
namespace {

struct Vec3 {
  float x = 0.0f;
  float y = 0.0f;
  float z = 0.0f;
};

Vec3 toVec3(const ArxVector3& value) { return {value.x, value.y, value.z}; }
ArxVector3 toArx(const Vec3& value) { return {value.x, value.y, value.z}; }
Vec3 sub(Vec3 a, Vec3 b) { return {a.x - b.x, a.y - b.y, a.z - b.z}; }
Vec3 cross(Vec3 a, Vec3 b) { return {a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x}; }
float dot(Vec3 a, Vec3 b) { return a.x * b.x + a.y * b.y + a.z * b.z; }
float length(Vec3 value) { return std::sqrt(dot(value, value)); }
Vec3 normalize(Vec3 value) {
  float value_length = length(value);
  if (value_length <= std::numeric_limits<float>::epsilon()) return {0.0f, 1.0f, 0.0f};
  return {value.x / value_length, value.y / value_length, value.z / value_length};
}
bool sameNormal(const ArxVector3& a, const ArxVector3& b) {
  return std::abs(a.x - b.x) <= kLevelGlbEpsilon && std::abs(a.y - b.y) <= kLevelGlbEpsilon &&
         std::abs(a.z - b.z) <= kLevelGlbEpsilon;
}
bool normalLess(const ArxVector3& a, const ArxVector3& b) { return std::tie(a.x, a.y, a.z) < std::tie(b.x, b.y, b.z); }
float clusterMinDot(const LevelNormalCluster& a, const LevelNormalCluster& b) {
  float min_dot = 1.0f;
  for (const ArxVector3& lhs : a.directions)
    for (const ArxVector3& rhs : b.directions) min_dot = std::min(min_dot, dot(toVec3(lhs), toVec3(rhs)));
  return min_dot;
}
ArxVector3 clusterRepresentative(const LevelNormalCluster& cluster) {
  ArxVector3 sum{};
  for (const ArxVector3& direction : cluster.directions) {
    sum.x += direction.x;
    sum.y += direction.y;
    sum.z += direction.z;
  }
  return toArx(normalize(toVec3(sum)));
}

}  // namespace

ArxReturnCode analyzeLevelNormals(std::span<const ArxLevelVertex> vertices, std::span<const ArxLevelFace> faces,
                                  float normal_weld_degrees, LevelNormalAnalysis& out) {
  if (!std::isfinite(normal_weld_degrees) || normal_weld_degrees < 0.0f || normal_weld_degrees >= 90.0f)
    return ARX_GLB_BAD_FORMAT;
  constexpr float kPi = 3.14159265358979323846f;
  float min_dot = normal_weld_degrees == 0.0f ? 1.0f : std::cos(normal_weld_degrees * kPi / 180.0f);

  struct CornerSample {
    ArxVector3 direction = {};
    std::size_t face = 0;
    std::size_t corner = 0;
  };

  LevelNormalAnalysis tmp;
  tmp.clusters.resize(vertices.size());
  tmp.corner_clusters.resize(faces.size());
  std::vector<std::vector<CornerSample>> samples(vertices.size());

  for (std::size_t face_index = 0; face_index < faces.size(); ++face_index) {
    const ArxLevelFace& face = faces[face_index];
    std::array<ArxVector3, 3> positions{};
    for (std::size_t corner_index = 0; corner_index < 3; ++corner_index) {
      const ArxLevelCorner& corner = face.corners[corner_index];
      if (corner.vertex >= vertices.size()) return ARX_GLB_BAD_FORMAT;
      positions[corner_index] = vertices[corner.vertex].position;
    }
    ArxVector3 face_normal = toArx(normalize(
        cross(sub(toVec3(positions[1]), toVec3(positions[0])), sub(toVec3(positions[2]), toVec3(positions[0])))));
    for (std::size_t corner_index = 0; corner_index < 3; ++corner_index) {
      const ArxLevelCorner& corner = face.corners[corner_index];
      ArxVector3 direction = corner.normal;
      if (length(toVec3(direction)) <= kLevelGlbEpsilon) direction = face_normal;
      direction = toArx(normalize(toVec3(direction)));
      samples[corner.vertex].push_back({direction, face_index, corner_index});
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

    std::vector<LevelNormalCluster> clusters;
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

    for (LevelNormalCluster& cluster : clusters) cluster.representative = clusterRepresentative(cluster);
    for (const CornerSample& sample : vertex_samples) {
      for (std::size_t cluster_index = 0; cluster_index < clusters.size(); ++cluster_index) {
        const auto& directions = clusters[cluster_index].directions;
        if (std::find_if(directions.begin(), directions.end(), [&](const ArxVector3& direction) {
              return sameNormal(direction, sample.direction);
            }) == directions.end()) {
          continue;
        }
        tmp.corner_clusters[sample.face][sample.corner] = static_cast<std::uint32_t>(cluster_index);
        break;
      }
    }
    tmp.clusters[vertex_index] = std::move(clusters);
  }

  out = std::move(tmp);
  return ARX_OK;
}

ArxVector3 debugVertexNormal(std::span<const LevelNormalCluster> clusters) {
  if (clusters.empty()) return {0.0f, 1.0f, 0.0f};
  if (clusters.size() == 1) return clusters.front().representative;
  ArxVector3 sum{};
  for (const LevelNormalCluster& cluster : clusters) {
    sum.x += cluster.representative.x;
    sum.y += cluster.representative.y;
    sum.z += cluster.representative.z;
  }
  if (length(toVec3(sum)) > kLevelGlbEpsilon) return toArx(normalize(toVec3(sum)));
  return clusters.front().representative;
}

}  // namespace pistoris::glb_level
