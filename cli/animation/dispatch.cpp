// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#include "animation/dispatch.h"

#include "animation/io.h"
#include "animation/modules.h"
#include "inspect.h"

#include <cstdio>

namespace cli::animation {
namespace {

bool validateOutputFormat(cli::Format output) {
  switch (output) {
    case cli::Format::kTea:
    case cli::Format::kJson:
      return true;
    case cli::Format::kUnset:
      std::fprintf(stderr, "Internal error: output format is not set\n");
      return false;
    case cli::Format::kFtl:
    case cli::Format::kObj:
    case cli::Format::kGlb:
      std::fprintf(stderr, "Animation route supports TEA and JSON output only\n");
      return false;
    case cli::Format::kUnknown:
      std::fprintf(stderr, "Unsupported output format\n");
      return false;
  }
  return false;
}

bool validateOptions(const CliArgs& args) {
  if (args.rename_selections) {
    std::fprintf(stderr, "--rename-selections is only applicable to model routes\n");
    return false;
  }
  if (args.reference_ftl || args.autosize_to_reference ||
      args.bone_origin_reference_mode != BoneOriginReferenceMode::kNone || args.snap_action_points ||
      args.copy_reference_affiliations) {
    std::fprintf(stderr, "reference FTL repair options are only applicable to model routes\n");
    return false;
  }
  if (args.pretty) {
    if (args.positionals.size() < 2 || cli::formatFromPath(args.positionals[1]) != cli::Format::kJson) {
      std::fprintf(stderr, "--pretty is only applicable to JSON output\n");
      return false;
    }
  }
  return true;
}

bool bindInvocation(const CliArgs& args, Invocation& inv) {
  if (args.positionals.empty()) return false;

  inv.input = args.positionals[0];
  if (args.positionals.size() >= 2) inv.output = args.positionals[1];
  if (args.positionals.size() > 2) {
    std::fprintf(stderr, "Multiple standalone TEA inputs are not currently supported\n");
    return false;
  }
  return true;
}

}  // namespace

int dispatch(const CliArgs& args, cli::State& state, const std::vector<std::uint8_t>& buf, cli::Route route) {
  Invocation inv;
  if (!bindInvocation(args, inv)) return 1;

  if (route.input == cli::Format::kUnknown) {
    std::fprintf(stderr, "Unsupported input format: %s\n", inv.input);
    return 1;
  }
  if (inv.output) {
    route.output = cli::formatFromPath(inv.output);
    if (!validateOutputFormat(route.output)) return 1;
  }
  if (!validateOptions(args)) return 1;

  Context ctx;
  if (!loadInput(buf, route, ctx)) return 1;

  if (!validateModules(args)) return 1;

  if (!applyModules(ctx, args)) return 1;

  if (!inv.output) return inspectContext(ctx);

  return saveOutput(ctx, args, state, inv, route) ? 0 : 1;
}

}  // namespace cli::animation
