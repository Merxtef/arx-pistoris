// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#include "discovery.h"

#include "arx_pistoris/base/status.h"
#include "arx_pistoris/runtime/types.h"

#include "cgltf/cgltf.h"
#include "external/glb/node_graph.h"
#include "names.h"
#include "utils/log.h"

#include <algorithm>
#include <cstddef>
#include <string_view>
#include <utility>
#include <vector>

namespace pistoris::glb_level {
namespace {

struct NodeContext {
  std::size_t room = glb::kInvalidNodeIndex;
  std::size_t terminal = glb::kInvalidNodeIndex;
  bool ignored = false;
  bool diagnostic = false;
};

struct IgnoredDescendants {
  std::size_t nodes = 0;
  std::size_t meshes = 0;
};

std::string_view nodeName(const cgltf_node& node) { return node.name != nullptr ? node.name : ""; }

std::string_view objectKindName(LevelObjectKind kind) {
  switch (kind) {
    case LevelObjectKind::kPortal:
      return "portal";
    case LevelObjectKind::kAnchor:
      return "anchor";
    case LevelObjectKind::kLight:
      return "light";
    case LevelObjectKind::kPlayerSpawn:
      return "player spawn";
    case LevelObjectKind::kEntity:
      return "entity";
    case LevelObjectKind::kFog:
      return "fog";
    case LevelObjectKind::kZone:
      return "zone";
    case LevelObjectKind::kPath:
      return "path";
    case LevelObjectKind::kNavSurface:
      return "navigation surface";
    case LevelObjectKind::kMinimap:
      return "minimap";
    case LevelObjectKind::kRoom:
      return "room";
    case LevelObjectKind::kNone:
      return "ordinary";
  }
  return "ordinary";
}

void appendObject(LevelDiscovery& discovery, LevelObjectKind kind, std::size_t node) {
  switch (kind) {
    case LevelObjectKind::kRoom:
      discovery.rooms.push_back(node);
      break;
    case LevelObjectKind::kPortal:
      discovery.portals.push_back(node);
      break;
    case LevelObjectKind::kAnchor:
      discovery.anchors.push_back(node);
      break;
    case LevelObjectKind::kLight:
      discovery.lights.push_back(node);
      break;
    case LevelObjectKind::kPlayerSpawn:
      discovery.player_spawns.push_back(node);
      break;
    case LevelObjectKind::kEntity:
      discovery.entities.push_back(node);
      break;
    case LevelObjectKind::kFog:
      discovery.fogs.push_back(node);
      break;
    case LevelObjectKind::kZone:
      discovery.zones.push_back(node);
      break;
    case LevelObjectKind::kPath:
      discovery.paths.push_back(node);
      break;
    case LevelObjectKind::kNavSurface:
      discovery.navigation_surfaces.push_back(node);
      break;
    case LevelObjectKind::kMinimap:
      discovery.minimaps.push_back(node);
      break;
    case LevelObjectKind::kNone:
      break;
  }
}

void sortObjectLists(LevelDiscovery& discovery) {
  auto sort = [](std::vector<std::size_t>& values) { std::ranges::sort(values); };
  sort(discovery.rooms);
  sort(discovery.portals);
  sort(discovery.anchors);
  sort(discovery.lights);
  sort(discovery.player_spawns);
  sort(discovery.entities);
  sort(discovery.fogs);
  sort(discovery.zones);
  sort(discovery.paths);
  sort(discovery.navigation_surfaces);
  sort(discovery.minimaps);
}

}  // namespace

ArxReturnCode discoverLevelNodes(const cgltf_data& data, const glb::NodeGraph& graph, LevelDiscovery& out) {
  if (graph.world.size() != data.nodes_count || graph.parent.size() != data.nodes_count ||
      graph.reachable.size() != data.nodes_count)
    return ARX_GLB_BAD_FORMAT;

  LevelDiscovery discovery;
  std::vector<NodeContext> contexts(data.nodes_count);
  std::vector<LevelObjectKind> objects(data.nodes_count, LevelObjectKind::kNone);
  std::vector<IgnoredDescendants> ignored(data.nodes_count);

  for (std::size_t node_index : graph.preorder) {
    if (node_index >= data.nodes_count || !graph.reachable[node_index]) return ARX_GLB_BAD_FORMAT;
    const cgltf_node& node = data.nodes[node_index];
    const std::size_t parent = graph.parent[node_index];
    if (parent != glb::kInvalidNodeIndex && parent >= data.nodes_count) return ARX_GLB_BAD_FORMAT;
    NodeContext context = parent == glb::kInvalidNodeIndex ? NodeContext{} : contexts[parent];

    if (context.diagnostic) {
      contexts[node_index] = context;
      continue;
    }
    if (context.ignored) {
      ++ignored[context.terminal].nodes;
      if (node.mesh != nullptr) ++ignored[context.terminal].meshes;
      contexts[node_index] = context;
      continue;
    }
    if (context.terminal != glb::kInvalidNodeIndex) {
      const bool direct = parent == context.terminal;
      if (direct && directLevelHelper(objects[context.terminal], nodeName(node))) {
        contexts[node_index] = context;
        continue;
      }
      context.ignored = true;
      ++ignored[context.terminal].nodes;
      if (node.mesh != nullptr) ++ignored[context.terminal].meshes;
      contexts[node_index] = context;
      continue;
    }

    const std::string_view name = nodeName(node);
    const LevelObjectKind kind = levelObjectKind(node);
    objects[node_index] = kind;
    if (kind == LevelObjectKind::kRoom) {
      if (context.room != glb::kInvalidNodeIndex) {
        log(ARX_LOG_DEBUG,
            "GLB -> Level object failure: room node {} '{}' is nested inside room node {} '{}'",
            node_index,
            name,
            context.room,
            nodeName(data.nodes[context.room]));
        return ARX_GLB_BAD_LEVEL_HIERARCHY;
      }
      context.room = node_index;
      appendObject(discovery, kind, node_index);
      if (node.mesh != nullptr) discovery.geometry.push_back({node_index, node_index});
      contexts[node_index] = context;
      continue;
    }
    if (kind != LevelObjectKind::kNone) {
      appendObject(discovery, kind, node_index);
      context.terminal = node_index;
      contexts[node_index] = context;
      continue;
    }
    if (levelDiagnosticRootName(name)) {
      context.diagnostic = true;
      contexts[node_index] = context;
      continue;
    }
    if (name.starts_with("arx_")) {
      log(ARX_LOG_WARN,
          "GLB -> Level: node {} '{}' uses the reserved arx_ namespace but is not recognized; treated as ordinary "
          "payload",
          node_index,
          name);
    }
    if (node.mesh != nullptr) discovery.geometry.push_back({node_index, context.room});
    contexts[node_index] = context;
  }

  for (std::size_t node_index : graph.preorder) {
    if (ignored[node_index].nodes == 0) continue;
    log(ARX_LOG_WARN,
        "GLB -> Level: {} node {} '{}' has unexpected descendants; ignored {} node(s), including {} mesh node(s)",
        objectKindName(objects[node_index]),
        node_index,
        nodeName(data.nodes[node_index]),
        ignored[node_index].nodes,
        ignored[node_index].meshes);
  }

  sortObjectLists(discovery);
  out = std::move(discovery);
  return ARX_OK;
}

}  // namespace pistoris::glb_level
