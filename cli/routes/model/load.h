// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#pragma once

#include "arx_pistoris/pistoris.hpp"

#include "routes/model/invocation.h"
#include "routes/model/state.h"
#include "routes/types.h"

#include <cstdint>
#include <vector>

namespace cli::model {

bool loadTeaFile(const ClassifiedPath& input, pistoris::Tea& out);
bool loadReferenceFtl(const char* path, Context& ctx);
bool loadInput(const std::vector<ClassifiedPath>& inputs, const Invocation& invocation, Route route, Context& ctx);
bool loadExtras(const std::vector<ClassifiedPath>& inputs, const Invocation& invocation, Context& ctx);
bool validateTeaCompatibility(const Context& ctx);

}  // namespace cli::model
