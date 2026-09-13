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

}  // namespace pistoris::paths
