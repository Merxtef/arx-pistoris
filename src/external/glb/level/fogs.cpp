// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#include "fogs.h"

#include "arx_pistoris/base/math.hpp"
#include "arx_pistoris/base/status.h"
#include "arx_pistoris/level.hpp"
#include "arx_pistoris/runtime/types.h"

#include "coordinates.h"
#include "external/glb/container.h"
#include "external/glb/node_graph.h"
#include "external/glb/utils/names.h"
#include "external/glb/utils/node.h"
#include "external/glb/utils/tokens.h"
#include "external/glb/utils/transform.h"
#include "level/data.h"
#include "modules/scene.h"
#include "objects.h"
#include "utils/log.h"
#include "utils/math/finite.h"
#include "utils/math/mat4.h"
#include "utils/math/quat.h"
#include "utils/math/rotation.h"
#include "utils/name_tokens.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <format>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace pistoris::glb_level {

using glb::parseFloatToken;
using glb::parseSignedToken;
namespace {

constexpr float kDirectionHelperRadius = 50.0f;
constexpr float kTransformTolerance = 1.0e-4f;
constexpr std::string_view kFogPrefix = "arx_fog__";
constexpr std::string_view kSettings = "SETTINGS__";
constexpr std::string_view kDirection = "DIRECTION";

ArxVector3 directionFromRotation(const ArxQuat& rotation) {
  return math::normalize(math::rotate(rotation, {0.0f, 0.0f, 1.0f}));
}

ArxAngle angleFromDirection(const ArxVector3& direction) {
  ArxVector3 normalized = math::normalize(direction);
  float pitch = std::asin(std::clamp(-normalized.y, -1.0f, 1.0f));
  float yaw = std::atan2(normalized.x, normalized.z);
  return {pitch * math::kDegreesPerRadian, yaw * math::kDegreesPerRadian, 0.0f};
}

ArxQuat rotationFromDirection(const ArxVector3& direction) { return math::angleToQuat(angleFromDirection(direction)); }

std::string fogSettingsName(std::string_view name, const Fog& fog) {
  std::string rgb = "RGB_" + glb::formatColor3Token(fog.color);
  std::string size = std::format("SIZE_{}", fog.size);
  std::string scale = std::format("SCALE_{}", fog.scale);
  std::string speed = std::format("SPEED_{}", fog.speed);
  std::string rotate_speed = std::format("ROTATESPEED_{}", fog.rotate_speed);
  std::string lifetime = std::format("LIFETIME_{}", fog.lifetime_ms);
  std::string frequency = std::format("FREQUENCY_{}", fog.frequency);
  return joinDoubleUnderscore({"SETTINGS", rgb, size, scale, speed, rotate_speed, lifetime, frequency, name});
}

ArxReturnCode parseSettings(const cgltf_node& root, Fog& fog) {
  const Fog defaults = fog;
  bool selected = false;
  for (std::size_t i = 0; i < root.children_count; ++i) {
    const cgltf_node* child = root.children[i];
    if (child == nullptr || child->name == nullptr) continue;
    std::string_view name(child->name);
    if (!name.starts_with(kSettings)) continue;
    if (!glb::simpleEmptyNode(*child)) return ARX_GLB_BAD_LEVEL_FOG;

    Fog candidate = defaults;
    glb::ParsedLabel label;
    if (!glb::parseRecoverableLabel(
            name,
            candidate,
            &label,
            glb::ConventionOptions{{},
                                   {"RGB_", "SIZE_", "SCALE_", "SPEED_", "ROTATESPEED_", "LIFETIME_", "FREQUENCY_"}},
            [](std::span<const std::string_view> tokens, Fog& value) {
              if (tokens.size() < 2 || tokens.front() != "SETTINGS") return false;
              bool seen_rgb = false;
              bool seen_size = false;
              bool seen_scale = false;
              bool seen_speed = false;
              bool seen_rotate = false;
              bool seen_lifetime = false;
              bool seen_frequency = false;
              for (std::string_view token : tokens.subspan(1)) {
                if (token.starts_with("RGB_")) {
                  if (seen_rgb || !glb::parseColor3Token(token.substr(4), value.color)) return false;
                  seen_rgb = true;
                } else if (token.starts_with("SIZE_")) {
                  if (seen_size || !parseFloatToken(token.substr(5), value.size)) return false;
                  seen_size = true;
                } else if (token.starts_with("SCALE_")) {
                  if (seen_scale || !parseFloatToken(token.substr(6), value.scale)) return false;
                  seen_scale = true;
                } else if (token.starts_with("SPEED_")) {
                  if (seen_speed || !parseFloatToken(token.substr(6), value.speed)) return false;
                  seen_speed = true;
                } else if (token.starts_with("ROTATESPEED_")) {
                  if (seen_rotate || !parseFloatToken(token.substr(12), value.rotate_speed)) return false;
                  seen_rotate = true;
                } else if (token.starts_with("LIFETIME_")) {
                  const auto lifetime = parseSignedToken(token.substr(9));
                  if (seen_lifetime || !lifetime) return false;
                  value.lifetime_ms = *lifetime;
                  seen_lifetime = true;
                } else if (token.starts_with("FREQUENCY_")) {
                  if (seen_frequency || !parseFloatToken(token.substr(10), value.frequency)) return false;
                  seen_frequency = true;
                } else {
                  return false;
                }
              }
              return true;
            }))
      return ARX_GLB_BAD_LEVEL_FOG;
    glb::reportConventionLabel("GLB -> Level fog settings", name, label);
    if (!selected) {
      fog = std::move(candidate);
      selected = true;
    }
  }
  return ARX_OK;
}

ArxReturnCode parseDirection(const cgltf_data& data, const std::vector<math::Mat4>& world, const cgltf_node& root,
                             const ArxVector3& position, const ImportUnits& units, Fog& fog) {
  bool selected = false;
  for (std::size_t i = 0; i < root.children_count; ++i) {
    const cgltf_node* child = root.children[i];
    if (child == nullptr) return ARX_GLB_BAD_FORMAT;
    std::string_view name = child->name != nullptr ? child->name : "";
    if (name != kDirection && !name.starts_with("DIRECTION__")) continue;
    bool direction = false;
    glb::ParsedLabel label;
    if (!glb::parseRecoverableLabel(name,
                                    direction,
                                    &label,
                                    {},
                                    [](std::span<const std::string_view> tokens, bool& out) {
                                      if (tokens.size() != 1 || tokens.front() != kDirection) return false;
                                      out = true;
                                      return true;
                                    }) ||
        !direction || !glb::simpleEmptyNode(*child))
      return ARX_GLB_BAD_LEVEL_FOG;
    glb::reportConventionLabel("GLB -> Level fog direction", name, label);
    std::ptrdiff_t child_index = child - data.nodes;
    if (child_index < 0 || static_cast<std::size_t>(child_index) >= data.nodes_count) return ARX_GLB_BAD_FORMAT;
    ArxVector3 target = math::translation(world[static_cast<std::size_t>(child_index)]);
    ArxVector3 delta{target.x - position.x, target.y - position.y, target.z - position.z};
    if (!math::finite(delta) || math::lengthf(delta) <= kTransformTolerance) return ARX_GLB_BAD_LEVEL_FOG;
    if (!selected) {
      const std::optional<ArxVector3> converted = toArxVector(delta, units);
      if (!converted) return ARX_GLB_BAD_FORMAT;
      fog.directional = true;
      fog.rotation = rotationFromDirection(*converted);
      selected = true;
    }
  }
  return ARX_OK;
}

}  // namespace

