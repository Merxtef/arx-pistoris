// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#pragma once

#include "arx_pistoris/base/status.h"
#include "arx_pistoris/level.hpp"

#include "external/glb/container.h"
#include "modules/scene.h"
#include "utils/log.h"
#include "utils/math/mat4.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <format>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <utility>
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

template <class... Args>
void logFailure(std::size_t node_index, std::string_view name, std::format_string<Args...> reason,
                Args&&... args) noexcept {
  logLazy(ARX_LOG_DEBUG, [&] {
    return std::format("GLB -> Level object failure: zone node {} '{}' {}",
                       node_index,
                       name,
                       std::format(reason, std::forward<Args>(args)...));
  });
}
ParsedName parseName(std::string_view name, std::size_t node_index);
std::string nodeName(const Zone& zone, std::size_t ordinal);
ArxReturnCode parseSettings(std::span<const std::string_view> settings, Settings& out);
std::string settingsHelperName(const Settings& settings, std::string_view label);

ArxReturnCode readMesh(const glb::Asset& asset, const cgltf_node& node, const math::Mat4& world,
                       const ImportUnits& units, std::size_t node_index, std::string_view name, Mesh& out);
ArxReturnCode reconstruct(const Mesh& mesh, Zone& zone, float& top_y, float& bottom_y, float& top_movement,
                          float& bottom_movement, std::size_t node_index, std::string_view name);

}  // namespace pistoris::glb_level::zone_internal
