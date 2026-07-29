// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#pragma once

#include "routes/animation/invocation.h"
#include "routes/descriptor.h"

#include <vector>

namespace cli::animation {

ProbeResult probeRoute(const RouteProbeContext& ctx, RouteInvocation& invocation);
bool resolveRoute(const RouteResolveContext& context, RouteInvocation& invocation);
bool printHelpSection(std::FILE* output, HelpSection section);
const RouteDescriptor& routeDescriptor();

ProbeResult probe(const std::vector<ClassifiedPath>& inputs, const OutputTarget& output, Format output_format,
                  Invocation& inv);
int dispatch(const ResolvedAnimationInvocation& inv);

}  // namespace cli::animation
