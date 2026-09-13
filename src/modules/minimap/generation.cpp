// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#include "arx_pistoris/base/flags.h"
#include "arx_pistoris/base/indices.h"
#include "arx_pistoris/base/math.hpp"

#include "modules/geometry.h"
#include "modules/minimap.h"
#include "utils/encoded_image.h"

#include <algorithm>
#include <array>
#include <cassert>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <new>
#include <span>
#include <utility>
#include <vector>

namespace pistoris::minimap {
namespace {

enum class PixelKind : std::uint8_t {
  kBackground,
  kForeground,
  kWater,
  kLava,
};

struct PreparedSampler {
  std::vector<std::uint8_t> rgb;
  ArxColor3 color{};
};

Error imageError(image::Error error) noexcept {
  if (error == image::Error::kNone) return Error::kNone;
  return error == image::Error::kOutOfMemory ? Error::kOutOfMemory : Error::kBadImage;
}

Error prepareSampler(const Sampler& sampler, PreparedSampler& out) {
  out.color = sampler.color;
  if (sampler.encoded_image.empty()) return Error::kNone;
  return imageError(image::decodeResizedRgb(sampler.encoded_image, kGenerationWidth, kGenerationHeight, out.rgb));
}

std::array<float, 3> sample(const PreparedSampler& sampler, std::size_t pixel) noexcept {
  if (sampler.rgb.empty()) return {sampler.color.r, sampler.color.g, sampler.color.b};
  const std::uint8_t* source = sampler.rgb.data() + pixel * 3U;
  constexpr float kByteScale = 1.0f / 255.0f;
  return {
      static_cast<float>(source[0]) * kByteScale * sampler.color.r,
      static_cast<float>(source[1]) * kByteScale * sampler.color.g,
      static_cast<float>(source[2]) * kByteScale * sampler.color.b,
  };
}

std::uint8_t colorByte(float value) noexcept {
  return static_cast<std::uint8_t>(std::lround(static_cast<double>(std::clamp(value, 0.0f, 1.0f)) * 255.0));
}

PixelKind classifyHit(const geometry::SurfaceSupportIndex& index, const GeometryData& geometry, float x,
                      float z) noexcept {
  constexpr float kMinSurfaceUpDot = 0.0871557427f;  // cos(85 degrees)
  struct Context {
    FaceIndex face = kInvalidFaceIndex;
    float y = std::numeric_limits<float>::max();
  } context;
  index.visitHitsAt(
      x,
      z,
      [](const geometry::SurfaceSupportHit& hit, void* raw) {
        auto& context = *static_cast<Context*>(raw);
        if (-hit.normal.y < kMinSurfaceUpDot) return;
        if (hit.position.y > context.y || (hit.position.y == context.y && hit.face >= context.face)) return;
        context.face = hit.face;
        context.y = hit.position.y;
      },
      &context);
  if (context.face == kInvalidFaceIndex) return PixelKind::kBackground;
  const FaceType flags = geometry.faces[context.face].flags;
  if ((flags & kFaceBitLava) != 0) return PixelKind::kLava;
  if ((flags & kFaceBitWater) != 0) return PixelKind::kWater;
  return PixelKind::kForeground;
}

void calculateChebyshevDistances(std::span<const PixelKind> pixels, std::vector<std::uint16_t>& distances) {
  constexpr std::uint16_t kUnreached = std::numeric_limits<std::uint16_t>::max();
  distances.resize(pixels.size());
  for (std::size_t i = 0; i < pixels.size(); ++i) distances[i] = pixels[i] == PixelKind::kBackground ? kUnreached : 0;

  const auto relax = [&](std::size_t target, std::size_t source) {
    if (distances[source] != kUnreached)
      distances[target] = std::min(distances[target], static_cast<std::uint16_t>(distances[source] + 1U));
  };
  for (std::uint32_t y = 0; y < kGenerationHeight; ++y) {
    for (std::uint32_t x = 0; x < kGenerationWidth; ++x) {
      const std::size_t pixel = static_cast<std::size_t>(y) * kGenerationWidth + x;
      if (x != 0) relax(pixel, pixel - 1U);
      if (y == 0) continue;
      if (x != 0) relax(pixel, pixel - kGenerationWidth - 1U);
      relax(pixel, pixel - kGenerationWidth);
      if (x + 1U < kGenerationWidth) relax(pixel, pixel - kGenerationWidth + 1U);
    }
  }
  for (std::uint32_t y = kGenerationHeight; y-- > 0;) {
    for (std::uint32_t x = kGenerationWidth; x-- > 0;) {
      const std::size_t pixel = static_cast<std::size_t>(y) * kGenerationWidth + x;
      if (x + 1U < kGenerationWidth) relax(pixel, pixel + 1U);
      if (y + 1U >= kGenerationHeight) continue;
      if (x != 0) relax(pixel, pixel + kGenerationWidth - 1U);
      relax(pixel, pixel + kGenerationWidth);
      if (x + 1U < kGenerationWidth) relax(pixel, pixel + kGenerationWidth + 1U);
    }
  }
}

Error generateImpl(MinimapData& out, const GeometryData& geometry, const GenerationOptions& options,
                   GenerationDiagnostics* diagnostics) {
  Error error = validateGenerationOptions(options);
  if (error != Error::kNone) return error;
  constexpr ArxRect kBounds{
      .min = {0.0f, 0.0f},
      .max = {static_cast<float>(kGenerationWidth) * kArxUnitsPerPixel,
              static_cast<float>(kGenerationHeight) * kArxUnitsPerPixel},
  };

  PreparedSampler foreground;
  PreparedSampler background;
  PreparedSampler water;
  PreparedSampler lava;
  error = prepareSampler(options.foreground, foreground);
  if (error != Error::kNone) return error;
  error = prepareSampler(options.background, background);
  if (error != Error::kNone) return error;
  error = prepareSampler(options.water, water);
  if (error != Error::kNone) return error;
  error = prepareSampler(options.lava, lava);
  if (error != Error::kNone) return error;

  const geometry::SurfaceSupportIndex index = geometry::buildSurfaceSupportIndex(geometry);
  assert(!index.empty());
  constexpr std::size_t kPixelCount = static_cast<std::size_t>(kGenerationWidth) * kGenerationHeight;
  std::vector<PixelKind> pixels(kPixelCount, PixelKind::kBackground);
  GenerationDiagnostics generated;
  const float left = kBounds.min.x;
  const float top = kBounds.max.y;
  const ArxAabb& support_bounds = index.bounds();
  for (std::uint32_t cell_y = 0; cell_y < kGenerationHeight / kGenerationPixelsPerCell; ++cell_y) {
    const std::uint32_t first_y = cell_y * kGenerationPixelsPerCell;
    const float cell_top = top - static_cast<float>(first_y) * kArxUnitsPerPixel;
    for (std::uint32_t cell_x = 0; cell_x < kGenerationWidth / kGenerationPixelsPerCell; ++cell_x) {
      const std::uint32_t first_x = cell_x * kGenerationPixelsPerCell;
      const float cell_left = left + static_cast<float>(first_x) * kArxUnitsPerPixel;
      const ArxAabb cell_bounds{
          .min = {cell_left, support_bounds.min.y, cell_top - kGenerationPixelsPerCell * kArxUnitsPerPixel},
          .max = {cell_left + kGenerationPixelsPerCell * kArxUnitsPerPixel, support_bounds.max.y, cell_top},
      };
      if (!index.mayHaveHitsInAabb(cell_bounds)) {
        ++generated.skipped_cells;
        continue;
      }
      ++generated.sampled_cells;
      for (std::uint32_t dy = 0; dy < kGenerationPixelsPerCell; ++dy) {
        const std::uint32_t y = first_y + dy;
        const float world_z = top - (static_cast<float>(y) + 0.5f) * kArxUnitsPerPixel;
        for (std::uint32_t dx = 0; dx < kGenerationPixelsPerCell; ++dx) {
          const std::uint32_t x = first_x + dx;
          const float world_x = left + (static_cast<float>(x) + 0.5f) * kArxUnitsPerPixel;
          PixelKind kind = classifyHit(index, geometry, world_x, world_z);
          pixels[static_cast<std::size_t>(y) * kGenerationWidth + x] = kind;
          if (kind == PixelKind::kForeground)
            ++generated.foreground_pixels;
          else if (kind == PixelKind::kWater)
            ++generated.water_pixels;
          else if (kind == PixelKind::kLava)
            ++generated.lava_pixels;
        }
      }
    }
  }

  std::vector<std::uint16_t> distances;
  const bool has_occupied =
      generated.foreground_pixels != 0 || generated.water_pixels != 0 || generated.lava_pixels != 0;
  if (options.halo_radius != 0 && has_occupied) calculateChebyshevDistances(pixels, distances);

  std::vector<std::uint8_t> rgb(kPixelCount * 3U);
  for (std::size_t pixel = 0; pixel < kPixelCount; ++pixel) {
    std::array<float, 3> color;
    switch (pixels[pixel]) {
      case PixelKind::kForeground:
        color = sample(foreground, pixel);
        break;
      case PixelKind::kWater:
        color = sample(water, pixel);
        break;
      case PixelKind::kLava:
        color = sample(lava, pixel);
        break;
      case PixelKind::kBackground:
        color = sample(background, pixel);
        if (!distances.empty() && distances[pixel] >= 1U && distances[pixel] <= options.halo_radius) {
          const float factor = static_cast<float>(distances[pixel] - 1U) / static_cast<float>(options.halo_radius);
          color = {
              std::lerp(options.halo_color.r, color[0], factor),
              std::lerp(options.halo_color.g, color[1], factor),
              std::lerp(options.halo_color.b, color[2], factor),
          };
          ++generated.halo_pixels;
        }
        break;
    }
    rgb[pixel * 3U] = colorByte(color[0]);
    rgb[pixel * 3U + 1U] = colorByte(color[1]);
    rgb[pixel * 3U + 2U] = colorByte(color[2]);
  }

  std::vector<std::uint8_t> encoded;
  error = imageError(image::encodeRgbPng(kGenerationWidth, kGenerationHeight, rgb, encoded));
  if (error != Error::kNone) return error;
  MinimapData result;
  setImage(result, std::move(encoded), kBounds);
  out = std::move(result);
  if (diagnostics != nullptr) *diagnostics = generated;
  return Error::kNone;
}

}  // namespace

Error generate(MinimapData& out, const GeometryData& geometry, const GenerationOptions& options,
               GenerationDiagnostics* diagnostics) {
  if (diagnostics != nullptr) *diagnostics = {};
  try {
    return generateImpl(out, geometry, options, diagnostics);
  } catch (const std::bad_alloc&) {
    return Error::kOutOfMemory;
  }
}

}  // namespace pistoris::minimap
