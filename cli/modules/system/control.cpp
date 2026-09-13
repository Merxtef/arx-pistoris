// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#include "arx_pistoris/runtime/types.h"

#include "base/unique_prefix.h"
#include "console/diagnostics.h"
#include "console/help_request.h"
#include "modules/module.h"
#include "modules/system/modules.h"
#include "pipeline/options.h"
#include "routes/registry.h"
#include "routes/types.h"

#include <cstddef>
#include <span>
#include <string>
#include <string_view>

namespace cli::modules::system {
namespace {

bool parseKind(const char* value, RouteKind& out) {
  std::string_view needle = value ? std::string_view(value) : std::string_view();
  RouteRegistryView routes = routeRegistry();
  const auto match = resolveUniquePrefix(needle,
                                         std::span<const RouteDescriptor>{routes.routes, routes.count},
                                         [](const RouteDescriptor& candidate) { return candidate.name; });
  if (match.status == UniquePrefixStatus::kAmbiguous) {
    diagnostic(DiagnosticCode::kKindAmbiguous, "--kind: ambiguous value '%s'", value);
    return false;
  }
  if (match.status == UniquePrefixStatus::kNone) {
    diagnostic(DiagnosticCode::kKindInvalid, "--kind: unknown route '%s'", value ? value : "");
    return false;
  }
  out = match.value->kind;
  return true;
}

const std::string& kindUsage() {
  static const std::string kUsage = [] {
    std::string result = "--kind <";
    RouteRegistryView routes = routeRegistry();
    for (std::size_t index = 0; index < routes.count; ++index) {
      if (index != 0) result.push_back('|');
      result += routes.routes[index].name;
    }
    result.push_back('>');
    return result;
  }();
  return kUsage;
}

bool parseLogLevel(const char* value, ArxLogLevel& out) {
  struct Candidate {
    const char* name;
    ArxLogLevel level;
  };
  static constexpr Candidate kCandidates[] = {
      {"debug", ARX_LOG_DEBUG},
      {"info", ARX_LOG_INFO},
      {"warn", ARX_LOG_WARN},
  };

  const auto match = resolveUniquePrefix(
      value ? std::string_view(value) : std::string_view(),
      std::span<const Candidate>{kCandidates},
      [](const Candidate& candidate) { return candidate.name; },
      PrefixCase::kAsciiInsensitive);
  if (match.status == UniquePrefixStatus::kAmbiguous) {
    diagnostic(DiagnosticCode::kLogLevelAmbiguous, "--log-level: ambiguous value '%s'", value);
    return false;
  }
  if (match.status == UniquePrefixStatus::kNone) {
    diagnostic(
        DiagnosticCode::kLogLevelInvalid, "--log-level: expected DEBUG, INFO, or WARN, got '%s'", value ? value : "");
    return false;
  }
  out = match.value->level;
  return true;
}

class HelpModule final : public SystemModule {
 public:
  std::span<const char* const> keywords() const noexcept override {
    static constexpr const char* kKeywords[] = {"--help", "-h"};
    return kKeywords;
  }

  ModuleHelp help(const RouteDescriptor*) const noexcept override {
    return {HelpSection::kOptions, "-h, --help [TOPIC [SUBTOPIC]]", "Print help."};
  }

  ModuleParseResult parse(ModuleParseContext& ctx) const override {
    HelpRequest request;
    std::span<const char* const> arguments(ctx.argv + ctx.index + 1,
                                           static_cast<std::size_t>(ctx.argc - ctx.index - 1));
    if (!resolveHelpRequest(arguments, request)) return {.ok = false};
    ctx.options.help = request;
    return {.stop = true};
  }
};

class VersionModule final : public SystemModule {
 public:
  std::span<const char* const> keywords() const noexcept override {
    static constexpr const char* kKeywords[] = {"--version", "-v"};
    return kKeywords;
  }

  ModuleHelp help(const RouteDescriptor*) const noexcept override {
    return {HelpSection::kOptions, "-v, --version", "Print version."};
  }

  ModuleParseResult parse(ModuleParseContext& ctx) const override {
    ctx.options.version = true;
    return {.stop = true};
  }
};

class KindModule final : public SystemModule {
 public:
  std::span<const char* const> keywords() const noexcept override {
    static constexpr const char* kKeywords[] = {"--kind"};
    return kKeywords;
  }

  ModuleHelp help(const RouteDescriptor*) const override {
    return {HelpSection::kOptions, kindUsage().c_str(), "Override the asset kind for this operation."};
  }

  ModuleParseResult parse(ModuleParseContext& ctx) const override {
    if (ctx.index + 1 >= ctx.argc) {
      diagnostic(DiagnosticCode::kMissingArgument, "--kind: expected a route name");
      return {.ok = false};
    }
    RouteKind parsed = RouteKind::kUnknown;
    if (!parseKind(ctx.argv[++ctx.index], parsed)) return {.ok = false};
    if (ctx.options.kind_override != RouteKind::kUnknown && ctx.options.kind_override != parsed) {
      diagnostic(DiagnosticCode::kKindConflict, "--kind specified multiple conflicting values");
      return {.ok = false};
    }
    ctx.options.kind_override = parsed;
    return {};
  }
};

class LogLevelModule final : public SystemModule {
 public:
  std::span<const char* const> keywords() const noexcept override {
    static constexpr const char* kKeywords[] = {"--log-level"};
    return kKeywords;
  }

  ModuleHelp help(const RouteDescriptor*) const noexcept override {
    return {HelpSection::kOptions, "--log-level <LEVEL=INFO>", "Set minimum log severity to DEBUG, INFO, or WARN."};
  }

  ModuleParseResult parse(ModuleParseContext& ctx) const override {
    if (ctx.index + 1 >= ctx.argc) {
      diagnostic(DiagnosticCode::kMissingArgument, "--log-level: expected DEBUG, INFO, or WARN");
      return {.ok = false};
    }
    ArxLogLevel parsed = ARX_LOG_INFO;
    if (!parseLogLevel(ctx.argv[++ctx.index], parsed)) return {.ok = false};
    ctx.options.log_level = parsed;
    return {};
  }
};

}  // namespace

const Module& helpModule() { return moduleInstance<HelpModule>(); }

const Module& versionModule() { return moduleInstance<VersionModule>(); }

const Module& kindModule() { return moduleInstance<KindModule>(); }

const Module& logLevelModule() { return moduleInstance<LogLevelModule>(); }

}  // namespace cli::modules::system
