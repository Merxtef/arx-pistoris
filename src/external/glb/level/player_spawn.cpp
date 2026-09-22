// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#include "player_spawn.h"

#include "arx_pistoris/base/status.h"
#include "arx_pistoris/runtime/types.h"

#include "coordinates.h"
#include "external/glb/node_graph.h"
#include "external/glb/utils/names.h"
#include "external/glb/utils/node.h"
#include "external/glb/utils/transform.h"
#include "external/glb/writer.h"
#include "level/data.h"
#include "modules/scene.h"
#include "names.h"
#include "utils/log.h"
#include "utils/math/mat4.h"
#include "utils/math/quat.h"

#include <cstddef>
#include <format>
#include <optional>
#include <span>
#include <string_view>
#include <vector>

namespace pistoris::glb_level {

void exportPlayerSpawn(const LevelModules& level, glb::Builder& builder) {
  if (!level.scene.player_spawn.has_value()) return;
  const PlayerSpawn& player_spawn = level.scene.player_spawn.value();
  const int node = builder.addNode(kExportPlayerSpawnRootName);
  builder.setNodeTranslation(node, {player_spawn.position.x, player_spawn.position.y, player_spawn.position.z});
  builder.setNodeRotation(node, player_spawn.rotation);
  builder.addRoot(node);
}

ArxReturnCode importPlayerSpawn(const cgltf_data& data, const std::vector<math::Mat4>& world,
                                std::span<const std::size_t> nodes, const ImportUnits& units, LevelModules& level) {
  bool seen = level.scene.player_spawn.has_value();
  for (std::size_t i : nodes) {
    if (i >= data.nodes_count || i >= world.size()) return ARX_GLB_BAD_FORMAT;
    const cgltf_node& node = data.nodes[i];
    const std::string_view name = node.name != nullptr ? node.name : "";
    log(ARX_LOG_DEBUG, "GLB -> Level: importing player spawn node {}", i);
    glb::ParsedLabel label;
    if (!isPlayerSpawnRootName(name, &label)) {
      log(ARX_LOG_DEBUG, "GLB -> Level object failure: player spawn node {} has malformed name '{}'", i, name);
      return ARX_GLB_BAD_LEVEL_PLAYER_SPAWN;
    }
    glb::reportConventionLabel("GLB -> Level player spawn", name, label);
    if (!glb::simpleEmptyNode(node)) {
      log(ARX_LOG_DEBUG, "GLB -> Level object failure: player spawn node {} must be empty", i);
      return ARX_GLB_BAD_LEVEL_PLAYER_SPAWN;
    }
    glb::DecomposedTransform transform;
    if (!glb::decomposeTransform(world[i], transform)) {
      log(ARX_LOG_DEBUG, "GLB -> Level object failure: player spawn node {} has invalid transform", i);
      return ARX_GLB_BAD_LEVEL_PLAYER_SPAWN;
    }
    if (glb::hasNonIdentityLocalScale(node))
      log(ARX_LOG_WARN, "GLB -> Level: player spawn node {} has nonidentity local scale; scale ignored", i);
    if (!seen) {
      const std::optional<ArxVector3> position = toArxPoint(transform.translation, units);
      if (!position) return ARX_GLB_BAD_FORMAT;
      const PlayerSpawn player_spawn{*position, toArxRotation(math::rotationToQuat(transform.rotation))};
      if (scene::validatePlayerSpawn(player_spawn) != scene::Error::kNone) return ARX_GLB_BAD_LEVEL_PLAYER_SPAWN;
      scene::setPlayerSpawn(level.scene, player_spawn);
      seen = true;
    }
  }
  return ARX_OK;
}

}  // namespace pistoris::glb_level
