// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#pragma once

#include "modules/module.h"

#include <span>

namespace cli::modules::transform {

const Module& rotateModule();
const Module& scaleModule();
const Module& offsetModule();
std::span<const ModuleRef> rootModules();

}  // namespace cli::modules::transform
