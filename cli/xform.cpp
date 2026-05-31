// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#include "xform.h"

namespace cli {

pistoris::AffineXform makeAffineXform(const CliArgs& args) {
  return pistoris::makeAffineXform(args.rotate[0], args.rotate[1], args.rotate[2], args.scale[0], args.scale[1],
                                   args.scale[2], args.offset[0], args.offset[1], args.offset[2]);
}

}  // namespace cli
