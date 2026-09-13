// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#include "arx_pistoris/base/flags.h"
#include "arx_pistoris/base/indices.h"
#include "arx_pistoris/base/math.hpp"
#include "arx_pistoris/runtime/types.h"

#include "modules/geometry.h"
#include "modules/lights.h"
#include "modules/lights/internal.h"
#include "utils/log.h"
#include "utils/math/finite.h"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <span>
#include <utility>
#include <vector>

namespace pistoris::lights {
namespace {

constexpr double kShadowEndpointEpsilon = 1.0e-4;

bool validOptions(const StaticLightingGenerationOptions& options) {
  return unitColor(options.ambient_color) && math::finite(options.global_factor) && options.global_factor >= 0.0f;
}

std::vector<geometry::IndexedTriangle> indexedGeometryTriangles(const GeometryData& geometry) {
  std::vector<geometry::IndexedTriangle> triangles;
  triangles.reserve(geometry.faces.size());
  for (const Face& face : geometry.faces) {
    std::array<ArxVector3, 3> vertices = geometry::facePositions(geometry, face);
    triangles.push_back({vertices, geometry::triangleBounds(vertices)});
  }
  return triangles;
}

bool shadowSegmentIntersectsTriangle(const ArxVector3& start, const ArxVector3& end,
                                     const geometry::IndexedTriangle& triangle) {
  double t = 0.0;
  if (!geometry::segmentTriangleIntersectionT(
          start, end, triangle.vertices[0], triangle.vertices[1], triangle.vertices[2], t))
    return false;
  return t > kShadowEndpointEpsilon && t < 1.0 - kShadowEndpointEpsilon;
}

bool segmentOccluded(const geometry::TriangleIndex& index, const ArxVector3& start, const ArxVector3& end,
                     FaceIndex target_face, std::vector<std::uint32_t>& candidates) {
  index.findCandidatesForSegment(candidates, start, end);
  for (std::uint32_t candidate : candidates) {
    if (candidate == target_face) continue;
    if (shadowSegmentIntersectsTriangle(start, end, index.triangle(candidate))) return true;
  }
  return false;
}

bool activeStaticLight(const Light& light) {
  if ((light.flags & kLightFlagSemidynamic) != 0) return false;
  if (light.intensity <= 0.0f) return false;
  if (light.fallend <= 0.0f) return false;
  return true;
}

float lightAttenuation(const Light& light, float distance) {
  if (distance >= light.fallend) return 0.0f;
  if (distance <= light.fallstart) return 1.0f;
  if (light.fallend <= light.fallstart) return 0.0f;
  return (light.fallend - distance) / (light.fallend - light.fallstart);
}

float normalResponse(const ArxVector3& normal, const ArxVector3& to_light, float distance) {
  if (distance <= std::numeric_limits<float>::epsilon()) return 1.0f;
  return std::max(0.0f, math::dotf(normal, to_light / distance));
}

ArxColor3 clampGeneratedColor(const std::array<double, 3>& value, const ArxColor3& ambient) {
  return {static_cast<float>(std::clamp(value[0], static_cast<double>(ambient.r), 1.0)),
          static_cast<float>(std::clamp(value[1], static_cast<double>(ambient.g), 1.0)),
          static_cast<float>(std::clamp(value[2], static_cast<double>(ambient.b), 1.0))};
}

}  // namespace

Error generateStaticLighting(std::vector<ArxColor3>& out, const GeometryData& geometry, std::span<const Light> lights,
                             const StaticLightingGenerationOptions& options, StaticLightingDiagnostics* diagnostics) {
  if (!validOptions(options)) return Error::kInvalidOptions;
  StaticLightingDiagnostics collected;
  const bool log_summary = log_fn != nullptr;
  StaticLightingDiagnostics* stats = diagnostics != nullptr ? diagnostics : (log_summary ? &collected : nullptr);
  if (stats) *stats = {};

  std::vector<ArxColor3> corner_colors(expectedCornerColorCount(geometry));
  std::vector<std::uint32_t> shadow_candidates;
  std::size_t skipped_lights = 0;
  bool needs_shadow_index = false;
  for (const Light& light : lights) {
    if (!activeStaticLight(light)) {
      ++skipped_lights;
    } else if (options.use_shadows && (light.flags & kLightFlagNoCasted) == 0) {
      needs_shadow_index = true;
    }
  }
  if (stats) stats->skipped_lights = skipped_lights;
  geometry::TriangleIndex lighting_index;
  if (needs_shadow_index) lighting_index = geometry::TriangleIndex(indexedGeometryTriangles(geometry));

  for (std::size_t face_index = 0; face_index < geometry.faces.size(); ++face_index) {
    const Face& face = geometry.faces[face_index];
    for (std::size_t corner_index = 0; corner_index < face.corners.size(); ++corner_index) {
      const Corner& corner = face.corners[corner_index];
      const ArxVector3& position = geometry.vertices[corner.vertex].position;
      std::array<double, 3> generated{};

      for (const Light& light : lights) {
        if (!activeStaticLight(light)) continue;

        const ArxVector3 to_light = light.position - position;
        const float distance = math::lengthf(to_light);
        const float attenuation = lightAttenuation(light, distance);
        if (attenuation <= 0.0f) continue;

        float response = 1.0f;
        if (options.use_normals) {
          response = normalResponse(corner.normal, to_light, distance);
          if (response <= 0.0f) continue;
        }

        if (needs_shadow_index && (light.flags & kLightFlagNoCasted) == 0) {
          if (stats) ++stats->shadow_rays;
          if (segmentOccluded(
                  lighting_index, light.position, position, static_cast<FaceIndex>(face_index), shadow_candidates)) {
            if (stats) ++stats->occluded_shadow_rays;
            continue;
          }
        }

        const double factor = static_cast<double>(light.intensity) * options.global_factor * response * attenuation;
        generated[0] += static_cast<double>(light.color.r) * factor;
        generated[1] += static_cast<double>(light.color.g) * factor;
        generated[2] += static_cast<double>(light.color.b) * factor;
        if (stats) ++stats->contributing_light_corners;
      }

      corner_colors[cornerColorIndex(static_cast<FaceIndex>(face_index), corner_index)] =
          clampGeneratedColor(generated, options.ambient_color);
      if (stats) ++stats->generated_corners;
    }
  }

  if (log_summary) {
    log(ARX_LOG_DEBUG,
        "Static-light generation: {} faces, {} corners, {} lights ({} active, {} skipped), {} contributing "
        "light-corners, {} shadow rays ({} occluded), normals {}, shadows {}, global factor {}",
        geometry.faces.size(),
        stats->generated_corners,
        lights.size(),
        lights.size() - skipped_lights,
        skipped_lights,
        stats->contributing_light_corners,
        stats->shadow_rays,
        stats->occluded_shadow_rays,
        options.use_normals,
        options.use_shadows,
        options.global_factor);
  }
  out = std::move(corner_colors);
  return Error::kNone;
}

}  // namespace pistoris::lights
