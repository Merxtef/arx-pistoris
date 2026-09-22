// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#pragma once

#include "routes/cinematic/invocation.h"
#include "routes/descriptor.h"

namespace cli::cinematic {

bool resolveInvocation(const RouteResolveContext& context, Invocation& invocation);

}  // namespace cli::cinematic
