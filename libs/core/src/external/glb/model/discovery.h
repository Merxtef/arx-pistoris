// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#pragma once

#include "arx_pistoris/base/status.h"

#include "external/glb/node_graph.h"
#include "external/glb/utils/names.h"

#include <cstddef>
#include <cstdint>
#include <vector>

struct cgltf_data;

namespace pistoris::glb_model {

struct ModelDiscovery {
  std::size_t root = glb::kInvalidNodeIndex;
  glb::ParsedLabel root_label;
  std::size_t outside_meshes = 0;
  std::vector<std::uint8_t> active;
  std::vector<std::size_t> active_nodes;
  std::vector<std::size_t> mesh_nodes;
  std::vector<std::size_t> skinned_mesh_nodes;
  std::vector<std::size_t> terminal_mesh_nodes;
  std::vector<std::size_t> bone_helpers;
  std::vector<std::size_t> action_nodes;
  std::vector<std::size_t> probe_nodes;
  std::vector<std::size_t> animation_helpers;
};

ArxReturnCode discoverModel(const cgltf_data& data, const glb::NodeGraph& graph, ModelDiscovery& out);

}  // namespace pistoris::glb_model
