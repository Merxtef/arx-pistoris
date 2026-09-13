// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#pragma once

#include "arx_pistoris/base/math.h"

#include <cstdint>
#include <optional>
#include <span>
#include <vector>

namespace pistoris {

struct GeometryData;

struct MinimapData {
  std::vector<std::uint8_t> encoded_image;
  ArxRect world_xz_bounds{};
};

namespace minimap {

inline constexpr std::uint32_t kGenerationWidth = 640;
inline constexpr std::uint32_t kGenerationHeight = 640;
inline constexpr std::uint32_t kGenerationPixelsPerCell = 4;
inline constexpr float kArxUnitsPerPixel = 25.0f;

enum class Error : std::uint8_t {
  kNone,
  kInvalidOptions,
  kBadImage,
  kBadBounds,
  kOutOfMemory,
};

struct RenderOptions {
  ArxVector2 projection_offset{};
  ArxColor3 fill_color{};
  std::optional<ArxColor3> border_color;
};

struct RenderInfo {
  bool padded = false;
  bool cropped = false;
  bool invisible = false;
};

struct Sampler {
  std::span<const std::uint8_t> encoded_image;
  ArxColor3 color{};
};

struct GenerationOptions {
  Sampler foreground;
  Sampler background;
  Sampler water;
  Sampler lava;
  ArxColor3 halo_color{};
  std::uint32_t halo_radius = 0;
};

struct GenerationDiagnostics {
  std::uint64_t sampled_cells = 0;
  std::uint64_t skipped_cells = 0;
  std::uint64_t foreground_pixels = 0;
  std::uint64_t water_pixels = 0;
  std::uint64_t lava_pixels = 0;
  std::uint64_t halo_pixels = 0;
};

// --- Validation ---

Error validate(const MinimapData& minimap) noexcept;
Error validateImage(std::span<const std::uint8_t> encoded) noexcept;
Error validateBounds(const ArxRect& bounds) noexcept;
Error validateRenderOptions(const RenderOptions& options) noexcept;
Error validateGenerationOptions(const GenerationOptions& options) noexcept;

// --- Queries ---

Error projectedBounds(std::span<const std::uint8_t> encoded, const ArxAabb& referenced_bounds,
                      const ArxVector2& projection_offset, ArxRect& out) noexcept;
Error compactProjectionOffset(const MinimapData& minimap, const ArxAabb& referenced_bounds, ArxVector2& out) noexcept;

// --- Mutation ---

void setImage(MinimapData& minimap, std::vector<std::uint8_t> encoded, const ArxRect& bounds) noexcept;
void clear(MinimapData& minimap) noexcept;
bool translate(MinimapData& minimap, const ArxVector3& offset) noexcept;

// --- Generation ---

Error generate(MinimapData& out, const GeometryData& geometry, const GenerationOptions& options,
               GenerationDiagnostics* diagnostics = nullptr);

Error renderPng(const MinimapData& minimap, const ArxAabb& referenced_bounds, const RenderOptions& options,
                std::vector<std::uint8_t>& out, RenderInfo* info = nullptr);
Error reprojectPng(std::span<const std::uint8_t> encoded, const ArxVector2& source_projection_offset,
                   const RenderOptions& options, std::vector<std::uint8_t>& out, RenderInfo* info = nullptr);

}  // namespace minimap
}  // namespace pistoris
