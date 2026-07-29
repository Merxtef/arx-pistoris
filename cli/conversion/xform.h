// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#pragma once

#include "arx_pistoris/pistoris.hpp"

#include "conversion/options.h"

namespace cli {

pistoris::AffineXform makeAffineXform(const SharedConversionOptions& options);

}  // namespace cli
