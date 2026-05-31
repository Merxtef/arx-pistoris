// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#pragma once

#include "arx_pistoris/arx_math.hpp"
#include "arx_pistoris/ftl_data.hpp"
#include "arx_pistoris/pistoris_types.h"
#include "arx_pistoris/tea_data.hpp"

#include <string_view>

namespace pistoris::api {

ArxReturnCode overwriteTexturePaths(ftl::Data& ftl, std::string_view path, std::string_view log_prefix);

ArxReturnCode applyTransform(ftl::Data& ftl, const AffineXform& xform);

ArxReturnCode applyTransform(tea::Data& tea, const AffineXform& xform);

ArxReturnCode snapFtlBoneOriginsToReference(ftl::Data& target, const ftl::Data& reference);

ArxReturnCode snapFtlActionPointsToReference(ftl::Data& target, const ftl::Data& reference);

ArxReturnCode copyFtlSyntheticSelectionAffiliations(ftl::Data& target, const ftl::Data& reference);

}  // namespace pistoris::api
