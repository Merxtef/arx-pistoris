// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#include "external/glb/level/lighting.h"

#include "arx_pistoris/base/math.h"
#include "arx_pistoris/level.hpp"

#include "external/glb/accessor.h"
#include "external/glb/level/coordinates.h"
#include "external/glb/level/export/internal.h"
#include "external/glb/level/objects.h"
#include "external/glb/writer.h"
#include "modules/lights.h"

#include <cstdint>
#include <string>
#include <vector>

namespace pistoris::glb_level_export {
namespace {
using glb::Builder;
using glb_level::bottomCenter;
using glb_level::hasEffectFields;
using glb_level::hasSettingsFields;
using glb_level::kLightParentOffset;
using glb_level::lightEffectHelperName;
using glb_level::lightFlagHelperName;
using glb_level::lightNodeName;
using glb_level::lightSettingsHelperName;
using GlbVec3 = glb::Vec3;

inline GlbVec3 toVec3(const ArxVector3& value) { return {value.x, value.y, value.z}; }
}  // namespace

std::uint64_t exportLights(const LevelModules& level, const ArxAabb& referenced_bounds,
                           const Level::GlbExportOptions& options, Builder& builder) {
  std::uint64_t defaulted_light_fallstarts = 0;
  if (!level.lighting.lights.empty()) {
    ArxVector3 light_root = bottomCenter(referenced_bounds);
    light_root.y += kLightParentOffset;
    int light_parent = builder.addNode("lights_parent");
    builder.setNodeTranslation(light_parent, toVec3(light_root));
    builder.addRoot(light_parent);
    for (const Light& light : level.lighting.lights) {
      bool real_light = light.fallend > 0.0f;
      bool default_fallstart = real_light && light.fallstart >= light.fallend;
      Light normalized_light = light;
      if (default_fallstart) normalized_light.fallstart = normalized_light.fallend * 0.5f;
      std::string name = lightNodeName(normalized_light, options);
      if (default_fallstart) ++defaulted_light_fallstarts;
      int node = builder.addNode(name);
      if (real_light) {
        int resource = builder.addPointLight(name,
                                             {light.color.r, light.color.g, light.color.b},
                                             light.intensity,
                                             glb_level::toGlbLength(normalized_light.fallend, options));
        builder.setNodeLight(node, resource);
      }
      builder.setNodeTranslation(
          node, {light.position.x - light_root.x, light.position.y - light_root.y, light.position.z - light_root.z});
      builder.addChild(light_parent, node);
      if (hasSettingsFields(normalized_light))
        builder.addChild(node, builder.addNode(lightSettingsHelperName(light.name, normalized_light)));
      if (light.flags != 0) builder.addChild(node, builder.addNode(lightFlagHelperName(light.name, light.flags)));
      if (hasEffectFields(normalized_light))
        builder.addChild(node, builder.addNode(lightEffectHelperName(light.name, normalized_light, options)));
    }
  }
  return defaulted_light_fallstarts;
}

}  // namespace pistoris::glb_level_export