void exportFogs(const LevelModules& level, const ArxAabb& referenced_bounds, const Level::GlbExportOptions& options,
                glb::Builder& builder) {
  if (level.scene.fogs.empty()) return;
  ArxVector3 parent_position = bottomCenter(referenced_bounds);
  parent_position.y += kFogParentOffset;
  int parent = builder.addNode("fogs_parent");
  builder.setNodeTranslation(parent, {parent_position.x, parent_position.y, parent_position.z});
  builder.addRoot(parent);

  for (std::size_t i = 0; i < level.scene.fogs.size(); ++i) {
    const Fog& fog = level.scene.fogs[i];
    std::string fallback_label = std::format("fog_{}", i);
    std::string_view label = fog.name.empty() ? std::string_view(fallback_label) : std::string_view(fog.name);
    std::string name = std::format("arx_fog__{}", label);
    int root = builder.addNode(name);
    builder.setNodeTranslation(
        root,
        {fog.position.x - parent_position.x, fog.position.y - parent_position.y, fog.position.z - parent_position.z});
    Fog exported_fog = fog;
    exported_fog.size = toGlbLength(exported_fog.size, options);
    exported_fog.scale = toGlbLength(exported_fog.scale, options);
    exported_fog.speed = toGlbLength(exported_fog.speed, options);
    builder.addChild(root, builder.addNode(fogSettingsName(label, exported_fog)));
    if (fog.directional) {
      ArxVector3 direction = directionFromRotation(fog.rotation);
      int helper = builder.addNode(std::format("DIRECTION__{}", label));
      builder.setNodeTranslation(helper,
                                 {direction.x * kDirectionHelperRadius,
                                  direction.y * kDirectionHelperRadius,
                                  direction.z * kDirectionHelperRadius});
      builder.addChild(root, helper);
    }
    builder.addChild(parent, root);
  }
}

