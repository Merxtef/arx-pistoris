// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#pragma once

#include "arx_pistoris/pistoris.hpp"

#include "animation/ctx.h"
#include "args.h"
#include "formats.h"
#include "state.h"

#include <cstdint>
#include <vector>

namespace cli::animation {

bool loadInput(const std::vector<std::uint8_t>& buf, cli::Route route, Context& ctx);
bool saveOutput(const Context& ctx, const CliArgs& args, cli::State& state, const Invocation& inv, cli::Route route);

}  // namespace cli::animation
