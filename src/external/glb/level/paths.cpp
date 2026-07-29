// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#include "paths.h"

#include "arx_pistoris/arx_math.hpp"
#include "arx_pistoris/pistoris_types.h"

#include "coordinates.h"
#include "external/glb/utils/level/tokens.h"
#include "external/glb/utils/node.h"
#include "level/data.h"
#include "modules/scene.h"
#include "objects.h"
#include "utils/math/mat3.h"
#include "utils/math/mat4.h"
#include "utils/name_tokens.h"

#include <algorithm>
#include <cmath>
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
namespace {

constexpr float kTransformTolerance = 1.0e-4f;

struct ParsedPathNode {
  std::uint32_t ordinal = 0;
  PathNodeType type = PathNodeType::kStandard;
  std::uint32_t time_ms = 0;
  std::size_t node_index = 0;
  ArxVector3 position = {};
};

bool parsePathName(std::string_view name, std::string& out) {
  constexpr std::string_view kPrefix = "arx_path__";
  std::string_view rest = name.substr(kPrefix.size());
  if (rest.empty() || hasDoubleUnderscore(rest)) return false;
  out = rest;
  return true;
}

std::optional<PathNodeType> pathNodeType(std::string_view token) {
  if (token == "STANDARD") return PathNodeType::kStandard;
  if (token == "BEZIER") return PathNodeType::kBezier;
  if (token == "CONTROL") return PathNodeType::kControlPoint;
  return std::nullopt;
}

std::string_view pathNodeType(PathNodeType type) {
  switch (type) {
    case PathNodeType::kStandard:
      return "STANDARD";
    case PathNodeType::kBezier:
      return "BEZIER";
    case PathNodeType::kControlPoint:
      return "CONTROL";
  }
  return "STANDARD";
}

bool parsePathNodeName(std::string_view name, ParsedPathNode& out) {
  std::vector<std::string_view> tokens;
  splitDoubleUnderscore(name, tokens);
  if (tokens.size() < 3 || tokens.size() > 4 || (tokens.size() == 4 && tokens.back().empty())) return false;

  auto ordinal = parseUnsignedToken(tokens[0]);
  if (!ordinal) return false;

  auto type = pathNodeType(tokens[1]);
  if (!type) return false;

  constexpr std::string_view kTime = "TIME_";
  if (!tokens[2].starts_with(kTime)) return false;
  auto time = parseUnsignedToken(tokens[2].substr(kTime.size()));
  if (!time) return false;

  out.ordinal = *ordinal;
  out.type = *type;
  out.time_ms = *time;
  return true;
}

bool usableTransform(const math::Mat4& transform) {
  ArxVector3 columns[3] = {
      {transform(0, 0), transform(1, 0), transform(2, 0)},
      {transform(0, 1), transform(1, 1), transform(2, 1)},
      {transform(0, 2), transform(1, 2), transform(2, 2)},
  };
  float scale[3] = {math::lengthf(columns[0]), math::lengthf(columns[1]), math::lengthf(columns[2])};
  for (float value : scale)
    if (!std::isfinite(value) || value <= 0.0f) return false;
  if (std::abs(math::dotf(columns[0], columns[1])) > kTransformTolerance * scale[0] * scale[1] ||
      std::abs(math::dotf(columns[0], columns[2])) > kTransformTolerance * scale[0] * scale[2] ||
      std::abs(math::dotf(columns[1], columns[2])) > kTransformTolerance * scale[1] * scale[2])
    return false;

  ArxMat3 rotation;
  for (int row = 0; row < 3; ++row)
    for (int column = 0; column < 3; ++column) rotation(row, column) = transform(row, column) / scale[column];
  if (!std::isfinite(math::determinant(rotation)) || math::determinant(rotation) <= 0.0f) return false;

  ArxVector3 position = math::translation(transform);
  return std::isfinite(position.x) && std::isfinite(position.y) && std::isfinite(position.z);
}

std::string pathNodeName(const PathNode& node, std::string_view helper_label, std::size_t ordinal) {
  std::string ordinal_text = std::format("{:03}", ordinal);
  std::string time = std::format("TIME_{}", node.time_ms);
  return joinDoubleUnderscore({ordinal_text, pathNodeType(node.type), time, helper_label});
}

ArxVector3 subtract(const ArxVector3& a, const ArxVector3& b) { return {a.x - b.x, a.y - b.y, a.z - b.z}; }

}  // namespace

