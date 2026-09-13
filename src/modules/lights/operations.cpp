// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#include "arx_pistoris/base/indices.h"
#include "arx_pistoris/base/math.h"

#include "modules/geometry.h"
#include "modules/lights.h"
#include "utils/identifier.h"

#include <array>
#include <cassert>
#include <cstddef>
#include <optional>
#include <span>
#include <utility>
#include <vector>

namespace pistoris::lights {
namespace {

constexpr std::size_t kCornersPerFace = 3;

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

void reserveCornerColorCapacity(LightingData& lighting, std::size_t capacity) {
  lighting.corner_colors.reserve(capacity);
}

void truncateCornerColors(LightingData& lighting, std::size_t size) noexcept {
  while (lighting.corner_colors.size() > size) lighting.corner_colors.pop_back();
}

void setLight(LightingData& lighting, LightIndex index, Light light) noexcept {
  assert(static_cast<std::size_t>(index) < lighting.lights.size());
  lighting.lights[index] = std::move(light);
}

LightIndex addLight(LightingData& lighting, Light light) {
  assert(lighting.lights.size() < static_cast<std::size_t>(kInvalidLightIndex));
  const LightIndex index = static_cast<LightIndex>(lighting.lights.size());
  lighting.lights.push_back(std::move(light));
  return index;
}

void removeLight(LightingData& lighting, LightIndex index) noexcept {
  assert(static_cast<std::size_t>(index) < lighting.lights.size());
  lighting.lights.erase(lighting.lights.begin() + static_cast<std::ptrdiff_t>(index));
}

void setFaceCornerColors(LightingData& lighting, FaceIndex face, const std::optional<std::array<ArxColor3, 3>>& colors,
                         std::size_t face_count) {
  assert(static_cast<std::size_t>(face) < face_count);
  assert(lighting.corner_colors.empty() || lighting.corner_colors.size() == expectedCornerColorCount(face_count));
  if (lighting.corner_colors.empty()) {
    if (!colors) return;
    lighting.corner_colors.assign(expectedCornerColorCount(face_count), kDefaultCornerColor);
  }
  const std::size_t first = cornerColorIndex(face, 0);
  for (std::size_t corner = 0; corner < kCornersPerFace; ++corner)
    lighting.corner_colors[first + corner] = colors ? (*colors)[corner] : kDefaultCornerColor;
}

void appendFaceCornerColors(LightingData& lighting, const std::optional<std::array<ArxColor3, 3>>& colors,
                            std::size_t existing_face_count) {
  assert(lighting.corner_colors.empty() ||
         lighting.corner_colors.size() == expectedCornerColorCount(existing_face_count));
  if (lighting.corner_colors.empty()) {
    if (!colors) return;
    std::vector<ArxColor3> corner_colors(expectedCornerColorCount(existing_face_count + 1U), kDefaultCornerColor);
    const std::size_t first = expectedCornerColorCount(existing_face_count);
    for (std::size_t corner = 0; corner < kCornersPerFace; ++corner) corner_colors[first + corner] = (*colors)[corner];
    lighting.corner_colors = std::move(corner_colors);
    return;
  }
  const std::size_t original_size = lighting.corner_colors.size();
  try {
    for (std::size_t corner = 0; corner < kCornersPerFace; ++corner)
      lighting.corner_colors.push_back(colors ? (*colors)[corner] : kDefaultCornerColor);
  } catch (...) {
    truncateCornerColors(lighting, original_size);
    throw;
  }
}

void removeFaceCornerColors(LightingData& lighting, FaceIndex face, [[maybe_unused]] std::size_t face_count) noexcept {
  assert(static_cast<std::size_t>(face) < face_count);
  if (lighting.corner_colors.empty()) return;
  assert(lighting.corner_colors.size() == expectedCornerColorCount(face_count));
  const std::size_t first = cornerColorIndex(face, 0);
  lighting.corner_colors.erase(lighting.corner_colors.begin() + static_cast<std::ptrdiff_t>(first),
                               lighting.corner_colors.begin() + static_cast<std::ptrdiff_t>(first + kCornersPerFace));
}

void setCornerColor(LightingData& lighting, FaceIndex face, std::size_t corner, ArxColor3 color,
                    std::size_t face_count) {
  assert(static_cast<std::size_t>(face) < face_count && corner < kCornersPerFace);
  if (lighting.corner_colors.empty())
    lighting.corner_colors.assign(expectedCornerColorCount(face_count), kDefaultCornerColor);
  else
    assert(lighting.corner_colors.size() == expectedCornerColorCount(face_count));
  lighting.corner_colors[cornerColorIndex(face, corner)] = color;
}

void replaceCornerColors(LightingData& lighting, std::vector<ArxColor3>&& colors) noexcept {
  lighting.corner_colors = std::move(colors);
}

void clearCornerColors(LightingData& lighting) noexcept { lighting.corner_colors.clear(); }

void clear(LightingData& lighting) noexcept {
  lighting.lights.clear();
  lighting.corner_colors.clear();
}

std::size_t repairLightNames(std::span<Light> lights) {
  IdentifierUniquifier names;
  names.reserve(lights.size());
  for (Light& light : lights) names.add(light.name);
  const IdentifierRepairSummary summary = names.apply();
  assert(!summary.exhausted);
  return summary.changed;
}

void repairLightName(const LightingData& lighting, Light& light, LightIndex ignored) {
  IdentifierUniquifier names;
  names.reserve(1, lighting.lights.size());
  for (std::size_t index = 0; index < lighting.lights.size(); ++index)
    if (index != static_cast<std::size_t>(ignored)) names.occupy(lighting.lights[index].name);
  names.add(light.name);
  [[maybe_unused]] const IdentifierRepairSummary summary = names.apply();
  assert(!summary.exhausted);
}

}  // namespace pistoris::lights
