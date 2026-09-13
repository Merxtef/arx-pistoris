// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#pragma once

#include "arx_pistoris/base/status.h"
#include "arx_pistoris/level.hpp"

#include <optional>

namespace pistoris {
struct LevelModules;
}

namespace pistoris::glb {
class Builder;
}

namespace pistoris::glb_level {

inline constexpr ArxQuat kLevelGlbBasisRotation = {0.0f, 1.0f, 0.0f, 0.0f};

struct ImportUnits {
  double arx_per_glb_unit = 100.0;
};

ArxReturnCode validateGlbImportOptions(const Level::GlbImportOptions& options);
std::optional<ArxVector3> toArxPoint(const ArxVector3& value, const ImportUnits& units) noexcept;
std::optional<ArxVector3> toArxVector(const ArxVector3& value, const ImportUnits& units) noexcept;
ArxQuat toArxRotation(const ArxQuat& value) noexcept;
std::optional<float> toArxLength(float value, const ImportUnits& units) noexcept;
ArxReturnCode applyGlbImportPlacement(LevelModules& level, const Level::GlbImportOptions& options,
                                      ArxLevelGlbImportInfo& info);
ArxReturnCode configureGlbExportCoordinates(glb::Builder& builder, const Level::GlbExportOptions& options);
float toGlbLength(float value, const Level::GlbExportOptions& options);

}  // namespace pistoris::glb_level
