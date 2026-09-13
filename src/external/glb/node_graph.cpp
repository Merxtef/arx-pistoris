// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#include "node_graph.h"

#include "arx_pistoris/base/status.h"
#include "arx_pistoris/runtime/types.h"

#include "external/glb/utils/transform.h"
#include "utils/log.h"
#include "utils/math/mat4.h"

#include <cmath>
#include <cstddef>
#include <cstdint>
#include <format>
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

bool normalizeNodeTransform(cgltf_node& node) {
  if (node.has_matrix) {
    math::Mat4 transform{};
    for (std::size_t component = 0; component < 16; ++component) transform.m[component] = node.matrix[component];
    if (!canonicalizeAffineTransform(transform)) return false;
    for (std::size_t component = 0; component < 16; ++component) node.matrix[component] = transform.m[component];
    return true;
  }
  if (node.has_translation)
    for (float value : node.translation)
      if (!std::isfinite(value)) return false;
  if (node.has_scale)
    for (float value : node.scale)
      if (!std::isfinite(value)) return false;
  if (node.has_rotation && !normalizeRotation(node.rotation)) return false;
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

bool isDescendantOrSelf(const NodeGraph& graph, std::size_t node, std::size_t ancestor) noexcept {
  while (node != kInvalidNodeIndex) {
    if (node == ancestor) return true;
    if (node >= graph.parent.size()) return false;
    node = graph.parent[node];
  }
  return false;
}

std::size_t nearestCommonAncestor(const NodeGraph& graph, std::size_t first, std::size_t second) noexcept {
  const auto depth = [&](std::size_t node) {
    std::size_t result = 0;
    while (node != kInvalidNodeIndex && node < graph.parent.size()) {
      ++result;
      node = graph.parent[node];
    }
    return result;
  };
  std::size_t first_depth = depth(first);
  std::size_t second_depth = depth(second);
  while (first_depth > second_depth && first < graph.parent.size()) {
    first = graph.parent[first];
    --first_depth;
  }
  while (second_depth > first_depth && second < graph.parent.size()) {
    second = graph.parent[second];
    --second_depth;
  }
  while (first != second) {
    if (first == kInvalidNodeIndex || second == kInvalidNodeIndex || first >= graph.parent.size() ||
        second >= graph.parent.size())
      return kInvalidNodeIndex;
    first = graph.parent[first];
    second = graph.parent[second];
  }
  return first;
}

ArxReturnCode buildNodeGraph(cgltf_data& data, NodeGraph& out) {
  const cgltf_scene* scene = data.scene;
  if (scene == nullptr && data.scenes_count == 1) scene = &data.scenes[0];
  if (scene == nullptr && data.scenes_count > 1) {
    log(ARX_LOG_DEBUG, "GLB node graph failure: {} scenes present without a selected scene", data.scenes_count);
    return ARX_GLB_AMBIGUOUS_SCENE;
  }

  NodeGraph graph;
  graph.parent.assign(data.nodes_count, kInvalidNodeIndex);
  for (std::size_t index = 0; index < data.nodes_count; ++index) {
    if (!normalizeNodeTransform(data.nodes[index])) {
      log(ARX_LOG_DEBUG,
          "GLB node graph failure: node {} '{}' has an invalid transform",
          index,
          data.nodes[index].name != nullptr ? data.nodes[index].name : "");
      return ARX_GLB_BAD_FORMAT;
    }
    const cgltf_node* parent = data.nodes[index].parent;
    if (parent != nullptr) {
      const std::size_t parent_index = nodeIndex(data, parent);
      if (parent_index >= data.nodes_count) {
        log(ARX_LOG_DEBUG, "GLB node graph failure: node {} has an invalid parent", index);
        return ARX_GLB_BAD_FORMAT;
      }
      graph.parent[index] = parent_index;
    }
  }

  graph.world.resize(data.nodes_count);
  std::vector<std::uint8_t> state(data.nodes_count, 0);
  std::vector<std::size_t> path;
  path.reserve(data.nodes_count);
  for (std::size_t start = 0; start < data.nodes_count; ++start) {
    if (state[start] != 0) continue;

    path.clear();
    std::size_t current = start;
    while (state[current] == 0) {
      state[current] = 1;
      path.push_back(current);
      current = graph.parent[current];
      if (current == kInvalidNodeIndex) break;
    }
    if (current != kInvalidNodeIndex && state[current] == 1) {
      log(ARX_LOG_DEBUG, "GLB node graph failure: cycle reaches node {}", current);
      return ARX_GLB_BAD_FORMAT;
    }

    for (auto item = path.rbegin(); item != path.rend(); ++item) {
      cgltf_float values[16];
      cgltf_node_transform_local(&data.nodes[*item], values);
      math::Mat4 local{};
      for (std::size_t component = 0; component < 16; ++component) local.m[component] = values[component];
      const std::size_t parent = graph.parent[*item];
      graph.world[*item] = parent == kInvalidNodeIndex ? local : graph.world[parent] * local;
      state[*item] = 2;
    }
  }

  graph.reachable.assign(data.nodes_count, false);
  if (scene == nullptr) {
    out = std::move(graph);
    return ARX_OK;
  }
  if (scene->nodes_count != 0 && scene->nodes == nullptr) {
    log(ARX_LOG_DEBUG, "GLB node graph failure: scene root array is missing");
    return ARX_GLB_BAD_FORMAT;
  }
  std::vector<const cgltf_node*> pending;
  pending.reserve(scene->nodes_count);
  for (std::size_t i = scene->nodes_count; i > 0; --i) {
    const cgltf_node* root = scene->nodes[i - 1];
    if (root == nullptr) {
      log(ARX_LOG_DEBUG, "GLB node graph failure: scene root {} is null", i - 1U);
      return ARX_GLB_BAD_FORMAT;
    }
    const std::size_t root_index = nodeIndex(data, root);
    if (root_index >= data.nodes_count || graph.parent[root_index] != kInvalidNodeIndex) {
      log(ARX_LOG_DEBUG, "GLB node graph failure: scene root {} is invalid or has a parent", root_index);
      return ARX_GLB_BAD_FORMAT;
    }
    pending.push_back(root);
  }
  while (!pending.empty()) {
    const cgltf_node* node = pending.back();
    pending.pop_back();
    if (node == nullptr) {
      log(ARX_LOG_DEBUG, "GLB node graph failure: reachable child is null");
      return ARX_GLB_BAD_FORMAT;
    }
    std::size_t index = nodeIndex(data, node);
    if (index >= data.nodes_count) {
      log(ARX_LOG_DEBUG, "GLB node graph failure: reachable child leaves node storage");
      return ARX_GLB_BAD_FORMAT;
    }
    if (graph.reachable[index]) continue;
    graph.reachable[index] = true;
    graph.preorder.push_back(index);
    if (node->children_count != 0 && node->children == nullptr) {
      log(ARX_LOG_DEBUG, "GLB node graph failure: node {} child array is missing", index);
      return ARX_GLB_BAD_FORMAT;
    }
    for (std::size_t i = node->children_count; i > 0; --i) pending.push_back(node->children[i - 1]);
  }
  out = std::move(graph);
  return ARX_OK;
}

}  // namespace pistoris::glb
