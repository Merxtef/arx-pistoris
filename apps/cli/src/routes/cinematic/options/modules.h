// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#pragma once

#include "modules/module.h"

#include <span>

namespace cli::cinematic::options {

const Module& rebaseSfxModule();
const Module& rebaseSpeechModule();
std::span<const ModuleRef> rootModules();

}  // namespace cli::cinematic::options
