// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#include "arx_pistoris/base/flags.h"
#include "arx_pistoris/base/indices.h"
#include "arx_pistoris/base/math.h"
#include "arx_pistoris/runtime/types.h"

#include "modules/geometry.h"
#include "modules/lights.h"
#include "modules/lights/internal.h"
#include "utils/identifier.h"
#include "utils/log.h"
#include "utils/math/finite.h"

#include <array>
#include <cstddef>
#include <optional>
#include <span>
#include <string_view>
#include <unordered_set>

namespace pistoris::lights {
using math::finite;

Error validateLight(const Light& light) {
  if (!isIdentifier(light.name)) return Error::kBadLightName;
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

Error validateLightCount(std::size_t count) noexcept {
  return count > static_cast<std::size_t>(kInvalidLightIndex) ? Error::kTooManyLights : Error::kNone;
}

Error validateLights(std::span<const Light> lights) {
  if (lights.size() > static_cast<std::size_t>(kInvalidLightIndex)) {
    log(ARX_LOG_DEBUG,
        "Lighting validation: light count {} exceeds limit {}",
        lights.size(),
        static_cast<std::size_t>(kInvalidLightIndex));
    return Error::kTooManyLights;
  }
  std::unordered_set<std::string_view> names;
  names.reserve(lights.size());
  for (std::size_t index = 0; index < lights.size(); ++index) {
    const Light& light = lights[index];
    Error error = validateLight(light);
    if (error != Error::kNone) {
      log(ARX_LOG_DEBUG,
          "Lighting validation: light {} '{}' is invalid: error {}",
          index,
          light.name,
          static_cast<int>(error));
      return error;
    }
    if (!names.insert(light.name).second) {
      log(ARX_LOG_DEBUG, "Lighting validation: light {} duplicates name '{}'", index, light.name);
      return Error::kDuplicateLightName;
    }
  }
  return Error::kNone;
}

Error validateLights(const LightingData& lighting) { return validateLights(lighting.lights); }

Error validateCornerColor(const ArxColor3& color) {
  if (!unitColor(color)) return Error::kBadCornerColor;
  return Error::kNone;
}

Error validateFaceCornerColors(const std::optional<std::array<ArxColor3, 3>>& colors) {
  return colors ? validateCornerColors(*colors) : Error::kNone;
}

Error validateCornerColors(std::span<const ArxColor3> colors) {
  for (std::size_t index = 0; index < colors.size(); ++index) {
    const ArxColor3& color = colors[index];
    Error error = validateCornerColor(color);
    if (error != Error::kNone) {
      log(ARX_LOG_DEBUG,
          "Lighting validation: corner color {} is invalid ({}, {}, {})",
          index,
          color.r,
          color.g,
          color.b);
      return error;
    }
  }
  return Error::kNone;
}

Error validateCornerColors(std::span<const ArxColor3> colors, std::size_t face_count) {
  Error error = validateCornerColors(colors);
  if (error != Error::kNone) return error;
  const std::size_t expected = expectedCornerColorCount(face_count);
  if (!colors.empty() && colors.size() != expected) {
    log(ARX_LOG_DEBUG,
        "Lighting validation: corner-color count {} does not match expected {}",
        colors.size(),
        expected);
    return Error::kBadCornerColorCount;
  }
  return Error::kNone;
}

Error validateCornerColors(const LightingData& lighting) { return validateCornerColors(lighting.corner_colors); }

Error validateCornerColors(const LightingData& lighting, const GeometryData& geometry) {
  return validateCornerColors(lighting.corner_colors, geometry.faces.size());
}

Error validate(const LightingData& lighting) {
  Error error = validateLights(lighting);
  if (error != Error::kNone) return error;
  return validateCornerColors(lighting);
}

Error validate(const LightingData& lighting, const GeometryData& geometry) {
  Error error = validateLights(lighting);
  if (error != Error::kNone) return error;
  return validateCornerColors(lighting, geometry);
}

}  // namespace pistoris::lights
