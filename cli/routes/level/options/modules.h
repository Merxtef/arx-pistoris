// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#pragma once

#include "modules/module.h"

#include <span>

namespace cli::level::options {

const Module& weldVerticesModule();
const Module& weldRadiusModule();
const Module& weldMetricModule();
const Module& weldDegenerateFacesModule();

const Module& flattenPortalsModule();
const Module& snapToPortalsModule();
const Module& portalSnapRadiusModule();

const Module& dlfOnlyModule();
const Module& noQuadReconstructionModule();
const Module& ftsSceneDirectoryModule();
const Module& signLevelModule();

const Module& generateNavigationModule();
const Module& generateNavSurfaceModule();
const Module& navFromFloorModule();
const Module& navRadiusModule();
const Module& navHeightModule();
const Module& navClearanceModule();
const Module& navMaxStepUpModule();
const Module& navMaxSlopeDegreesModule();
const Module& navIgnoreFlagsModule();
const Module& pruneNavSurfaceIslandsModule();
const Module& navPruneRatioModule();
const Module& navPruneMinAreaModule();

const Module& generateRoomDistancesModule();
const Module& roomDistanceSpacingModule();
const Module& roomDistanceOffsetModule();
const Module& roomDistanceHeightModule();
const Module& roomDistanceLinkDistanceModule();

const Module& generateAnchorsModule();
const Module& anchorSpacingModule();
const Module& anchorRadiusModule();
const Module& anchorHeightModule();
const Module& connectAnchorsModule();
const Module& anchorLinkDistanceModule();
const Module& anchorLinkRadiusScaleModule();
const Module& pruneAnchorIslandsModule();
const Module& anchorPruneRatioModule();
const Module& anchorPruneMinCountModule();

const Module& generateStaticLightingModule();
const Module& lightAmbientModule();
const Module& lightGlobalFactorModule();
const Module& lightNoNormalsModule();
const Module& lightNoShadowsModule();

const Module& generateMinimapModule();
const Module& minimapBorderColorModule();
const Module& minimapForegroundColorModule();
const Module& minimapForegroundImageModule();
const Module& minimapBackgroundColorModule();
const Module& minimapBackgroundImageModule();
const Module& minimapWaterColorModule();
const Module& minimapWaterImageModule();
const Module& minimapLavaColorModule();
const Module& minimapLavaImageModule();
const Module& minimapHaloColorModule();
const Module& minimapHaloRadiusModule();

const Module& debugCellsModule();
const Module& debugNavigationModule();
const Module& debugRoomDistancesModule();
const Module& loadPreviewsModule();

std::span<const ModuleRef> rootModules();

}  // namespace cli::level::options
