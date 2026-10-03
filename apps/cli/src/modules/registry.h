// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#pragma once

#include "modules/module.h"

#include <cstddef>

namespace cli {

struct RouteDescriptor;

struct OptionMatch {
  const Module* module = nullptr;
  bool ambiguous = false;
};

struct RegisteredModule {
  const Module* module = nullptr;
  const Module* parent = nullptr;
  const RouteDescriptor* owner = nullptr;
  std::uint8_t depth = 0;
};

struct ModuleRegistryView {
  const RegisteredModule* modules = nullptr;
  std::size_t count = 0;
};

ModuleRegistryView moduleRegistry();
const char* moduleRegistryError();
bool validateModuleRegistry();
const RegisteredModule* registeredModule(const Module& module);
const Module* moduleParent(const Module& module);
bool moduleListContains(std::span<const ModuleRef> modules, const Module& module);
RouteMask routesSupportingModule(const Module& module);
OptionMatch resolveModule(const char* token);
void printModuleSuggestions(const char* token);

}  // namespace cli
