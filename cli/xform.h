// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#pragma once

#include "arx_pistoris/pistoris.hpp"

#include "args.h"

namespace cli {

pistoris::AffineXform makeAffineXform(const CliArgs& args);

}  // namespace cli
