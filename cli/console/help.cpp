// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#include "console/help.h"

#include "console/diagnostics.h"
#include "modules/module.h"
#include "modules/registry.h"
#include "routes/registry.h"

#include <cctype>
#include <cstdio>
#include <string>
#include <string_view>
#include <vector>

namespace {

struct HelpFilter {
  bool cli = true;
  cli::RouteMask routes = cli::kNoRoutes;
  bool options = true;
  bool debug = false;
  bool conventions = false;
};

bool startsWith(std::string_view text, std::string_view prefix) {
  return text.size() >= prefix.size() && text.substr(0, prefix.size()) == prefix;
}

bool topicMatches(const char* topic, const char* full) {
  std::string_view needle = topic ? std::string_view(topic) : std::string_view();
  std::string_view name = full;
  return !needle.empty() && startsWith(name, needle);
}

HelpFilter makeHelpFilter(const std::vector<const char*>& topics) {
  HelpFilter filter;
  cli::RouteRegistryView routes = cli::routeRegistry();
  for (std::size_t index = 0; index < routes.count; ++index) {
    filter.routes |= cli::routeBit(routes.routes[index].kind);
  }
  bool any_group = false;
  bool any_section = false;

  for (const char* topic : topics) {
    const bool matches_cli = topicMatches(topic, "cli");
    cli::RouteMask matching_routes = cli::kNoRoutes;
    for (std::size_t index = 0; index < routes.count; ++index) {
      if (topicMatches(topic, routes.routes[index].name)) {
        matching_routes |= cli::routeBit(routes.routes[index].kind);
      }
    }
    if (matches_cli || matching_routes != cli::kNoRoutes) {
      if (!any_group) {
        filter.cli = false;
        filter.routes = cli::kNoRoutes;
        any_group = true;
      }
      filter.cli = filter.cli || matches_cli;
      filter.routes |= matching_routes;
    }

    if (topicMatches(topic, "options")) {
      if (!any_section) {
        filter.options = filter.debug = filter.conventions = false;
        any_section = true;
      }
      filter.options = true;
    }
    if (topicMatches(topic, "debug")) {
      if (!any_section) {
        filter.options = filter.debug = filter.conventions = false;
        any_section = true;
      }
      filter.debug = true;
    }
    if (topicMatches(topic, "conventions")) {
      if (!any_section) {
        filter.options = filter.debug = filter.conventions = false;
        any_section = true;
      }
      filter.conventions = true;
    }
  }

  return filter;
}

bool moduleVisible(const cli::RegisteredModule& registered, const cli::RouteDescriptor* route) {
  const cli::Module& module = *registered.module;
  if (!route)
    return !registered.owner && (module.category() == cli::ModuleCategory::kSystem ||
                                 module.category() == cli::ModuleCategory::kTerminalAction);
  if (registered.owner) return registered.owner == route;

  switch (module.category()) {
    case cli::ModuleCategory::kOutputFormat:
      return (module.outputFormats() & route->output_formats) != 0;
    case cli::ModuleCategory::kFormatModifier: {
      const cli::FormatMask formats = route->primary_input_formats | route->output_formats;
      return (module.modifiedFormats() & formats) != 0;
    }
    case cli::ModuleCategory::kSharedConversion:
    case cli::ModuleCategory::kNativeBakeModifier:
    case cli::ModuleCategory::kInputModifier:
      return (cli::routesSupportingModule(module) & cli::routeBit(route->kind)) != 0;
    case cli::ModuleCategory::kSystem:
    case cli::ModuleCategory::kTerminalAction:
    case cli::ModuleCategory::kRoute:
    case cli::ModuleCategory::kOutputConverter:
      return false;
  }
  return false;
}

void printModuleLine(std::FILE* output, const cli::RegisteredModule& registered, const cli::RouteDescriptor* route) {
  constexpr int kUsageColumnWidth = 48;
  const cli::Module& module = *registered.module;
  const cli::ModuleHelp help = module.help(route);
  int indent = static_cast<int>(registered.depth) * 2;
  int usage_width = kUsageColumnWidth > indent ? kUsageColumnWidth - indent : 0;
  if (help.description) {
    std::fprintf(output, "  %*s%-*s %s\n", indent, "", usage_width, help.usage ? help.usage : "", help.description);
  } else if (help.usage) {
    std::fprintf(output, "  %*s%s\n", indent, "", help.usage);
  }
}

bool printModuleHelp(std::FILE* output, const cli::RouteDescriptor* route, cli::HelpSection section) {
  bool any = false;
  cli::ModuleRegistryView registry = cli::moduleRegistry();
  for (std::size_t index = 0; index < registry.count; ++index) {
    const cli::RegisteredModule& registered = registry.modules[index];
    const cli::Module& module = *registered.module;
    const cli::ModuleHelp help = module.help(route);
    if (help.section != section) continue;
    if (!help.usage && !help.description) continue;
    if (!moduleVisible(registered, route)) continue;
    printModuleLine(output, registered, route);
    any = true;
  }
  return any;
}

std::string routeDisplayName(const cli::RouteDescriptor& route) {
  std::string name = route.name;
  if (!name.empty()) name.front() = static_cast<char>(std::toupper(static_cast<unsigned char>(name.front())));
  return name;
}

const char* helpSectionName(cli::HelpSection section) {
  switch (section) {
    case cli::HelpSection::kOptions:
      return "options";
    case cli::HelpSection::kDebug:
      return "debug";
    case cli::HelpSection::kConventions:
      return "conventions";
  }
  return "unknown";
}

void printCliSection(std::FILE* output, cli::HelpSection section) {
  switch (section) {
    case cli::HelpSection::kOptions:
      std::fprintf(output, "\nCLI options:\n");
      std::fprintf(output, "  Options may appear before, between, or after file paths.\n");
      printModuleHelp(output, nullptr, section);
      break;
    case cli::HelpSection::kDebug:
      std::fprintf(output, "\nCLI debug:\n");
      std::fprintf(output, "  Nothing here yet.\n");
      break;
    case cli::HelpSection::kConventions:
      std::fprintf(output, "\nCLI conventions:\n");
      std::fprintf(output, "  Option prefixes are convenience only; full option names are stable for scripting.\n");
      break;
  }
}

void printRouteSection(std::FILE* output, const cli::RouteDescriptor& route, cli::HelpSection section) {
  bool any = false;
  std::string route_name = routeDisplayName(route);
  std::fprintf(output, "\n%s %s:\n", route_name.c_str(), helpSectionName(section));
  any = printModuleHelp(output, &route, section) || any;
  if (route.help_provider) any = route.help_provider(output, section) || any;
  if (!any) std::fprintf(output, "  Nothing here yet.\n");
}

}  // namespace

