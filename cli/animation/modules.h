// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#pragma once

#include "arx_pistoris/pistoris.hpp"

#include "animation/ctx.h"
#include "args.h"

namespace cli::animation {

bool validateModules(const CliArgs& args);
bool applyModules(Context& ctx, const CliArgs& args);

}  // namespace cli::animation
