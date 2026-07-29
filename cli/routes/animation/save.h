// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#pragma once

#include "formats/options.h"
#include "io/service.h"
#include "routes/animation/invocation.h"
#include "routes/animation/state.h"
#include "routes/types.h"

namespace cli::animation {

bool saveOutput(const Context& ctx, const FormatOptions& format, IoService& io, const Invocation& invocation,
                Route route);

}  // namespace cli::animation
