// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#pragma once

#include "container.h"

#include <cstddef>
#include <cstdint>
#include <vector>

namespace pistoris::glb {

struct Vec2 {
  float x = 0.0f;
  float y = 0.0f;
};

struct Vec3 {
  float x = 0.0f;
  float y = 0.0f;
  float z = 0.0f;
};

struct Vec4 {
  float x = 0.0f;
  float y = 0.0f;
  float z = 0.0f;
  float w = 1.0f;
};

struct AccessorView {
  const cgltf_accessor* source = nullptr;
  std::size_t count = 0;
  cgltf_type type = cgltf_type_invalid;
  cgltf_component_type component_type = cgltf_component_type_invalid;
  bool normalized = false;
  std::vector<float> floats;
  std::vector<std::uint32_t> indices;
};

ArxReturnCode getAccessor(const Asset& asset, const cgltf_accessor* accessor, AccessorView& out);
ArxReturnCode validatePositionAccessor(const AccessorView& view);
ArxReturnCode validateNormalAccessor(const AccessorView& view);
ArxReturnCode validateTexcoordAccessor(const AccessorView& view);
ArxReturnCode validateColorAccessor(const AccessorView& view);
ArxReturnCode validateIndexAccessor(const AccessorView& view);

Vec3 readVec3(const AccessorView& view, std::size_t index);
Vec2 readVec2(const AccessorView& view, std::size_t index);
Vec3 readColor3(const AccessorView& view, std::size_t index);

}  // namespace pistoris::glb
