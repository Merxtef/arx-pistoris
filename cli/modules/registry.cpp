// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#include "modules/registry.h"

#include "console/diagnostics.h"
#include "formats/format.h"
#include "modules/module.h"
#include "modules/root.h"
#include "routes/descriptor.h"
#include "routes/registry.h"
#include "routes/types.h"

#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <limits>
#include <span>
#include <string>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

namespace {

std::string_view lookupKey(const char* token) {
  if (!token || token[0] != '-') return {};
  return std::string_view(token + 1);
}

struct RegistryData {
  std::vector<cli::RegisteredModule> modules;
  std::string error;
};

void fail(RegistryData& data, std::string message) {
  if (data.error.empty()) data.error = std::move(message);
}

bool nonEmpty(const char* text) { return text && *text; }

bool completeHelp(const cli::ModuleHelp& help) { return nonEmpty(help.usage) && nonEmpty(help.description); }

bool hiddenHelp(const cli::ModuleHelp& help) { return !help.usage && !help.description; }

void registerModule(cli::ModuleRef reference, const cli::Module* parent, std::uint8_t depth,
                    const cli::RouteDescriptor* owning_route, RegistryData& data,
                    std::unordered_set<const cli::Module*>& active, std::unordered_set<const cli::Module*>& registered,
                    std::unordered_map<std::string_view, const cli::Module*>& keywords) {
  if (!reference) {
    fail(data, "null module reference");
    return;
  }

  const cli::Module& module = reference();
  if (active.contains(&module)) {
    fail(data, std::string("module hierarchy cycle at ") + (module.stableName() ? module.stableName() : "<unnamed>"));
    return;
  }
  if (!registered.insert(&module).second) {
    fail(data,
         std::string("module registered more than once: ") + (module.stableName() ? module.stableName() : "<unnamed>"));
    return;
  }

  std::span<const char* const> names = module.keywords();
  if (names.empty() || !names.front() || std::string_view(names.front()).substr(0, 2) != "--") {
    fail(data, "module canonical keyword must start with --");
    return;
  }
  for (const char* name : names) {
    if (!name || name[0] != '-' || name[1] == '\0') {
      fail(data, std::string("invalid keyword on ") + names.front());
      return;
    }
    if (!keywords.emplace(name, &module).second) {
      fail(data, std::string("duplicate module keyword: ") + name);
      return;
    }
  }

  const cli::ModuleHelp help = module.help(owning_route);
  if (!completeHelp(help)) {
    fail(data, std::string("incomplete help metadata on ") + module.stableName());
    return;
  }
  if (module.requestedOutputFormat() != cli::Format::kUnknown &&
      (module.outputFormats() & cli::formatBit(module.requestedOutputFormat())) == 0) {
    fail(data, std::string("requested output is outside output constraints on ") + module.stableName());
    return;
  }
  const bool output_converter = module.category() == cli::ModuleCategory::kOutputConverter;
  if (output_converter != (module.singleton() == cli::SingletonCategory::kOutputConverter)) {
    fail(data, std::string("invalid output converter metadata on ") + module.stableName());
    return;
  }
  const bool terminal_action = module.category() == cli::ModuleCategory::kTerminalAction;
  if (terminal_action != (module.singleton() == cli::SingletonCategory::kTerminalAction)) {
    fail(data, std::string("invalid terminal action metadata on ") + module.stableName());
    return;
  }
  const bool format_modifier = module.category() == cli::ModuleCategory::kFormatModifier;
  if (format_modifier != (module.modifiedFormats() != cli::kNoFormats)) {
    fail(data, std::string("invalid format modifier metadata on ") + module.stableName());
    return;
  }
  const bool must_be_route_owned =
      module.category() == cli::ModuleCategory::kRoute || module.category() == cli::ModuleCategory::kOutputConverter;
  if (must_be_route_owned && !owning_route) {
    fail(data, std::string("route-owned module registered globally: ") + module.stableName());
    return;
  }
  const bool must_be_global = module.category() == cli::ModuleCategory::kSystem ||
                              module.category() == cli::ModuleCategory::kTerminalAction ||
                              module.category() == cli::ModuleCategory::kSharedConversion ||
                              module.category() == cli::ModuleCategory::kFormatModifier;
  if (must_be_global && owning_route) {
    fail(data, std::string("global module registered under route ") + owning_route->name + ": " + module.stableName());
    return;
  }
  data.modules.push_back({&module, parent, owning_route, depth});
  active.insert(&module);
  if (depth == std::numeric_limits<std::uint8_t>::max()) {
    fail(data, std::string("module hierarchy is too deep at ") + module.stableName());
  } else {
    for (cli::ModuleRef child : module.children()) {
      if (!data.error.empty()) break;
      registerModule(
          child, &module, static_cast<std::uint8_t>(depth + 1), owning_route, data, active, registered, keywords);
    }
  }
  active.erase(&module);
}

bool registeredReference(cli::ModuleRef reference, const std::unordered_set<const cli::Module*>& registered) {
  return reference && registered.contains(&reference());
}

const cli::RegisteredModule* findRegistered(const RegistryData& data, const cli::Module& module) {
  for (const cli::RegisteredModule& item : data.modules)
    if (item.module == &module) return &item;
  return nullptr;
}

void validateRelationships(RegistryData& data, const std::unordered_set<const cli::Module*>& registered) {
  for (const cli::RegisteredModule& item : data.modules) {
    const cli::Module& module = *item.module;
    for (std::span<const cli::ModuleRef> references : {module.dependencies(), module.incompatibleWith()}) {
      std::unordered_set<const cli::Module*> unique;
      for (cli::ModuleRef reference : references) {
        if (!registeredReference(reference, registered)) {
          fail(data, std::string("unregistered relationship on ") + module.stableName());
          return;
        }
        const cli::Module* target = &reference();
        if (target == &module || !unique.insert(target).second) {
          fail(data, std::string("invalid repeated/self relationship on ") + module.stableName());
          return;
        }
      }
    }

    for (const cli::ModuleImplication& implication : module.implications()) {
      if (!registeredReference(implication.target, registered)) {
        fail(data, std::string("unregistered implication on ") + module.stableName());
        return;
      }
      if (module.category() == cli::ModuleCategory::kTerminalAction ||
          implication.target().category() == cli::ModuleCategory::kTerminalAction) {
        fail(data, std::string("terminal action participates in implication on ") + module.stableName());
        return;
      }
      if (&implication.target() == &module) {
        fail(data, std::string("self implication on ") + module.stableName());
        return;
      }
      const cli::RegisteredModule* target = findRegistered(data, implication.target());
      if (target && target->owner && target->owner != item.owner) {
        fail(data, std::string("implication crosses route ownership on ") + module.stableName());
        return;
      }
    }
  }
}

bool validateImplicationBranch(const cli::Module& module, RegistryData& data,
                               std::unordered_set<const cli::Module*>& active,
                               std::unordered_set<const cli::Module*>& completed) {
  if (completed.contains(&module)) return true;
  if (!active.insert(&module).second) {
    fail(data, std::string("module implication cycle at ") + module.stableName());
    return false;
  }

  for (const cli::ModuleImplication& implication : module.implications()) {
    if (!validateImplicationBranch(implication.target(), data, active, completed)) return false;
  }
  active.erase(&module);
  completed.insert(&module);
  return true;
}

void validateImplicationGraph(RegistryData& data) {
  std::unordered_set<const cli::Module*> active;
  std::unordered_set<const cli::Module*> completed;
  for (const cli::RegisteredModule& item : data.modules) {
    if (!validateImplicationBranch(*item.module, data, active, completed)) return;
  }
}

RegistryData buildRegistry() {
  RegistryData data;
  std::unordered_set<const cli::Module*> active;
  std::unordered_set<const cli::Module*> registered;
  std::unordered_map<std::string_view, const cli::Module*> keywords;

  auto register_roots = [&](std::span<const cli::ModuleRef> roots, const cli::RouteDescriptor* owning_route) {
    for (cli::ModuleRef root : roots) {
      if (!data.error.empty()) break;
      registerModule(root, nullptr, 0, owning_route, data, active, registered, keywords);
    }
  };

  register_roots(cli::modules::rootModules(), nullptr);
  cli::RouteRegistryView routes = cli::routeRegistry();
  std::unordered_set<cli::RouteKind> route_kinds;
  std::unordered_set<std::string_view> route_names;
  for (std::size_t index = 0; index < routes.count; ++index) {
    const cli::RouteDescriptor& route = routes.routes[index];
    if (route.kind == cli::RouteKind::kUnknown || !route.name || std::string_view(route.name).empty() ||
        route.primary_input_formats == cli::kNoFormats || route.output_formats == cli::kNoFormats ||
        !route.create_invocation || !route.probe || !route.resolve || !route.execute ||
        !nonEmpty(route.help.synopsis) || !nonEmpty(route.help.summary) ||
        route.help.examples.size() > cli::kMaxRouteHelpExamples) {
      fail(data, "incomplete route descriptor");
      return data;
    }
    for (const cli::HelpExample& example : route.help.examples) {
      if (!nonEmpty(example.arguments) || !nonEmpty(example.description)) {
        fail(data, std::string("incomplete help example on route ") + route.name);
        return data;
      }
    }
    if (!route.modules.empty() && !route.create_options) {
      fail(data, std::string("route modules require an options factory: ") + route.name);
      return data;
    }
    if (!route_kinds.insert(route.kind).second || !route_names.insert(route.name).second) {
      fail(data, std::string("duplicate route descriptor: ") + route.name);
      return data;
    }
  }
  for (std::size_t index = 0; index < routes.count && data.error.empty(); ++index) {
    const cli::RouteDescriptor& route = routes.routes[index];
    register_roots(route.modules, &route);
  }
  if (!data.error.empty()) return data;

  for (const cli::RegisteredModule& item : data.modules) {
    const cli::ModuleHelp registered_help = item.module->help(item.owner);
    for (std::size_t index = 0; index < routes.count; ++index) {
      const cli::ModuleHelp help = item.module->help(&routes.routes[index]);
      if (hiddenHelp(help)) continue;
      if (!completeHelp(help)) {
        fail(data, std::string("incomplete route help metadata on ") + item.module->stableName());
        return data;
      }
      if (help.section != registered_help.section) {
        fail(data, std::string("inconsistent route help section on ") + item.module->stableName());
        return data;
      }
    }
  }

  validateRelationships(data, registered);
  if (!data.error.empty()) return data;
  validateImplicationGraph(data);
  if (!data.error.empty()) return data;

  std::unordered_set<const cli::Module*> supported_modules;
  for (std::size_t index = 0; index < routes.count; ++index) {
    const cli::RouteDescriptor& route = routes.routes[index];
    for (cli::ModuleRef reference : route.supported_modules) {
      if (!registeredReference(reference, registered)) {
        fail(data, std::string("invalid supported module registration on route ") + route.name);
        return data;
      }
      const cli::RegisteredModule* item = findRegistered(data, reference());
      if (!item || item->owner) {
        fail(data, std::string("route support must reference a global module on ") + route.name);
        return data;
      }
      supported_modules.insert(&reference());
    }
  }
  for (const cli::RegisteredModule& item : data.modules) {
    const cli::ModuleCategory category = item.module->category();
    const bool requires_route_support = category == cli::ModuleCategory::kSharedConversion ||
                                        category == cli::ModuleCategory::kNativeBakeModifier ||
                                        category == cli::ModuleCategory::kInputModifier;
    if (!item.owner && requires_route_support && !supported_modules.contains(item.module)) {
      fail(data, std::string("route-sensitive module has no supported routes: ") + item.module->stableName());
      return data;
    }
  }
  return data;
}

const RegistryData& registryData() {
  static const RegistryData kData = buildRegistry();
  return kData;
}

}  // namespace

