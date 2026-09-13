// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#pragma once

#include "arx_pistoris/base/flags.h"
#include "arx_pistoris/base/indices.h"
#include "arx_pistoris/base/math.hpp"

#include "modules/geometry.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <optional>
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
  kBadIndex,
};

inline constexpr ArxColor3 kDefaultCornerColor = {0.5f, 0.5f, 0.5f};
inline constexpr std::size_t kInvalidCornerColorIndex = std::numeric_limits<std::size_t>::max();

struct StaticLightingGenerationOptions {
  ArxColor3 ambient_color = kDefaultStaticLightingAmbientColor;
  float global_factor = kDefaultStaticLightingGlobalFactor;
  bool use_normals = true;
  bool use_shadows = true;
};

// --- Validation ---

Error validateLightCount(std::size_t count) noexcept;
Error validateLight(const Light& light);
Error validateLights(std::span<const Light> lights);
Error validateLights(const LightingData& lighting);
Error validateCornerColor(const ArxColor3& color);
Error validateCornerColors(std::span<const ArxColor3> colors);
Error validateCornerColors(std::span<const ArxColor3> colors, std::size_t face_count);
Error validateCornerColors(const LightingData& lighting);
Error validateCornerColors(const LightingData& lighting, const GeometryData& geometry);
Error validateFaceCornerColors(const std::optional<std::array<ArxColor3, 3>>& colors);
Error validate(const LightingData& lighting);
Error validate(const LightingData& lighting, const GeometryData& geometry);

// --- Queries ---

std::size_t expectedCornerColorCount(std::size_t face_count);
std::size_t expectedCornerColorCount(const GeometryData& geometry);
std::size_t cornerColorIndex(FaceIndex face, std::size_t corner_index);
bool hasCornerColors(const LightingData& lighting);
ArxColor3 cornerColorOr(const LightingData& lighting, FaceIndex face, std::size_t corner_index, ArxColor3 fallback);

// --- Mutation ---

void reserveCornerColorCapacity(LightingData& lighting, std::size_t capacity);
void truncateCornerColors(LightingData& lighting, std::size_t size) noexcept;
void setLight(LightingData& lighting, LightIndex index, Light light) noexcept;
LightIndex addLight(LightingData& lighting, Light light);
void removeLight(LightingData& lighting, LightIndex index) noexcept;
void setFaceCornerColors(LightingData& lighting, FaceIndex face, const std::optional<std::array<ArxColor3, 3>>& colors,
                         std::size_t face_count);
void appendFaceCornerColors(LightingData& lighting, const std::optional<std::array<ArxColor3, 3>>& colors,
                            std::size_t existing_face_count);
void removeFaceCornerColors(LightingData& lighting, FaceIndex face, std::size_t face_count) noexcept;
void setCornerColor(LightingData& lighting, FaceIndex face, std::size_t corner, ArxColor3 color,
                    std::size_t face_count);
void replaceCornerColors(LightingData& lighting, std::vector<ArxColor3>&& colors) noexcept;
void clearCornerColors(LightingData& lighting) noexcept;
void clear(LightingData& lighting) noexcept;

// --- Repair ---

void repairLightName(const LightingData& lighting, Light& light, LightIndex ignored = kInvalidLightIndex);
std::size_t repairLightNames(std::span<Light> lights);

// --- Transformation ---

// Empty means identity; otherwise the map must describe an order-preserving compaction
void remapCornerColors(LightingData& lighting, std::span<const FaceIndex> face_remap) noexcept;

// --- Generation ---

Error generateStaticLighting(std::vector<ArxColor3>& out, const GeometryData& geometry, std::span<const Light> lights,
                             const StaticLightingGenerationOptions& options,
                             StaticLightingDiagnostics* diagnostics = nullptr);

}  // namespace lights
}  // namespace pistoris
