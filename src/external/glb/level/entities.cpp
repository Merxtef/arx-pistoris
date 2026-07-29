// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#include "entities.h"

#include "arx_pistoris/arx_math.h"
#include "arx_pistoris/paths.hpp"
#include "arx_pistoris/pistoris_types.h"

#include "arx/resource_path.h"
#include "coordinates.h"
#include "external/glb/node_graph.h"
#include "external/glb/utils/level/tokens.h"
#include "external/glb/utils/node.h"
#include "external/glb/utils/transform.h"
#include "external/glb/writer.h"
#include "level/data.h"
#include "modules/scene.h"
#include "objects.h"
#include "utils/log.h"
#include "utils/math/mat4.h"
#include "utils/math/quat.h"
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
namespace {

std::string_view finalPathComponent(std::string_view path) {
  const std::size_t slash = path.find_last_of('/');
  return slash == std::string_view::npos ? path : path.substr(slash + 1);
}

std::string_view entityLabel(const Entity& entity) {
  return entity.name.empty() ? finalPathComponent(entity.class_path) : std::string_view(entity.name);
}

std::string entityNodeName(const Entity& entity, std::size_t ordinal) {
  const std::string ordinal_text = std::format("{:03}", ordinal);
  const std::string_view label = entityLabel(entity);
  if (entity.ident != -1 || label.starts_with("IDENT_")) {
    return joinDoubleUnderscore({"arx_entity", ordinal_text, std::format("IDENT_{}", entity.ident), label});
  }
  return joinDoubleUnderscore({"arx_entity", ordinal_text, label});
}

std::string entityClassHelperName(const Entity& entity) {
  paths::ModelPathView model;
  if (paths::modelFromEntityClass(entity.class_path, model)) {
    std::string shorthand;
    if (paths::modelShorthand(model, shorthand)) return std::format("CLASS_{}__{}", shorthand, entityLabel(entity));
  }
  return std::format("CLASS_{}__{}", entity.class_path, entityLabel(entity));
}

bool entityClassFromModelShorthand(std::string_view shorthand, std::string& out, bool& normalized_legacy_teo) {
  normalized_legacy_teo = false;
  if (shorthand.size() >= 4 && isLegacyTeoExtension(shorthand.substr(shorthand.size() - 4))) {
    shorthand.remove_suffix(4);
    normalized_legacy_teo = true;
  }
  paths::ModelPathView model;
  if (!paths::modelFromShorthand(shorthand, model)) return false;
  return paths::entityClassFromModel(model, out);
}

struct ParsedEntityRoot {
  std::optional<std::uint32_t> ordinal;
  std::int32_t ident = -1;
  std::string name;
};

ArxReturnCode parseEntityRoot(std::string_view name, ParsedEntityRoot& out) {
  constexpr std::string_view kPrefix = "arx_entity__";
  if (!name.starts_with(kPrefix)) return ARX_GLB_BAD_LEVEL_ENTITY;
  std::vector<std::string_view> tokens;
  splitDoubleUnderscore(name.substr(kPrefix.size()), tokens);
  if (std::ranges::any_of(tokens, [](std::string_view token) { return token.empty(); }))
    return ARX_GLB_BAD_LEVEL_ENTITY;

  std::size_t next = 0;
  if (tokens.size() > 1) {
    if (const auto ordinal = parseUnsignedToken(tokens.front())) {
      out.ordinal = ordinal;
      ++next;
    }
  }
  if (next < tokens.size() && tokens[next].starts_with("IDENT_")) {
    const auto ident = parseSignedToken(tokens[next].substr(6));
    if (!ident) return ARX_GLB_BAD_LEVEL_ENTITY;
    out.ident = *ident;
    ++next;
  }
  if (next < tokens.size()) out.name = tokens[next++];
  return next == tokens.size() ? ARX_OK : ARX_GLB_BAD_LEVEL_ENTITY;
}

ArxReturnCode entityClassPath(const cgltf_data& data, const cgltf_node& root, std::string& out,
                              bool& normalized_legacy_teo) {
  constexpr std::string_view kPrefix = "CLASS_";
  std::string normalized;
  bool found = false;
  normalized_legacy_teo = false;
  for (std::size_t i = 0; i < root.children_count; ++i) {
    if (root.children[i] == nullptr) return ARX_GLB_BAD_FORMAT;
    const cgltf_node& child = *root.children[i];
    const std::ptrdiff_t index = root.children[i] - data.nodes;
    if (index < 0 || static_cast<std::size_t>(index) >= data.nodes_count) return ARX_GLB_BAD_FORMAT;
    const std::string_view name = child.name != nullptr ? child.name : "";
    if (!name.starts_with(kPrefix)) continue;
    const std::size_t label_separator = name.rfind("__");
    if (label_separator == std::string_view::npos || label_separator < kPrefix.size() ||
        label_separator + 2 == name.size() || !glb::simpleEmptyNode(child))
      return ARX_GLB_BAD_LEVEL_ENTITY;
    const std::string_view encoded = name.substr(kPrefix.size(), label_separator - kPrefix.size());
    std::string candidate;
    bool candidate_legacy_teo = false;
    if (constexpr std::string_view kModelPrefix = "model:"; encoded.starts_with(kModelPrefix)) {
      if (!entityClassFromModelShorthand(encoded, candidate, candidate_legacy_teo)) return ARX_GLB_BAD_LEVEL_ENTITY;
    } else {
      std::string_view removed_extension;
      if (!normalizeEntityClassPath(encoded, candidate, removed_extension) ||
          (!removed_extension.empty() && !isLegacyTeoExtension(removed_extension))) {
        return ARX_GLB_BAD_LEVEL_ENTITY;
      }
      candidate_legacy_teo = isLegacyTeoExtension(removed_extension);
    }
    if (!found) {
      normalized = std::move(candidate);
      normalized_legacy_teo = candidate_legacy_teo;
      found = true;
    }
  }
  if (!found) return ARX_GLB_BAD_LEVEL_ENTITY;
  out = std::move(normalized);
  return ARX_OK;
}

}  // namespace

