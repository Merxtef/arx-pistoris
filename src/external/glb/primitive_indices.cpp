// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#include "primitive_indices.h"

#include "arx_pistoris/base/status.h"

#include "cgltf/cgltf.h"
#include "external/glb/accessor.h"

#include <cstddef>
#include <cstdint>
#include <limits>

namespace pistoris::glb {

std::uint32_t PrimitiveIndices::operator[](std::size_t index) const noexcept {
  if (mirrored_) {
    if (index % 3U == 1U)
      ++index;
    else if (index % 3U == 2U)
      --index;
  }
  return explicit_.empty() ? static_cast<std::uint32_t>(index) : explicit_[index];
}

ArxReturnCode readPrimitiveIndices(AccessorCache& accessors, const cgltf_primitive& primitive,
                                   std::size_t position_count, bool require_indices, bool mirrored,
                                   PrimitiveIndices& out) {
  PrimitiveIndices result;
  result.mirrored_ = mirrored;
  if (primitive.indices == nullptr) {
    if (require_indices || position_count > std::numeric_limits<std::uint32_t>::max()) return ARX_GLB_BAD_FORMAT;
    result.count_ = position_count;
  } else {
    const AccessorView* indices = nullptr;
    ArxReturnCode rc = accessors.get(primitive.indices, indices);
    if (rc != ARX_OK) return rc;
    rc = validateIndexAccessor(*indices);
    if (rc != ARX_OK) return rc;
    result.explicit_ = indices->indices;
    result.count_ = result.explicit_.size();
  }
  out = result;
  return ARX_OK;
}

}  // namespace pistoris::glb
