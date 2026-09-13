// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#include "arx_pistoris/base/status.h"
#include "arx_pistoris/model.h"
#include "arx_pistoris/model.hpp"

#include "api/c/internal.h"
#include "api/c/model/internal.h"  // IWYU pragma: keep

#include <cstddef>
#include <cstdint>
#include <utility>
#include <vector>

// NOLINTBEGIN(readability-identifier-naming)

ArxReturnCode arx_pistoris_model_render_icon_png(const ArxModel* model,
                                                 const ArxModelInventoryIconRenderOptions* options, uint8_t** out_data,
                                                 size_t* out_size) noexcept {
  if (!model) return ARX_INVALID_HANDLE;
  if (!options) return ARX_INVALID_OPTIONS;
  if (!out_data || !out_size) return ARX_INVALID_DATA_POINTER;
  *out_data = nullptr;
  *out_size = 0;
  return pistoris::c_api::guard([&] {
    std::vector<std::uint8_t> rendered;
    const ArxReturnCode rc = model->value.renderIconPng(
        {
            .width_slots = options->width_slots,
            .height_slots = options->height_slots,
            .layout = static_cast<pistoris::Model::InventoryIconLayout>(options->layout),
        },
        rendered);
    if (rc != ARX_OK) return rc;
    return pistoris::c_api::publishBytes(std::move(rendered), out_data, out_size);
  });
}

// NOLINTEND(readability-identifier-naming)
