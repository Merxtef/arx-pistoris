// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#include "console/help.h"

#include "arx_pistoris/paths.hpp"

#include "console/help_request.h"
#include "console/style.h"
#include "formats/format.h"
#include "modules/module.h"
#include "modules/registry.h"
#include "routes/descriptor.h"
#include "routes/registry.h"
#include "routes/types.h"

#include <cassert>
#include <cstddef>
#include <cstdio>
#include <span>
#include <string>
#include <string_view>

namespace {

constexpr std::size_t kLineWidth = 100;
constexpr std::size_t kDescriptionColumn = 48;
constexpr cli::Format kFormats[] = {
    cli::Format::kFtl,
    cli::Format::kFts,
    cli::Format::kDlf,
    cli::Format::kLlf,
    cli::Format::kTea,
    cli::Format::kAmb,
    cli::Format::kCin,
    cli::Format::kObj,
    cli::Format::kJson,
    cli::Format::kGlb,
};

struct DomainPresentation {
  const char* title;
  const char* keyword;
  cli::ConsoleStyle style;
};

std::string_view view(const char* text) { return text ? std::string_view(text) : std::string_view(); }

DomainPresentation routePresentation(cli::RouteKind kind) {
  switch (kind) {
    case cli::RouteKind::kLevel:
      return {"LEVEL", "level", {.color = cli::ConsoleColor::kGreen, .bold = true}};
    case cli::RouteKind::kModel:
      return {"MODEL", "model", {.color = cli::ConsoleColor::kBrightMagenta, .bold = true}};
    case cli::RouteKind::kAnimation:
      return {"ANIMATION", "animation", {.color = cli::ConsoleColor::kMagenta, .bold = true}};
    case cli::RouteKind::kAmbiance:
      return {"AMBIANCE", "ambiance", {.color = cli::ConsoleColor::kYellow, .bold = true}};
    case cli::RouteKind::kCinematic:
      return {"CINEMATIC", "cinematic", {.color = cli::ConsoleColor::kCyan, .bold = true}};
    case cli::RouteKind::kUnknown:
      break;
  }
  return {"CLI", "cli", {.bold = true}};
}

bool selectorStyle(std::string_view token, cli::ConsoleStyle& style) {
  if (token.starts_with("level:")) {
    style = routePresentation(cli::RouteKind::kLevel).style;
    return true;
  }
  if (token.starts_with("model:")) {
    style = routePresentation(cli::RouteKind::kModel).style;
    return true;
  }
  if (token.starts_with("anim:")) {
    style = routePresentation(cli::RouteKind::kAnimation).style;
    return true;
  }
  if (token.starts_with("ambiance:")) {
    style = routePresentation(cli::RouteKind::kAmbiance).style;
    return true;
  }
  if (token.starts_with("cinematic:")) {
    style = routePresentation(cli::RouteKind::kCinematic).style;
    return true;
  }
  return false;
}

cli::ConsoleStyle formatStyle(cli::Format format) {
  switch (format) {
    case cli::Format::kFts:
    case cli::Format::kDlf:
    case cli::Format::kLlf:
      return {.color = cli::ConsoleColor::kGreen};
    case cli::Format::kFtl:
      return {.color = cli::ConsoleColor::kBrightMagenta};
    case cli::Format::kTea:
      return {.color = cli::ConsoleColor::kMagenta};
    case cli::Format::kAmb:
      return {.color = cli::ConsoleColor::kYellow};
    case cli::Format::kCin:
      return {.color = cli::ConsoleColor::kCyan};
    case cli::Format::kObj:
    case cli::Format::kJson:
    case cli::Format::kGlb:
      return {.bold = true};
    case cli::Format::kUnset:
    case cli::Format::kUnknown:
      return {};
  }
  return {};
}

const char* pageName(cli::HelpPage page) {
  switch (page) {
    case cli::HelpPage::kGeneral:
      return "GENERAL";
    case cli::HelpPage::kSelectors:
      return "SELECTORS";
    case cli::HelpPage::kFormats:
      return "FORMATS";
    case cli::HelpPage::kDebug:
      return "DEBUG";
  }
  return "GENERAL";
}

class HelpPrinter {
 public:
  explicit HelpPrinter(std::FILE* output) : output_(output) {}

  void breadcrumb(DomainPresentation domain, cli::HelpPage page) const {
    cli::writeStyled(output_, domain.style, domain.title);
    std::fputs(" / ", output_);
    cli::writeStyled(output_, {.bold = true}, pageName(page));
    std::fputc('\n', output_);
  }

  void blank() const { std::fputc('\n', output_); }

