// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#pragma once

#include "conversion/options.h"
#include "routes/model/options.h"
#include "routes/model/state.h"

namespace cli::model::operations {

bool apply(IntermediateModel& model, const ModelOptions& options, const SharedConversionOptions& conversion);

}  // namespace cli::model::operations
