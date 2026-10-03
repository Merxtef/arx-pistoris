// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#pragma once

#include "arx_pistoris/base/status.h"

#include "external/glb/node_graph.h"

#include <cstddef>
#include <vector>

struct cgltf_data;

namespace pistoris::glb_level {

struct DiscoveredGeometryNode {
  std::size_t node = 0;
  std::size_t room = glb::kInvalidNodeIndex;
};

struct LevelDiscovery {
  std::vector<std::size_t> rooms;
  std::vector<std::size_t> portals;
  std::vector<std::size_t> anchors;
  std::vector<std::size_t> lights;
  std::vector<std::size_t> player_spawns;
  std::vector<std::size_t> entities;
  std::vector<std::size_t> fogs;
  std::vector<std::size_t> zones;
  std::vector<std::size_t> paths;
  std::vector<std::size_t> navigation_surfaces;
  std::vector<std::size_t> minimaps;
  std::vector<DiscoveredGeometryNode> geometry;
};

ArxReturnCode discoverLevelNodes(const cgltf_data& data, const glb::NodeGraph& graph, LevelDiscovery& out);

}  // namespace pistoris::glb_level
