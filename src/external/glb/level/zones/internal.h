// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#pragma once

#include "arx_pistoris/level.hpp"
#include "arx_pistoris/pistoris_types.h"

#include "external/glb/container.h"
#include "modules/scene.h"
#include "utils/math/mat4.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace pistoris::glb_level {
struct ImportUnits;
}

namespace pistoris::glb_level::zone_internal {

inline constexpr float kXzPairEpsilon = 1.0e-4f;
inline constexpr float kPlaneEpsilon = 1.0e-4f;
inline constexpr float kVertexWeldEpsilon = 1.0e-4f;
inline constexpr float kInfinityEpsilon = 1.0e-4f;
inline constexpr float kExportMargin = 100.0f;

struct ParsedName {
  std::optional<std::uint32_t> ordinal;
  std::string name;
};

struct Settings {
  std::optional<ArxColor3> color;
  std::optional<float> farclip;
  std::optional<float> volume;
};

struct Mesh {
  std::vector<ArxVector3> positions;
  std::vector<std::array<std::uint32_t, 3>> triangles;
};

void logFailure(std::size_t node_index, std::string_view name, std::string_view reason);
ParsedName parseName(std::string_view name, std::size_t node_index);
std::string nodeName(const Zone& zone, std::size_t ordinal);
ArxReturnCode parseSettings(std::string_view payload, Settings& out);
std::string settingsHelperName(const Zone& zone);

ArxReturnCode readMesh(const glb::Asset& asset, const cgltf_node& node, const math::Mat4& world,
                       const ImportUnits& units, std::size_t node_index, std::string_view name, Mesh& out);
ArxReturnCode reconstruct(const Mesh& mesh, Zone& zone, float& top_y, float& bottom_y, float& top_movement,
                          float& bottom_movement, std::size_t node_index, std::string_view name);

}  // namespace pistoris::glb_level::zone_internal
