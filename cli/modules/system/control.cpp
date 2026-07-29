// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#include "arx_pistoris/pistoris_types.h"

#include "console/diagnostics.h"
#include "modules/module.h"
#include "modules/system/modules.h"
#include "pipeline/options.h"
#include "routes/registry.h"
#include "routes/types.h"

#include <cctype>
#include <cstddef>
#include <span>
#include <string>
#include <string_view>

namespace cli::modules::system {
namespace {

bool startsWith(std::string_view text, std::string_view prefix) {
  return text.size() >= prefix.size() && text.substr(0, prefix.size()) == prefix;
}

std::string asciiLower(std::string_view value) {
  std::string out;
  out.reserve(value.size());
  for (unsigned char c : value) out.push_back(static_cast<char>(std::tolower(c)));
  return out;
}

bool parseKind(const char* value, RouteKind& out) {
  const RouteDescriptor* unique = nullptr;
  std::string_view needle = value ? std::string_view(value) : std::string_view();
  RouteRegistryView routes = routeRegistry();
  for (std::size_t index = 0; index < routes.count; ++index) {
    const RouteDescriptor& candidate = routes.routes[index];
    std::string_view name = candidate.name;
    if (name == needle) {
      out = candidate.kind;
      return true;
    }
    if (!startsWith(name, needle)) continue;
    if (unique) {
      diagnostic(DiagnosticCode::kKindAmbiguous, "--kind: ambiguous value '%s'", value);
      return false;
    }
    unique = &candidate;
  }
  if (!unique) {
    diagnostic(DiagnosticCode::kKindInvalid, "--kind: unknown route '%s'", value ? value : "");
    return false;
  }
  out = unique->kind;
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

  const Candidate* unique = nullptr;
  std::string needle = asciiLower(value ? std::string_view(value) : std::string_view());
  for (const Candidate& candidate : kCandidates) {
    std::string_view name = candidate.name;
    if (name == needle) {
      out = candidate.level;
      return true;
    }
    if (!startsWith(name, needle)) continue;
    if (unique) {
      diagnostic(DiagnosticCode::kLogLevelAmbiguous, "--log-level: ambiguous value '%s'", value);
      return false;
    }
    unique = &candidate;
  }
  if (!unique) {
    diagnostic(
        DiagnosticCode::kLogLevelInvalid, "--log-level: expected DEBUG, INFO, or WARN, got '%s'", value ? value : "");
    return false;
  }
  out = unique->level;
  return true;
}

class HelpModule final : public SystemModule {
 public:
  std::span<const char* const> keywords() const noexcept override {
    static constexpr const char* kKeywords[] = {"--help", "-h"};
    return kKeywords;
  }

  ModuleHelp help(const RouteDescriptor*) const noexcept override {
    return {HelpSection::kOptions, "-h, --help [topics...]", "Print help."};
  }

  ModuleParseResult parse(ModuleParseContext& ctx) const override {
    ctx.options.help = true;
    for (int index = ctx.index + 1; index < ctx.argc && !(ctx.argv[index][0] == '-' && ctx.argv[index][1] != '\0');
         ++index) {
      ctx.options.help_topics.push_back(ctx.argv[index]);
    }
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
