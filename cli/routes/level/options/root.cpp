// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#include "modules/module.h"
#include "routes/level/options/modules.h"

#include <span>

namespace cli::level::options {

std::span<const ModuleRef> rootModules() {
  static constexpr ModuleRef kModules[] = {
      weldVerticesModule,
      dlfOnlyModule,
      noQuadReconstructionModule,
      skipTextureExportModule,
      outputTextureFolderModule,
      inputTextureFolderModule,
      ftsSceneDirectoryModule,
      signLevelModule,
      generateNavSurfaceModule,
      pruneNavSurfaceIslandsModule,
      generateRoomDistancesModule,
      generateAnchorsModule,
      connectAnchorsModule,
      pruneAnchorIslandsModule,
      generateStaticLightingModule,
      debugCellsModule,
      debugNavigationModule,
      debugRoomDistancesModule,
  };
  return kModules;
}

}  // namespace cli::level::options
