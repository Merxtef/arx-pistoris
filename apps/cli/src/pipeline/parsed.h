// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#pragma once

#include "modules/module.h"
#include "pipeline/options.h"

#include <vector>

namespace cli {

struct ParsedCli {
  std::vector<const char*> inputs;
  const char* output = nullptr;
  ParsedOptions options;
  std::vector<ModuleInvocation> explicit_modules;
};

}  // namespace cli
