// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#pragma once

#include "conversion/options.h"
#include "formats/options.h"
#include "pipeline/execution_context.h"
#include "routes/descriptor.h"
#include "routes/model/options.h"

#include <cstddef>
#include <vector>

namespace cli::model {

struct Invocation final : RouteInvocation {
  std::size_t input = kNoClassifiedPath;
  OutputTarget output;
  std::vector<std::size_t> extras;
  ModelOptions options;
  SharedConversionOptions conversion;
  FormatOptions format;
};

struct ResolvedModelInvocation {
  const ExecutionContext& common;
  Invocation& invocation;
};

}  // namespace cli::model
