// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#pragma once

#include "arx_pistoris/pistoris.hpp"

#include "conversion/options.h"
#include "routes/model/options.h"
#include "routes/model/state.h"

namespace cli::model {

bool applyModules(Context& ctx, const ModelOptions& options, const SharedConversionOptions& conversion);

}  // namespace cli::model
