// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#include "routes/registry.h"

#include "routes/ambiance/route.h"
#include "routes/animation/route.h"
#include "routes/cinematic/route.h"
#include "routes/descriptor.h"
#include "routes/level/route.h"
#include "routes/model/route.h"

#include <iterator>

namespace cli {

RouteRegistryView routeRegistry() {
  static const RouteDescriptor kRoutes[] = {
      animation::routeDescriptor(),
      ambiance::routeDescriptor(),
      cinematic::routeDescriptor(),
      model::routeDescriptor(),
      level::routeDescriptor(),
  };
  return {kRoutes, std::size(kRoutes)};
}

}  // namespace cli
