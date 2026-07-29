// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#include "modules/implication.h"

#include "console/diagnostics.h"
#include "modules/module.h"
#include "pipeline/options.h"
#include "routes/options.h"

#include <algorithm>
#include <cstddef>
#include <span>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

namespace {

const char* moduleName(const cli::Module* module) {
  return module && module->stableName() ? module->stableName() : "<unnamed>";
}

bool applyInvocation(const cli::ModuleInvocation& invocation, cli::ParsedOptions& options) {
  std::vector<const char*> argv;
  argv.reserve(invocation.arguments.size() + 1);
  argv.push_back(moduleName(invocation.module));
  for (const std::string& argument : invocation.arguments) argv.push_back(argument.c_str());

  int index = 0;
  cli::RouteOptions* route_options =
      invocation.options_route ? cli::ensureRouteOptions(options, *invocation.options_route) : nullptr;
  cli::ModuleParseContext ctx{options, route_options, static_cast<int>(argv.size()), argv.data(), index};
  cli::ModuleParseResult result = invocation.module->parse(ctx);
  if (!result.ok) return false;
  if (result.stop || index + 1 != ctx.argc) {
    cli::diagnostic(cli::DiagnosticCode::kModuleImplicationInvalid,
                    "%s cannot be applied as an effective module invocation",
                    moduleName(invocation.module));
    return false;
  }
  return true;
}

class EffectiveModuleResolver {
 public:
  EffectiveModuleResolver(std::span<const cli::ModuleInvocation> explicit_modules, cli::ParsedOptions& options,
                          std::vector<cli::ModuleInvocation>& modules)
      : explicit_modules_(explicit_modules), options_(options), modules_(modules) {}

  bool resolve() {
    for (const cli::ModuleInvocation& invocation : explicit_modules_) {
      if (!invocation.module) continue;
      if (invocation.module->repeatable()) continue;
      if (explicit_singletons_.insert(invocation.module).second) continue;
      cli::diagnostic(
          cli::DiagnosticCode::kDuplicateModule, "%s specified more than once", moduleName(invocation.module));
      return false;
    }

    for (const cli::ModuleInvocation& invocation : explicit_modules_) {
      if (!invocation.module) continue;
      cli::ModuleInvocation effective = invocation;
      effective.origin = cli::ModuleOrigin::kExplicit;
      effective.implied_by = nullptr;
      if (!appendAndExpand(std::move(effective))) return false;
    }
    return true;
  }

 private:
  bool appendAndExpand(cli::ModuleInvocation invocation) {
    const std::size_t index = modules_.size();
    modules_.push_back(std::move(invocation));
    if (!applyInvocation(modules_[index], options_)) return false;
    return expand(index);
  }

  bool expand(std::size_t source_index) {
    const cli::Module* source = modules_[source_index].module;
    if (std::ranges::find(expansion_stack_, source) != expansion_stack_.end()) {
      cli::diagnostic(
          cli::DiagnosticCode::kModuleImplicationInvalid, "Module implication cycle reaches %s", moduleName(source));
      return false;
    }

    const std::vector<std::string> source_arguments = modules_[source_index].arguments;
    expansion_stack_.push_back(source);
    for (const cli::ModuleImplication& implication : source->implications()) {
      if (!implication.target) {
        cli::diagnostic(
            cli::DiagnosticCode::kModuleImplicationInvalid, "%s has a null implication target", moduleName(source));
        expansion_stack_.pop_back();
        return false;
      }

      const cli::Module* target = &implication.target();
      if (std::ranges::find(expansion_stack_, target) != expansion_stack_.end()) {
        cli::diagnostic(cli::DiagnosticCode::kModuleImplicationInvalid,
                        "Module implication cycle: %s implies %s",
                        moduleName(source),
                        moduleName(target));
        expansion_stack_.pop_back();
        return false;
      }
      if (!target->repeatable() && explicit_singletons_.contains(target)) continue;

      std::vector<std::string> arguments;
      if (implication.arguments && !implication.arguments(source_arguments, arguments)) {
        cli::diagnostic(cli::DiagnosticCode::kModuleImplicationInvalid,
                        "%s failed to construct implied values for %s",
                        moduleName(source),
                        moduleName(target));
        expansion_stack_.pop_back();
        return false;
      }

      cli::ModuleInvocation candidate{.module = target,
                                      .options_route = modules_[source_index].options_route,
                                      .origin = cli::ModuleOrigin::kImplied,
                                      .implied_by = source,
                                      .arguments = std::move(arguments)};
      if (!acceptImplied(std::move(candidate))) {
        expansion_stack_.pop_back();
        return false;
      }
    }
    expansion_stack_.pop_back();
    return true;
  }

  bool acceptImplied(cli::ModuleInvocation candidate) {
    const cli::Module* target = candidate.module;
    if (!target->repeatable()) {
      if (explicit_singletons_.contains(target)) return true;

      auto existing = implied_singletons_.find(target);
      if (existing != implied_singletons_.end()) {
        const cli::ModuleInvocation& selected = modules_[existing->second];
        if (selected.arguments == candidate.arguments) return true;
        cli::diagnostic(cli::DiagnosticCode::kModuleImplicationConflict,
                        "%s and %s imply conflicting values for %s; specify %s explicitly",
                        moduleName(selected.implied_by),
                        moduleName(candidate.implied_by),
                        moduleName(target),
                        moduleName(target));
        return false;
      }
      implied_singletons_.emplace(target, modules_.size());
    }
    return appendAndExpand(std::move(candidate));
  }

  std::span<const cli::ModuleInvocation> explicit_modules_;
  cli::ParsedOptions& options_;
  std::vector<cli::ModuleInvocation>& modules_;
  std::unordered_set<const cli::Module*> explicit_singletons_;
  std::unordered_map<const cli::Module*, std::size_t> implied_singletons_;
  std::vector<const cli::Module*> expansion_stack_;
};

}  // namespace

namespace cli {

bool resolveEffectiveModules(std::span<const ModuleInvocation> explicit_modules, ParsedOptions& out_options,
                             std::vector<ModuleInvocation>& out_modules) {
  ParsedOptions options;
  std::vector<ModuleInvocation> modules;
  EffectiveModuleResolver resolver(explicit_modules, options, modules);
  if (!resolver.resolve()) return false;

  out_options = std::move(options);
  out_modules = std::move(modules);
  return true;
}

}  // namespace cli
