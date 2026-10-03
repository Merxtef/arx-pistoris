// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#include "modules/module.h"
#include "routes/model/options/modules.h"

#include <span>

namespace cli::model::options {

std::span<const ModuleRef> rootModules() {
  static constexpr ModuleRef kModules[] = {
      ftlReferenceModule,
      inferBoneSelectionsModule,
      asLevelPreviewModule,
      allowEmptyAnimationModule,
      inputIconModule,
      iconSlotsModule,
      iconLayoutModule,
  };
  return kModules;
}

}  // namespace cli::model::options
