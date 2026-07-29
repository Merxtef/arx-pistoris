// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#include "node_graph.h"

#include "arx_pistoris/pistoris_types.h"

#include <cmath>
#include <cstddef>
#include <cstdint>
#include <utility>
#include <vector>

namespace pistoris::glb {
namespace {

constexpr double kScaleTolerance = 1.0e-4;
constexpr double kRotationMinimumLength = 1.0e-6;

bool normalizeRotation(cgltf_float* values) {
  const double x = values[0];
  const double y = values[1];
  const double z = values[2];
  const double w = values[3];
  const double length_squared = x * x + y * y + z * z + w * w;
  if (!std::isfinite(length_squared) || length_squared <= kRotationMinimumLength * kRotationMinimumLength) return false;
  const double inverse = 1.0 / std::sqrt(length_squared);
  values[0] = static_cast<float>(x * inverse);
  values[1] = static_cast<float>(y * inverse);
  values[2] = static_cast<float>(z * inverse);
  values[3] = static_cast<float>(w * inverse);
  return std::isfinite(values[0]) && std::isfinite(values[1]) && std::isfinite(values[2]) && std::isfinite(values[3]);
}

bool finiteTransform(cgltf_node& node) {
  const float* values = nullptr;
  std::size_t count = 0;
  if (node.has_matrix) {
    values = node.matrix;
    count = 16;
  } else {
    if (node.has_translation)
      for (float value : node.translation)
        if (!std::isfinite(value)) return false;
    if (node.has_scale)
      for (float value : node.scale)
        if (!std::isfinite(value)) return false;
    if (node.has_rotation && !normalizeRotation(node.rotation)) return false;
    return true;
  }
  for (std::size_t i = 0; i < count; ++i)
    if (!std::isfinite(values[i])) return false;
  return true;
}

std::size_t nodeIndex(const cgltf_data& data, const cgltf_node* node) {
  return static_cast<std::size_t>(node - data.nodes);
}

}  // namespace

bool hasNonIdentityLocalScale(const cgltf_node& node) {
  if (!node.has_matrix) {
    if (!node.has_scale) return false;
    for (float scale : node.scale)
      if (!std::isfinite(scale) || std::abs(static_cast<double>(scale) - 1.0) > kScaleTolerance) return true;
    return false;
  }

  cgltf_float matrix[16];
  cgltf_node_transform_local(&node, matrix);
  for (std::size_t column = 0; column < 3; ++column) {
    const std::size_t offset = column * 4;
    const double x = matrix[offset];
    const double y = matrix[offset + 1];
    const double z = matrix[offset + 2];
    const double scale = std::sqrt(x * x + y * y + z * z);
    if (!std::isfinite(scale) || std::abs(scale - 1.0) > kScaleTolerance) return true;
  }
  const double determinant = static_cast<double>(matrix[0]) * (matrix[5] * matrix[10] - matrix[9] * matrix[6]) -
                             static_cast<double>(matrix[4]) * (matrix[1] * matrix[10] - matrix[9] * matrix[2]) +
                             static_cast<double>(matrix[8]) * (matrix[1] * matrix[6] - matrix[5] * matrix[2]);
  return !std::isfinite(determinant) || determinant < 0.0;
}

ArxReturnCode buildNodeGraph(cgltf_data& data, NodeGraph& out) {
  const cgltf_scene* scene = data.scene;
  if (scene == nullptr && data.scenes_count == 1) scene = &data.scenes[0];
  if (scene == nullptr && data.scenes_count > 1) return ARX_GLB_AMBIGUOUS_SCENE;

  NodeGraph graph;
  graph.parent.assign(data.nodes_count, kInvalidNodeIndex);
  std::vector<std::uint8_t> state(data.nodes_count, 0);
  for (std::size_t start = 0; start < data.nodes_count; ++start) {
    if (!finiteTransform(data.nodes[start])) return ARX_GLB_BAD_FORMAT;
    const cgltf_node* parent = data.nodes[start].parent;
    if (parent != nullptr) {
      const std::size_t parent_index = nodeIndex(data, parent);
      if (parent_index >= data.nodes_count) return ARX_GLB_BAD_FORMAT;
      graph.parent[start] = parent_index;
    }
    if (state[start] != 0) continue;

    std::vector<std::size_t> path;
    std::size_t current = start;
    while (state[current] == 0) {
      state[current] = 1;
      path.push_back(current);
      const cgltf_node* parent = data.nodes[current].parent;
      if (parent == nullptr) break;
      current = nodeIndex(data, parent);
      if (current >= data.nodes_count) return ARX_GLB_BAD_FORMAT;
    }
    if (state[current] == 1 && data.nodes[current].parent != nullptr) return ARX_GLB_BAD_FORMAT;
    for (std::size_t index : path) state[index] = 2;
  }

  graph.world.resize(data.nodes_count);
  for (std::size_t i = 0; i < data.nodes_count; ++i) {
    cgltf_float matrix[16];
    cgltf_node_transform_world(&data.nodes[i], matrix);
    for (std::size_t j = 0; j < 16; ++j) graph.world[i].m[j] = matrix[j];
  }

  graph.reachable.assign(data.nodes_count, false);
  if (scene == nullptr) {
    out = std::move(graph);
    return ARX_OK;
  }
  if (scene->nodes_count != 0 && scene->nodes == nullptr) return ARX_GLB_BAD_FORMAT;
  std::vector<const cgltf_node*> pending;
  pending.reserve(scene->nodes_count);
  for (std::size_t i = scene->nodes_count; i > 0; --i) {
    const cgltf_node* root = scene->nodes[i - 1];
    if (root == nullptr) return ARX_GLB_BAD_FORMAT;
    const std::size_t root_index = nodeIndex(data, root);
    if (root_index >= data.nodes_count || graph.parent[root_index] != kInvalidNodeIndex) return ARX_GLB_BAD_FORMAT;
    pending.push_back(root);
  }
  while (!pending.empty()) {
    const cgltf_node* node = pending.back();
    pending.pop_back();
    if (node == nullptr) return ARX_GLB_BAD_FORMAT;
    std::size_t index = nodeIndex(data, node);
    if (index >= data.nodes_count) return ARX_GLB_BAD_FORMAT;
    if (graph.reachable[index]) continue;
    graph.reachable[index] = true;
    graph.preorder.push_back(index);
    if (node->children_count != 0 && node->children == nullptr) return ARX_GLB_BAD_FORMAT;
    for (std::size_t i = node->children_count; i > 0; --i) pending.push_back(node->children[i - 1]);
  }
  out = std::move(graph);
  return ARX_OK;
}

}  // namespace pistoris::glb
