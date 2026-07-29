// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#pragma once

#include "routes/descriptor.h"
#include "routes/level/invocation.h"

namespace cli::level {

bool resolveInvocation(const RouteResolveContext& context, Invocation& invocation);

}  // namespace cli::level
