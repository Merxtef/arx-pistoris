// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#pragma once

#include "conversion/options.h"
#include "formats/options.h"
#include "pipeline/execution_context.h"
#include "routes/descriptor.h"

#include <cstddef>
#include <vector>

namespace cli::animation {

struct Invocation final : RouteInvocation {
  std::vector<std::size_t> inputs;
  OutputTarget output;
  SharedConversionOptions conversion;
  FormatOptions format;
};

struct ResolvedAnimationInvocation {
  const ExecutionContext& common;
  Invocation& invocation;
};

}  // namespace cli::animation
