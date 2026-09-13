// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#include "arx_pistoris/base/math.h"
#include "arx_pistoris/base/status.h"
#include "arx_pistoris/runtime/types.h"

#include "external/glb/container.h"
#include "external/glb/level/coordinates.h"
#include "external/glb/level/import/internal.h"
#include "modules/lights.h"
#include "utils/log.h"
#include "utils/math/mat4.h"

#include <cmath>
#include <cstddef>
#include <optional>
#include <utility>

namespace pistoris::glb_level_import {

using namespace detail;

namespace {

ArxReturnCode convertLightSpatialFields(Light& light, const ImportUnits& units) {
  const std::optional<ArxVector3> position = glb_level::toArxPoint(light.position, units);
  const std::optional<float> fallstart = glb_level::toArxLength(light.fallstart, units);
  const std::optional<float> fallend = glb_level::toArxLength(light.fallend, units);
  const std::optional<float> effect_radius = glb_level::toArxLength(light.effect_radius, units);
  if (!position || !fallstart || !fallend || !effect_radius) return ARX_GLB_BAD_FORMAT;
  light.position = *position;
  light.fallstart = *fallstart;
  light.fallend = *fallend;
  light.effect_radius = *effect_radius;
  return ARX_OK;
}

}  // namespace

ArxReturnCode importPointLight(const cgltf_node& node, std::size_t node_index, const math::Mat4& world,
                               const ImportUnits& units, std::size_t ordinal, ImportWarnings& diagnostics, Light& out) {
  const cgltf_light& source = *node.light;
  if (!std::isfinite(source.range) || !std::isfinite(source.intensity) || source.intensity < 0.0f) {
    log(ARX_LOG_DEBUG,
        "GLB -> Level light failure: node {} '{}' has invalid range/intensity",
        node_index,
        nodeName(node));
    return ARX_GLB_BAD_LEVEL_LIGHT;
  }
  GlbVec3 color{source.color[0], source.color[1], source.color[2]};
  if (!std::isfinite(color.x) || !std::isfinite(color.y) || !std::isfinite(color.z) || !unitColor(color)) {
    log(ARX_LOG_DEBUG, "GLB -> Level light failure: node {} '{}' has invalid color", node_index, nodeName(node));
    return ARX_GLB_BAD_LEVEL_LIGHT;
  }

  ParsedLightName parsed_name;
  float fallback_fallend = source.range > 0.0f ? source.range : 0.0f;
  ArxReturnCode rc =
      parseLightName(nodeName(node), source.name != nullptr ? source.name : "", ordinal, fallback_fallend, parsed_name);
  if (rc != ARX_OK) return rc;
  if (!parsed_name.has_fallend) {
    log(ARX_LOG_DEBUG,
        "GLB -> Level light failure: node {} '{}' has no FALLEND or positive range",
        node_index,
        nodeName(node));
    return ARX_GLB_BAD_LEVEL_LIGHT;
  }

  Light light;
  light.name = std::move(parsed_name.name);
  light.position = math::translation(world);
  light.fallstart = parsed_name.fallstart;
  light.fallend = parsed_name.fallend;
  if (!finite(light.position)) return ARX_GLB_BAD_FORMAT;
  rc = parseLightSettings(node, true, true, {color.x, color.y, color.z}, source.intensity, light);
  if (rc != ARX_OK) return rc;
  rc = parseLightFlags(node, light.flags);
  if (rc != ARX_OK) return rc;
  rc = parseLightEffects(node, light);
  if (rc != ARX_OK) return rc;
  rc = convertLightSpatialFields(light, units);
  if (rc != ARX_OK) return rc;

  if (parsed_name.generated) ++diagnostics.generated_light_names;
  if (parsed_name.repaired)
    log(ARX_LOG_WARN, "GLB -> Level: light '{}' has invalid FALLSTART; using {}", light.name, light.fallstart);
  log(ARX_LOG_DEBUG,
      "GLB -> Level: imported point light node {} '{}' as '{}', fallstart {}, fallend {}, intensity {}",
      node_index,
      nodeName(node),
      light.name,
      light.fallstart,
      light.fallend,
      light.intensity);
  out = std::move(light);
  return ARX_OK;
}

ArxReturnCode importReservedLight(const cgltf_node& node, std::size_t node_index, const math::Mat4& world,
                                  const ImportUnits& units, std::size_t ordinal, ImportWarnings& diagnostics,
                                  Light& out) {
  if (!isReservedLightName(nodeName(node))) return ARX_GLB_BAD_LEVEL_LIGHT;
  if (node.camera != nullptr) {
    log(ARX_LOG_DEBUG,
        "GLB -> Level light failure: reserved light node {} '{}' has camera payload",
        node_index,
        nodeName(node));
    return ARX_GLB_BAD_LEVEL_LIGHT;
  }
  if (node.light != nullptr) {
    if (node.light->type != cgltf_light_type_point) {
      log(ARX_LOG_DEBUG,
          "GLB -> Level light failure: reserved light node {} '{}' is not a point light",
          node_index,
          nodeName(node));
      return ARX_GLB_BAD_LEVEL_LIGHT;
    }
    return importPointLight(node, node_index, world, units, ordinal, diagnostics, out);
  }
  ParsedLightName parsed_name;
  ArxReturnCode rc = parseLightName(nodeName(node), "", ordinal, 0.0f, parsed_name);
  if (rc != ARX_OK) return rc;
  Light light;
  light.name = std::move(parsed_name.name);
  light.position = math::translation(world);
  light.fallstart = parsed_name.fallstart;
  light.fallend = parsed_name.fallend;
  if (!finite(light.position)) return ARX_GLB_BAD_FORMAT;
  rc = parseLightSettings(node, parsed_name.has_fallend, false, {}, 0.0f, light);
  if (rc != ARX_OK) return rc;
  rc = parseLightFlags(node, light.flags);
  if (rc != ARX_OK) return rc;
  rc = parseLightEffects(node, light);
  if (rc != ARX_OK) return rc;
  rc = convertLightSpatialFields(light, units);
  if (rc != ARX_OK) return rc;
  if (parsed_name.generated) ++diagnostics.generated_light_names;
  if (parsed_name.repaired)
    log(ARX_LOG_WARN, "GLB -> Level: light '{}' has invalid FALLSTART; using {}", light.name, light.fallstart);
  log(ARX_LOG_DEBUG,
      "GLB -> Level: imported reserved light node {} '{}' as '{}', fallstart {}, fallend {}, intensity {}",
      node_index,
      nodeName(node),
      light.name,
      light.fallstart,
      light.fallend,
      light.intensity);
  out = std::move(light);
  return ARX_OK;
}

}  // namespace pistoris::glb_level_import
