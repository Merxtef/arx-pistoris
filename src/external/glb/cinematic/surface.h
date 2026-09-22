// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#pragma once

#include "arx_pistoris/base/math.hpp"

#include "external/glb/accessor.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <vector>

namespace pistoris::glb_cinematic {

struct SurfaceTriangle {
  std::array<ArxVector2, 3> position;
  std::array<glb::Vec2, 3> texcoord;
};

struct SurfaceSample {
  glb::Vec2 texcoord;
  bool outside = false;
  bool ambiguous = false;
};

enum class SurfaceMappingMode : std::uint8_t {
  kBounds,
  kAffineUv,
  kPiecewiseUv,
};

class IllustrationSurface {
 public:
  void addTriangle(SurfaceTriangle triangle);
  void includePosition(const ArxVector2& position) noexcept;
  void noteHeight(float height) noexcept;
  void noteMissingTexcoords() noexcept;
  bool finalize() noexcept;
  bool sample(const ArxVector2& position, SurfaceSample& out) const noexcept;
  float pixelsPerUnit(const ArxVector2& image_size) const noexcept;

  [[nodiscard]] SurfaceMappingMode mappingMode() const noexcept { return mapping_mode_; }
  [[nodiscard]] bool missingTexcoords() const noexcept { return missing_texcoords_; }
  [[nodiscard]] bool varyingHeight() const noexcept { return varying_height_; }
  [[nodiscard]] float averageHeight() const noexcept;
  [[nodiscard]] bool collapsedAxis() const noexcept { return collapsed_axis_; }

 private:
  std::vector<SurfaceTriangle> triangles_;
  ArxRect bounds_{};
  float first_height_ = 0.0f;
  double height_sum_ = 0.0;
  std::size_t height_count_ = 0;
  bool has_position_ = false;
  bool has_height_ = false;
  bool missing_texcoords_ = false;
  bool varying_height_ = false;
  bool collapsed_axis_ = false;
  SurfaceMappingMode mapping_mode_ = SurfaceMappingMode::kBounds;
};

}  // namespace pistoris::glb_cinematic
