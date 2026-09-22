// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#include "paths.h"

#include "arx_pistoris/base/math.hpp"
#include "arx_pistoris/base/status.h"
#include "arx_pistoris/runtime/types.h"

#include "coordinates.h"
#include "external/glb/utils/names.h"
#include "external/glb/utils/node.h"
#include "external/glb/utils/tokens.h"
#include "external/glb/utils/transform.h"
#include "level/data.h"
#include "modules/scene.h"
#include "objects.h"
#include "utils/log.h"
#include "utils/math/mat4.h"
#include "utils/name_tokens.h"

#include <algorithm>
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

using glb::parseUnsignedToken;
namespace {

struct ParsedPathNode {
  std::uint32_t ordinal = 0;
  PathNodeType type = PathNodeType::kStandard;
  std::uint32_t time_ms = 0;
  glb::ParsedLabel label;
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
  return std::nullopt;
}

std::string_view pathNodeType(PathNodeType type) {
  switch (type) {
    case PathNodeType::kStandard:
      return "STANDARD";
    case PathNodeType::kBezier:
      return "BEZIER";
  }
  return "STANDARD";
}

bool parsePathNodeName(std::string_view name, ParsedPathNode& out) {
  glb::ParsedLabel label;
  const bool parsed = glb::parseRecoverableLabel(name,
                                                 out,
                                                 &label,
                                                 glb::ConventionOptions{{"STANDARD", "BEZIER"}, {"TIME_"}},
                                                 [](std::span<const std::string_view> tokens, ParsedPathNode& value) {
                                                   if (tokens.size() != 3) return false;
                                                   const auto ordinal = parseUnsignedToken(tokens[0]);
                                                   const auto type = pathNodeType(tokens[1]);
                                                   constexpr std::string_view kTime = "TIME_";
                                                   if (!ordinal || !type || !tokens[2].starts_with(kTime)) return false;
                                                   const auto time = parseUnsignedToken(tokens[2].substr(kTime.size()));
                                                   if (!time) return false;
                                                   value.ordinal = *ordinal;
                                                   value.type = *type;
                                                   value.time_ms = *time;
                                                   return true;
                                                 });
  if (parsed) out.label = label;
  return parsed;
}

std::string pathNodeName(const PathNode& node, std::string_view helper_label, std::size_t ordinal) {
  std::string ordinal_text = std::format("{:03}", ordinal);
  std::string time = std::format("TIME_{}", node.time_ms);
  return joinDoubleUnderscore({ordinal_text, pathNodeType(node.type), time, helper_label});
}

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
                          std::span<const std::size_t> roots, const ImportUnits& units, LevelModules& level) {
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
    glb::DecomposedTransform decomposed;
    if (!glb::decomposeTransform(world[node_index], decomposed)) return ARX_GLB_BAD_LEVEL_PATH;

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
      glb::reportConventionLabel(
          "GLB -> Level path node", child->name != nullptr ? child->name : "", parsed_node.label);
      if (std::find(node_ordinals.begin(), node_ordinals.end(), parsed_node.ordinal) != node_ordinals.end())
        return ARX_GLB_BAD_LEVEL_PATH;
      node_ordinals.push_back(parsed_node.ordinal);
      parsed_node.node_index = static_cast<std::size_t>(child_index);
      parsed_node.position = math::translation(world[parsed_node.node_index]);
      if (!glb::decomposeTransform(world[parsed_node.node_index], decomposed)) return ARX_GLB_BAD_LEVEL_PATH;
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
      const std::optional<ArxVector3> relative = toArxVector(node.position - nodes.front().position, units);
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
  const std::size_t repaired = scene::repairPathNames(level.scene.paths);
  if (repaired != 0) log(ARX_LOG_WARN, "GLB -> Level repairs: {} path name(s) repaired", repaired);
  return ARX_OK;
}

}  // namespace pistoris::glb_level