  void heading(std::string_view text) const {
    cli::writeStyled(output_, {.bold = true}, text);
    std::fputs(":\n", output_);
  }

  void line(std::string_view text = {}) const {
    write(text);
    std::fputc('\n', output_);
  }

  void wrapped(std::string_view text, std::size_t indent = 0) const {
    spaces(indent);
    writeWrapped(text, indent, indent);
  }

  void wrapped(std::string_view text, std::size_t indent, std::size_t continuation_indent) const {
    spaces(indent);
    writeWrapped(text, indent, continuation_indent);
  }

  void entry(std::size_t depth, std::string_view prefix, std::string_view usage, std::string_view description,
             bool style_selectors = false) const {
    const std::size_t usage_column = 2 + depth * 2;
    spaces(usage_column);
    write(prefix);
    if (style_selectors) {
      writeCommandArguments(usage);
    } else {
      write(usage);
    }
    const std::size_t usage_end = usage_column + prefix.size() + usage.size();
    if (description.empty()) {
      std::fputc('\n', output_);
      return;
    }

    if (usage_end >= kDescriptionColumn - 1) {
      std::fputc('\n', output_);
      spaces(kDescriptionColumn);
    } else {
      spaces(kDescriptionColumn - usage_end);
    }
    writeWrapped(description, kDescriptionColumn, kDescriptionColumn);
  }

  void topic(DomainPresentation domain, std::string_view suffix, std::string_view description,
             std::size_t description_column = 26) const {
    spaces(2);
    cli::writeStyled(output_, domain.style, domain.keyword);
    if (!suffix.empty()) cli::writeStyled(output_, domain.style, suffix);
    std::size_t column = 2 + std::string_view(domain.keyword).size() + suffix.size();
    if (column >= description_column - 1) {
      std::fputc('\n', output_);
      spaces(description_column);
    } else {
      spaces(description_column - column);
    }
    writeWrapped(description, description_column, description_column);
  }

  void formatLine(std::string_view label, cli::FormatMask formats) const {
    spaces(2);
    write(label);
    if (label.size() < 18) spaces(18 - label.size());
    bool first = true;
    for (cli::Format format : kFormats) {
      if ((formats & cli::formatBit(format)) == 0) continue;
      if (!first) std::fputs(", ", output_);
      cli::writeStyled(output_, formatStyle(format), cli::formatName(format));
      first = false;
    }
    if (first) std::fputc('-', output_);
    std::fputc('\n', output_);
  }

  std::FILE* output() const { return output_; }

 private:
  void spaces(std::size_t count) const {
    for (std::size_t index = 0; index < count; ++index) std::fputc(' ', output_);
  }

  void write(std::string_view text) const { std::fwrite(text.data(), 1, text.size(), output_); }

  void writeCommandArguments(std::string_view arguments) const {
    while (!arguments.empty()) {
      const std::size_t separator = arguments.find(' ');
      const std::string_view token = arguments.substr(0, separator);
      cli::ConsoleStyle style;
      if (selectorStyle(token, style)) {
        cli::writeStyled(output_, style, token);
      } else {
        write(token);
      }
      if (separator == std::string_view::npos) return;
      std::fputc(' ', output_);
      arguments.remove_prefix(separator + 1);
    }
  }

  void writeWrapped(std::string_view text, std::size_t column, std::size_t continuation_indent) const {
    bool word_on_line = false;
    while (!text.empty()) {
      const std::size_t first = text.find_first_not_of(' ');
      if (first == std::string_view::npos) break;
      text.remove_prefix(first);
      const std::size_t separator = text.find(' ');
      const std::string_view word = text.substr(0, separator);
      if (word_on_line && column + 1 + word.size() > kLineWidth) {
        std::fputc('\n', output_);
        spaces(continuation_indent);
        column = continuation_indent;
        word_on_line = false;
      }
      if (word_on_line) {
        std::fputc(' ', output_);
        ++column;
      }
      write(word);
      column += word.size();
      word_on_line = true;
      if (separator == std::string_view::npos) break;
      text.remove_prefix(separator + 1);
    }
    std::fputc('\n', output_);
  }

