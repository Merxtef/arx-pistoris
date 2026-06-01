// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#pragma once

#include "arx_pistoris/pistoris.hpp"

#include "args.h"
#include "formats.h"
#include "model/ctx.h"
#include "state.h"

#include <cstdint>
#include <vector>

namespace cli::model {

bool loadTeaFile(const char* path, pistoris::Tea& out);
bool loadReferenceFtl(const char* path, Context& ctx);
bool loadInput(const std::vector<std::uint8_t>& buf, const Invocation& inv, cli::Route route, Context& ctx);
bool loadExtras(const Invocation& inv, Context& ctx);
bool validateTeaCompatibility(const Context& ctx);
bool saveOutput(const Context& ctx, const CliArgs& args, cli::State& state, const Invocation& inv, cli::Route route);

}  // namespace cli::model
