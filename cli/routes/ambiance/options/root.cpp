// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#include "modules/module.h"
#include "routes/ambiance/options/modules.h"

#include <span>

namespace cli::ambiance::options {

std::span<const ModuleRef> rootModules() {
  static constexpr ModuleRef kModules[] = {trimTracksToMasterModule, referenceModelModule};
  return kModules;
}

}  // namespace cli::ambiance::options
