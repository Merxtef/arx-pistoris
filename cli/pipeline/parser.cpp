// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#include "pipeline/parser.h"

#include "console/diagnostics.h"
#include "modules/module.h"
#include "modules/registry.h"
#include "pipeline/options.h"
#include "pipeline/parsed.h"
#include "routes/options.h"

#include <cstdio>
#include <utility>
#include <vector>

namespace {

bool hasExplicitModule(const cli::ParsedCli& parsed, const cli::Module& module) {
  for (const cli::ModuleInvocation& invocation : parsed.explicit_modules)
    if (invocation.module == &module) return true;
  return false;
}

bool hasTerminalAction(const cli::ParsedCli& parsed) {
  for (const cli::ModuleInvocation& invocation : parsed.explicit_modules)
    if (invocation.module && invocation.module->category() == cli::ModuleCategory::kTerminalAction) return true;
  return false;
}

}  // namespace

namespace cli {

bool parseArgs(int argc, char* argv[], ParsedCli& parsed) {
  ParsedOptions& options = parsed.options;
  std::vector<const char*> positionals;
  for (int index = 1; index < argc; ++index) {
    if (argv[index][0] == '-' && argv[index][1] != '\0') {
      OptionMatch match = resolveModule(argv[index]);
      if (match.ambiguous) {
        diagnosticPrefix(DiagnosticCode::kAmbiguousOption);
        std::fprintf(stderr, "Ambiguous option prefix: %s\nPossible commands: ", argv[index]);
        printModuleSuggestions(argv[index]);
        return false;
      }
      if (!match.module) {
        diagnostic(DiagnosticCode::kUnknownOption, "Unknown option: %s", argv[index]);
        return false;
      }

      const Module& module = *match.module;
      if (!module.repeatable() && hasExplicitModule(parsed, module)) {
        diagnostic(DiagnosticCode::kDuplicateModule, "%s specified more than once", module.stableName());
        return false;
      }

      const int option_index = index;
      const RegisteredModule* registered = registeredModule(module);
      const RouteDescriptor* options_route = registered ? registered->owner : nullptr;
      RouteOptions* route_options = options_route ? ensureRouteOptions(options, *options_route) : nullptr;
      ModuleParseContext ctx{options, route_options, argc, argv, index};
      ModuleParseResult result = module.parse(ctx);
      if (!result.ok) return false;
      ModuleInvocation invocation;
      invocation.module = &module;
      invocation.options_route = options_route;
      invocation.arguments.reserve(static_cast<std::size_t>(index - option_index));
      for (int argument_index = option_index + 1; argument_index <= index; ++argument_index) {
        invocation.arguments.emplace_back(argv[argument_index]);
      }
      parsed.explicit_modules.push_back(std::move(invocation));
      if (result.stop) return true;
    } else {
      positionals.push_back(argv[index]);
    }
  }

  if (hasTerminalAction(parsed)) return true;
  if (positionals.size() < 2) {
    diagnostic(DiagnosticCode::kMissingInputOutput, "Expected at least one input and one output path");
    return false;
  }
  parsed.output = positionals.back();
  parsed.inputs.assign(positionals.begin(), positionals.end() - 1);
  return true;
}

}  // namespace cli
