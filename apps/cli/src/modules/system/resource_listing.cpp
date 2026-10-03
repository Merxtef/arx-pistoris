// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#include "base/unique_prefix.h"
#include "console/diagnostics.h"
#include "modules/module.h"
#include "modules/system/modules.h"
#include "pipeline/options.h"
#include "resources/discovery.h"

#include <span>
#include <string_view>

namespace cli::modules::system {
namespace {

bool parseListingKind(const char* value, ResourceListingKind& out) {
  struct Candidate {
    std::string_view name;
    ResourceListingKind kind;
  };
  static constexpr Candidate kCandidates[] = {
      {"level", ResourceListingKind::kLevel},
      {"model", ResourceListingKind::kModel},
      {"animation", ResourceListingKind::kAnimation},
      {"cinematic", ResourceListingKind::kCinematic},
      {"ambiance", ResourceListingKind::kAmbiance},
      {"all", ResourceListingKind::kAll},
  };

  const auto match = resolveUniquePrefix(
      value ? std::string_view(value) : std::string_view(),
      std::span<const Candidate>{kCandidates},
      [](const Candidate& candidate) { return candidate.name; },
      PrefixCase::kAsciiInsensitive);
  if (match.status == UniquePrefixStatus::kAmbiguous) {
    diagnostic(DiagnosticCode::kInvalidMode, "--list-resources: ambiguous resource kind '%s'", value);
    return false;
  }
  if (match.status == UniquePrefixStatus::kNone) {
    diagnostic(DiagnosticCode::kInvalidMode,
               "--list-resources: expected level, model, animation, cinematic, ambiance, or all");
    return false;
  }
  out = match.value->kind;
  return true;
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
    ResourceListingKind requested = ResourceListingKind::kNone;
    if (!parseListingKind(value, requested)) return {.ok = false};
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
