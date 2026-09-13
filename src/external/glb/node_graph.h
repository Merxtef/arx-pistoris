// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#pragma once

#include "arx_pistoris/base/status.h"

#include "cgltf/cgltf.h"
#include "utils/math/mat4.h"

#include <cstddef>
#include <limits>
#include <vector>

namespace pistoris::glb {

inline constexpr std::size_t kInvalidNodeIndex = std::numeric_limits<std::size_t>::max();

struct NodeGraph {
  std::vector<math::Mat4> world;
  std::vector<std::size_t> parent;
  std::vector<std::size_t> preorder;
  std::vector<bool> reachable;
};

ArxReturnCode buildNodeGraph(cgltf_data& data, NodeGraph& out);
[[nodiscard]] bool hasNonIdentityLocalScale(const cgltf_node& node);
[[nodiscard]] bool isDescendantOrSelf(const NodeGraph& graph, std::size_t node, std::size_t ancestor) noexcept;
[[nodiscard]] std::size_t nearestCommonAncestor(const NodeGraph& graph, std::size_t first, std::size_t second) noexcept;

}  // namespace pistoris::glb
