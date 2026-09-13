// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#include "discovery.h"

#include "arx_pistoris/base/status.h"

#include "cgltf/cgltf.h"
#include "external/glb/model/internal.h"
#include "external/glb/node_graph.h"
#include "utils/name_tokens.h"

#include <cstddef>
#include <cstdint>
#include <string_view>
#include <utility>
#include <vector>

namespace pistoris::glb_model {
namespace {

bool terminalNode(const cgltf_node& node) {
  if (node.name == nullptr) return false;
  const std::string_view name(node.name);
  return name.starts_with(kActionPrefix) || name.starts_with(kAnimationPrefix) || name.starts_with(kBonePrefix) ||
         name.starts_with(kProbePrefix);
}

}  // namespace

ArxReturnCode discoverModel(const cgltf_data& data, const glb::NodeGraph& graph, ModelDiscovery& out) {
  ModelDiscovery result;
  std::vector<std::uint8_t> terminal_ancestor(data.nodes_count, 0);
  for (std::size_t node : graph.preorder) {
    const std::size_t parent = graph.parent[node];
    terminal_ancestor[node] =
        parent != glb::kInvalidNodeIndex && (terminal_ancestor[parent] != 0 || terminalNode(data.nodes[parent])) ? 1U
                                                                                                                 : 0U;
    const cgltf_node& source = data.nodes[node];
    if (terminal_ancestor[node] == 0 && source.name != nullptr) {
      const std::string_view name(source.name);
      if (name.starts_with(kBonePrefix)) result.bone_helpers.push_back(node);
      if (name.starts_with(kAnimationPrefix)) result.animation_helpers.push_back(node);
    }
    if (terminal_ancestor[node] != 0 || source.name == nullptr ||
        !std::string_view(source.name).starts_with(kOriginPrefix))
      continue;
    const std::string_view label = std::string_view(source.name).substr(kOriginPrefix.size());
    if (result.root != glb::kInvalidNodeIndex || label.empty() || hasDoubleUnderscore(label))
      return ARX_GLB_BAD_MODEL_HIERARCHY;
    result.root = node;
  }

  result.active.assign(data.nodes_count, 0);
  std::vector<std::uint8_t> in_scope(data.nodes_count, result.root == glb::kInvalidNodeIndex ? 1U : 0U);
  for (std::size_t node : graph.preorder) {
    const std::size_t parent = graph.parent[node];
    if (result.root != glb::kInvalidNodeIndex)
      in_scope[node] = node == result.root || (parent != glb::kInvalidNodeIndex && in_scope[parent] != 0) ? 1U : 0U;

    const cgltf_node& source = data.nodes[node];
    if (in_scope[node] == 0) {
      if (source.mesh != nullptr && terminal_ancestor[node] == 0 && !terminalNode(source)) ++result.outside_meshes;
      continue;
    }
    if (terminal_ancestor[node] != 0) continue;
    result.active[node] = 1;
    result.active_nodes.push_back(node);

    if (source.name != nullptr) {
      const std::string_view name(source.name);
      if (name.starts_with(kActionPrefix)) result.action_nodes.push_back(node);
      if (name.starts_with(kProbePrefix)) result.probe_nodes.push_back(node);
    }
    if (terminalNode(source)) {
      if (source.mesh != nullptr) result.terminal_mesh_nodes.push_back(node);
      continue;
    }
    if (source.mesh == nullptr) continue;
    result.mesh_nodes.push_back(node);
    if (source.skin != nullptr) result.skinned_mesh_nodes.push_back(node);
  }

  out = std::move(result);
  return ARX_OK;
}

}  // namespace pistoris::glb_model
