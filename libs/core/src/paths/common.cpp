// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#include "arx_pistoris/paths.hpp"
#include "arx_pistoris/paths/types.h"

#include "paths/internal.h"
#include "utils/portable_filename.h"
#include "utils/resource_path.h"

#include <cstddef>
#include <string>
#include <string_view>
#include <utility>

namespace pistoris::paths {

using namespace detail;

bool isPortableFilename(std::string_view filename) noexcept { return isPortableName(filename); }

std::string sanitizePortableFilename(std::string_view filename) { return makeUniquePortableName(filename); }

bool isPortableResourcePathComponent(std::string_view component) noexcept {
  return pistoris::isPortableResourcePathComponent(component);
}

std::string_view textureDirectory() noexcept { return "graph/obj3d/textures"; }

std::string_view soundDirectory() noexcept { return "sfx"; }

ArxResourceKind resourceSelectorKind(std::string_view selector) noexcept {
  const std::size_t delimiter = selector.find(':');
  if (delimiter == std::string_view::npos) return ARX_RESOURCE_KIND_NONE;
  const std::string_view kind = selector.substr(0, delimiter);
  if (selectorKind(kind, "level")) return ARX_RESOURCE_KIND_LEVEL;
  if (selectorKind(kind, "model")) return ARX_RESOURCE_KIND_MODEL;
  if (selectorKind(kind, "anim")) return ARX_RESOURCE_KIND_ANIMATION;
  if (selectorKind(kind, "cinematic")) return ARX_RESOURCE_KIND_CINEMATIC;
  if (selectorKind(kind, "ambiance")) return ARX_RESOURCE_KIND_AMBIANCE;
  return ARX_RESOURCE_KIND_NONE;
}

bool parseResourceSelector(std::string_view selector, ResourceSelector& out) {
  ResourceSelector parsed;
  switch (resourceSelectorKind(selector)) {
    case ARX_RESOURCE_KIND_LEVEL:
      parsed.kind = ARX_RESOURCE_KIND_LEVEL;
      if (!levelFromSelector(selector, parsed.level)) return false;
      break;
    case ARX_RESOURCE_KIND_MODEL: {
      ModelPathView model;
      if (!modelFromSelector(selector, model)) return false;
      parsed.kind = ARX_RESOURCE_KIND_MODEL;
      parsed.model_type = model.type;
      parsed.name = model.name;
      parsed.tweak = model.tweak;
      break;
    }
    case ARX_RESOURCE_KIND_ANIMATION: {
      AnimationPathView animation;
      if (!animationFromSelector(selector, animation)) return false;
      parsed.kind = ARX_RESOURCE_KIND_ANIMATION;
      parsed.animation_type = animation.type;
      parsed.name = animation.name;
      break;
    }
    case ARX_RESOURCE_KIND_CINEMATIC: {
      CinematicPathView cinematic;
      if (!cinematicFromSelector(selector, cinematic)) return false;
      parsed.kind = ARX_RESOURCE_KIND_CINEMATIC;
      parsed.name = cinematic.name;
      break;
    }
    case ARX_RESOURCE_KIND_AMBIANCE: {
      AmbiancePathView ambiance;
      if (!ambianceFromSelector(selector, ambiance)) return false;
      parsed.kind = ARX_RESOURCE_KIND_AMBIANCE;
      parsed.name = ambiance.name;
      break;
    }
    case ARX_RESOURCE_KIND_NONE:
    default:
      return false;
  }
  out = std::move(parsed);
  return true;
}

bool resourceSelector(const ResourceSelector& resource, std::string& out) {
  switch (resource.kind) {
    case ARX_RESOURCE_KIND_LEVEL:
      if (resource.model_type != ModelPathType::kNone || resource.animation_type != AnimationPathType::kNone ||
          !resource.name.empty() || !resource.tweak.empty())
        return false;
      out = levelSelector(resource.level);
      return true;
    case ARX_RESOURCE_KIND_MODEL:
      if (resource.animation_type != AnimationPathType::kNone || resource.level != 0) return false;
      return modelSelector({resource.model_type, resource.name, resource.tweak}, out);
    case ARX_RESOURCE_KIND_ANIMATION:
      if (resource.model_type != ModelPathType::kNone || !resource.tweak.empty() || resource.level != 0) return false;
      return animationSelector({resource.animation_type, resource.name}, out);
    case ARX_RESOURCE_KIND_CINEMATIC:
      if (resource.model_type != ModelPathType::kNone || resource.animation_type != AnimationPathType::kNone ||
          !resource.tweak.empty() || resource.level != 0)
        return false;
      return cinematicSelector({resource.name}, out);
    case ARX_RESOURCE_KIND_AMBIANCE:
      if (resource.model_type != ModelPathType::kNone || resource.animation_type != AnimationPathType::kNone ||
          !resource.tweak.empty() || resource.level != 0)
        return false;
      return ambianceSelector({resource.name}, out);
    case ARX_RESOURCE_KIND_NONE:
    default:
      return false;
  }
}

}  // namespace pistoris::paths
