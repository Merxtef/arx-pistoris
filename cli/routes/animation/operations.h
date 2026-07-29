// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#pragma once

#include "arx_pistoris/pistoris.hpp"

#include "conversion/options.h"
#include "routes/animation/state.h"

namespace cli::animation {

bool applyModules(Context& ctx, const SharedConversionOptions& conversion);

}  // namespace cli::animation
