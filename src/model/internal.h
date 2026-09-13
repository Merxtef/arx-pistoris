// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#pragma once

#include "arx_pistoris/base/status.h"

#include "modules/action_points.h"
#include "modules/geometry.h"
#include "modules/selections.h"
#include "modules/skeleton.h"
#include "modules/textures.h"

#include <string>
#include <string_view>

namespace pistoris {

struct ModelModules;

namespace model_detail {

ArxReturnCode geometryError(geometry::Error error) noexcept;
ArxReturnCode textureError(textures::Error error) noexcept;
ArxReturnCode skeletonError(skeleton::Error error) noexcept;
ArxReturnCode actionPointError(action_points::Error error) noexcept;
ArxReturnCode selectionError(selections::Error error) noexcept;
ArxReturnCode validateStructure(const ModelModules& modules) noexcept;

bool normalizedName(std::string_view name, std::string& out);
bool normalizedSelectionName(std::string_view name, std::string& out);

}  // namespace model_detail
}  // namespace pistoris
