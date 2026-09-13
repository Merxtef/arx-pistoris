// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#include "arx_pistoris/animation/types.h"
#include "arx_pistoris/base/flags.h"
#include "arx_pistoris/base/indices.h"
#include "arx_pistoris/base/math.hpp"
#include "arx_pistoris/base/status.h"
#include "arx_pistoris/model.hpp"
#include "arx_pistoris/runtime/types.h"
#include "arx_pistoris/sound.hpp"

#include "animation/data.h"
#include "external/glb/accessor.h"
#include "external/glb/container.h"
#include "external/glb/geometry_material.h"
#include "external/glb/model/animation_import.h"
#include "external/glb/model/api.h"
#include "external/glb/model/discovery.h"
#include "external/glb/model/internal.h"
#include "external/glb/node_graph.h"
#include "external/glb/object_coordinates.h"
#include "external/glb/utils/names.h"
#include "external/glb/utils/texture.h"
#include "external/glb/utils/transform.h"
#include "model/data.h"
#include "model/internal.h"
#include "modules/action_points.h"
#include "modules/geometry.h"
#include "modules/selections.h"
#include "modules/skeleton.h"
#include "utils/identifier.h"
#include "utils/log.h"
#include "utils/math/mat4.h"
#include "utils/name_tokens.h"

#include <algorithm>
#include <array>
#include <charconv>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <format>
#include <functional>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <system_error>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

namespace pistoris::glb_model {
using glb_object::toArxDirection;
using glb_object::toArxPoint;
namespace {

struct ImportedMaterial {
  TextureIndex texture = kNoTexture;
  FaceType flags = 0;
  float transval = 0.0f;
  std::size_t uv_set = 0;
};

std::size_t nodeIndex(const cgltf_data& data, const cgltf_node* node) {
  return static_cast<std::size_t>(node - data.nodes);
}

bool reportableIdentifierRepair(IdentifierRepair repair) noexcept {
  return repair != IdentifierRepair::kNone && repair != IdentifierRepair::kCase;
}

std::optional<std::pair<std::string_view, std::string_view>> labeledValue(std::string_view name,
                                                                          std::string_view prefix) {
  if (!name.starts_with(prefix)) return std::nullopt;
  const auto parsed = glb::splitRequiredLabel(name.substr(prefix.size()));
  if (!parsed || hasDoubleUnderscore(parsed->value)) return std::nullopt;
  return std::pair{parsed->value, parsed->label};
}

template <class Visitor>
ArxReturnCode visitSelectionHelpers(const cgltf_node& parent, Visitor&& visitor) {
  for (std::size_t index = 0; index < parent.children_count; ++index) {
    const cgltf_node* child = parent.children[index];
    if (child == nullptr || child->name == nullptr) continue;
    const auto parsed = labeledValue(child->name, kSelectionPrefix);
    if (!parsed) {
      if (std::string_view(child->name).starts_with(kSelectionPrefix)) return ARX_GLB_BAD_MODEL_SELECTION;
      continue;
    }
    const ArxReturnCode rc = visitor(parsed->first);
    if (rc != ARX_OK) return rc;
  }
  return ARX_OK;
}

std::optional<float> parseFloat(std::string_view text) {
  float value = 0.0f;
  const auto [end, error] = std::from_chars(text.data(), text.data() + text.size(), value, std::chars_format::general);
  if (error != std::errc{} || end != text.data() + text.size() || !std::isfinite(value)) return std::nullopt;
  return value;
}

std::optional<std::pair<std::size_t, std::string_view>> boneName(std::string_view name) {
  const std::size_t separator = name.find("__");
  if (separator == std::string_view::npos || separator == 0 || separator + 2 == name.size()) return std::nullopt;
  std::size_t ordinal = 0;
  const std::string_view ordinal_text = name.substr(0, separator);
  const auto [end, error] =
      std::from_chars(ordinal_text.data(), ordinal_text.data() + ordinal_text.size(), ordinal, 10);
  if (error != std::errc{} || end != ordinal_text.data() + ordinal_text.size()) return std::nullopt;
  const std::string_view bone_name = name.substr(separator + 2);
  if (hasDoubleUnderscore(bone_name)) return std::nullopt;
  return std::pair{ordinal, bone_name};
}

ArxVector3 nodePosition(const glb::NodeGraph& graph, const math::Mat4& inverse_root, std::size_t node, float units) {
  return toArxPoint(math::translation(inverse_root * graph.world[node]), units);
}

const cgltf_attribute* findAttribute(const cgltf_primitive& primitive, cgltf_attribute_type type, std::size_t set = 0) {
  for (std::size_t index = 0; index < primitive.attributes_count; ++index) {
    const cgltf_attribute& attribute = primitive.attributes[index];
    if (attribute.type == type && attribute.index >= 0 && static_cast<std::size_t>(attribute.index) == set)
      return &attribute;
  }
  return nullptr;
}

struct SkinBinding {
  std::vector<BoneIndex> joint_bones;
  std::vector<math::Mat4> rest_projections;
};

struct SkinAttributeSet {
  const cgltf_attribute* joint_attribute = nullptr;
  const cgltf_attribute* weight_attribute = nullptr;
  const glb::AccessorView* joints = nullptr;
  const glb::AccessorView* weights = nullptr;
  const SkinBinding* binding = nullptr;
};

using SkinBindings = std::unordered_map<const cgltf_skin*, SkinBinding>;

struct ImportedVertexKey {
  std::size_t node = 0;
  const cgltf_accessor* positions = nullptr;
  std::uint32_t source = 0;
  BoneIndex bone = kInvalidBoneIndex;
  SelectionMask selections = 0;