ArxReturnCode importFogs(const cgltf_data& data, const std::vector<math::Mat4>& world,
                         std::span<const std::size_t> nodes, const ImportUnits& units, LevelModules& level) {
  for (std::size_t node_index : nodes) {
    if (node_index >= data.nodes_count || node_index >= world.size()) return ARX_GLB_BAD_FORMAT;
    const cgltf_node& node = data.nodes[node_index];
    std::string_view name = node.name != nullptr ? node.name : "";
    std::string_view fog_name = name.substr(kFogPrefix.size());
    if (fog_name.empty() || hasDoubleUnderscore(fog_name) || !glb::simpleEmptyNode(node)) return ARX_GLB_BAD_LEVEL_FOG;

    glb::DecomposedTransform decomposed;
    if (!glb::decomposeTransform(world[node_index], decomposed)) return ARX_GLB_BAD_LEVEL_FOG;

    Fog fog;
    fog.position = math::translation(world[node_index]);
    fog.name = fog_name;
    ArxReturnCode rc = parseSettings(node, fog);
    if (rc != ARX_OK) return rc;
    rc = parseDirection(data, world, node, fog.position, units, fog);
    if (rc != ARX_OK) return rc;
    const std::optional<ArxVector3> position = toArxPoint(fog.position, units);
    const std::optional<float> size = toArxLength(fog.size, units);
    const std::optional<float> scale = toArxLength(fog.scale, units);
    const std::optional<float> speed = toArxLength(fog.speed, units);
    if (!position || !size || !scale || !speed) return ARX_GLB_BAD_FORMAT;
    fog.position = *position;
    fog.size = *size;
    fog.scale = *scale;
    fog.speed = *speed;
    if (glb::hasNonIdentityLocalScale(node))
      log(ARX_LOG_WARN, "GLB -> Level: fog '{}' has nonidentity local scale; scale ignored", name);
    if (!math::normalizeRotation(fog.rotation)) return ARX_GLB_BAD_LEVEL_FOG;
    level.scene.fogs.push_back(fog);
  }
  return ARX_OK;
}

}  // namespace pistoris::glb_level
