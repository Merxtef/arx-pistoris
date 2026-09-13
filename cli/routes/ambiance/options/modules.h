// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#pragma once

#include "modules/module.h"

#include <span>

namespace cli::ambiance::options {

const Module& trimTracksToMasterModule();
const Module& referenceModelModule();
std::span<const ModuleRef> rootModules();

}  // namespace cli::ambiance::options
