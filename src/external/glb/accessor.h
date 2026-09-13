// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#pragma once

#include "container.h"

#include <cstddef>
#include <cstdint>
#include <functional>
#include <unordered_map>
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

struct AccessorElementKey {
  const cgltf_accessor* accessor = nullptr;
  std::uint32_t index = 0;

  bool operator==(const AccessorElementKey&) const = default;
};

struct AccessorElementKeyHash {
  std::size_t operator()(const AccessorElementKey& key) const noexcept {
    const std::size_t pointer = std::hash<const cgltf_accessor*>{}(key.accessor);
    return pointer ^ (std::hash<std::uint32_t>{}(key.index) + 0x9e3779b9U + (pointer << 6U) + (pointer >> 2U));
  }
};

ArxReturnCode getAccessor(const Asset& asset, const cgltf_accessor* accessor, AccessorView& out);

class AccessorCache {
 public:
  explicit AccessorCache(const Asset& asset, std::size_t expected_accessors = 0);

  ArxReturnCode get(const cgltf_accessor* accessor, const AccessorView*& out);

 private:
  const Asset& asset_;
  std::unordered_map<const cgltf_accessor*, AccessorView> views_;
};

ArxReturnCode validatePositionAccessor(const AccessorView& view);
ArxReturnCode validateNormalAccessor(const AccessorView& view);
ArxReturnCode validateTexcoordAccessor(const AccessorView& view);
ArxReturnCode validateColorAccessor(const AccessorView& view);
ArxReturnCode validateIndexAccessor(const AccessorView& view);

Vec3 readVec3(const AccessorView& view, std::size_t index);
Vec2 readVec2(const AccessorView& view, std::size_t index);
Vec3 readColor3(const AccessorView& view, std::size_t index);

}  // namespace pistoris::glb
