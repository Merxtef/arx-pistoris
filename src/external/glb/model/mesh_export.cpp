// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#include "mesh_export.h"

#include "arx_pistoris/base/flags.h"
#include "arx_pistoris/base/indices.h"
#include "arx_pistoris/base/math.hpp"
#include "arx_pistoris/base/status.h"
#include "arx_pistoris/runtime/types.h"

#include "cgltf/cgltf.h"
#include "external/glb/accessor.h"
#include "external/glb/geometry_material.h"
#include "external/glb/object_coordinates.h"
#include "external/glb/utils/texture.h"
#include "external/glb/writer.h"
#include "external/material_name.h"
#include "model/data.h"
#include "modules/geometry.h"
#include "modules/selections.h"
#include "modules/textures.h"
#include "utils/encoded_image.h"
#include "utils/log.h"

#include <array>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <limits>
#include <map>
#include <set>
#include <span>
#include <string>
#include <string_view>
#include <tuple>
#include <unordered_map>
#include <utility>
#include <vector>

namespace pistoris::glb_model {
namespace {

struct MaterialKey {
  TextureIndex texture = kNoTexture;
  FaceType flags = 0;
  float transval = 0.0f;

  bool operator<(const MaterialKey& other) const {
    return std::tie(texture, flags, transval) < std::tie(other.texture, other.flags, other.transval);
  }
};

std::uint32_t floatKey(float value) noexcept { return value == 0.0f ? 0U : std::bit_cast<std::uint32_t>(value); }

struct ExportVertexKey {
  VertexIndex vertex = kInvalidVertexIndex;
  std::array<std::uint32_t, 5> attributes{};

