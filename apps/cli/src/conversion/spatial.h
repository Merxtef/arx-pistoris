// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#pragma once

#include "arx_pistoris/base/math.h"

#include "conversion/options.h"

namespace cli {

pistoris::ArxQuat rotationQuaternion(const SharedConversionOptions& options) noexcept;

}  // namespace cli
