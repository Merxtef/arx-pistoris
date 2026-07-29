// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#include "pipeline/options.h"

#include "routes/descriptor.h"
#include "routes/options.h"

#include <memory>
#include <utility>

namespace cli {

RouteOptions* ensureRouteOptions(ParsedOptions& options, const RouteDescriptor& route) {
  for (RouteOptionEntry& entry : options.route_options)
    if (entry.route == &route) return entry.options.get();
  if (!route.create_options) return nullptr;
  std::unique_ptr<RouteOptions> created = route.create_options();
  if (!created) return nullptr;
  RouteOptions* result = created.get();
  options.route_options.push_back({&route, std::move(created)});
  return result;
}

const RouteOptions* findRouteOptions(const ParsedOptions& options, const RouteDescriptor& route) noexcept {
  for (const RouteOptionEntry& entry : options.route_options)
    if (entry.route == &route) return entry.options.get();
  return nullptr;
}

}  // namespace cli
