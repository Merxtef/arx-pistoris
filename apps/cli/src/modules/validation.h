// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#pragma once

#include "pipeline/options.h"

#include <span>
namespace cli {

bool validateModules(std::span<const ModuleInvocation> modules);
bool validateModuleValues(std::span<const ModuleInvocation> modules, const ParsedOptions& options);

}  // namespace cli
