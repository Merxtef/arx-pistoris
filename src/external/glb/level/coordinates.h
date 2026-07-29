// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#pragma once

#include "arx_pistoris/level.hpp"
#include "arx_pistoris/pistoris_types.h"

#include <optional>

namespace pistoris {
struct LevelModules;
}

namespace pistoris::glb {
class Builder;
}

namespace pistoris::glb_level {

struct ImportUnits {
  double arx_per_glb_unit = 100.0;
};

ArxReturnCode validateGlbImportOptions(const Level::GlbImportOptions& options);
std::optional<ArxVector3> toArxPoint(const ArxVector3& value, const ImportUnits& units) noexcept;
std::optional<ArxVector3> toArxVector(const ArxVector3& value, const ImportUnits& units) noexcept;
ArxQuat toArxRotation(const ArxQuat& value) noexcept;
std::optional<float> toArxLength(float value, const ImportUnits& units) noexcept;
ArxReturnCode applyGlbImportPlacement(LevelModules& level, const Level::GlbImportOptions& options,
                                      Level::GlbImportInfo& info);
ArxReturnCode configureGlbExportCoordinates(glb::Builder& builder, const Level::GlbExportOptions& options);
float toGlbLength(float value, const Level::GlbExportOptions& options);

}  // namespace pistoris::glb_level
