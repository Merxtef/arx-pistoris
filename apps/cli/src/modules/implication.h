// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#pragma once

#include "modules/module.h"

#include <span>
#include <vector>

namespace cli {

bool resolveEffectiveModules(std::span<const ModuleInvocation> explicit_modules, ParsedOptions& out_options,
                             std::vector<ModuleInvocation>& out_modules);

}  // namespace cli