  bool operator==(const ExportVertexKey&) const = default;
};

struct ExportVertexKeyHash {
  std::size_t operator()(const ExportVertexKey& value) const noexcept {
    std::size_t result = std::hash<VertexIndex>{}(value.vertex);
    for (std::uint32_t component : value.attributes) {
      const std::size_t hash = std::hash<std::uint32_t>{}(component);
      result ^= hash + 0x9e3779b9U + (result << 6U) + (result >> 2U);
    }
    return result;
  }
};

struct ProjectedPrimitive {
  MaterialKey material;
  std::size_t first_index = 0;
  std::size_t index_count = 0;
};

struct MeshExportProjection {
  std::vector<ProjectedPrimitive> primitives;
  std::vector<std::uint32_t> indices;
  std::vector<glb::Vec3> positions;
  std::vector<glb::Vec3> normals;
  std::vector<glb::Vec2> texcoords;
  std::vector<std::array<std::uint16_t, 4>> joints;
  std::vector<std::array<float, 4>> weights;
  std::array<std::vector<std::array<float, 4>>, 64> selections;
};

MaterialKey materialKey(const Face& face) {
  return {face.texture, face.flags, (face.flags & kFaceBitTrans) != 0 ? face.transval : 0.0f};
}

std::vector<std::string> materialStems(const ModelModules& model, const MeshExportProjection& projection,
                                       std::string_view context) {
  std::vector<std::uint8_t> referenced(model.textures.textures.size(), 0);
  for (const ProjectedPrimitive& primitive : projection.primitives)
    if (primitive.material.texture != kNoTexture) referenced[primitive.material.texture] = 1;

  constexpr std::array<std::string_view, 1> kReserved = {"no_tex"};
  return material_names::fallbackStems(model.textures.textures, referenced, kReserved, context);
}

std::string selectionAttributeName(std::string_view name) {
  std::string result("_");
  result.append(name);
  for (char& value : result)
    if (value >= 'a' && value <= 'z') value = static_cast<char>(value - 'a' + 'A');
  return result;
}

MeshExportProjection projectMesh(const ModelModules& model, const ModelMeshExportOptions& options) {
  struct MaterialSlot {
    std::size_t face_count = 0;
    std::size_t primitive = 0;
    std::size_t next_index = 0;
  };
  std::map<MaterialKey, MaterialSlot> material_slots;
  for (const Face& face : model.geometry.faces) ++material_slots[materialKey(face)].face_count;

  MeshExportProjection result;
  result.primitives.reserve(material_slots.size());
  std::size_t next_index = 0;
  for (auto& [material, slot] : material_slots) {
    slot.primitive = result.primitives.size();
    slot.next_index = next_index;
    const std::size_t index_count = slot.face_count * 3U;
    result.primitives.push_back({material, next_index, index_count});
    next_index += index_count;
  }
  result.indices.resize(next_index);

  const std::size_t corner_count = model.geometry.faces.size() * 3U;
  result.positions.reserve(corner_count);
  result.normals.reserve(corner_count);
  result.texcoords.reserve(corner_count);
  if (options.include_authoring_attributes && !model.skeleton.bones.empty()) {
    result.joints.reserve(corner_count);
    result.weights.reserve(corner_count);
  }
  if (options.include_authoring_attributes) {
    for (SelectionId id = 0; id < 64U; ++id)
      if (selections::occupied(model.selections, id)) result.selections[id].reserve(corner_count);
  }

  std::unordered_map<ExportVertexKey, std::uint32_t, ExportVertexKeyHash> exported_vertices;
  exported_vertices.reserve(corner_count);
  for (const Face& face : model.geometry.faces) {
    MaterialSlot& slot = material_slots.find(materialKey(face))->second;
    for (const Corner& corner : face.corners) {
      const ExportVertexKey vertex_key{corner.vertex,
                                       {floatKey(corner.normal.x),
                                        floatKey(corner.normal.y),
                                        floatKey(corner.normal.z),
                                        floatKey(corner.u),
                                        floatKey(corner.v)}};
      const auto [found, inserted] =
          exported_vertices.emplace(vertex_key, static_cast<std::uint32_t>(result.positions.size()));
      result.indices[slot.next_index++] = found->second;
      if (!inserted) continue;

      const ArxVector3 source_position = model.geometry.vertices[corner.vertex].position;
      const ArxVector3 position = options.level_basis
                                      ? source_position * options.position_scale
                                      : glb_object::toGlbPoint(source_position, 1.0f / options.position_scale);
      const ArxVector3 source_normal = options.level_basis ? corner.normal : glb_object::toGlbDirection(corner.normal);
      const ArxVector3 normal = math::normalize(source_normal);
      result.positions.push_back({position.x, position.y, position.z});
      result.normals.push_back({normal.x, normal.y, normal.z});
      result.texcoords.push_back({corner.u, corner.v});
      if (options.include_authoring_attributes && !model.skeleton.bones.empty()) {
        const BoneIndex bone = model.skeleton.vertex_bones[corner.vertex];
        result.joints.push_back(
            {bone == kInvalidBoneIndex ? std::uint16_t{0} : static_cast<std::uint16_t>(bone), 0, 0, 0});
        result.weights.push_back({bone == kInvalidBoneIndex ? 0.0f : 1.0f, 0.0f, 0.0f, 0.0f});
      }
      if (options.include_authoring_attributes) {
        const SelectionMask mask = model.selections.vertex_masks[corner.vertex];
        for (SelectionId id = 0; id < 64U; ++id) {
          if (!selections::occupied(model.selections, id)) continue;
          const float member = (mask & selections::bit(id)) != 0 ? 1.0f : 0.0f;
          result.selections[id].push_back({member, member, member, 1.0f});
        }
      }
    }
  }
  return result;
}

}  // namespace

ArxReturnCode addModelMesh(const ModelModules& model, const ModelMeshExportOptions& options, glb::Builder& builder,
                           int& out_mesh) {
  MeshExportProjection projection = projectMesh(model, options);
  const std::vector<std::string> texture_stems = materialStems(model, projection, options.context);

  const int position_accessor = builder.addVec3Accessor(projection.positions);
  const int normal_accessor = builder.addVec3Accessor(projection.normals);
  const int texcoord_accessor = builder.addAccessor(
      std::span<const glb::Vec2>(projection.texcoords), cgltf_component_type_r_32f, cgltf_type_vec2);
  int joint_accessor = -1;
  int weight_accessor = -1;
  if (!projection.joints.empty()) {
    joint_accessor = builder.addAccessor(
        std::span<const std::array<std::uint16_t, 4>>(projection.joints), cgltf_component_type_r_16u, cgltf_type_vec4);
    weight_accessor = builder.addAccessor(
        std::span<const std::array<float, 4>>(projection.weights), cgltf_component_type_r_32f, cgltf_type_vec4);
  }
  std::array<int, 64> selection_accessors{};
  selection_accessors.fill(-1);
  if (options.include_authoring_attributes) {
    for (SelectionId id = 0; id < 64U; ++id) {
      if (!selections::occupied(model.selections, id)) continue;
      selection_accessors[id] = builder.addAccessor(std::span<const std::array<float, 4>>(projection.selections[id]),
                                                    cgltf_component_type_r_32f,
                                                    cgltf_type_vec4);
    }
  }

  std::vector<glb::ExportedTexture> glb_textures(model.textures.textures.size());
  std::vector<std::size_t> prepared_by_texture(model.textures.textures.size(), std::numeric_limits<std::size_t>::max());
  std::vector<textures::ImagePreparationRequest> requests;
  for (const ProjectedPrimitive& primitive : projection.primitives) {
    const TextureIndex texture = primitive.material.texture;
    if (texture == kNoTexture || model.textures.textures[texture].encoded_image.empty() ||
        prepared_by_texture[texture] != std::numeric_limits<std::size_t>::max())
      continue;
    prepared_by_texture[texture] = requests.size();
    requests.push_back(
        {texture,
         {.accepted_formats = image::formatFlag(image::Format::kPng) | image::formatFlag(image::Format::kJpeg),
          .fallback_format = image::Format::kPng,
          .require_power_of_two = false}});
  }
  std::vector<textures::PreparedImage> prepared;
  const textures::Error preparation_error = textures::prepareImages(model.textures, requests, prepared);
  if (preparation_error == textures::Error::kOutOfMemory) return ARX_BAD_ALLOC;
  if (preparation_error != textures::Error::kNone) return ARX_MODEL_BAD_TEXTURE_IMAGE;

  std::vector<glb::Primitive> primitives;
  primitives.reserve(projection.primitives.size());
  std::set<TextureIndex> unknown_alpha_textures;
  std::size_t nonstandard_transval_materials = 0;
  for (const ProjectedPrimitive& projected : projection.primitives) {
    const MaterialKey& key = projected.material;
    glb::Primitive primitive;
    primitive.indices = builder.addAccessor(
        std::span<const std::uint32_t>(projection.indices).subspan(projected.first_index, projected.index_count),
        cgltf_component_type_r_32u,
        cgltf_type_scalar);
    primitive.attributes.emplace_back("POSITION", position_accessor);
    primitive.attributes.emplace_back("NORMAL", normal_accessor);
    primitive.attributes.emplace_back("TEXCOORD_0", texcoord_accessor);
    if (joint_accessor >= 0) {
      primitive.attributes.emplace_back("JOINTS_0", joint_accessor);
      primitive.attributes.emplace_back("WEIGHTS_0", weight_accessor);
    }
    if (options.include_authoring_attributes) {
      for (SelectionId id = 0; id < 64U; ++id) {
        if (!selections::occupied(model.selections, id)) continue;
        primitive.attributes.emplace_back(selectionAttributeName(model.selections.slots[id].name),
                                          selection_accessors[id]);
      }
    }

    const std::string_view stem = key.texture == kNoTexture ? std::string_view("no_tex") : texture_stems[key.texture];
    int texture = -1;
    glb::TextureAlpha texture_alpha = glb::TextureAlpha::kAbsent;
    if (key.texture != kNoTexture) {
      glb::ExportedTexture& exported = glb_textures[key.texture];
      if (exported.index < 0) {
        const Texture& source = model.textures.textures[key.texture];
        const std::size_t prepared_index = prepared_by_texture[key.texture];
        const textures::PreparedImage* image =
            prepared_index == std::numeric_limits<std::size_t>::max() ? nullptr : &prepared[prepared_index];
        glb::exportTexture(builder, source, image, exported);
        if (exported.assumed_png)
          log(ARX_LOG_WARN, "{}: external texture format unknown; assuming PNG: {}.png", options.context, source.path);
      }
      texture = exported.index;
      texture_alpha = exported.alpha;
    }
    glb::ExportedGeometryMaterial material = glb::exportGeometryMaterial(stem, key.flags, key.transval, texture_alpha);
    if (material.unknown_alpha) unknown_alpha_textures.insert(key.texture);
    if (material.nonstandard_transval) ++nonstandard_transval_materials;
    primitive.material =
        builder.addMaterial(std::move(material.name), texture, key.flags, material.alpha, material.alpha_cutout);
    primitives.push_back(std::move(primitive));
  }
  if (nonstandard_transval_materials != 0)
    log(ARX_LOG_WARN,
        "{}: {} transparent material group(s) use nonstandard Arx blend modes; raw transval is preserved in "
        "material names and previewed with alpha 1",
        options.context,
        nonstandard_transval_materials);
  if (!unknown_alpha_textures.empty()) {
    log(ARX_LOG_WARN,
        "{}: alpha presence unknown for {} external texture(s); non-TRANS materials exported as OPAQUE",
        options.context,
        unknown_alpha_textures.size());
  }

  out_mesh = builder.addMesh(std::string(options.mesh_name), std::move(primitives));
  return ARX_OK;
}

}  // namespace pistoris::glb_model
