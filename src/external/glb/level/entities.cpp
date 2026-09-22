// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#include "entities.h"

#include "arx_pistoris/base/math.h"
#include "arx_pistoris/base/status.h"
#include "arx_pistoris/level/types.h"
#include "arx_pistoris/paths.hpp"
#include "arx_pistoris/runtime/types.h"

#include "coordinates.h"
#include "external/glb/model/mesh_export.h"
#include "external/glb/node_graph.h"
#include "external/glb/utils/names.h"
#include "external/glb/utils/node.h"
#include "external/glb/utils/tokens.h"
#include "external/glb/utils/transform.h"
#include "external/glb/writer.h"
#include "level/data.h"
#include "model/data.h"
#include "modules/scene.h"
#include "objects.h"
#include "paths/entity_class.h"
#include "utils/identifier.h"
#include "utils/log.h"
#include "utils/math/mat4.h"
#include "utils/math/quat.h"
#include "utils/math/rotation.h"
#include "utils/name_tokens.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <format>
#include <functional>
#include <map>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace pistoris::glb_level {

using glb::parseSignedToken;
using glb::parseUnsignedToken;
namespace {

std::string_view finalPathComponent(std::string_view path) {
  const std::size_t slash = path.find_last_of('/');
  return slash == std::string_view::npos ? path : path.substr(slash + 1);
}

std::string_view entityExportName(const Entity& entity) {
  return entity.name.empty() ? finalPathComponent(entity.class_path) : std::string_view(entity.name);
}

}  // namespace

std::string entityNodeName(std::string_view name, std::size_t ordinal, std::int32_t ident) {
  const std::string ordinal_text = std::format("{:03}", ordinal);
  if (ident != -1 || name.starts_with("IDENT_")) {
    return joinDoubleUnderscore({"arx_entity", ordinal_text, std::format("IDENT_{}", ident), name});
  }
  return joinDoubleUnderscore({"arx_entity", ordinal_text, name});
}

std::string entityClassHelperName(std::string_view class_path, std::string_view label) {
  paths::ModelPathView model;
  if (paths::modelFromEntityClass(class_path, model)) {
    std::string selector;
    if (paths::modelSelector(model, selector)) return std::format("CLASS_{}__{}", selector, label);
  }
  return std::format("CLASS_{}__{}", class_path, label);
}

