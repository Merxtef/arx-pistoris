// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#include "modules/module.h"

#include <span>

namespace cli {

const char* Module::stableName() const noexcept {
  std::span<const char* const> names = keywords();
  return names.empty() ? nullptr : names.front();
}

bool Module::repeatable() const noexcept { return false; }

std::span<const ModuleRef> Module::children() const noexcept { return {}; }

std::span<const ModuleRef> Module::dependencies() const noexcept { return {}; }

std::span<const ModuleRef> Module::incompatibleWith() const noexcept { return {}; }

std::span<const ModuleImplication> Module::implications() const noexcept { return {}; }

bool Module::validate(const ModuleValidationContext&) const { return true; }

}  // namespace cli
