// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#pragma once

#include "conversion/options.h"
#include "routes/animation/state.h"

namespace cli::animation::operations {

bool apply(IntermediateAnimation& animation, const SharedConversionOptions& conversion);

}  // namespace cli::animation::operations
