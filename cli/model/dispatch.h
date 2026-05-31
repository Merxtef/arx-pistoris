// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#pragma once

#include "args.h"
#include "formats.h"
#include "state.h"

#include <cstdint>
#include <vector>

namespace cli::model {

int dispatch(const CliArgs& args, cli::State& state, const std::vector<std::uint8_t>& buf, cli::Route route);

}  // namespace cli::model
