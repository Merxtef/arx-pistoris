// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#pragma once

#include "routes/descriptor.h"
#include "routes/model/invocation.h"

#include <vector>

namespace cli::model {

ProbeResult probeRoute(const RouteProbeContext& ctx, RouteInvocation& invocation);
bool resolveRoute(const RouteResolveContext& context, RouteInvocation& invocation);
bool printHelpSection(std::FILE* output, HelpSection section);
const RouteDescriptor& routeDescriptor();

ProbeResult probe(const std::vector<ClassifiedPath>& inputs, const OutputTarget& output, Format output_format,
                  Invocation& inv);
int dispatch(const ResolvedModelInvocation& inv);

}  // namespace cli::model
