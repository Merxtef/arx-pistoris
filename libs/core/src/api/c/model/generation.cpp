// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#include "arx_pistoris/base/error.h"
#include "arx_pistoris/base/image.hpp"
#include "arx_pistoris/base/status.h"
#include "arx_pistoris/model.h"
#include "arx_pistoris/model.hpp"

#include "api/c/internal.h"
#include "api/c/model/internal.h"  // IWYU pragma: keep

#include <cstddef>
#include <cstdint>
#include <optional>
#include <utility>
#include <vector>

// NOLINTBEGIN(readability-identifier-naming)

ArxReturnCode arx_pistoris_model_render_icon(const ArxModel* model, const ArxModelInventoryIconRenderOptions* options,
                                             uint8_t** out_data, size_t* out_size, ArxError* error) noexcept {
  if (!model) return pistoris::c_api::publishCode(ARX_INVALID_HANDLE, error);
  if (!options) return pistoris::c_api::publishCode(ARX_INVALID_OPTIONS, error);
  if (!out_data || !out_size) return pistoris::c_api::publishCode(ARX_INVALID_DATA_POINTER, error);
  *out_data = nullptr;
  *out_size = 0;
  return pistoris::c_api::guard(error, [&] {
    const pistoris::Model::InventoryIconRenderOptions render_options{
        .width_slots = options->width_slots == 0 ? std::nullopt : std::optional(options->width_slots),
        .height_slots = options->height_slots == 0 ? std::nullopt : std::optional(options->height_slots),
        .layout = static_cast<pistoris::Model::InventoryIconLayout>(options->layout),
        .format = static_cast<pistoris::ImageFormat>(options->format),
    };
    auto rendered = model->value.renderIcon(render_options);
    if (!rendered) return pistoris::c_api::publish(rendered, error);
    const ArxReturnCode code = pistoris::c_api::publishBytes(std::move(*rendered), out_data, out_size);
    return code == ARX_OK ? pistoris::c_api::publish(rendered, error) : pistoris::c_api::publishCode(code, error);
  });
}

// NOLINTEND(readability-identifier-naming)
