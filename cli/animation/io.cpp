// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#include "animation/io.h"

#include "../io.h"

#include <cstdio>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace cli::animation {

static std::string_view textView(const std::vector<std::uint8_t>& buf) {
  return {reinterpret_cast<const char*>(buf.data()), buf.size()};
}

static bool outputFailure(const char* what, ArxReturnCode rc) {
  std::fprintf(stderr, "%s failed: %s (code %d)\n", what, pistoris::errorString(rc), static_cast<int>(rc));
  return false;
}

bool loadInput(const std::vector<std::uint8_t>& buf, cli::Route route, Context& ctx) {
  ArxReturnCode rc = ARX_OK;
  switch (route.input) {
    case cli::Format::kTea:
      rc = pistoris::readTea(buf, ctx.tea);
      break;
    case cli::Format::kJson:
      rc = pistoris::importJson(textView(buf), ctx.tea);
      break;
    default:
      std::fprintf(stderr, "Unsupported input format\n");
      return false;
  }
  if (rc != ARX_OK) {
    std::fprintf(stderr, "%s input failed: %s (code %d)\n", cli::formatName(route.input), pistoris::errorString(rc),
                 static_cast<int>(rc));
    return false;
  }
  return true;
}

bool saveOutput(const Context& ctx, const CliArgs& args, cli::State& state, const Invocation& inv, cli::Route route) {
  switch (route.output) {
    case cli::Format::kTea: {
      std::vector<std::uint8_t> out;
      ArxReturnCode rc = pistoris::writeTea(ctx.tea, out);
      if (rc != ARX_OK) return outputFailure("TEA output", rc);
      if (!writeFile(state, inv.output, out.data(), out.size())) return false;
      return true;
    }

    case cli::Format::kJson: {
      std::string out;
      ArxReturnCode rc = pistoris::exportJson(ctx.tea, out, args.pretty);
      if (rc != ARX_OK) return outputFailure("JSON output", rc);
      if (!writeFile(state, inv.output, out.data(), out.size())) return false;
      return true;
    }

    default:
      std::fprintf(stderr, "Unsupported output format\n");
      return false;
  }
}

}  // namespace cli::animation
