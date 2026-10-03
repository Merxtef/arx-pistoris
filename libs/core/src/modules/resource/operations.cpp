// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#include "arx_pistoris/paths.hpp"
#include "arx_pistoris/paths/types.h"

#include "modules/resource.h"
#include "modules/resource/internal.h"
#include "utils/resource_path.h"

#include <cstdint>
#include <string>
#include <string_view>
#include <utility>

namespace pistoris::resource {
namespace {

Error pathFromSelector(ArxResourceKind kind, std::string_view input, std::string& out) {
  switch (kind) {
    case ARX_RESOURCE_KIND_LEVEL: {
      std::uint32_t level = 0;
      if (!paths::levelFromSelector(input, level)) return Error::kBadPath;
      out = paths::levelDlf(level);
      return Error::kNone;
    }
    case ARX_RESOURCE_KIND_MODEL: {
      paths::ModelPathView model;
      if (!paths::modelFromSelector(input, model)) return Error::kBadPath;
      return paths::modelFtl(model, out) ? Error::kNone : Error::kBadPath;
    }
    case ARX_RESOURCE_KIND_ANIMATION: {
      paths::AnimationPathView animation;
      if (!paths::animationFromSelector(input, animation)) return Error::kBadPath;
      return paths::animationTea(animation, out) ? Error::kNone : Error::kBadPath;
    }
    case ARX_RESOURCE_KIND_CINEMATIC: {
      paths::CinematicPathView cinematic;
      if (!paths::cinematicFromSelector(input, cinematic)) return Error::kBadPath;
      return paths::cinematicCin(cinematic, out) ? Error::kNone : Error::kBadPath;
    }
    case ARX_RESOURCE_KIND_AMBIANCE: {
      paths::AmbiancePathView ambiance;
      if (!paths::ambianceFromSelector(input, ambiance)) return Error::kBadPath;
      return paths::ambianceAmb(ambiance, out) ? Error::kNone : Error::kBadPath;
    }
    default:
      return Error::kBadKind;
  }
}

}  // namespace

Error repairPath(ArxResourceKind kind, std::string_view input, std::string& out) {
  if (input.empty()) {
    out.clear();
    return Error::kNone;
  }

  std::string expanded;
  const ArxResourceKind selector_kind = paths::resourceSelectorKind(input);
  if (selector_kind != ARX_RESOURCE_KIND_NONE) {
    if (selector_kind != kind) return Error::kBadPath;
    const Error error = pathFromSelector(kind, input, expanded);
    if (error != Error::kNone) return error;
    input = expanded;
  }

  ResourcePathNormalization normalized = normalizeResourcePath(input);
  if (normalized.error != ResourcePathError::kNone) return Error::kBadPath;
  const Error error = validatePath(normalized.value, kind);
  if (error != Error::kNone) return error;
  out = std::move(normalized.value);
  return Error::kNone;
}

void setPath(ResourceData& resource, std::string path) noexcept { resource.path = std::move(path); }

}  // namespace pistoris::resource
