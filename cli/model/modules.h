// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#pragma once

#include "arx_pistoris/pistoris.hpp"

#include "args.h"
#include "model/ctx.h"

namespace cli::model {

bool applyModules(Context& ctx, const CliArgs& args);

}  // namespace cli::model