  std::FILE* output_;
};

bool moduleVisible(const cli::RegisteredModule& registered, const cli::RouteDescriptor* route) {
  const cli::Module& module = *registered.module;
  if (!route) {
    return !registered.owner && (module.category() == cli::ModuleCategory::kSystem ||
                                 module.category() == cli::ModuleCategory::kTerminalAction);
  }
  if (registered.owner) return registered.owner == route;

  switch (module.category()) {
    case cli::ModuleCategory::kOutputFormat: {
      const cli::RouteMask routes = cli::routesSupportingModule(module);
      return (module.outputFormats() & route->output_formats) != 0 &&
             (routes == cli::kNoRoutes || (routes & cli::routeBit(route->kind)) != 0);
    }
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

void printImplicationSummary(const HelpPrinter& printer, const cli::RegisteredModule& registered) {
  const std::span<const cli::ModuleImplication> implications = registered.module->implications();
  if (implications.empty()) return;

  std::string summary = "Includes: ";
  bool first = true;
  for (std::size_t index = 0; index < implications.size(); ++index) {
    const cli::Module* target = &implications[index].target();
    bool duplicate = false;
    for (std::size_t previous = 0; previous < index; ++previous) {
      if (&implications[previous].target() == target) {
        duplicate = true;
        break;
      }
    }
    if (duplicate) continue;
    if (!first) summary += ", ";
    summary += target->stableName();
    first = false;
  }
  summary += ". Included flags and their options may also be specified explicitly.";

  constexpr std::string_view kPrefix = "Includes: ";
  const std::size_t indent = 4 + registered.depth * 2;
  printer.wrapped(summary, indent, indent + kPrefix.size());
}

bool printModuleHelp(const HelpPrinter& printer, const cli::RouteDescriptor* route, cli::HelpSection section) {
  bool any = false;
  cli::ModuleRegistryView registry = cli::moduleRegistry();
  for (std::size_t index = 0; index < registry.count; ++index) {
    const cli::RegisteredModule& registered = registry.modules[index];
    const cli::ModuleHelp help = registered.module->help(route);
    if (help.section != section || (!help.usage && !help.description) || !moduleVisible(registered, route)) continue;
    printer.entry(registered.depth, {}, view(help.usage), view(help.description));
    printImplicationSummary(printer, registered);
    any = true;
  }
  return any;
}

void printExamples(const HelpPrinter& printer, std::span<const cli::HelpExample> examples) {
  assert(examples.size() <= cli::kMaxRouteHelpExamples);
  for (const cli::HelpExample& example : examples) {
    printer.entry(0, "arx-pistor ", view(example.arguments), view(example.description), true);
  }
}

void printModelTypes(const HelpPrinter& printer) {
  constexpr std::size_t kIndent = 15;
  std::FILE* output = printer.output();
  std::fputs("  Model types: ", output);
  std::size_t column = kIndent;
  bool first = true;
  for (std::string_view type : pistoris::paths::modelSelectorTypes()) {
    const std::size_t separator = first ? 0 : 2;
    if (!first && column + separator + type.size() > kLineWidth) {
      std::fputs(",\n               ", output);
      column = kIndent;
      first = true;
    }
    if (!first) {
      std::fputs(", ", output);
      column += 2;
    }
    std::fwrite(type.data(), 1, type.size(), output);
    column += type.size();
    first = false;
  }
  std::fputc('\n', output);
}

void printCliGeneral(const HelpPrinter& printer) {
  static constexpr cli::HelpExample kExamples[] = {
      {"--auto-mount level:1 level1.glb", "Export a mounted Level to an editable GLB."},
      {"--auto-mount level1.glb level:1", "Bake that GLB back into the game resource layout."},
      {"--help level", "Show Level conversion options."},
  };

  printer.breadcrumb({"CLI", "cli", {.bold = true}}, cli::HelpPage::kGeneral);
  printer.wrapped("Convert Arx Fatalis resources between native, JSON, OBJ, and GLB representations.");
  printer.blank();
  printer.heading("Usage");
  printer.line("  arx-pistor <inputs...> <output> [options]");
  printer.line("  arx-pistor --help [TOPIC [SUBTOPIC]]");
  printer.blank();
  printer.heading("Examples");
  printExamples(printer, kExamples);
  printer.blank();
  printer.heading("Help topics");
  printer.topic({"CLI", "cli", {.bold = true}}, " selectors", "Resource selector syntax and mount behavior.");
  printer.topic({"CLI", "cli", {.bold = true}}, " formats", "Formats accepted for each asset type.");
  cli::RouteRegistryView routes = cli::routeRegistry();
  for (std::size_t index = 0; index < routes.count; ++index) {
    const cli::RouteDescriptor& route = routes.routes[index];
    printer.topic(routePresentation(route.kind), {}, view(route.help.summary));
  }
  printer.topic(routePresentation(cli::RouteKind::kLevel), " debug", "Level diagnostic GLB outputs.");
  printer.wrapped("Topic names accept unambiguous prefixes. Use full names in scripts.", 2);
  printer.blank();
  printer.heading("Options");
  printModuleHelp(printer, nullptr, cli::HelpSection::kOptions);
}

void printCliSelectors(const HelpPrinter& printer) {
  constexpr std::size_t kDescriptionColumn = 40;
  printer.breadcrumb({"CLI", "cli", {.bold = true}}, cli::HelpPage::kSelectors);
  printer.wrapped("Selectors are convenient aliases for common paths in the mounted game resource namespace.");
  printer.blank();
  printer.heading("Selectors");
  printer.topic(routePresentation(cli::RouteKind::kLevel), ":<N>", "One numbered Level bundle.", kDescriptionColumn);
  printer.topic(
      routePresentation(cli::RouteKind::kModel), ":<type>:<name>[:<tweak>]", "One Model resource.", kDescriptionColumn);
  DomainPresentation animation = routePresentation(cli::RouteKind::kAnimation);
  animation.keyword = "anim";
  printer.topic(animation, ":<npc|fix_inter>:<name>", "One Animation resource.", kDescriptionColumn);
  printer.topic(routePresentation(cli::RouteKind::kAmbiance), ":<name>", "One Ambiance resource.", kDescriptionColumn);
  printer.topic(
      routePresentation(cli::RouteKind::kCinematic), ":<name>", "One Cinematic resource.", kDescriptionColumn);
  printModelTypes(printer);
  printer.blank();
  printer.wrapped(
      "--mount folders are searched from left to right. --auto-mount appends the standard game and "
      "unpacked folders. Without an explicit --mount, reads begin in the current directory.");
  printer.wrapped("--write-mount selects the output root. Absolute paths bypass mounts.");
}

void printCliFormats(const HelpPrinter& printer) {
  printer.breadcrumb({"CLI", "cli", {.bold = true}}, cli::HelpPage::kFormats);
  printer.wrapped("Format support is determined by the selected asset type.");

  cli::RouteRegistryView routes = cli::routeRegistry();
  for (std::size_t index = 0; index < routes.count; ++index) {
    const cli::RouteDescriptor& route = routes.routes[index];
    printer.blank();
    DomainPresentation domain = routePresentation(route.kind);
    cli::writeStyled(printer.output(), domain.style, domain.title);
    std::fputc('\n', printer.output());
    printer.formatLine("Primary inputs", route.primary_input_formats);
    if (route.extra_input_formats != cli::kNoFormats) printer.formatLine("Companion inputs", route.extra_input_formats);
    printer.formatLine("Outputs", route.output_formats);
  }
}

void printRouteGeneral(const HelpPrinter& printer, const cli::RouteDescriptor& route) {
  printer.breadcrumb(routePresentation(route.kind), cli::HelpPage::kGeneral);
  printer.wrapped(view(route.help.summary));
  printer.blank();
  printer.heading("Usage");
  printer.entry(0, "arx-pistor ", view(route.help.synopsis), {});
  printer.blank();
  printer.heading("Examples");
  printExamples(printer, route.help.examples);
  printer.blank();
  printer.heading("Formats");
  printer.formatLine("Primary inputs", route.primary_input_formats);
  if (route.extra_input_formats != cli::kNoFormats) printer.formatLine("Companion inputs", route.extra_input_formats);
  printer.formatLine("Outputs", route.output_formats);
  printer.blank();
  printer.heading("Options");
  printModuleHelp(printer, &route, cli::HelpSection::kOptions);
}

void printLevelDebug(const HelpPrinter& printer, const cli::RouteDescriptor& route) {
  printer.breadcrumb(routePresentation(route.kind), cli::HelpPage::kDebug);
  printer.wrapped("Export diagnostic GLB views while processing a Level.");
  printer.blank();
  printer.heading("Options");
  printModuleHelp(printer, &route, cli::HelpSection::kDebug);
}

}  // namespace

namespace cli {

void printHelp(std::FILE* output, const HelpRequest& request) {
  HelpPrinter printer(output);
  if (request.route) {
    if (request.page == HelpPage::kDebug && request.route->kind == RouteKind::kLevel) {
      printLevelDebug(printer, *request.route);
    } else {
      printRouteGeneral(printer, *request.route);
    }
    return;
  }

  switch (request.page) {
    case HelpPage::kSelectors:
      printCliSelectors(printer);
      return;
    case HelpPage::kFormats:
      printCliFormats(printer);
      return;
    case HelpPage::kGeneral:
    case HelpPage::kDebug:
      printCliGeneral(printer);
      return;
  }
}

}  // namespace cli
