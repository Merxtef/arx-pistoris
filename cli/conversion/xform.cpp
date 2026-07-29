// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#include "conversion/xform.h"

#include "arx_pistoris/arx_math.hpp"
#include "arx_pistoris/pistoris.hpp"

#include "conversion/options.h"

namespace cli {

pistoris::AffineXform makeAffineXform(const SharedConversionOptions& options) {
  return pistoris::makeAffineXform(options.rotate[0],
                                   options.rotate[1],
                                   options.rotate[2],
                                   options.scale[0],
                                   options.scale[1],
                                   options.scale[2],
                                   options.offset[0],
                                   options.offset[1],
                                   options.offset[2]);
}

}  // namespace cli