  bool operator==(const ImportedVertexKey&) const = default;
};

struct ImportedVertexKeyHash {
  std::size_t operator()(const ImportedVertexKey& key) const noexcept {
    std::size_t hash = key.node;
    hash = hash * 131U + std::hash<const cgltf_accessor*>{}(key.positions);
    hash = hash * 131U + key.source;
    hash = hash * 131U + std::hash<BoneIndex>{}(key.bone);
    hash = hash * 131U + std::hash<SelectionMask>{}(key.selections);
    return hash;
  }
};

ArxReturnCode readSkinAttributes(glb::AccessorCache& accessors, const cgltf_primitive& primitive,
                                 const cgltf_node& node, const SkinBindings& bindings, std::size_t vertex_count,
                                 std::vector<SkinAttributeSet>& out) {
  std::vector<SkinAttributeSet> sets;
  for (std::size_t attribute_index = 0; attribute_index < primitive.attributes_count; ++attribute_index) {
    const cgltf_attribute& attribute = primitive.attributes[attribute_index];
    if (attribute.type != cgltf_attribute_type_joints && attribute.type != cgltf_attribute_type_weights) continue;
    if (attribute.index < 0 || static_cast<std::size_t>(attribute.index) >= primitive.attributes_count)
      return ARX_GLB_BAD_MODEL_SKINNING;
    const std::size_t set_index = static_cast<std::size_t>(attribute.index);
    if (sets.size() <= set_index) sets.resize(set_index + 1U);
    const cgltf_attribute*& slot = attribute.type == cgltf_attribute_type_joints ? sets[set_index].joint_attribute
                                                                                 : sets[set_index].weight_attribute;
    if (slot != nullptr) return ARX_GLB_BAD_MODEL_SKINNING;
    slot = &attribute;
  }
  if (sets.empty()) {
    if (node.skin != nullptr) return ARX_GLB_BAD_MODEL_SKINNING;
    out.clear();
    return ARX_OK;
  }
  if (node.skin == nullptr) return ARX_GLB_BAD_MODEL_SKINNING;
  const auto binding = bindings.find(node.skin);
  if (binding == bindings.end()) return ARX_GLB_BAD_MODEL_SKINNING;

  for (SkinAttributeSet& set : sets) {
    if (set.joint_attribute == nullptr || set.weight_attribute == nullptr) return ARX_GLB_BAD_MODEL_SKINNING;
    ArxReturnCode rc = accessors.get(set.joint_attribute->data, set.joints);
    if (rc != ARX_OK) return rc;
    if (set.joints->type != cgltf_type_vec4 || set.joints->normalized ||
        (set.joints->component_type != cgltf_component_type_r_8u &&
         set.joints->component_type != cgltf_component_type_r_16u) ||
        set.joints->count != vertex_count)
      return ARX_GLB_BAD_MODEL_SKINNING;
    rc = accessors.get(set.weight_attribute->data, set.weights);
    if (rc != ARX_OK) return rc;
    const bool valid_weight_type =
        (set.weights->component_type == cgltf_component_type_r_32f && !set.weights->normalized) ||
        ((set.weights->component_type == cgltf_component_type_r_8u ||
          set.weights->component_type == cgltf_component_type_r_16u) &&
         set.weights->normalized);
    if (set.weights->type != cgltf_type_vec4 || !valid_weight_type || set.weights->count != vertex_count)
      return ARX_GLB_BAD_MODEL_SKINNING;
    set.binding = &binding->second;
  }
  out = std::move(sets);
  return ARX_OK;
}

struct SkinVertexBinding {
  BoneIndex bone = kInvalidBoneIndex;
  const math::Mat4* rest_projection = nullptr;
};

ArxReturnCode readSkinVertex(std::span<const SkinAttributeSet> sets, std::uint32_t source, SkinVertexBinding& out) {
  SkinVertexBinding result;
  const std::size_t base = static_cast<std::size_t>(source) * 4U;
  float best_weight = 0.0f;
  for (const SkinAttributeSet& set : sets) {
    if (set.binding == nullptr) return ARX_GLB_BAD_MODEL_SKINNING;
    for (std::size_t influence = 0; influence < 4; ++influence) {
      const float joint_value = set.joints->floats[base + influence];
      const float weight = set.weights->floats[base + influence];
      if (joint_value < 0.0f || std::floor(joint_value) != joint_value ||
          joint_value >= static_cast<float>(set.binding->joint_bones.size()) || !std::isfinite(weight) || weight < 0.0f)
        return ARX_GLB_BAD_MODEL_SKINNING;
      if (weight <= best_weight) continue;
      const std::size_t joint = static_cast<std::size_t>(joint_value);
      if (joint >= set.binding->rest_projections.size()) return ARX_GLB_BAD_MODEL_SKINNING;
      best_weight = weight;
      result.bone = set.binding->joint_bones[joint];
      result.rest_projection = &set.binding->rest_projections[joint];
    }
  }
  out = result;
  return ARX_OK;
}

class SelectionRegistry {
 public:
  SelectionRegistry(SelectionsData& selections, std::size_t bone_count)
      : selections_(selections), bone_count_(bone_count) {
    ids_.reserve(64);
    pending_.reserve(64);
  }

  ArxReturnCode discover(std::string_view requested) {
    if (hasDoubleUnderscore(requested)) return ARX_GLB_BAD_MODEL_SELECTION;
    std::string key(requested);
    for (char& value : key)
      if (value >= 'A' && value <= 'Z') value = static_cast<char>(value - 'A' + 'a');
    if (ids_.contains(key)) return ARX_OK;
    if (pending_.size() >= 64U) return ARX_MODEL_TOO_MANY_SELECTIONS;
    const SelectionId id = static_cast<SelectionId>(pending_.size());
    ids_.emplace(std::move(key), id);
    pending_.push_back({requested, std::string(requested)});
    return ARX_OK;
  }

  ArxReturnCode finalize() {
    IdentifierUniquifier names({.letter_case = IdentifierCase::kLower, .max_length = selections::kMaxNameLength});
    names.reserve(pending_.size());
    for (PendingSelection& pending : pending_) names.add(pending.name);
    std::vector<IdentifierRepair> repairs(pending_.size());
    if (names.apply(repairs).exhausted) return ARX_GLB_BAD_MODEL_SELECTION;

    for (std::size_t index = 0; index < pending_.size(); ++index) {
      Selection selection{std::move(pending_[index].name), std::nullopt};
      if (selections::validateSelection(selection, bone_count_) != selections::Error::kNone)
        return ARX_GLB_BAD_MODEL_SELECTION;
      const SelectionId id = selections::addSelection(selections_, std::move(selection));
      if (id != index) return ARX_GLB_BAD_MODEL_SELECTION;
      if (reportableIdentifierRepair(repairs[index]))
        log(ARX_LOG_INFO,
            "GLB -> Model: selection name '{}' normalized to '{}'",
            pending_[index].requested,
            selections_.slots[id].name);
    }
    return ARX_OK;
  }

  ArxReturnCode get(std::string_view requested, SelectionId& out) const {
    std::string key(requested);
    for (char& value : key)
      if (value >= 'A' && value <= 'Z') value = static_cast<char>(value - 'A' + 'a');
    const auto found = ids_.find(key);
    if (found == ids_.end()) return ARX_GLB_BAD_MODEL_SELECTION;
    out = found->second;
    return ARX_OK;
  }

 private:
  struct PendingSelection {
    std::string_view requested;
    std::string name;
  };

  SelectionsData& selections_;
  std::size_t bone_count_ = 0;
  std::unordered_map<std::string, SelectionId> ids_;
  std::vector<PendingSelection> pending_;
};

struct ImportContext {
  glb::AccessorCache& accessors;
  const cgltf_data& data;
  const glb::NodeGraph& graph;
  const ModelDiscovery& discovery;
  std::size_t root = 0;
  math::Mat4 inverse_root = math::kIdentityMat4;
  float units = 10.0f;
  ModelModules& model;
  SelectionRegistry selections;
  glb::TextureImporter textures;
  std::unordered_map<const cgltf_material*, ImportedMaterial> materials;
  SkinBindings skin_bindings;
  std::vector<BoneIndex> node_bones;
  std::vector<BoneIndex> owning_bones;
  std::vector<std::size_t> bone_helpers;

