// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#pragma once

#include "modules/module.h"

#include <span>

namespace cli::model::options {

const Module& ftlReferenceModule();
const Module& snapBoneOriginsModule();
const Module& copyBoneSelectionsModule();
const Module& copyActionSelectionsModule();
const Module& inferBoneSelectionsModule();
const Module& asLevelPreviewModule();
const Module& previewClassPathModule();
const Module& allowEmptyAnimationModule();
const Module& inputIconModule();
const Module& iconSlotsModule();
const Module& iconLayoutModule();
std::span<const ModuleRef> rootModules();

}  // namespace cli::model::options
