// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#include "arx_pistoris/arx_math.h"
#include "arx_pistoris/flags.h"
#include "arx_pistoris/indices.h"

#include "modules/geometry.h"
#include "modules/lights.h"
#include "utils/math/finite.h"
#include "utils/name_tokens.h"
#include "utils/unique_name.h"

#include <cassert>
#include <cstddef>
#include <span>
#include <string>
#include <string_view>
#include <unordered_set>

namespace pistoris::lights {
namespace {

constexpr std::size_t kCornersPerFace = 3;

using math::finite;

bool unitColor(const ArxColor3& color) {
  return finite(color) && color.r >= 0.0f && color.r <= 1.0f && color.g >= 0.0f && color.g <= 1.0f && color.b >= 0.0f &&
         color.b <= 1.0f;
}

}  // namespace

std::size_t expectedCornerColorCount(const GeometryData& geometry) {
  return expectedCornerColorCount(geometry.faces.size());
}

std::size_t expectedCornerColorCount(std::size_t face_count) { return face_count * kCornersPerFace; }

std::size_t cornerColorIndex(FaceIndex face, std::size_t corner_index) {
  if (face == kInvalidFaceIndex || corner_index >= kCornersPerFace) return kInvalidCornerColorIndex;
  return static_cast<std::size_t>(face) * kCornersPerFace + corner_index;
}

bool hasCornerColors(const LightingData& lighting) { return !lighting.corner_colors.empty(); }

ArxColor3 cornerColorOr(const LightingData& lighting, FaceIndex face, std::size_t corner_index, ArxColor3 fallback) {
  if (!hasCornerColors(lighting)) return fallback;
  const std::size_t index = cornerColorIndex(face, corner_index);
  if (index >= lighting.corner_colors.size()) return fallback;
  return lighting.corner_colors[index];
}

void remapCornerColors(LightingData& lighting, std::span<const FaceIndex> face_remap) noexcept {
  if (face_remap.empty() || lighting.corner_colors.empty()) return;
  assert(lighting.corner_colors.size() == expectedCornerColorCount(face_remap.size()));
  std::size_t next_face = 0;
  for (std::size_t old = 0; old < face_remap.size(); ++old) {
    const FaceIndex next = face_remap[old];
    if (next == kInvalidFaceIndex) continue;
    assert(next == next_face);
    for (std::size_t corner = 0; corner < kCornersPerFace; ++corner) {
      const std::size_t source = cornerColorIndex(static_cast<FaceIndex>(old), corner);
      const std::size_t target = cornerColorIndex(static_cast<FaceIndex>(next_face), corner);
      if (target != source) lighting.corner_colors[target] = lighting.corner_colors[source];
    }
    ++next_face;
  }
  const std::size_t next_color_count = expectedCornerColorCount(next_face);
  while (lighting.corner_colors.size() > next_color_count) lighting.corner_colors.pop_back();
}

Error validateLightSource(const Light& light) {
  if (light.name.empty() || !validSemanticString(light.name)) return Error::kBadLightName;
  if (!finite(light.position)) return Error::kBadLightPosition;
  if (!unitColor(light.color)) return Error::kBadLightColor;
  const bool zero_range = light.fallstart == 0.0f && light.fallend == 0.0f;
  if (!finite(light.fallstart) || light.fallstart < 0.0f || !finite(light.fallend) ||
      (!zero_range && light.fallend <= light.fallstart))
    return Error::kBadLightFalloff;
  if (!finite(light.intensity) || light.intensity < 0.0f) return Error::kBadLightIntensity;
  if (!finite(light.flicker) || !finite(light.effect_radius) || !finite(light.effect_frequency) ||
      !finite(light.effect_size) || !finite(light.effect_speed) || !finite(light.flare_size))
    return Error::kBadLightEffect;
  if ((light.flags & ~kLightFlagsAll) != 0) return Error::kBadLightFlags;
  return Error::kNone;
}

Error validateLightSources(std::span<const Light> lights) {
  if (lights.size() > static_cast<std::size_t>(kInvalidLightIndex)) return Error::kTooManyLights;
  std::unordered_set<std::string_view> names;
  names.reserve(lights.size());
  for (const Light& light : lights) {
    Error error = validateLightSource(light);
    if (error != Error::kNone) return error;
    if (!names.insert(light.name).second) return Error::kDuplicateLightName;
  }
  return Error::kNone;
}

std::size_t makeLightNamesUnique(std::span<Light> lights) {
  std::unordered_set<std::string> unavailable;
  unavailable.reserve(lights.size());
  for (const Light& light : lights) unavailable.insert(light.name);

  std::unordered_set<std::string> assigned;
  assigned.reserve(lights.size());
  std::size_t renamed = 0;
  for (Light& light : lights) {
    if (assigned.insert(light.name).second) continue;
    light.name = makeUniqueName(light.name, unavailable);
    unavailable.insert(light.name);
    assigned.insert(light.name);
    ++renamed;
  }
  return renamed;
}

Error validateLightSources(const LightingData& lighting) { return validateLightSources(lighting.lights); }

Error validateCornerColor(const ArxColor3& color) {
  if (!unitColor(color)) return Error::kBadCornerColor;
  return Error::kNone;
}

Error validateCornerColors(std::span<const ArxColor3> colors) {
  for (const ArxColor3& color : colors) {
    Error error = validateCornerColor(color);
    if (error != Error::kNone) return error;
  }
  return Error::kNone;
}

Error validateCornerColors(std::span<const ArxColor3> colors, std::size_t face_count) {
  Error error = validateCornerColors(colors);
  if (error != Error::kNone) return error;
  if (!colors.empty() && colors.size() != expectedCornerColorCount(face_count)) return Error::kBadCornerColorCount;
  return Error::kNone;
}

Error validateCornerColors(const LightingData& lighting) { return validateCornerColors(lighting.corner_colors); }

Error validateCornerColors(const LightingData& lighting, const GeometryData& geometry) {
  return validateCornerColors(lighting.corner_colors, geometry.faces.size());
}

Error validate(const LightingData& lighting) {
  Error error = validateLightSources(lighting);
  if (error != Error::kNone) return error;
  return validateCornerColors(lighting);
}

Error validate(const LightingData& lighting, const GeometryData& geometry) {
  Error error = validateLightSources(lighting);
  if (error != Error::kNone) return error;
  return validateCornerColors(lighting, geometry);
}

}  // namespace pistoris::lights