void exportEntities(const LevelModules& level, const ArxAabb& referenced_bounds, glb::Builder& builder) {
  if (level.scene.entities.empty()) return;
  ArxVector3 root = bottomCenter(referenced_bounds);
  root.y += kEntityParentOffset;
  const int parent = builder.addNode("entities_parent");
  builder.setNodeTranslation(parent, {root.x, root.y, root.z});
  builder.addRoot(parent);
  for (std::size_t i = 0; i < level.scene.entities.size(); ++i) {
    const Entity& entity = level.scene.entities[i];
    const std::string name = entityNodeName(entity, i);
    const int node = builder.addNode(name);
    builder.setNodeTranslation(node,
                               {entity.position.x - root.x, entity.position.y - root.y, entity.position.z - root.z});
    builder.setNodeRotation(node, entity.rotation);
    builder.addChild(node, builder.addNode(entityClassHelperName(entity)));
    builder.addChild(parent, node);
  }
}

ArxReturnCode importEntities(const cgltf_data& data, const std::vector<math::Mat4>& world,
                             std::span<const std::size_t> nodes, const ImportUnits& units, LevelModules& level,
                             std::uint64_t& normalized_legacy_teo) {
  struct PendingEntity {
    std::optional<std::uint32_t> ordinal;
    std::size_t node_index = 0;
    Entity entity;
  };
  std::vector<PendingEntity> entities;
  std::vector<std::uint32_t> ordinals;
  for (std::size_t i : nodes) {
    if (i >= data.nodes_count || i >= world.size()) return ARX_GLB_BAD_FORMAT;
    const cgltf_node& node = data.nodes[i];
    const std::string_view name = node.name != nullptr ? node.name : "";
    log(ARX_LOG_DEBUG, std::format("GLB -> Level: importing entity node {} '{}'", i, name));
    if (node.camera != nullptr || node.light != nullptr || node.skin != nullptr || node.extensions_count != 0 ||
        node.has_mesh_gpu_instancing) {
      log(ARX_LOG_DEBUG,
          std::format("GLB -> Level object failure: entity node {} '{}' has unsupported root payload", i, name));
      return ARX_GLB_BAD_LEVEL_ENTITY;
    }
    ParsedEntityRoot parsed;
    ArxReturnCode rc = parseEntityRoot(name, parsed);
    if (rc != ARX_OK) {
      log(ARX_LOG_DEBUG, std::format("GLB -> Level object failure: entity node {} '{}' has invalid name", i, name));
      return rc;
    }
    const auto& ordinal = parsed.ordinal;
    if (ordinal) {
      if (std::find(ordinals.begin(), ordinals.end(), *ordinal) != ordinals.end()) {
        log(ARX_LOG_DEBUG,
            std::format("GLB -> Level object failure: entity node {} '{}' duplicates ordinal {}", i, name, *ordinal));
        return ARX_GLB_BAD_LEVEL_ENTITY;
      }
      ordinals.push_back(*ordinal);
    }
    glb::DecomposedTransform transform;
    if (!glb::decomposeTransform(world[i], transform)) {
      log(ARX_LOG_DEBUG,
          std::format("GLB -> Level object failure: entity node {} '{}' has invalid transform", i, name));
      return ARX_GLB_BAD_LEVEL_ENTITY;
    }
    if (glb::hasNonIdentityLocalScale(node))
      log(ARX_LOG_WARN, std::format("GLB -> Level: entity '{}' has nonidentity local scale; scale ignored", name));
    const std::optional<ArxVector3> position = toArxPoint(transform.translation, units);
    if (!position) return ARX_GLB_BAD_FORMAT;
    Entity entity;
    entity.ident = parsed.ident;
    entity.position = *position;
    entity.rotation = toArxRotation(math::rotationToQuat(transform.rotation));
    entity.name = std::move(parsed.name);
    if (!scene::normalizeRotation(entity.rotation)) return ARX_GLB_BAD_LEVEL_ENTITY;
    bool entity_legacy_teo = false;
    rc = entityClassPath(data, node, entity.class_path, entity_legacy_teo);
    if (rc != ARX_OK) {
      log(ARX_LOG_DEBUG,
          std::format("GLB -> Level object failure: entity node {} '{}' has invalid/missing class helper", i, name));
      return rc;
    }
    if (entity_legacy_teo) ++normalized_legacy_teo;
    if (scene::validateEntity(entity) != scene::Error::kNone) return ARX_GLB_BAD_LEVEL_ENTITY;
    entities.push_back({parsed.ordinal, i, std::move(entity)});
  }
  std::sort(entities.begin(), entities.end(), [](const PendingEntity& a, const PendingEntity& b) {
    const auto& a_ordinal = a.ordinal;
    const auto& b_ordinal = b.ordinal;
    if (a_ordinal.has_value() != b_ordinal.has_value()) return a_ordinal.has_value();
    if (a_ordinal && b_ordinal && *a_ordinal != *b_ordinal) return *a_ordinal < *b_ordinal;
    return a.node_index < b.node_index;
  });
  level.scene.entities.reserve(level.scene.entities.size() + entities.size());
  for (PendingEntity& pending : entities) level.scene.entities.push_back(std::move(pending.entity));
  scene::makeEntityNamesUnique(level.scene.entities);
  return ARX_OK;
}

}  // namespace pistoris::glb_level