namespace cli {

bool validateHelpTopics(const std::vector<const char*>& topics) {
  RouteRegistryView routes = routeRegistry();
  for (const char* topic : topics) {
    bool known = topicMatches(topic, "cli") || topicMatches(topic, "options") || topicMatches(topic, "debug") ||
                 topicMatches(topic, "conventions");
    for (std::size_t index = 0; !known && index < routes.count; ++index) {
      known = topicMatches(topic, routes.routes[index].name);
    }
    if (!known) {
      diagnostic(DiagnosticCode::kUnknownHelpTopic, "--help: unknown topic '%s'", topic ? topic : "");
      return false;
    }
  }
  return true;
}

void printUsage(std::FILE* output, const char* argv0, const std::vector<const char*>& topics) {
  HelpFilter help = makeHelpFilter(topics);
  std::fprintf(output, "Usage:\n");
  std::fprintf(output, "  %s <inputs...> <output> [options]   convert file bundle\n", argv0);
  std::fprintf(output, "\nExamples:\n");
  std::fprintf(output, "  %s level.fts out.glb                export FTS scene GLB\n", argv0);
  std::fprintf(output, "  %s level.fts level.llf level.dlf out.glb\n", argv0);
  std::fprintf(output, "                                      export native Level bundle to GLB\n");
  std::fprintf(output, "  %s --kind level level.glb out.glb   roundtrip Level GLB\n", argv0);
  std::fprintf(output, "  %s --kind level level.glb out.fts   bake loose native Level bundle\n", argv0);
  std::fprintf(output, "  %s --kind level level.glb out.dlf   bake game-resource native Level bundle\n", argv0);
  std::fprintf(output, "  %s model.ftl anim1.tea out.glb      export FTL+TEA bundle\n", argv0);
  std::fprintf(output, "  %s anim.tea out.tea                 rewrite/transform TEA\n", argv0);
  std::fprintf(output, "  %s anim.tea out.json                export TEA JSON\n", argv0);
  std::fprintf(output, "  %s anim.json out.tea                import TEA JSON\n", argv0);
  std::fprintf(output, "  %s --mount data level:1 out.glb     load a mounted native Level bundle\n", argv0);
  std::fprintf(output, "  %s --mount data --list-resources all\n", argv0);
  std::fprintf(output, "                                      list mounted game resources\n");
  std::fprintf(output, "  %s --mount data model.ftl model:npc:hero\n", argv0);
  std::fprintf(output, "                                      write a mounted native Model bundle\n");

  if (help.cli && help.options) printCliSection(output, HelpSection::kOptions);
  if (help.cli && help.debug) printCliSection(output, HelpSection::kDebug);
  if (help.cli && help.conventions) printCliSection(output, HelpSection::kConventions);

  RouteRegistryView routes = routeRegistry();
  for (std::size_t index = 0; index < routes.count; ++index) {
    const RouteDescriptor& route = routes.routes[index];
    if ((help.routes & routeBit(route.kind)) == 0) continue;
    if (help.options) printRouteSection(output, route, HelpSection::kOptions);
    if (help.debug) printRouteSection(output, route, HelpSection::kDebug);
    if (help.conventions) printRouteSection(output, route, HelpSection::kConventions);
  }

  std::fprintf(output, "\nFormats:\n");
  std::fprintf(output, "  %s --kind model <input.glb> <out.ftl> GLB -> FTL + sibling .tea files\n", argv0);
  std::fprintf(output, "  Supported input formats: FTL, FTS, DLF, LLF, TEA, OBJ, JSON, GLB\n");
  std::fprintf(output, "Model outputs:           FTL, OBJ, JSON, GLB\n");
  std::fprintf(output, "Level outputs:           GLB, JSON, loose FTS bundle, DLF game-resource bundle\n");
  std::fprintf(output, "Animation outputs:       TEA, JSON\n");
  std::fprintf(output, "\nResource selectors:\n");
  std::fprintf(output, "  level:<N>\n");
  std::fprintf(output, "  model:<type>:<name>[:<tweak>]\n");
  std::fprintf(output, "  Model types: npc, fix_inter, system, armor, jewelry, magic, movable, provisions,\n");
  std::fprintf(output, "               quest_item, special, weapons\n");
  std::fprintf(output, "  anim:<npc|fix_inter>:<name>\n");
  std::fprintf(output, "  cinematic:<name>\n");
  std::fprintf(output, "  ambiance:<name>\n");
  std::fprintf(output, "  Each selector names exactly one game resource.\n");
  std::fprintf(output, "  Reads search mounts left-to-right; native resource outputs use the first mount.\n");
  std::fprintf(output, "  With no --mount, the current directory is the only mount.\n");
}

}  // namespace cli