  ImportContext(const glb::Asset& asset_value, glb::AccessorCache& accessors_value, const glb::NodeGraph& graph_value,
                const ModelDiscovery& discovery_value, math::Mat4 inverse_root_value, float units_value,
                ModelModules& model_value, std::vector<std::string>* texture_source_paths_value)
      : accessors(accessors_value),
        data(*asset_value.data()),
        graph(graph_value),
        discovery(discovery_value),
        root(discovery.root),
        inverse_root(inverse_root_value),
        units(units_value),
        model(model_value),
        selections(model.selections, model.skeleton.bones.size()),
        textures(model.textures, texture_source_paths_value, "GLB -> Model"),
        node_bones(data.nodes_count, kInvalidBoneIndex),
        owning_bones(data.nodes_count, kInvalidBoneIndex),
        bone_helpers(model.skeleton.bones.size(), glb::kInvalidNodeIndex) {}
};

ArxReturnCode discoverBoneHelpers(ImportContext& context) {
  std::unordered_map<std::string_view, BoneIndex> bones;
  bones.reserve(context.model.skeleton.bones.size());
  for (std::size_t index = 0; index < context.model.skeleton.bones.size(); ++index)
    bones.emplace(context.model.skeleton.bones[index].name, static_cast<BoneIndex>(index));

  for (std::size_t node : context.discovery.bone_helpers) {
    const cgltf_node& source = context.data.nodes[node];
    if (source.name == nullptr) continue;
    const std::string_view name(source.name);
    if (!name.starts_with(kBonePrefix)) continue;
    const std::string_view requested = name.substr(kBonePrefix.size());
    if (requested.empty() || hasDoubleUnderscore(requested)) return ARX_GLB_BAD_MODEL_BONE_HELPER;
    IdentifierNormalization normalized =
        normalizeIdentifier(requested, {.letter_case = IdentifierCase::kLower, .max_length = skeleton::kMaxNameLength});
    const auto found = bones.find(normalized.value);
    if (found == bones.end()) {
      log(ARX_LOG_WARN, "GLB -> Model: bone helper '{}' has no matching bone; ignored", requested);
      continue;
    }
    if (reportableIdentifierRepair(normalized.repair))
      log(ARX_LOG_INFO, "GLB -> Model: bone helper name '{}' normalized to '{}'", requested, normalized.value);
    std::size_t& helper = context.bone_helpers[found->second];
    if (helper != glb::kInvalidNodeIndex) {
      log(ARX_LOG_WARN, "GLB -> Model: multiple bone helpers for '{}'; first used", normalized.value);
      continue;
    }
    helper = node;
  }
  return ARX_OK;
}

ArxReturnCode importMaterial(ImportContext& context, const cgltf_material* material, ImportedMaterial& out) {
  if (const auto found = context.materials.find(material); found != context.materials.end()) {
    out = found->second;
    return ARX_OK;
  }
  glb::GeometryMaterial decoded;
  glb::GeometryMaterialInfo info;
  switch (glb::decodeGeometryMaterial(material, decoded, &info)) {
    case glb::GeometryMaterialError::kNone:
      break;
    case glb::GeometryMaterialError::kBadMaterial:
    case glb::GeometryMaterialError::kBadAlpha:
      return ARX_GLB_BAD_MODEL_MATERIAL;
    case glb::GeometryMaterialError::kUnsupportedFeature:
      return ARX_GLB_UNSUPPORTED_FEATURE;
  }

  ImportedMaterial result{
      .flags = decoded.flags, .transval = decoded.transval, .uv_set = static_cast<std::size_t>(decoded.uv_set)};
  const std::string_view material_name = material != nullptr && material->name != nullptr ? material->name : "";
  if (info.duplicate_flags != 0)
    log(ARX_LOG_WARN,
        "GLB -> Model: material '{}' repeats {} face flag token(s); duplicates ignored",
        material_name,
        info.duplicate_flags);
  if (info.stripped_quad) log(ARX_LOG_WARN, "GLB -> Model: material '{}' QUAD flag ignored", material_name);
  if (info.normalized_mask) {
    const float alpha =
        material->has_pbr_metallic_roughness ? material->pbr_metallic_roughness.base_color_factor[3] : 1.0f;
    log(ARX_LOG_WARN,
        "GLB -> Model: material '{}' MASK base alpha {} and cutoff {} normalized to texture alpha with cutoff 0.5",
        material_name,
        alpha,
        material->alpha_cutoff);
  }
  if (info.no_tex_with_image)
    log(ARX_LOG_WARN, "GLB -> Model: no_tex material '{}' uses its referenced texture", material_name);

  if (decoded.texture.has_value()) {
    switch (context.textures.import(*decoded.texture, result.texture)) {
      case glb::TextureImportError::kNone:
        break;
      case glb::TextureImportError::kOutOfMemory:
        return ARX_BAD_ALLOC;
      case glb::TextureImportError::kBadImage:
        return ARX_GLB_BAD_FORMAT;
      case glb::TextureImportError::kBadPath:
        return ARX_GLB_BAD_MODEL_MATERIAL;
      case glb::TextureImportError::kTooManyTextures:
        return ARX_MODEL_TOO_MANY_TEXTURES;
    }
  }
  if (info.normalized_blend) {
    if (result.texture != kNoTexture)
      log(ARX_LOG_WARN,
          "GLB -> Model: material '{}' BLEND with base alpha 1 imported without TRANS; texture alpha, if present, "
          "remains native cutout",
          material_name);
    else
      log(ARX_LOG_WARN,
          "GLB -> Model: material '{}' BLEND with base alpha 1 and no texture imported as opaque",
          material_name);
  }
  context.materials.emplace(material, result);
  out = result;
  return ARX_OK;
}

ArxReturnCode importSkeleton(const glb::Asset& asset, glb::AccessorCache& accessors, const glb::NodeGraph& graph,
                             const ModelDiscovery& discovery, const math::Mat4& inverse_root, float units,
                             ModelModules& model, SkinBindings& skin_bindings, std::vector<BoneIndex>& node_bones) {
  if (discovery.skinned_mesh_nodes.empty()) return ARX_OK;
  const cgltf_data& data = *asset.data();
  SkinBindings bindings;
  bindings.reserve(discovery.skinned_mesh_nodes.size());
  const math::Mat4 root_transform =
      discovery.root == glb::kInvalidNodeIndex ? math::kIdentityMat4 : graph.world[discovery.root];
  std::vector<const cgltf_node*> by_ordinal;
  std::vector<std::string_view> bone_names;
  std::size_t bone_count = 0;

  for (std::size_t mesh_node : discovery.skinned_mesh_nodes) {
    const cgltf_skin* skin = data.nodes[mesh_node].skin;
    if (skin == nullptr || skin->extensions_count != 0) return ARX_GLB_UNSUPPORTED_FEATURE;
    if (bindings.contains(skin)) continue;
    const std::size_t skin_index = cgltf_skin_index(&data, skin);
    const auto skin_failure = [&]<class... Args>(
                                  ArxReturnCode code, std::format_string<Args...> reason, Args&&... args) {
      logLazy(ARX_LOG_DEBUG, [&] {
        return std::format("GLB -> Model skinning failure: skin {} '{}' {} (code {})",
                           skin_index,
                           skin->name != nullptr ? skin->name : "",
                           std::format(reason, std::forward<Args>(args)...),
                           code);
      });
      return code;
    };
    if (skin->joints_count == 0) return skin_failure(ARX_GLB_BAD_MODEL_SKINNING, "has no joints");
    if (skin->joints_count > skeleton::kMaxBones) return ARX_MODEL_TOO_MANY_BONES;

    SkinBinding binding;
    binding.joint_bones.resize(skin->joints_count, kInvalidBoneIndex);
    binding.rest_projections.resize(skin->joints_count);
    std::unordered_set<const cgltf_node*> local_joints;
    local_joints.reserve(skin->joints_count);
    for (std::size_t joint = 0; joint < skin->joints_count; ++joint) {
      const cgltf_node* node = skin->joints[joint];
      if (node == nullptr || node->name == nullptr)
        return skin_failure(ARX_GLB_BAD_MODEL_SKINNING, "has an unnamed joint at index {}", joint);
      if (node->extensions_count != 0) return ARX_GLB_UNSUPPORTED_FEATURE;
      if (!local_joints.insert(node).second)
        return skin_failure(ARX_GLB_BAD_MODEL_SKINNING, "repeats joint {} '{}'", joint, node->name);
      const std::size_t index = nodeIndex(data, node);
      if (discovery.active[index] == 0)
        return skin_failure(ARX_GLB_BAD_MODEL_SKINNING, "joint {} '{}' is outside Model scope", joint, node->name);
      const auto parsed = boneName(node->name);
      if (!parsed)
        return skin_failure(ARX_GLB_BAD_MODEL_SKELETON, "joint {} '{}' has an invalid bone name", joint, node->name);
      if (parsed->first >= skeleton::kMaxBones) return ARX_MODEL_TOO_MANY_BONES;
      if (by_ordinal.size() <= parsed->first) {
        by_ordinal.resize(parsed->first + 1U, nullptr);
        bone_names.resize(parsed->first + 1U);
      }
      if (by_ordinal[parsed->first] != nullptr && by_ordinal[parsed->first] != node)
        return skin_failure(ARX_GLB_BAD_MODEL_SKELETON, "bone ordinal {} is used by multiple joints", parsed->first);
      if (node_bones[index] != kInvalidBoneIndex && node_bones[index] != static_cast<BoneIndex>(parsed->first))
        return skin_failure(ARX_GLB_BAD_MODEL_SKELETON, "joint '{}' is assigned conflicting bone ordinals", node->name);
      if (by_ordinal[parsed->first] == nullptr) {
        by_ordinal[parsed->first] = node;
        bone_names[parsed->first] = parsed->second;
        node_bones[index] = static_cast<BoneIndex>(parsed->first);
        ++bone_count;
      }
      binding.joint_bones[joint] = static_cast<BoneIndex>(parsed->first);
    }

    const glb::AccessorView* inverse_bind = nullptr;
    if (skin->inverse_bind_matrices != nullptr) {
      const ArxReturnCode rc = accessors.get(skin->inverse_bind_matrices, inverse_bind);
      if (rc != ARX_OK) return skin_failure(rc, "cannot decode inverse bind matrices");
      if (inverse_bind->type != cgltf_type_mat4 || inverse_bind->component_type != cgltf_component_type_r_32f ||
          inverse_bind->normalized || inverse_bind->count < skin->joints_count)
        return skin_failure(ARX_GLB_BAD_MODEL_SKINNING, "has an invalid inverse bind matrix accessor");
    }
    for (std::size_t joint = 0; joint < skin->joints_count; ++joint) {
      math::Mat4 inverse_bind_matrix = math::kIdentityMat4;
      if (inverse_bind != nullptr) {
        std::copy_n(inverse_bind->floats.data() + joint * 16U, 16U, inverse_bind_matrix.m);
        if (!glb::canonicalizeAffineTransform(inverse_bind_matrix))
          return skin_failure(ARX_GLB_BAD_MODEL_SKINNING, "has a non-affine inverse bind matrix at index {}", joint);
        if (!math::inverseAffine(inverse_bind_matrix))
          return skin_failure(ARX_GLB_BAD_MODEL_SKINNING, "has a singular inverse bind matrix at index {}", joint);
      }
      inverse_bind_matrix = inverse_bind_matrix * root_transform;
      const math::Mat4 joint_model = inverse_root * graph.world[nodeIndex(data, skin->joints[joint])];
      if (!math::isRotationUniformScale(joint_model))
        return skin_failure(ARX_GLB_MODEL_NON_UNIFORM_SCALE, "joint {} has non-uniform default scale or shear", joint);
      if (math::linearDeterminant(joint_model) < 0.0)
        return skin_failure(ARX_GLB_MODEL_NON_UNIFORM_SCALE, "joint {} has a reflected default transform", joint);
      binding.rest_projections[joint] = joint_model * inverse_bind_matrix;
      if (!math::isRotationUniformScale(binding.rest_projections[joint]))
        return skin_failure(
            ARX_GLB_MODEL_NON_UNIFORM_SCALE, "joint {} has a non-uniform rest projection or shear", joint);
      if (math::linearDeterminant(binding.rest_projections[joint]) < 0.0)
        return skin_failure(ARX_GLB_MODEL_NON_UNIFORM_SCALE, "joint {} has a reflected rest projection", joint);
    }
    bindings.emplace(skin, std::move(binding));
  }

  if (by_ordinal.size() != bone_count || std::ranges::find(by_ordinal, nullptr) != by_ordinal.end()) {
    log(ARX_LOG_DEBUG, "GLB -> Model skeleton failure: bone ordinals are not contiguous");
    return ARX_GLB_BAD_MODEL_SKELETON;
  }
  model.skeleton.bones.resize(by_ordinal.size());
  for (std::size_t ordinal = 0; ordinal < by_ordinal.size(); ++ordinal) {
    const cgltf_node* node = by_ordinal[ordinal];
    const std::size_t index = nodeIndex(data, node);
    Bone& bone = model.skeleton.bones[ordinal];
    bone.name = bone_names[ordinal];

    for (std::size_t parent = graph.parent[index]; parent != glb::kInvalidNodeIndex; parent = graph.parent[parent]) {
      if (node_bones[parent] == kInvalidBoneIndex) continue;
      bone.parent = node_bones[parent];
      break;
    }
    if (bone.parent != kInvalidBoneIndex && bone.parent >= ordinal) {
      log(ARX_LOG_DEBUG,
          "GLB -> Model skeleton failure: bone {} '{}' has non-preceding parent {}",
          ordinal,
          bone.name,
          bone.parent);
      return ARX_GLB_BAD_MODEL_SKELETON;
    }
    const math::Mat4 joint_model = inverse_root * graph.world[index];
    if (!math::isRotationUniformScale(joint_model)) {
      log(ARX_LOG_DEBUG,
          "GLB -> Model skeleton failure: bone {} '{}' has non-uniform default scale or shear",
          ordinal,
          bone.name);
      return ARX_GLB_MODEL_NON_UNIFORM_SCALE;
    }
    if (math::linearDeterminant(joint_model) < 0.0) {
      log(ARX_LOG_DEBUG,
          "GLB -> Model skeleton failure: bone {} '{}' has a reflected default transform",
          ordinal,
          bone.name);
      return ARX_GLB_MODEL_NON_UNIFORM_SCALE;
    }
    bone.position = toArxPoint(math::translation(joint_model), units);
  }
  IdentifierUniquifier names({.letter_case = IdentifierCase::kLower, .max_length = skeleton::kMaxNameLength});
  names.reserve(model.skeleton.bones.size());
  for (Bone& bone : model.skeleton.bones) names.add(bone.name);
  std::vector<IdentifierRepair> bone_repairs(model.skeleton.bones.size());
  if (names.apply(bone_repairs).exhausted) return ARX_GLB_BAD_MODEL_SKELETON;
  for (std::size_t index = 0; index < bone_repairs.size(); ++index)
    if (reportableIdentifierRepair(bone_repairs[index]))
      log(ARX_LOG_INFO,
          "GLB -> Model: bone name '{}' normalized to '{}'",
          bone_names[index],
          model.skeleton.bones[index].name);

  skin_bindings = std::move(bindings);
  return ARX_OK;
}

BoneIndex owningBone(const ImportContext& context, std::size_t node) { return context.owning_bones[node]; }

BoneIndex rigidMeshBone(const ImportContext& context, std::size_t node) {
  return context.node_bones[node] != kInvalidBoneIndex ? context.node_bones[node] : owningBone(context, node);
}

ArxReturnCode discoverSelections(ImportContext& context) {
  for (std::size_t node_index_value : context.discovery.mesh_nodes) {
    const cgltf_node& node = context.data.nodes[node_index_value];
    for (std::size_t primitive_index = 0; primitive_index < node.mesh->primitives_count; ++primitive_index) {
      const cgltf_primitive& primitive = node.mesh->primitives[primitive_index];
      for (std::size_t attribute_index = 0; attribute_index < primitive.attributes_count; ++attribute_index) {
        const cgltf_attribute& attribute = primitive.attributes[attribute_index];
        if (attribute.type != cgltf_attribute_type_custom || attribute.name == nullptr || attribute.name[0] != '_' ||
            attribute.name[1] == '\0')
          continue;
        const ArxReturnCode rc = context.selections.discover(std::string_view(attribute.name).substr(1));
        if (rc != ARX_OK) return rc;
      }
    }
  }

  for (std::size_t helper : context.bone_helpers) {
    if (helper == glb::kInvalidNodeIndex) continue;
    const ArxReturnCode rc = visitSelectionHelpers(
        context.data.nodes[helper], [&](std::string_view name) { return context.selections.discover(name); });
    if (rc != ARX_OK) return rc;
  }

  for (std::size_t node_index_value : context.discovery.action_nodes) {
    const cgltf_node& node = context.data.nodes[node_index_value];
    const std::string_view name(node.name);
    if (!labeledValue(name, kActionPrefix)) return ARX_GLB_BAD_MODEL_ACTION_POINT;
    const ArxReturnCode rc =
        visitSelectionHelpers(node, [&](std::string_view selection) { return context.selections.discover(selection); });
    if (rc != ARX_OK) return rc;
  }
  for (std::size_t node_index_value : context.discovery.probe_nodes) {
    const cgltf_node& node = context.data.nodes[node_index_value];
    const auto parsed = labeledValue(node.name, kProbePrefix);
    if (!parsed) return ARX_GLB_BAD_MODEL_SELECTION;
    const ArxReturnCode rc = context.selections.discover(parsed->first);
    if (rc != ARX_OK) return rc;
  }

  if (context.root != glb::kInvalidNodeIndex) {
    const ArxReturnCode rc = visitSelectionHelpers(
        context.data.nodes[context.root], [&](std::string_view name) { return context.selections.discover(name); });
    if (rc != ARX_OK) return rc;
  }
  return context.selections.finalize();
}

ArxReturnCode importSelectionHelpers(ImportContext& context, const cgltf_node& parent, SelectionMask& out_mask) {
  return visitSelectionHelpers(parent, [&](std::string_view name) -> ArxReturnCode {
    SelectionId id = kInvalidSelectionId;
    const ArxReturnCode rc = context.selections.get(name, id);
    if (rc != ARX_OK) return rc;
    out_mask |= selections::bit(id);
    return ARX_OK;
  });
}

ArxReturnCode importSemantics(ImportContext& context) {
  context.model.selections.bone_masks.assign(context.model.skeleton.bones.size(), 0);
  const std::size_t action_count = context.discovery.action_nodes.size();
  if (action_count > static_cast<std::size_t>(kInvalidActionPointIndex)) return ARX_MODEL_TOO_MANY_ACTION_POINTS;
  action_points::reserveActionPointCapacity(context.model.action_points, action_count);
  selections::reserveActionPointCapacity(context.model.selections, action_count);
  bool origin_owner_set = false;
  std::array<bool, 64> probe_set{};

  for (std::size_t bone = 0; bone < context.bone_helpers.size(); ++bone) {
    const std::size_t helper = context.bone_helpers[bone];
    if (helper == glb::kInvalidNodeIndex) continue;
    const cgltf_node& node = context.data.nodes[helper];
    ArxReturnCode rc = importSelectionHelpers(context, node, context.model.selections.bone_masks[bone]);
    if (rc != ARX_OK) return rc;
    bool blob_set = false;
    for (std::size_t child_index = 0; child_index < node.children_count; ++child_index) {
      const cgltf_node* child = node.children[child_index];
      if (child == nullptr || child->name == nullptr) continue;
      const std::string_view name(child->name);
      if (name.starts_with(kSelectionPrefix)) continue;
      if (name.starts_with(kOriginOwnerPrefix)) {
        const std::string_view label = name.substr(kOriginOwnerPrefix.size());
        if (label.empty() || hasDoubleUnderscore(label) || origin_owner_set) return ARX_GLB_BAD_MODEL_BONE_HELPER;
        context.model.skeleton.origin_bone = static_cast<BoneIndex>(bone);
        origin_owner_set = true;
      } else if (name.starts_with(kBlobShadowPrefix)) {
        const auto parsed = labeledValue(name, kBlobShadowPrefix);
        if (!parsed || blob_set) return ARX_GLB_BAD_MODEL_BONE_HELPER;
        const std::optional<float> value = parseFloat(parsed->first);
        if (!value || *value < 0.0f) return ARX_GLB_BAD_MODEL_BONE_HELPER;
        context.model.skeleton.bones[bone].blob_shadow_size = *value * context.units;
        blob_set = true;
      } else {
        log(ARX_LOG_WARN, "GLB -> Model: bone helper '{}' has unrecognized child '{}'; ignored", node.name, name);
      }
    }
  }

  for (std::size_t node_index_value : context.discovery.active_nodes) {
    const cgltf_node& node = context.data.nodes[node_index_value];
    if (node.name == nullptr) continue;
    const std::string_view name(node.name);
    if (name.starts_with(kActionPrefix)) {
      const auto parsed = labeledValue(name, kActionPrefix);
      if (!parsed) return ARX_GLB_BAD_MODEL_ACTION_POINT;
      const std::string_view requested = parsed->first;
      ActionPoint point;
      point.name = requested;
      const IdentifierRepair name_repair = action_points::repairName(point.name);
      point.position = nodePosition(context.graph, context.inverse_root, node_index_value, context.units);
      point.bone = owningBone(context, node_index_value);
      SelectionMask mask = 0;
      ArxReturnCode rc = importSelectionHelpers(context, node, mask);
      if (rc != ARX_OK) return rc;
      context.model.action_points.points.push_back(std::move(point));
      context.model.selections.action_point_masks.push_back(mask);
      if (reportableIdentifierRepair(name_repair))
        log(ARX_LOG_INFO,
            "GLB -> Model: action point name '{}' normalized to '{}'",
            requested,
            context.model.action_points.points.back().name);
      for (std::size_t child = 0; child < node.children_count; ++child) {
        const cgltf_node* value = node.children[child];
        if (value != nullptr && value->name != nullptr && !std::string_view(value->name).starts_with(kSelectionPrefix))
          log(ARX_LOG_WARN, "GLB -> Model: action point '{}' has unexpected descendants; ignored", node.name);
      }
    } else if (name.starts_with(kProbePrefix)) {
      const auto parsed = labeledValue(name, kProbePrefix);
      if (!parsed) return ARX_GLB_BAD_MODEL_SELECTION;
      SelectionId id = kInvalidSelectionId;
      ArxReturnCode rc = context.selections.get(parsed->first, id);
      if (rc != ARX_OK) return rc;
      if (probe_set[id]) {
        log(ARX_LOG_WARN,
            "GLB -> Model: multiple selection probes for '{}'; first used",
            context.model.selections.slots[id].name);
      } else {
        context.model.selections.slots[id].leading_vertex =
            SelectionLeadingVertex{nodePosition(context.graph, context.inverse_root, node_index_value, context.units),
                                   owningBone(context, node_index_value)};
        probe_set[id] = true;
      }
      if (node.children_count != 0)
        log(ARX_LOG_WARN, "GLB -> Model: selection probe '{}' has unexpected descendants; ignored", node.name);
    } else if (name.starts_with(kAnimationPrefix) || name.starts_with(kBonePrefix)) {
      continue;
    } else if (name.starts_with("arx_") && node_index_value != context.root) {
      log(ARX_LOG_WARN, "GLB -> Model: unrecognized semantic node '{}'; ignored", name);
    }
  }

  if (context.root != glb::kInvalidNodeIndex) {
    const ArxReturnCode rc =
        importSelectionHelpers(context, context.data.nodes[context.root], context.model.selections.origin_mask);
    if (rc != ARX_OK) return rc;
  }
  if (!context.model.skeleton.bones.empty() && !origin_owner_set) {
    for (std::size_t index = 0; index < context.model.skeleton.bones.size(); ++index) {
      if (context.model.skeleton.bones[index].parent == kInvalidBoneIndex) {
        context.model.skeleton.origin_bone = static_cast<BoneIndex>(index);
        break;
      }
    }
  }
  return ARX_OK;
}

ArxReturnCode importGeometry(ImportContext& context) {
  std::unordered_map<ImportedVertexKey, VertexIndex, ImportedVertexKeyHash> imported_vertices;
  for (std::size_t node_index_value : context.discovery.terminal_mesh_nodes) {
    const cgltf_node& node = context.data.nodes[node_index_value];
    log(ARX_LOG_WARN, "GLB -> Model: terminal node '{}' has attached mesh; ignored", node.name);
  }
  for (std::size_t node_index_value : context.discovery.mesh_nodes) {
    const cgltf_node& node = context.data.nodes[node_index_value];
    if (node.extensions_count != 0 || node.mesh->extensions_count != 0 || node.has_mesh_gpu_instancing ||
        node.weights_count != 0 || node.mesh->weights_count != 0 || node.mesh->target_names_count != 0) {
      log(ARX_LOG_DEBUG,
          "GLB -> Model geometry failure: node {} '{}' uses an unsupported mesh feature",
          node_index_value,
          node.name != nullptr ? node.name : "");
      return ARX_GLB_UNSUPPORTED_FEATURE;
    }
    const math::Mat4 node_relative = context.inverse_root * context.graph.world[node_index_value];
    for (std::size_t primitive_index = 0; primitive_index < node.mesh->primitives_count; ++primitive_index) {
      const cgltf_primitive& primitive = node.mesh->primitives[primitive_index];
      const auto fail = [&](ArxReturnCode code, std::string_view reason) {
        log(ARX_LOG_DEBUG,
            "GLB -> Model geometry failure: node {} '{}' primitive {} {} (code {})",
            node_index_value,
            node.name != nullptr ? node.name : "",
            primitive_index,
            reason,
            code);
        return code;
      };
      if (primitive.type != cgltf_primitive_type_triangles || primitive.extensions_count != 0 ||
          primitive.targets_count != 0 || primitive.has_draco_mesh_compression || primitive.mappings_count != 0)
        return fail(ARX_GLB_UNSUPPORTED_FEATURE, "uses an unsupported primitive feature");
      const cgltf_attribute* position_attribute = findAttribute(primitive, cgltf_attribute_type_position);
      if (position_attribute == nullptr) return fail(ARX_GLB_BAD_MODEL_POSITION_ATTRIBUTE, "has no POSITION attribute");
      const glb::AccessorView* positions = nullptr;
      ArxReturnCode rc = context.accessors.get(position_attribute->data, positions);
      if (rc != ARX_OK) return fail(rc, "cannot decode POSITION");
      if (glb::validatePositionAccessor(*positions) != ARX_OK)
        return fail(ARX_GLB_BAD_MODEL_POSITION_ATTRIBUTE, "has an invalid POSITION attribute");

      const glb::AccessorView* indices = nullptr;
      std::size_t index_count = positions->count;
      if (primitive.indices != nullptr) {
        rc = context.accessors.get(primitive.indices, indices);
        if (rc != ARX_OK) return fail(rc, "cannot decode indices");
        if (glb::validateIndexAccessor(*indices) != ARX_OK)
          return fail(ARX_GLB_BAD_MODEL_INDEX_ACCESSOR, "has an invalid index accessor");
        index_count = indices->count;
      }
      if (index_count % 3U != 0) return fail(ARX_GLB_BAD_MODEL_GEOMETRY, "does not contain complete triangles");

      const glb::AccessorView* normals = nullptr;
      const cgltf_attribute* normal_attribute = findAttribute(primitive, cgltf_attribute_type_normal);
      if (normal_attribute != nullptr) {
        rc = context.accessors.get(normal_attribute->data, normals);
        if (rc != ARX_OK) return fail(rc, "cannot decode NORMAL");
        if (glb::validateNormalAccessor(*normals) != ARX_OK || normals->count != positions->count)
          return fail(ARX_GLB_BAD_MODEL_NORMAL_ATTRIBUTE, "has an invalid NORMAL attribute");
      }

      ImportedMaterial material;
      rc = importMaterial(context, primitive.material, material);
      if (rc != ARX_OK) return fail(rc, "has an invalid material");
      const glb::AccessorView* texcoords = nullptr;
      const cgltf_attribute* texcoord_attribute =
          findAttribute(primitive, cgltf_attribute_type_texcoord, material.uv_set);
      if (texcoord_attribute != nullptr) {
        rc = context.accessors.get(texcoord_attribute->data, texcoords);
        if (rc != ARX_OK) return fail(rc, "cannot decode the selected TEXCOORD attribute");
        if (glb::validateTexcoordAccessor(*texcoords) != ARX_OK || texcoords->count != positions->count)
          return fail(ARX_GLB_BAD_MODEL_TEXCOORD_ATTRIBUTE, "has an invalid selected TEXCOORD attribute");
      } else if (material.texture != kNoTexture) {
        return fail(ARX_GLB_BAD_MODEL_TEXCOORD_ATTRIBUTE, "lacks the selected TEXCOORD attribute");
      }

      std::vector<SkinAttributeSet> skinning;
      rc = readSkinAttributes(context.accessors, primitive, node, context.skin_bindings, positions->count, skinning);
      if (rc != ARX_OK) return fail(rc, "has invalid skinning attributes");

      const bool skinned = node.skin != nullptr;
      const BoneIndex rigid_bone = skinned ? kInvalidBoneIndex : rigidMeshBone(context, node_index_value);
      if (!skinned && !math::isRotationUniformScale(node_relative))
        return fail(ARX_GLB_MODEL_NON_UNIFORM_SCALE, "has non-uniform scale or shear");
      const bool mirrored = !skinned && math::linearDeterminant(node_relative) < 0.0;

      struct SelectionAttribute {
        SelectionId id = kInvalidSelectionId;
        const glb::AccessorView* values = nullptr;
      };
      std::vector<SelectionAttribute> selection_attributes;
      for (std::size_t attribute_index = 0; attribute_index < primitive.attributes_count; ++attribute_index) {
        const cgltf_attribute& attribute = primitive.attributes[attribute_index];
        if (attribute.type != cgltf_attribute_type_custom || attribute.name == nullptr || attribute.name[0] != '_' ||
            attribute.name[1] == '\0')
          continue;
        SelectionAttribute imported;
        rc = context.selections.get(std::string_view(attribute.name).substr(1), imported.id);
        if (rc != ARX_OK) return fail(rc, "has an invalid selection attribute name");
        rc = context.accessors.get(attribute.data, imported.values);
        if (rc != ARX_OK) return fail(rc, "cannot decode a selection attribute");
        if (glb::validateColorAccessor(*imported.values) != ARX_OK || imported.values->type != cgltf_type_vec4 ||
            imported.values->count != positions->count)
          return fail(ARX_GLB_BAD_MODEL_SELECTION, "has an invalid selection attribute");
        selection_attributes.push_back(imported);
      }

      auto source_index = [&](std::size_t index) {
        return indices == nullptr ? static_cast<std::uint32_t>(index) : indices->indices[index];
      };
      for (std::size_t triangle = 0; triangle < index_count; triangle += 3U) {
        if (context.model.geometry.faces.size() >= static_cast<std::size_t>(kInvalidFaceIndex))
          return fail(ARX_MODEL_TOO_MANY_FACES, "exceeds the Model face limit");
        Face face;
        face.texture = material.texture;
        face.flags = material.flags;
        face.transval = material.transval;
        std::array<std::size_t, 3> order =
            mirrored ? std::array<std::size_t, 3>{0, 2, 1} : std::array<std::size_t, 3>{0, 1, 2};
        std::array<ArxVector3, 3> face_positions{};
        for (std::size_t corner_index = 0; corner_index < 3; ++corner_index) {
          const std::uint32_t source = source_index(triangle + order[corner_index]);
          if (source >= positions->count)
            return fail(ARX_GLB_BAD_MODEL_INDEX_ACCESSOR, "references an out-of-range vertex");
          SkinVertexBinding skin_vertex;
          if (skinned) {
            rc = readSkinVertex(skinning, source, skin_vertex);
            if (rc != ARX_OK) return fail(rc, "contains an invalid skin influence");
          }
          const math::Mat4& relative = skin_vertex.rest_projection != nullptr
                                           ? *skin_vertex.rest_projection
                                           : (skinned ? math::kIdentityMat4 : node_relative);
          const glb::Vec3 position = glb::readVec3(*positions, source);
          const ArxVector3 transformed = math::xformPoint(relative, {position.x, position.y, position.z});
          const ArxVector3 arx_position = toArxPoint(transformed, context.units);
          face_positions[corner_index] = arx_position;

          const BoneIndex bone = skinned ? skin_vertex.bone : rigid_bone;

          SelectionMask mask = 0;
          for (const SelectionAttribute& attribute : selection_attributes) {
            const float member = attribute.values->floats[static_cast<std::size_t>(source) * 4U];
            if (member < 0.0f || member > 1.0f)
              return fail(ARX_GLB_BAD_MODEL_SELECTION, "contains an out-of-range selection weight");
            if (member > 0.5f) mask |= selections::bit(attribute.id);
          }

          const ImportedVertexKey vertex_key{node_index_value, position_attribute->data, source, bone, mask};
          VertexIndex vertex = kInvalidVertexIndex;
          if (const auto found = imported_vertices.find(vertex_key); found != imported_vertices.end()) {
            vertex = found->second;
          } else {
            if (context.model.geometry.vertices.size() >= static_cast<std::size_t>(kInvalidVertexIndex))
              return fail(ARX_MODEL_TOO_MANY_VERTICES, "exceeds the Model vertex limit");
            vertex = static_cast<VertexIndex>(context.model.geometry.vertices.size());
            context.model.geometry.vertices.push_back({arx_position});
            context.model.skeleton.vertex_bones.push_back(bone);
            context.model.selections.vertex_masks.push_back(mask);
            imported_vertices.emplace(vertex_key, vertex);
          }

          Corner& corner = face.corners[corner_index];
          corner.vertex = vertex;
          if (normals != nullptr) {
            const glb::Vec3 normal = glb::readVec3(*normals, source);
            const ArxVector3 transformed_normal =
                math::normalize(math::xformDir(relative, {normal.x, normal.y, normal.z}));
            corner.normal = math::normalize(toArxDirection(transformed_normal));
          }
          if (texcoords != nullptr) {
            const glb::Vec2 uv = glb::readVec2(*texcoords, source);
            corner.u = uv.x;
            corner.v = uv.y;
          }
        }
        face.normal =
            math::normalize(math::cross(face_positions[1] - face_positions[0], face_positions[2] - face_positions[0]));
        if (math::lengthSquaredf(face.normal) == 0.0f)
          return fail(ARX_GLB_BAD_MODEL_GEOMETRY, "contains a degenerate triangle");
        if (normals == nullptr)
          for (Corner& corner : face.corners) corner.normal = face.normal;
        context.model.geometry.faces.push_back(face);
      }
    }
  }
  return context.model.geometry.faces.empty() ? ARX_GLB_NO_MODEL_GEOMETRY : ARX_OK;
}

}  // namespace

ArxReturnCode importModelFromGlbImpl(std::span<const std::uint8_t> bytes, const Model::GlbImportOptions& options,
                                     ModelModules& out, std::vector<AnimationModules>* out_animations,
                                     ArxAnimationConversionReport* report,
                                     std::vector<std::string>* texture_source_paths,
                                     std::vector<AnimationSoundSourceReference>* sound_sources) {
  if (!glb_object::validUnits(options.arx_units_per_glb_unit)) return ARX_INVALID_OPTIONS;
  if (report) *report = {};
  glb::Asset asset;
  ArxReturnCode rc = glb::parse(bytes, asset);
  if (rc != ARX_OK) return rc;
  cgltf_data& data = *asset.data();
  if (data.extensions_required_count != 0) {
    for (std::size_t index = 0; index < data.extensions_required_count; ++index)
      if (data.extensions_required[index] == nullptr) return ARX_GLB_BAD_FORMAT;
    return ARX_GLB_UNSUPPORTED_FEATURE;
  }
  glb::NodeGraph graph;
  rc = glb::buildNodeGraph(data, graph);
  if (rc != ARX_OK) return rc;

  ModelDiscovery discovery;
  rc = discoverModel(data, graph, discovery);
  if (rc != ARX_OK) return rc;
  const std::size_t root = discovery.root;
  math::Mat4 inverse_root = math::kIdentityMat4;
  if (root != glb::kInvalidNodeIndex) {
    const std::optional<math::Mat4> inverse = math::inverseAffine(graph.world[root]);
    if (!inverse) return ARX_GLB_BAD_MODEL_HIERARCHY;
    inverse_root = *inverse;
    if (discovery.outside_meshes != 0)
      log(ARX_LOG_WARN, "GLB -> Model: {} mesh node(s) outside the model origin; ignored", discovery.outside_meshes);
  }

  ModelModules model;
  glb::AccessorCache accessors(asset, asset.data()->accessors_count);
  SkinBindings skin_bindings;
  std::vector<BoneIndex> node_bones(data.nodes_count, kInvalidBoneIndex);
  rc = importSkeleton(asset,
                      accessors,
                      graph,
                      discovery,
                      inverse_root,
                      options.arx_units_per_glb_unit,
                      model,
                      skin_bindings,
                      node_bones);
  if (rc != ARX_OK) return rc;

  ImportContext context(
      asset, accessors, graph, discovery, inverse_root, options.arx_units_per_glb_unit, model, texture_source_paths);
  context.skin_bindings = std::move(skin_bindings);
  context.node_bones = std::move(node_bones);
  for (std::size_t node : graph.preorder) {
    const std::size_t parent = graph.parent[node];
    if (parent == glb::kInvalidNodeIndex) continue;
    context.owning_bones[node] =
        context.node_bones[parent] != kInvalidBoneIndex ? context.node_bones[parent] : context.owning_bones[parent];
  }
  rc = discoverBoneHelpers(context);
  if (rc != ARX_OK) return rc;
  rc = discoverSelections(context);
  if (rc != ARX_OK) return rc;
  rc = importGeometry(context);
  if (rc != ARX_OK) return rc;
  if (!glb::makeTexturePathsUnique(model.textures.textures, "GLB -> Model")) return ARX_GLB_BAD_MODEL_MATERIAL;
  rc = importSemantics(context);
  if (rc != ARX_OK) return rc;

  rc = model_detail::validateStructure(model);
  if (rc != ARX_OK) {
    log(ARX_LOG_DEBUG, "GLB -> Model constructed Model validation failed with code {}", rc);
    return rc;
  }
  std::vector<AnimationModules> animations;
  if (out_animations != nullptr) {
    rc = importAnimations(asset,
                          accessors,
                          graph,
                          discovery,
                          options.arx_units_per_glb_unit,
                          model,
                          context.node_bones,
                          animations,
                          report,
                          sound_sources);
    if (rc != ARX_OK) return rc;
  }
  out = std::move(model);
  if (out_animations != nullptr) *out_animations = std::move(animations);
  return ARX_OK;
}

}  // namespace pistoris::glb_model

namespace pistoris {

ArxReturnCode importModelFromGlb(std::span<const std::uint8_t> glb, const Model::GlbImportOptions& options,
                                 ModelModules& out, std::vector<AnimationModules>* out_animations,
                                 ArxAnimationConversionReport* report, std::vector<std::string>* texture_source_paths,
                                 std::vector<AnimationSoundSourceReference>* sound_sources) {
  return glb_model::importModelFromGlbImpl(
      glb, options, out, out_animations, report, texture_source_paths, sound_sources);
}

}  // namespace pistoris