namespace {

bool entityClassFromModelSelector(std::string_view selector, std::string& out, bool& normalized_legacy_teo) {
  normalized_legacy_teo = false;
  if (selector.size() >= 4 && isLegacyTeoExtension(selector.substr(selector.size() - 4))) {
    selector.remove_suffix(4);
    normalized_legacy_teo = true;
  }
  paths::ModelPathView model;
  if (!paths::modelFromSelector(selector, model)) return false;
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
    const std::optional<glb::LabeledValue> labeled = glb::splitRequiredLabel(name.substr(kPrefix.size()));
    if (!labeled || !glb::simpleEmptyNode(child)) return ARX_GLB_BAD_LEVEL_ENTITY;
    const std::string_view encoded = labeled->value;
    std::string candidate;
    bool candidate_legacy_teo = false;
    if (constexpr std::string_view kModelPrefix = "model:"; encoded.starts_with(kModelPrefix)) {
      if (!entityClassFromModelSelector(encoded, candidate, candidate_legacy_teo)) return ARX_GLB_BAD_LEVEL_ENTITY;
    } else {
      std::string_view removed_extension;
      bool discarded_prefix = false;
      if (!normalizeEntityClassPath(encoded, candidate, removed_extension, &discarded_prefix) ||
          (!removed_extension.empty() && !isLegacyTeoExtension(removed_extension))) {
        return ARX_GLB_BAD_LEVEL_ENTITY;
      }
      if (discarded_prefix) {
        log(ARX_LOG_WARN, "GLB -> Level: entity class path '{}' is resolved by the game as '{}'", encoded, candidate);
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

ArxReturnCode exportEntities(const LevelModules& level, const ArxAabb& referenced_bounds,
                             std::span<const ModelModules* const> model_previews, ArxLevelModelPreviewReport& report,
                             glb::Builder& builder) {
  std::map<std::string, const ModelModules*, std::less<>> previews;
  for (const ModelModules* model : model_previews) {
    if (model->resource.path.empty()) {
      ++report.skipped_anonymous_models;
      continue;
    }
    paths::ModelPathView model_path;
    std::string class_path;
    if (!paths::modelFromFtl(model->resource.path, model_path) ||
        !paths::baseEntityClassFromModel(model_path, class_path)) {
      ++report.skipped_unmappable_models;
      continue;
    }
    if (!previews.emplace(std::move(class_path), model).second) {
      ++report.skipped_duplicate_models;
      continue;
    }
    ++report.mapped_models;
  }

  if (level.scene.entities.empty()) return ARX_OK;
  ArxVector3 root = bottomCenter(referenced_bounds);
  root.y += kEntityParentOffset;
  const int parent = builder.addNode("entities_parent");
  builder.setNodeTranslation(parent, {root.x, root.y, root.z});
  builder.addRoot(parent);
  std::map<std::string_view, int, std::less<>> meshes;
  for (std::size_t i = 0; i < level.scene.entities.size(); ++i) {
    const Entity& entity = level.scene.entities[i];
    const std::string_view entity_name = entityExportName(entity);
    const std::string name = entityNodeName(entity_name, i, entity.ident);
    int mesh = -1;
    const auto preview = previews.find(entity.class_path);
    if (preview != previews.end()) {
      const auto [cached, inserted] = meshes.emplace(preview->first, -1);
      if (inserted) {
        const glb_model::ModelMeshExportOptions options{
            .level_basis = true,
            .context = "Model Level preview -> GLB",
            .mesh_name = entity_name,
        };
        const ArxReturnCode rc = glb_model::addModelMesh(*preview->second, options, builder, cached->second);
        if (rc != ARX_OK) return rc;
      }
      mesh = cached->second;
      ++report.previewed_entities;
    }
    const int node = builder.addNode(name, mesh);
    builder.setNodeTranslation(node,
                               {entity.position.x - root.x, entity.position.y - root.y, entity.position.z - root.z});
    builder.setNodeRotation(node, entity.rotation);
    builder.addChild(node, builder.addNode(entityClassHelperName(entity.class_path, entity_name)));
    builder.addChild(parent, node);
  }
  return ARX_OK;
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
    log(ARX_LOG_DEBUG, "GLB -> Level: importing entity node {} '{}'", i, name);
    if (node.camera != nullptr || node.light != nullptr || node.skin != nullptr || node.extensions_count != 0 ||
        node.has_mesh_gpu_instancing) {
      log(ARX_LOG_DEBUG, "GLB -> Level object failure: entity node {} '{}' has unsupported root payload", i, name);
      return ARX_GLB_BAD_LEVEL_ENTITY;
    }
    ParsedEntityRoot parsed;
    ArxReturnCode rc = parseEntityRoot(name, parsed);
    if (rc != ARX_OK) {
      log(ARX_LOG_DEBUG, "GLB -> Level object failure: entity node {} '{}' has invalid name", i, name);
      return rc;
    }
    const auto& ordinal = parsed.ordinal;
    if (ordinal) {
      if (std::find(ordinals.begin(), ordinals.end(), *ordinal) != ordinals.end()) {
        log(ARX_LOG_DEBUG, "GLB -> Level object failure: entity node {} '{}' duplicates ordinal {}", i, name, *ordinal);
        return ARX_GLB_BAD_LEVEL_ENTITY;
      }
      ordinals.push_back(*ordinal);
    }
    glb::DecomposedTransform transform;
    if (!glb::decomposeTransform(world[i], transform)) {
      log(ARX_LOG_DEBUG, "GLB -> Level object failure: entity node {} '{}' has invalid transform", i, name);
      return ARX_GLB_BAD_LEVEL_ENTITY;
    }
    if (glb::hasNonIdentityLocalScale(node))
      log(ARX_LOG_WARN, "GLB -> Level: entity '{}' has nonidentity local scale; scale ignored", name);
    const std::optional<ArxVector3> position = toArxPoint(transform.translation, units);
    if (!position) return ARX_GLB_BAD_FORMAT;
    Entity entity;
    entity.ident = parsed.ident;
    entity.position = *position;
    entity.rotation = toArxRotation(math::rotationToQuat(transform.rotation));
    if (!parsed.name.empty()) {
      IdentifierNormalization normalized = normalizeIdentifier(parsed.name);
      if (normalized.repair != IdentifierRepair::kNone)
        log(ARX_LOG_INFO, "GLB -> Level: entity name '{}' normalized to '{}'", parsed.name, normalized.value);
      entity.name = std::move(normalized.value);
    }
    if (!math::normalizeRotation(entity.rotation)) return ARX_GLB_BAD_LEVEL_ENTITY;
    bool entity_legacy_teo = false;
    rc = entityClassPath(data, node, entity.class_path, entity_legacy_teo);
    if (rc != ARX_OK) {
      log(ARX_LOG_DEBUG, "GLB -> Level object failure: entity node {} '{}' has invalid/missing class helper", i, name);
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
  scene::repairEntityNames(level.scene.entities);
  return ARX_OK;
}

}  // namespace pistoris::glb_level
