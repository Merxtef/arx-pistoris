// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#include "app.h"

#include "arx_pistoris/runtime.hpp"

#include "console/diagnostics.h"
#include "console/help.h"
#include "console/logging.h"
#include "io/service.h"
#include "modules/implication.h"
#include "modules/module.h"
#include "modules/registry.h"
#include "modules/validation.h"
#include "pipeline/executor.h"
#include "pipeline/options.h"
#include "pipeline/parsed.h"
#include "pipeline/parser.h"
#include "pipeline/resolver.h"

#include <cstdio>
#include <exception>
#include <span>
#include <utility>
#include <vector>

namespace {

const cli::TerminalActionModule* terminalAction(std::span<const cli::ModuleInvocation> modules) {
  for (const cli::ModuleInvocation& invocation : modules) {
    if (!invocation.module || invocation.module->category() != cli::ModuleCategory::kTerminalAction) continue;
    return static_cast<const cli::TerminalActionModule*>(invocation.module);
  }
  return nullptr;
}

int dispatch(const cli::ParsedCli& parsed, std::span<const cli::ModuleInvocation> effective_modules) {
  const cli::ParsedOptions& options = parsed.options;
  cli::IoService io{options.overwrite, options.dry_run, options.read_mounts, options.write_mount, options.auto_mount};
  if (!io.valid()) return 1;
  if (const cli::TerminalActionModule* terminal = terminalAction(effective_modules)) {
    return terminal->execute(options, io);
  }
  cli::CliResolution resolved;
  if (!cli::resolveCli(parsed, effective_modules, io, resolved)) return 1;
  return cli::executeResolved(parsed, resolved, io);
}

}  // namespace

namespace cli {

int runCli(int argc, char* argv[]) {
  try {
    pistoris::setLogCallback(pistorisLog, nullptr);
    if (!validateModuleRegistry()) return 1;

    ParsedCli parsed;
    if (!parseArgs(argc, argv, parsed)) {
      std::fputs("Run arx-pistor --help for usage.\n", stderr);
      return 1;
    }
    if (parsed.options.help) {
      printHelp(stdout, *parsed.options.help);
      return 0;
    }

    if (parsed.options.version) {
      std::printf("arx-pistor %s\n", pistoris::version());
      return 0;
    }

    ParsedOptions effective_options;
    std::vector<ModuleInvocation> effective_modules;
    if (!resolveEffectiveModules(parsed.explicit_modules, effective_options, effective_modules)) return 1;
    if (!validateModules(effective_modules) || !validateModuleValues(effective_modules, effective_options)) return 1;

    parsed.options = std::move(effective_options);
    setLogLevel(parsed.options.log_level);
    return dispatch(parsed, effective_modules);
  } catch (const std::exception& e) {
    diagnostic(DiagnosticCode::kUnhandledException, "Unhandled exception: %s", e.what());
    return 1;
  } catch (...) {
    diagnostic(DiagnosticCode::kUnhandledException, "Unhandled non-standard exception");
    return 1;
  }
}

}  // namespace cli
