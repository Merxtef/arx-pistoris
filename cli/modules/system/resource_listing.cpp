// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#include "console/diagnostics.h"
#include "modules/module.h"
#include "modules/system/modules.h"
#include "pipeline/options.h"
#include "resources/discovery.h"

#include <cstddef>
#include <span>
#include <string_view>

namespace cli::modules::system {
namespace {

char lowerAscii(char value) {
  if (value >= 'A' && value <= 'Z') return static_cast<char>(value - 'A' + 'a');
  return value;
}

bool equalAsciiInsensitive(std::string_view lhs, std::string_view rhs) {
  if (lhs.size() != rhs.size()) return false;
  for (std::size_t i = 0; i < lhs.size(); ++i)
    if (lowerAscii(lhs[i]) != lowerAscii(rhs[i])) return false;
  return true;
}

ResourceListingKind listingKind(std::string_view value) {
  if (equalAsciiInsensitive(value, "level")) return ResourceListingKind::kLevel;
  if (equalAsciiInsensitive(value, "model")) return ResourceListingKind::kModel;
  if (equalAsciiInsensitive(value, "animation")) return ResourceListingKind::kAnimation;
  if (equalAsciiInsensitive(value, "cinematic")) return ResourceListingKind::kCinematic;
  if (equalAsciiInsensitive(value, "ambiance")) return ResourceListingKind::kAmbiance;
  if (equalAsciiInsensitive(value, "all")) return ResourceListingKind::kAll;
  return ResourceListingKind::kNone;
}

class ResourceListingModule final : public TerminalActionModule {
 public:
  std::span<const char* const> keywords() const noexcept override {
    static constexpr const char* kKeywords[] = {"--list-resources"};
    return kKeywords;
  }

  ModuleHelp help(const RouteDescriptor*) const noexcept override {
    return {HelpSection::kOptions,
            "--list-resources <level|model|animation|cinematic|ambiance|all>",
            "List mounted game resources as canonical selectors."};
  }

  ModuleParseResult parse(ModuleParseContext& ctx) const override {
    if (ctx.index + 1 >= ctx.argc) {
      diagnostic(DiagnosticCode::kMissingArgument, "--list-resources: expected a resource kind");
      return {.ok = false};
    }
    const char* value = ctx.argv[++ctx.index];
    ResourceListingKind requested = listingKind(value ? value : "");
    if (requested == ResourceListingKind::kNone) {
      diagnostic(DiagnosticCode::kInvalidMode,
                 "--list-resources: expected level, model, animation, cinematic, ambiance, or all");
      return {.ok = false};
    }
    ctx.options.resource_listing = requested;
    return {};
  }

  int execute(const ParsedOptions& options, IoService& io) const override {
    return printResourceListing(options.resource_listing, io) ? 0 : 1;
  }
};

}  // namespace

const Module& resourceListingModule() { return moduleInstance<ResourceListingModule>(); }

}  // namespace cli::modules::system
