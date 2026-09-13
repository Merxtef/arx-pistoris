// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#pragma once

#include "routes/ambiance/invocation.h"
#include "routes/descriptor.h"

namespace cli::ambiance {

bool resolveInvocation(const RouteResolveContext& context, Invocation& invocation);

}  // namespace cli::ambiance
