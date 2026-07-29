// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#pragma once

#include "routes/animation/invocation.h"
#include "routes/animation/state.h"
#include "routes/types.h"

#include <cstdint>
#include <vector>

namespace cli::animation {

bool loadInput(const std::vector<ClassifiedPath>& inputs, const Invocation& invocation, Route route, Context& ctx);

}  // namespace cli::animation
