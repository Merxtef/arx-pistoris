// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#pragma once

#include "modules/module.h"

#include <span>

namespace cli::modules::system {

const Module& helpModule();
const Module& versionModule();
const Module& kindModule();
const Module& logLevelModule();
const Module& overwriteModule();
const Module& noOverwriteModule();
const Module& dryRunModule();
const Module& keepFirstResourceModule();
const Module& mountModule();
const Module& autoMountModule();
const Module& writeMountModule();
const Module& resourceListingModule();
std::span<const ModuleRef> rootModules();

}  // namespace cli::modules::system
