// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#include "arx_pistoris/base/status.h"
#include "arx_pistoris/model.hpp"
#include "arx_pistoris/runtime/types.h"

#include "api/status_boundary.h"
#include "model/data.h"
#include "modules/selections.h"
#include "utils/log.h"

#include <cstddef>
#include <utility>

namespace pistoris {

ArxReturnCode Model::inferBoneOriginSelections() noexcept {
  return api_detail::statusBoundary([&]() -> ArxReturnCode {
    selections::BoneMaskInferenceResult result = selections::inferBoneMasksFromVertices(
        data_->selections, data_->skeleton.vertex_bones, data_->skeleton.bones.size());
    const std::size_t membership_count = result.membership_count;
    const std::size_t bones_without_vertices = result.bones_without_vertices;
    selections::replaceBoneMasks(data_->selections, std::move(result.masks));
    log(ARX_LOG_INFO,
        "Model bone-origin selection inference: inferred {} membership(s), {} bone(s) had no owned vertices",
        membership_count,
        bones_without_vertices);
    return ARX_OK;
  });
}

}  // namespace pistoris