namespace cli {

ModuleRegistryView moduleRegistry() {
  const RegistryData& data = registryData();
  return {data.modules.data(), data.modules.size()};
}

const char* moduleRegistryError() { return registryData().error.c_str(); }

bool validateModuleRegistry() {
  if (registryData().error.empty()) return true;
  diagnostic(DiagnosticCode::kUnhandledException, "Invalid CLI module registry: %s", registryData().error.c_str());
  return false;
}

const RegisteredModule* registeredModule(const Module& module) {
  ModuleRegistryView registry = moduleRegistry();
  for (std::size_t index = 0; index < registry.count; ++index)
    if (registry.modules[index].module == &module) return &registry.modules[index];
  return nullptr;
}

const Module* moduleParent(const Module& module) {
  const RegisteredModule* registered = registeredModule(module);
  return registered ? registered->parent : nullptr;
}

bool moduleListContains(std::span<const ModuleRef> modules, const Module& module) {
  return std::ranges::any_of(modules, [&](ModuleRef reference) { return reference && &reference() == &module; });
}

RouteMask routesSupportingModule(const Module& module) {
  const RegisteredModule* registered = registeredModule(module);
  if (registered && registered->owner) return routeBit(registered->owner->kind);

  RouteMask routes = kNoRoutes;
  RouteRegistryView registry = routeRegistry();
  for (std::size_t index = 0; index < registry.count; ++index) {
    const RouteDescriptor& route = registry.routes[index];
    if (moduleListContains(route.supported_modules, module)) routes |= routeBit(route.kind);
  }
  return routes;
}

OptionMatch resolveModule(const char* token) {
  std::string_view key = lookupKey(token);
  if (key.empty()) return {};

  const Module* unique = nullptr;
  ModuleRegistryView registry = moduleRegistry();
  for (std::size_t index = 0; index < registry.count; ++index) {
    const Module& module = *registry.modules[index].module;
    for (const char* name : module.keywords()) {
      std::string_view option_key = std::string_view(name).substr(1);
      if (option_key == key) return {.module = &module};
      if (!option_key.starts_with(key)) continue;
      if (unique && unique != &module) return {.ambiguous = true};
      unique = &module;
    }
  }
  return {.module = unique};
}

void printModuleSuggestions(const char* token) {
  std::string_view key = lookupKey(token);
  if (key.empty()) return;

  bool any = false;
  ModuleRegistryView registry = moduleRegistry();
  for (std::size_t index = 0; index < registry.count; ++index) {
    const Module& module = *registry.modules[index].module;
    bool match = std::ranges::any_of(
        module.keywords(), [&](const char* name) { return std::string_view(name).substr(1).starts_with(key); });
    if (!match) continue;
    std::fprintf(stderr, "%s%s", any ? ", " : "", module.stableName());
    any = true;
  }
  if (any) std::fprintf(stderr, "\n");
}

}  // namespace cli
