// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#pragma once

#include "arx_pistoris/base/flags.h"
#include "arx_pistoris/base/math.hpp"

#include <cstdint>
#include <string>
#include <string_view>

namespace pistoris::glb_model {

inline constexpr std::string_view kOriginName = "arx_model_origin";
inline constexpr std::string_view kOriginPrefix = "arx_model_origin__";
inline constexpr std::string_view kBonePrefix = "arx_bone__";
inline constexpr std::string_view kActionPrefix = "arx_action__";
inline constexpr std::string_view kAnimationPrefix = "arx_animation__";
inline constexpr std::string_view kProbePrefix = "arx_selection_probe__";
inline constexpr std::string_view kSelectionPrefix = "SELECTION_";
inline constexpr std::string_view kOriginOwnerPrefix = "ORIGIN_OWNER__";
inline constexpr std::string_view kBlobShadowPrefix = "SETTINGS__BLOB_SHADOW_";

}  // namespace pistoris::glb_model
