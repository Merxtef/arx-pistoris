// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#include "modules/module.h"
#include "routes/level/options/modules.h"

#include <span>

namespace cli::level::options {

std::span<const ModuleRef> rootModules() {
  static constexpr ModuleRef kModules[] = {
      weldVerticesModule,
      flattenPortalsModule,
      snapToPortalsModule,
      dlfOnlyModule,
      noQuadReconstructionModule,
      ftsSceneDirectoryModule,
      signLevelModule,
      generateNavigationModule,
      generateNavSurfaceModule,
      pruneNavSurfaceIslandsModule,
      generateRoomDistancesModule,
      generateAnchorsModule,
      connectAnchorsModule,
      pruneAnchorIslandsModule,
      generateStaticLightingModule,
      generateMinimapModule,
      minimapBorderColorModule,
      loadPreviewsModule,
      debugCellsModule,
      debugNavigationModule,
      debugRoomDistancesModule,
  };
  return kModules;
}

}  // namespace cli::level::options