void exportPaths(const LevelModules& level, const ArxAabb& referenced_bounds, glb::Builder& builder) {
  if (level.scene.paths.empty()) return;
  ArxVector3 parent_position = bottomCenter(referenced_bounds);
  parent_position.y += kPathParentOffset;
  int parent = builder.addNode("paths_parent");
  builder.setNodeTranslation(parent, {parent_position.x, parent_position.y, parent_position.z});
  builder.addRoot(parent);

  for (const Path& path : level.scene.paths) {
    int root = builder.addNode(std::format("arx_path__{}", path.name));
    builder.setNodeTranslation(root,
                               {path.position.x - parent_position.x,
                                path.position.y - parent_position.y,
                                path.position.z - parent_position.z});
    builder.addChild(parent, root);
    for (std::size_t node_ordinal = 0; node_ordinal < path.nodes.size(); ++node_ordinal) {
      const PathNode& path_node = path.nodes[node_ordinal];
      int node = builder.addNode(pathNodeName(path_node, path.name, node_ordinal));
      builder.setNodeTranslation(
          node, {path_node.relative_position.x, path_node.relative_position.y, path_node.relative_position.z});
      builder.addChild(root, node);
    }
  }
}

ArxReturnCode importPaths(const cgltf_data& data, const std::vector<math::Mat4>& world,
                          std::span<const std::size_t> roots, const ImportUnits& units, LevelModules& level,
                          std::vector<std::string>& warnings) {
  struct PendingPath {
    std::size_t node_index = 0;
    Path path;
  };
  std::vector<PendingPath> paths;

  for (std::size_t node_index : roots) {
    if (node_index >= data.nodes_count || node_index >= world.size()) return ARX_GLB_BAD_FORMAT;
    const cgltf_node& root = data.nodes[node_index];
    std::string_view name = root.name != nullptr ? root.name : "";
    if (!glb::simpleEmptyNode(root)) return ARX_GLB_BAD_LEVEL_PATH;

    std::string path_name;
    if (!parsePathName(name, path_name)) return ARX_GLB_BAD_LEVEL_PATH;
    if (!usableTransform(world[node_index])) return ARX_GLB_BAD_LEVEL_PATH;

    std::vector<ParsedPathNode> nodes;
    std::vector<std::uint32_t> node_ordinals;
    for (std::size_t i = 0; i < root.children_count; ++i) {
      const cgltf_node* child = root.children[i];
      if (child == nullptr) return ARX_GLB_BAD_FORMAT;
      std::ptrdiff_t child_index = child - data.nodes;
      if (child_index < 0 || static_cast<std::size_t>(child_index) >= data.nodes_count) return ARX_GLB_BAD_FORMAT;
      ParsedPathNode parsed_node;
      if (!parsePathNodeName(child->name != nullptr ? child->name : "", parsed_node)) return ARX_GLB_BAD_LEVEL_PATH;
      if (!glb::simpleEmptyNode(*child)) return ARX_GLB_BAD_LEVEL_PATH;
      if (std::find(node_ordinals.begin(), node_ordinals.end(), parsed_node.ordinal) != node_ordinals.end())
        return ARX_GLB_BAD_LEVEL_PATH;
      node_ordinals.push_back(parsed_node.ordinal);
      parsed_node.node_index = static_cast<std::size_t>(child_index);
      parsed_node.position = math::translation(world[parsed_node.node_index]);
      if (!usableTransform(world[parsed_node.node_index])) return ARX_GLB_BAD_LEVEL_PATH;
      nodes.push_back(parsed_node);
    }
    if (nodes.empty()) return ARX_GLB_BAD_LEVEL_PATH;
    std::sort(nodes.begin(), nodes.end(), [](const ParsedPathNode& a, const ParsedPathNode& b) {
      return a.ordinal < b.ordinal;
    });

    PendingPath pending;
    const std::optional<ArxVector3> position = toArxPoint(nodes.front().position, units);
    if (!position) return ARX_GLB_BAD_FORMAT;
    pending.node_index = node_index;
    pending.path.name = std::move(path_name);
    pending.path.position = *position;
    pending.path.nodes.reserve(nodes.size());
    for (ParsedPathNode& node : nodes) {
      const std::optional<ArxVector3> relative = toArxVector(subtract(node.position, nodes.front().position), units);
      if (!relative) return ARX_GLB_BAD_FORMAT;
      pending.path.nodes.push_back({*relative, node.type, node.time_ms});
    }
    pending.path.nodes.front().relative_position = {};
    pending.path.nodes.front().time_ms = 0;
    paths.push_back(std::move(pending));
  }

  std::sort(paths.begin(), paths.end(), [](const PendingPath& a, const PendingPath& b) {
    return a.node_index < b.node_index;
  });
  level.scene.paths.reserve(paths.size());
  for (PendingPath& pending : paths) level.scene.paths.push_back(std::move(pending.path));
  std::size_t renamed = scene::makePathNamesUnique(level.scene.paths);
  if (renamed != 0) warnings.push_back(std::format("GLB -> Level repairs: {} duplicate path name(s) renamed", renamed));
  return ARX_OK;
}

}  // namespace pistoris::glb_level
