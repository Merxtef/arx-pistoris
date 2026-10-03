// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#pragma once

#include "conversion/options.h"
#include "routes/options.h"

namespace cli::cinematic {

struct CinematicOptions final : RouteOptions {
  DirectoryRebaseRequest effects;
  DirectoryRebaseRequest speech;
};

}  // namespace cli::cinematic
