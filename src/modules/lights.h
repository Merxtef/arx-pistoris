// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#pragma once

#include "arx_pistoris/arx_math.hpp"
#include "arx_pistoris/flags.h"
#include "arx_pistoris/indices.h"
#include "arx_pistoris/pistoris_types.h"

#include "modules/geometry.h"

#include <cstddef>
#include <cstdint>
#include <limits>
#include <span>
#include <string>
#include <vector>

namespace pistoris {

struct Light {
  std::string name;
  ArxVector3 position = {};
  ArxColor3 color = {};
  float fallstart = 0.0f;
  float fallend = 1.0f;
  float intensity = 0.0f;
  ArxColor3 flicker = {};
  float effect_radius = 0.0f;
  float effect_frequency = 0.0f;
  float effect_size = 0.0f;
  float effect_speed = 0.0f;
  float flare_size = 0.0f;
  LightFlags flags = 0;
};

struct LightingData {
  std::vector<Light> lights;
  std::vector<ArxColor3> corner_colors;
};

namespace lights {

inline constexpr ArxColor3 kDefaultStaticLightingAmbientColor = {0.25f, 0.25f, 0.25f};
inline constexpr float kDefaultStaticLightingGlobalFactor = 0.85f;

struct StaticLightingDiagnostics {
  std::uint64_t generated_corners = 0;
  std::uint64_t contributing_light_corners = 0;
  std::uint64_t skipped_lights = 0;
  std::uint64_t shadow_rays = 0;
  std::uint64_t occluded_shadow_rays = 0;
};

enum class Error : std::uint8_t {
  kNone,
  kInvalidOptions,
  kTooManyLights,
  kBadLightName,
  kDuplicateLightName,
  kBadLightPosition,
  kBadLightColor,
  kBadLightFalloff,
  kBadLightIntensity,
  kBadLightEffect,
  kBadLightFlags,
  kBadCornerColorCount,
  kBadCornerColor,
};

inline constexpr ArxColor3 kDefaultCornerColor = {0.5f, 0.5f, 0.5f};
inline constexpr std::size_t kInvalidCornerColorIndex = std::numeric_limits<std::size_t>::max();

struct StaticLightingGenOptions {
  ArxColor3 ambient_color = kDefaultStaticLightingAmbientColor;
  float global_factor = kDefaultStaticLightingGlobalFactor;
  bool use_normals = true;
  bool use_shadows = true;
};

std::size_t expectedCornerColorCount(std::size_t face_count);
std::size_t expectedCornerColorCount(const GeometryData& geometry);
std::size_t cornerColorIndex(FaceIndex face, std::size_t corner_index);
bool hasCornerColors(const LightingData& lighting);
ArxColor3 cornerColorOr(const LightingData& lighting, FaceIndex face, std::size_t corner_index, ArxColor3 fallback);
// Empty means identity; otherwise the map must describe an order-preserving compaction
void remapCornerColors(LightingData& lighting, std::span<const FaceIndex> face_remap) noexcept;

Error validateLightSource(const Light& light);
Error validateLightSources(std::span<const Light> lights);
std::size_t makeLightNamesUnique(std::span<Light> lights);
Error validateLightSources(const LightingData& lighting);
Error validateCornerColor(const ArxColor3& color);
Error validateCornerColors(std::span<const ArxColor3> colors);
Error validateCornerColors(std::span<const ArxColor3> colors, std::size_t face_count);
Error validateCornerColors(const LightingData& lighting);
Error validateCornerColors(const LightingData& lighting, const GeometryData& geometry);
Error validate(const LightingData& lighting);
Error validate(const LightingData& lighting, const GeometryData& geometry);

Error generateStaticLighting(std::vector<ArxColor3>& out, const GeometryData& geometry, std::span<const Light> lights,
                             const StaticLightingGenOptions& options, StaticLightingDiagnostics* diagnostics = nullptr);

}  // namespace lights
}  // namespace pistoris
