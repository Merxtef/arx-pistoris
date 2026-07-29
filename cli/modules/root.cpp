// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#include "modules/root.h"

#include "modules/format/modules.h"
#include "modules/module.h"
#include "modules/output/modules.h"
#include "modules/system/modules.h"
#include "modules/transform/modules.h"

#include <span>
#include <vector>

namespace cli::modules {

std::span<const ModuleRef> rootModules() {
  static const std::vector<ModuleRef> kModules = [] {
    std::vector<ModuleRef> result;
    for (std::span<const ModuleRef> group :
         {system::rootModules(), format::rootModules(), output::rootModules(), transform::rootModules()}) {
      result.insert(result.end(), group.begin(), group.end());
    }
    return result;
  }();
  return kModules;
}

}  // namespace cli::modules
