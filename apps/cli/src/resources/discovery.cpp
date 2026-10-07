// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#include "resources/discovery.h"

#include "arx_pistoris/paths/types.h"
#include "arx_pistoris/resource_io/catalog.hpp"
#include "arx_pistoris/resource_io/location.hpp"
#include "arx_pistoris/runtime.hpp"

#include "base/natural_order.h"
#include "console/diagnostics.h"
#include "io/service.h"

#include <algorithm>
#include <cstdio>
#include <string>
#include <vector>

namespace cli {
namespace {

ArxResourceKind resourceKind(ResourceListingKind kind) {
  switch (kind) {
    case ResourceListingKind::kLevel:
      return ARX_RESOURCE_KIND_LEVEL;
    case ResourceListingKind::kModel:
      return ARX_RESOURCE_KIND_MODEL;
    case ResourceListingKind::kAnimation:
      return ARX_RESOURCE_KIND_ANIMATION;
    case ResourceListingKind::kCinematic:
      return ARX_RESOURCE_KIND_CINEMATIC;
    case ResourceListingKind::kAmbiance:
      return ARX_RESOURCE_KIND_AMBIANCE;
    case ResourceListingKind::kNone:
    case ResourceListingKind::kAll:
      break;
  }
  return ARX_RESOURCE_KIND_NONE;
}

}  // namespace

bool printResourceListing(ResourceListingKind kind, IoService& io) {
  auto catalog = io.scanCatalog();
  if (!catalog) {
    const auto* error = catalog.error();
    const std::string description =
        error ? pistoris::resource_io::describeError(*error) : std::string(pistoris::errorString(catalog.code()));
    diagnostic(DiagnosticCode::kIoStatFailed, "Cannot scan mounted resources: %s", description.c_str());
    return false;
  }
  std::vector<std::string> resources;
  const ArxResourceKind requested = resourceKind(kind);
  for (const pistoris::resource_io::ResourceCatalogEntry& entry : catalog->entries())
    if (kind == ResourceListingKind::kAll || entry.resource.kind == requested) resources.push_back(entry.selector);

  std::ranges::sort(resources, naturalStringLess);
  for (const std::string& resource : resources) std::printf("%s\n", resource.c_str());
  return true;
}

}  // namespace cli
