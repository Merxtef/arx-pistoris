// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#include "animation/modules.h"

#include "xform.h"

#include <cstdio>

namespace cli::animation {

bool validateModules(const CliArgs& args) {
  if (args.overwrite_texture) {
    std::fprintf(stderr, "Texture overwrite is not applicable to standalone TEA animation data\n");
    return false;
  }
  return true;
}

bool applyModules(Context& ctx, const CliArgs& args) {
  if (!args.has_xform) return true;

  pistoris::AffineXform xform = cli::makeAffineXform(args);
  ArxReturnCode rc            = pistoris::applyTransform(ctx.tea, xform);
  if (rc != ARX_OK) {
    std::fprintf(stderr, "Animation transform failed: %s (code %d)\n", pistoris::errorString(rc), static_cast<int>(rc));
    return false;
  }
  return true;
}

}  // namespace cli::animation
