// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#include "modules/module.h"
#include "routes/cinematic/options/modules.h"

#include <span>

namespace cli::cinematic::options {

std::span<const ModuleRef> rootModules() {
  static constexpr ModuleRef kModules[] = {rebaseSfxModule, rebaseSpeechModule};
  return kModules;
}

}  // namespace cli::cinematic::options
