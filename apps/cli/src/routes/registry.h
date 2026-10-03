// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#pragma once

#include "routes/descriptor.h"

#include <cstddef>

namespace cli {

struct RouteRegistryView {
  const RouteDescriptor* routes = nullptr;
  std::size_t count = 0;
};

RouteRegistryView routeRegistry();

}  // namespace cli
