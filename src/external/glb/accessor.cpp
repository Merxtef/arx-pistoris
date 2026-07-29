// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#include "accessor.h"

#include "arx_pistoris/pistoris_types.h"

#include "external/glb/container.h"

#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <utility>

namespace pistoris::glb {
namespace {

bool unsignedIndexType(cgltf_component_type type) {
  return type == cgltf_component_type_r_8u || type == cgltf_component_type_r_16u || type == cgltf_component_type_r_32u;
}

}  // namespace

ArxReturnCode getAccessor(const Asset&, const cgltf_accessor* accessor, AccessorView& out) {
  if (accessor == nullptr) return ARX_GLB_BAD_FORMAT;
  if (accessor->count == 0) return ARX_GLB_BAD_FORMAT;
  if (accessor->buffer_view == nullptr && !accessor->is_sparse) return ARX_GLB_BAD_FORMAT;
  AccessorView tmp;
  tmp.source = accessor;
  tmp.count = accessor->count;
  tmp.type = accessor->type;
  tmp.component_type = accessor->component_type;
  tmp.normalized = accessor->normalized != 0;

  if (accessor->type == cgltf_type_scalar && unsignedIndexType(accessor->component_type)) {
    if (accessor->is_sparse) return ARX_GLB_UNSUPPORTED_FEATURE;
    tmp.indices.resize(accessor->count);
    if (cgltf_accessor_unpack_indices(accessor, tmp.indices.data(), sizeof(std::uint32_t), accessor->count) !=
        accessor->count) {
      return ARX_GLB_BAD_FORMAT;
    }
  } else {
    cgltf_size component_count = cgltf_num_components(accessor->type);
    if (component_count == 0 || accessor->count > std::numeric_limits<std::size_t>::max() / component_count)
      return ARX_GLB_BAD_FORMAT;
    tmp.floats.resize(accessor->count * component_count);
    if (cgltf_accessor_unpack_floats(accessor, tmp.floats.data(), tmp.floats.size()) != tmp.floats.size())
      return ARX_GLB_BAD_FORMAT;
    for (float value : tmp.floats)
      if (!std::isfinite(value)) return ARX_GLB_BAD_FORMAT;
  }
  out = std::move(tmp);
  return ARX_OK;
}

ArxReturnCode validatePositionAccessor(const AccessorView& view) {
  return view.type == cgltf_type_vec3 && view.component_type == cgltf_component_type_r_32f && !view.normalized &&
                 !view.floats.empty()
             ? ARX_OK
             : ARX_GLB_BAD_FORMAT;
}

ArxReturnCode validateNormalAccessor(const AccessorView& view) { return validatePositionAccessor(view); }

ArxReturnCode validateTexcoordAccessor(const AccessorView& view) {
  if (view.type != cgltf_type_vec2 || view.floats.empty()) return ARX_GLB_BAD_FORMAT;
  if (view.component_type == cgltf_component_type_r_32f) return !view.normalized ? ARX_OK : ARX_GLB_BAD_FORMAT;
  if (view.component_type == cgltf_component_type_r_8u || view.component_type == cgltf_component_type_r_16u) {
    return view.normalized ? ARX_OK : ARX_GLB_BAD_FORMAT;
  }
  return ARX_GLB_BAD_FORMAT;
}

ArxReturnCode validateColorAccessor(const AccessorView& view) {
  if ((view.type != cgltf_type_vec3 && view.type != cgltf_type_vec4) || view.floats.empty()) return ARX_GLB_BAD_FORMAT;
  if (view.component_type == cgltf_component_type_r_32f) return !view.normalized ? ARX_OK : ARX_GLB_BAD_FORMAT;
  if (view.component_type == cgltf_component_type_r_8u || view.component_type == cgltf_component_type_r_16u)
    return view.normalized ? ARX_OK : ARX_GLB_BAD_FORMAT;
  return ARX_GLB_BAD_FORMAT;
}

ArxReturnCode validateIndexAccessor(const AccessorView& view) {
  return view.type == cgltf_type_scalar && unsignedIndexType(view.component_type) && !view.normalized &&
                 !view.indices.empty()
             ? ARX_OK
             : ARX_GLB_BAD_FORMAT;
}

Vec3 readVec3(const AccessorView& view, std::size_t index) {
  std::size_t base = index * 3;
  return {view.floats[base], view.floats[base + 1], view.floats[base + 2]};
}

Vec2 readVec2(const AccessorView& view, std::size_t index) {
  std::size_t base = index * 2;
  return {view.floats[base], view.floats[base + 1]};
}

Vec3 readColor3(const AccessorView& view, std::size_t index) {
  std::size_t base = index * (view.type == cgltf_type_vec4 ? 4U : 3U);
  return {view.floats[base], view.floats[base + 1], view.floats[base + 2]};
}

}  // namespace pistoris::glb
