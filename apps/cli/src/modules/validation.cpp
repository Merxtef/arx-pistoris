// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#include "modules/validation.h"

#include "console/diagnostics.h"
#include "modules/module.h"
#include "modules/registry.h"
#include "pipeline/options.h"
#include "routes/options.h"

#include <cstddef>
#include <span>

namespace {

bool hasModule(std::span<const cli::ModuleInvocation> modules, const cli::Module& expected) {
  for (const cli::ModuleInvocation& invocation : modules)
    if (invocation.module == &expected) return true;
  return false;
}

bool incompatible(const cli::Module& left, const cli::Module& right) {
  return cli::moduleListContains(left.incompatibleWith(), right) ||
         cli::moduleListContains(right.incompatibleWith(), left);
}

}  // namespace

namespace cli {

bool validateModules(std::span<const ModuleInvocation> modules) {
  int output_converter_count = 0;
  int terminal_action_count = 0;
  for (std::size_t index = 0; index < modules.size(); ++index) {
    const Module* module = modules[index].module;
    if (!module) continue;

    if (module->singleton() == SingletonCategory::kOutputConverter) ++output_converter_count;
    if (module->singleton() == SingletonCategory::kTerminalAction) ++terminal_action_count;

    for (std::size_t other_index = index + 1; other_index < modules.size(); ++other_index) {
      const Module* other = modules[other_index].module;
      if (!other || !incompatible(*module, *other)) continue;
      diagnostic(DiagnosticCode::kIncompatibleModules,
                 "%s is incompatible with %s",
                 module->stableName(),
                 other->stableName());
      return false;
    }

    const Module* parent = moduleParent(*module);
    if (parent && !hasModule(modules, *parent)) {
      diagnostic(DiagnosticCode::kMissingDependency, "%s requires %s", module->stableName(), parent->stableName());
      return false;
    }

    for (ModuleRef dependency_ref : module->dependencies()) {
      const Module& dependency = dependency_ref();
      if (hasModule(modules, dependency)) continue;
      diagnostic(DiagnosticCode::kMissingDependency, "%s requires %s", module->stableName(), dependency.stableName());
      return false;
    }
  }

  if (output_converter_count > 1) {
    diagnostic(DiagnosticCode::kOutputConverterConflict, "Use only one non-default output converter option");
    return false;
  }
  if (terminal_action_count > 1) {
    diagnostic(DiagnosticCode::kIncompatibleModules, "Use only one terminal action");
    return false;
  }
  return true;
}

bool validateModuleValues(std::span<const ModuleInvocation> modules, const ParsedOptions& options) {
  for (const ModuleInvocation& invocation : modules) {
    if (!invocation.module) continue;
    const RouteOptions* route_options =
        invocation.options_route ? findRouteOptions(options, *invocation.options_route) : nullptr;
    ModuleValidationContext context{options, route_options};
    if (!invocation.module->validate(context)) return false;
  }
  return true;
}

}  // namespace cli
