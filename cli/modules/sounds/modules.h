// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#pragma once

#include "modules/module.h"

#include <span>

namespace cli::modules::sounds {

const Module& skipSoundExportModule();
const Module& inputSoundFolderModule();
std::span<const ModuleRef> rootModules();

}  // namespace cli::modules::sounds
