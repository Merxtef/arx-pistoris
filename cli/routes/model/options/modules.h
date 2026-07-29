// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#pragma once

#include "modules/module.h"

#include <span>

namespace cli::model::options {

const Module& overwriteTextureModule();
const Module& renameSelectionsModule();
const Module& ftlReferenceModule();
const Module& autosizeToReferenceModule();
const Module& snapBoneOriginsModule();
const Module& snapActionPointsModule();
const Module& copySyntheticSelectionAffiliationsModule();
std::span<const ModuleRef> rootModules();

}  // namespace cli::model::options
