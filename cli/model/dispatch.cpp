// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#include "model/dispatch.h"

#include "../io.h"
#include "inspect.h"
#include "model/io.h"
#include "model/modules.h"

#include <cstdio>
#include <vector>

namespace cli::model {
namespace {

bool isExtraPath(const char* path) {
  cli::Format format = cli::formatFromPath(path);
  if (format == cli::Format::kTea) return true;
  if (format != cli::Format::kJson) return false;

  std::vector<std::uint8_t> buf;
  return readFileOptional(path, buf) && cli::detectRoute(buf, path).kind == cli::RouteKind::kAnimation;
}

bool validateOutputFormat(cli::Format output) {
  switch (output) {
    case cli::Format::kFtl:
    case cli::Format::kObj:
    case cli::Format::kJson:
    case cli::Format::kGlb:
      return true;
    case cli::Format::kTea:
      std::fprintf(stderr, "Model route does not support TEA output\n");
      return false;
    case cli::Format::kUnset:
      std::fprintf(stderr, "Internal error: output format is not set\n");
      return false;
    case cli::Format::kUnknown:
      std::fprintf(stderr, "Unsupported output format\n");
      return false;
  }
  return false;
}

bool validateOptions(const CliArgs& args, cli::Route route) {
  if (args.pretty && route.output != cli::Format::kJson) {
    std::fprintf(stderr, "--pretty is only applicable to model JSON output\n");
    return false;
  }
  return true;
}

bool bindInvocation(const CliArgs& args, cli::Format primary_format, Invocation& inv) {
  if (args.positionals.empty()) return false;

  inv.input = args.positionals[0];

  if (primary_format == cli::Format::kTea) {
    std::fprintf(stderr, "Internal error: standalone TEA was routed to model path\n");
    return false;
  }

  bool output_seen = false;
  for (std::size_t i = 1; i < args.positionals.size(); ++i) {
    const char* path = args.positionals[i];
    if (!output_seen && isExtraPath(path)) {
      inv.extras.push_back(path);
      continue;
    }

    if (!output_seen) {
      inv.output  = path;
      output_seen = true;
      continue;
    }

    std::fprintf(stderr, "Unexpected positional after output: %s\n", path);
    return false;
  }

  return true;
}

}  // namespace

int dispatch(const CliArgs& args, cli::State& state, const std::vector<std::uint8_t>& buf, cli::Route route) {
  Invocation inv;
  if (!bindInvocation(args, route.input, inv)) return 1;

  if (route.input == cli::Format::kUnknown) {
    std::fprintf(stderr, "Unsupported input format: %s\n", inv.input);
    return 1;
  }
  if (inv.output) {
    route.output = cli::formatFromPath(inv.output);
    if (!validateOutputFormat(route.output)) return 1;
  }
  if (!validateOptions(args, route)) return 1;

  Context ctx;
  if (!loadInput(buf, inv, route, ctx)) return 1;

  if (!loadExtras(inv, ctx)) return 1;

  if (args.reference_ftl) {
    if (!loadReferenceFtl(args.reference_ftl, ctx)) return 1;
  }

  if (!applyModules(ctx, args)) return 1;

  if (!inv.output) return inspectContext(ctx);

  return saveOutput(ctx, args, state, inv, route) ? 0 : 1;
}

}  // namespace cli::model
