// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#include "arx_pistoris/paths/types.h"

#include "modules/resource.h"
#include "modules/resource/internal.h"
#include "utils/resource_path.h"

#include <string_view>

namespace pistoris::resource {
namespace {

std::string_view extensionFor(ArxResourceKind kind) noexcept {
  switch (kind) {
    case ARX_RESOURCE_KIND_LEVEL:
      return ".dlf";
    case ARX_RESOURCE_KIND_MODEL:
      return ".ftl";
    case ARX_RESOURCE_KIND_ANIMATION:
      return ".tea";
    case ARX_RESOURCE_KIND_CINEMATIC:
      return ".cin";
    case ARX_RESOURCE_KIND_AMBIANCE:
      return ".amb";
    default:
      return {};
  }
}

}  // namespace

Error validatePath(std::string_view path, ArxResourceKind kind) noexcept {
  const std::string_view extension = extensionFor(kind);
  if (extension.empty()) return Error::kBadKind;
  return isResourcePath(path) && path.ends_with(extension) ? Error::kNone : Error::kBadPath;
}

Error validate(const ResourceData& resource, ArxResourceKind kind) {
  if (resource.path.empty()) return Error::kNone;
  return validatePath(resource.path, kind);
}

}  // namespace pistoris::resource
