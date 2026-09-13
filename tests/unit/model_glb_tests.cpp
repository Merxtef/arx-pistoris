// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#include "doctest/doctest.h"

#include "arx_pistoris/animation.hpp"
#include "arx_pistoris/animation/types.h"
#include "arx_pistoris/base/flags.h"
#include "arx_pistoris/base/indices.h"
#include "arx_pistoris/base/math.h"
#include "arx_pistoris/base/status.h"
#include "arx_pistoris/base/string_view.h"
#include "arx_pistoris/model.hpp"
#include "arx_pistoris/model/bake.hpp"
#include "arx_pistoris/model/types.h"
#include "arx_pistoris/native/tea.hpp"
#include "arx_pistoris/runtime/types.h"
#include "arx_pistoris/sound.h"
#include "arx_pistoris/texture.h"

#include "external/glb/accessor.h"
#include "external/glb/container.h"
#include "external/glb/writer.h"
#include "model_helpers.h"
#include "support/animation_equivalence.h"
#include "support/model_equivalence.h"
#include "utils/log.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <format>
#include <limits>
#include <memory>
#include <nlohmann/json.hpp>
#include <set>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace {

std::string_view view(ArxStringView value) { return {value.data, value.size}; }
ArxStringView view(std::string_view value) { return {value.data(), value.size()}; }

pistoris::SoundIndex addSound(pistoris::Animation& animation, std::string_view path) {
  pistoris::SoundIndex result = pistoris::kNoSound;
  const ArxSoundView sound{view(path), {}};
  const ArxReturnCode rc = animation.addSound(sound, result);
  REQUIRE(rc == ARX_OK);
  return result;
}

nlohmann::json parseGlbJson(std::span<const std::uint8_t> source) {
  std::uint32_t size = 0;
  std::memcpy(&size, source.data() + 12U, sizeof(size));
  return nlohmann::json::parse(source.begin() + 20, source.begin() + 20 + size);
}

std::size_t glbNodeIndex(const nlohmann::json& gltf, std::string_view name) {
  return static_cast<std::size_t>(
      std::ranges::find_if(gltf["nodes"],
                           [name](const nlohmann::json& node) { return node.value("name", std::string{}) == name; }) -
      gltf["nodes"].begin());
}

std::size_t glbNodeIndexWithPrefix(const nlohmann::json& gltf, std::string_view prefix) {
  return static_cast<std::size_t>(std::ranges::find_if(gltf["nodes"],
                                                       [prefix](const nlohmann::json& node) {
                                                         return node.value("name", std::string{}).starts_with(prefix);
                                                       }) -
                                  gltf["nodes"].begin());
}

void configureMotionAnimation(pistoris::Animation& animation) {
  REQUIRE(animation.setName("walk") == ARX_OK);
  REQUIRE(animation.setResourcePath("anim:npc:walk__fast") == ARX_OK);
  const pistoris::SoundIndex step_sound = addSound(animation, "sfx/step__hard.wav");
  std::array<ArxAnimationGroupTransform, 2> first_transforms{};
  std::array<ArxAnimationGroupTransform, 2> second_transforms{};
  first_transforms[1].translation = {1.0f, 2.0f, 3.0f};
  second_transforms[0].translation = {4.0f, 5.0f, 6.0f};
  second_transforms[0].scale = {2.0f, 3.0f, 4.0f};
  second_transforms[1].translation = {7.0f, 8.0f, 9.0f};
  second_transforms[1].rotation = {0.70710678f, 0.0f, 0.0f, 0.70710678f};
  second_transforms[1].scale = {1.5f, 1.0f, 0.5f};
  const std::array<ArxAnimationKeyframeInput, 2> inputs = {{
      {{0, {}, {}, 0, pistoris::kNoSound}, first_transforms.data(), first_transforms.size()},
      {{6, {3.0f, 4.0f, 5.0f}, {0.70710678f, 0.0f, 0.0f, 0.70710678f}, 1, step_sound},
       second_transforms.data(),
       second_transforms.size()},
  }};
  REQUIRE(animation.replaceKeyframes(10, inputs.data(), inputs.size()) == ARX_OK);
}

std::vector<std::uint8_t> makeMotionModelGlb(pistoris::Model& model, float arx_units_per_glb_unit = 10.0f,
                                             ArxAnimationConversionReport* report = nullptr) {
  REQUIRE(pistoris::Model::importNative(model, makeSemanticModelFtl()) == ARX_OK);
  pistoris::Animation animation;
  configureMotionAnimation(animation);
  const std::array<const pistoris::Animation*, 1> animations = {&animation};
  std::vector<std::uint8_t> result;
  REQUIRE(model.exportGlb(result, animations, {.arx_units_per_glb_unit = arx_units_per_glb_unit}, report) == ARX_OK);
  return result;
}

struct ModelGlbLogCapture {
  std::vector<std::string> messages;

  ModelGlbLogCapture() {
    pistoris::log_fn = [](ArxLogLevel, const char* message, void* userdata) {
      if (message) static_cast<ModelGlbLogCapture*>(userdata)->messages.emplace_back(message);
    };
    pistoris::log_ud = this;
  }

  ~ModelGlbLogCapture() {
    pistoris::log_fn = nullptr;
    pistoris::log_ud = nullptr;
  }

  bool containsCode(ArxReturnCode code) const {
    const std::string needle = std::format("code {}", static_cast<int>(code));
    return std::ranges::any_of(messages,
                               [&](const std::string& message) { return message.find(needle) != std::string::npos; });
  }

  bool contains(std::string_view needle) const {
    return std::ranges::any_of(
        messages, [needle](const std::string& message) { return message.find(needle) != std::string::npos; });
  }
};

std::vector<pistoris::VertexIndex> selectionVertices(const pistoris::Model& model, pistoris::SelectionId id) {
  std::size_t count = 0;
  REQUIRE(model.selectionVertexCount(id, count) == ARX_OK);
  std::vector<pistoris::VertexIndex> result(count);
  REQUIRE(model.copySelectionVertices(id, 0, count, result.data()) == ARX_OK);
  return result;
}

bool hasNode(const cgltf_data& data, std::string_view name) {
  return std::ranges::any_of(std::span<const cgltf_node>(data.nodes, data.nodes_count), [name](const cgltf_node& node) {
    return node.name != nullptr && std::string_view(node.name) == name;
  });
}

const cgltf_node* findNode(const cgltf_data& data, std::string_view name) {
  const std::span<const cgltf_node> nodes(data.nodes, data.nodes_count);
  const auto found = std::ranges::find_if(
      nodes, [name](const cgltf_node& node) { return node.name != nullptr && std::string_view(node.name) == name; });
  return found == nodes.end() ? nullptr : &*found;
}

const cgltf_accessor* positionAccessor(const cgltf_primitive& primitive) {
  for (const cgltf_attribute& attribute :
       std::span<const cgltf_attribute>(primitive.attributes, primitive.attributes_count))
    if (attribute.type == cgltf_attribute_type_position) return attribute.data;
  return nullptr;
}

std::vector<std::uint8_t> replaceGlbJson(std::span<const std::uint8_t> source, const nlohmann::json& gltf) {
  REQUIRE(source.size() >= 20U);
  std::uint32_t old_json_size = 0;
  std::memcpy(&old_json_size, source.data() + 12U, sizeof(old_json_size));
  const std::size_t binary_header = 20U + old_json_size;
  REQUIRE(binary_header + 8U <= source.size());
  std::uint32_t binary_size = 0;
  std::memcpy(&binary_size, source.data() + binary_header, sizeof(binary_size));
  REQUIRE(binary_header + 8U + binary_size <= source.size());

  std::string json = gltf.dump();
  while (json.size() % 4U != 0) json.push_back(' ');
  const std::uint32_t total_size = static_cast<std::uint32_t>(20U + json.size() + 8U + binary_size);
  std::vector<std::uint8_t> result;
  result.reserve(total_size);
  const auto append_u32 = [&](std::uint32_t value) {
    const auto* bytes = reinterpret_cast<const std::uint8_t*>(&value);
    result.insert(result.end(), bytes, bytes + sizeof(value));
  };
  append_u32(0x46546C67U);
  append_u32(2U);
  append_u32(total_size);
  append_u32(static_cast<std::uint32_t>(json.size()));
  append_u32(0x4E4F534AU);
  result.insert(result.end(), json.begin(), json.end());
  append_u32(binary_size);
  append_u32(0x004E4942U);
  result.insert(result.end(),
                source.begin() + static_cast<std::ptrdiff_t>(binary_header + 8U),
                source.begin() + static_cast<std::ptrdiff_t>(binary_header + 8U + binary_size));
  return result;
}

std::vector<std::uint8_t> replaceFirstAnimationTime(std::span<const std::uint8_t> source, float value) {
  std::vector<std::uint8_t> result(source.begin(), source.end());
  std::uint32_t json_size = 0;
  std::memcpy(&json_size, result.data() + 12U, sizeof(json_size));
  const nlohmann::json gltf = nlohmann::json::parse(result.begin() + 20, result.begin() + 20 + json_size);
  const std::size_t accessor_index = gltf["animations"][0]["samplers"][0]["input"];
  const nlohmann::json& accessor = gltf["accessors"][accessor_index];
  const nlohmann::json& view = gltf["bufferViews"][static_cast<std::size_t>(accessor["bufferView"])];
  const std::size_t binary_data = 20U + json_size + 8U;
  const std::size_t offset = binary_data + view.value("byteOffset", 0U) + accessor.value("byteOffset", 0U);
  REQUIRE(offset + sizeof(value) <= result.size());
  std::memcpy(result.data() + offset, &value, sizeof(value));
  return result;
}

std::vector<std::uint8_t> replaceInverseBindComponent(std::span<const std::uint8_t> source, std::size_t matrix,
                                                      std::size_t component, float value) {
  REQUIRE(component < 16U);
  std::vector<std::uint8_t> result(source.begin(), source.end());
  std::uint32_t json_size = 0;
  std::memcpy(&json_size, result.data() + 12U, sizeof(json_size));
  const nlohmann::json gltf = nlohmann::json::parse(result.begin() + 20, result.begin() + 20 + json_size);
  const std::size_t accessor_index = gltf["skins"][0]["inverseBindMatrices"];
  const nlohmann::json& accessor = gltf["accessors"][accessor_index];
  const nlohmann::json& view = gltf["bufferViews"][static_cast<std::size_t>(accessor["bufferView"])];
  const std::size_t binary_data = 20U + json_size + 8U;
  const std::size_t stride = view.value("byteStride", 16U * sizeof(float));
  const std::size_t offset = binary_data + view.value("byteOffset", 0U) + accessor.value("byteOffset", 0U) +
                             matrix * stride + component * sizeof(float);
  REQUIRE(offset + sizeof(value) <= result.size());
  std::memcpy(result.data() + offset, &value, sizeof(value));
  return result;
}

std::vector<std::uint8_t> replaceAnimationSettings(std::span<const std::uint8_t> source, std::string name) {
  std::uint32_t json_size = 0;
  std::memcpy(&json_size, source.data() + 12U, sizeof(json_size));
  nlohmann::json gltf = nlohmann::json::parse(source.begin() + 20, source.begin() + 20 + json_size);
  const auto helper = std::ranges::find_if(gltf["nodes"], [](const nlohmann::json& node) {
    return node.value("name", std::string{}).starts_with("arx_animation__");
  });
  REQUIRE(helper != gltf["nodes"].end());
  bool replaced = false;
  for (const nlohmann::json& child : (*helper)["children"]) {
    nlohmann::json& node = gltf["nodes"][static_cast<std::size_t>(child)];
    if (!node.value("name", std::string{}).starts_with("SETTINGS__")) continue;
    node["name"] = std::move(name);
    replaced = true;
    break;
  }
  REQUIRE(replaced);
  return replaceGlbJson(source, gltf);
}

std::vector<std::uint8_t> scaleFirstAnimationRotation(std::span<const std::uint8_t> source, float factor) {
  std::vector<std::uint8_t> result(source.begin(), source.end());
  std::uint32_t json_size = 0;
  std::memcpy(&json_size, result.data() + 12U, sizeof(json_size));
  const nlohmann::json gltf = nlohmann::json::parse(result.begin() + 20, result.begin() + 20 + json_size);
  std::size_t accessor_index = gltf["accessors"].size();
  for (const nlohmann::json& channel : gltf["animations"][0]["channels"]) {
    if (channel["target"].value("path", std::string{}) != "rotation") continue;
    const std::size_t sampler = channel["sampler"];
    accessor_index = gltf["animations"][0]["samplers"][sampler]["output"];
    break;
  }
  REQUIRE(accessor_index < gltf["accessors"].size());
  const nlohmann::json& accessor = gltf["accessors"][accessor_index];
  const nlohmann::json& buffer_view = gltf["bufferViews"][static_cast<std::size_t>(accessor["bufferView"])];
  const std::size_t binary_data = 20U + json_size + 8U;
  const std::size_t start = binary_data + buffer_view.value("byteOffset", 0U) + accessor.value("byteOffset", 0U);
  const std::size_t stride = buffer_view.value("byteStride", 4U * sizeof(float));
  for (std::size_t index = 0; index < static_cast<std::size_t>(accessor["count"]); ++index) {
    const std::size_t offset = start + index * stride;
    REQUIRE(offset + 4U * sizeof(float) <= result.size());
    for (std::size_t component = 0; component < 4U; ++component) {
      float value = 0.0f;
      std::memcpy(&value, result.data() + offset + component * sizeof(float), sizeof(value));
      value *= factor;
      std::memcpy(result.data() + offset + component * sizeof(float), &value, sizeof(value));
    }
  }
  return result;
}

std::vector<std::uint8_t> addMorphTarget(std::span<const std::uint8_t> source) {
  std::uint32_t json_size = 0;
  std::memcpy(&json_size, source.data() + 12U, sizeof(json_size));
  nlohmann::json gltf = nlohmann::json::parse(source.begin() + 20, source.begin() + 20 + json_size);
  gltf["meshes"][0]["primitives"][0]["targets"] = nlohmann::json::array({{{"POSITION", 0}}});
  return replaceGlbJson(source, gltf);
}

std::vector<std::uint8_t> attachMeshToTerminalNode(std::span<const std::uint8_t> source, std::string_view name) {
  std::uint32_t json_size = 0;
  std::memcpy(&json_size, source.data() + 12U, sizeof(json_size));
  nlohmann::json gltf = nlohmann::json::parse(source.begin() + 20, source.begin() + 20 + json_size);
  nlohmann::json& nodes = gltf["nodes"];
  std::size_t parent_index = nodes.size();
  for (std::size_t index = 0; index < nodes.size(); ++index)
    if (nodes[index].value("name", std::string{}) == "1__child") parent_index = index;
  REQUIRE(parent_index != nodes.size());
  const std::size_t node_index = nodes.size();
  nodes.push_back({{"mesh", 0}, {"name", name}});
  nodes[parent_index]["children"].push_back(node_index);
  return replaceGlbJson(source, gltf);
}

enum class SkinFixture : std::uint8_t {
  kPrimary,
  kTwoSets,
  kMissingPrimary,
  kUnpairedSecondary,
  kNonUniformBind,
};

std::vector<std::uint8_t> makeSkinnedModelGlb(SkinFixture fixture, bool reversed_hierarchy = false,
                                              bool joint_outside_root = false, bool extra_inverse_bind = false) {
  pistoris::glb::Builder builder;
  const std::array<pistoris::glb::Vec3, 3> positions = {{{0.0f, 0.0f, 0.0f}, {1.0f, 0.0f, 0.0f}, {0.0f, 1.0f, 0.0f}}};
  const std::array<std::uint32_t, 3> indices = {0, 1, 2};
  const std::array<std::array<std::uint16_t, 4>, 3> primary_joints = {
      std::array<std::uint16_t, 4>{0, 0, 0, 0},
      std::array<std::uint16_t, 4>{0, 0, 0, 0},
      std::array<std::uint16_t, 4>{0, 0, 0, 0},
  };
  const std::array<std::array<std::uint16_t, 4>, 3> secondary_joints = {
      std::array<std::uint16_t, 4>{1, 0, 0, 0},
      std::array<std::uint16_t, 4>{1, 0, 0, 0},
      std::array<std::uint16_t, 4>{1, 0, 0, 0},
  };
  const std::array<std::array<float, 4>, 3> primary_weights = {
      std::array<float, 4>{0.25f, 0.0f, 0.0f, 0.0f},
      std::array<float, 4>{0.25f, 0.0f, 0.0f, 0.0f},
      std::array<float, 4>{0.25f, 0.0f, 0.0f, 0.0f},
  };
  const std::array<std::array<float, 4>, 3> secondary_weights = {
      std::array<float, 4>{0.75f, 0.0f, 0.0f, 0.0f},
      std::array<float, 4>{0.75f, 0.0f, 0.0f, 0.0f},
      std::array<float, 4>{0.75f, 0.0f, 0.0f, 0.0f},
  };

  pistoris::glb::Primitive primitive;
  primitive.indices =
      builder.addAccessor(std::span<const std::uint32_t>(indices), cgltf_component_type_r_32u, cgltf_type_scalar);
  primitive.attributes.emplace_back("POSITION", builder.addVec3Accessor(positions));
  const auto add_skin_set = [&](std::size_t set, bool add_joints, bool add_weights) {
    const auto& joints = set == 0 ? primary_joints : secondary_joints;
    const auto& weights = set == 0 ? primary_weights : secondary_weights;
    if (add_joints)
      primitive.attributes.emplace_back(
          "JOINTS_" + std::to_string(set),
          builder.addAccessor(
              std::span<const std::array<std::uint16_t, 4>>(joints), cgltf_component_type_r_16u, cgltf_type_vec4));
    if (add_weights)
      primitive.attributes.emplace_back(
          "WEIGHTS_" + std::to_string(set),
          builder.addAccessor(
              std::span<const std::array<float, 4>>(weights), cgltf_component_type_r_32f, cgltf_type_vec4));
  };
  if (fixture != SkinFixture::kMissingPrimary) add_skin_set(0, true, true);
  if (fixture == SkinFixture::kTwoSets || fixture == SkinFixture::kMissingPrimary) add_skin_set(1, true, true);
  if (fixture == SkinFixture::kUnpairedSecondary) add_skin_set(1, true, false);

  const int mesh = builder.addMesh("model", {std::move(primitive)});
  const int mesh_node = builder.addNode("mesh", mesh);
  const int root = builder.addNode("arx_model_origin__origin");
  builder.addChild(root, mesh_node);
  builder.addRoot(root);

  const int bone0 = builder.addNode("0__root");
  const int bone1 = builder.addNode("1__child");
  if (joint_outside_root) {
    const int outside = builder.addNode("outside");
    builder.addChild(root, bone0);
    builder.addChild(outside, bone1);
    builder.addRoot(outside);
  } else if (reversed_hierarchy) {
    builder.addChild(root, bone1);
    builder.addChild(bone1, bone0);
  } else {
    builder.addChild(root, bone0);
    builder.addChild(bone0, bone1);
  }

  const std::array<int, 2> joints = {bone0, bone1};
  constexpr std::array<float, 16> kIdentity = {1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1};
  std::vector<std::array<float, 16>> inverse_bind = {kIdentity, kIdentity};
  if (extra_inverse_bind) inverse_bind.push_back(kIdentity);
  if (fixture == SkinFixture::kNonUniformBind) inverse_bind[0][0] = 2.0f;
  const int inverse_bind_accessor = builder.addAccessor(
      std::span<const std::array<float, 16>>(inverse_bind), cgltf_component_type_r_32f, cgltf_type_mat4);
  const int skin = builder.addSkin("skeleton", joints, bone0, inverse_bind_accessor);
  builder.setNodeSkin(mesh_node, skin);

  std::vector<std::uint8_t> encoded;
  REQUIRE(builder.write(encoded) == ARX_OK);
  return encoded;
}

std::vector<std::uint8_t> makeTexturedModelGlb(std::string first_uri, std::string second_uri,
                                               bool reuse_first_image = false) {
  pistoris::glb::Builder builder;
  const std::array<pistoris::glb::Vec3, 3> positions = {{{0.0f, 0.0f, 0.0f}, {1.0f, 0.0f, 0.0f}, {0.0f, 1.0f, 0.0f}}};
  const std::array<pistoris::glb::Vec2, 3> texcoords = {{{0.0f, 0.0f}, {1.0f, 0.0f}, {0.0f, 1.0f}}};
  const std::array<std::uint32_t, 3> indices = {0, 1, 2};
  const int position_accessor = builder.addVec3Accessor(positions);
  const int texcoord_accessor =
      builder.addAccessor(std::span<const pistoris::glb::Vec2>(texcoords), cgltf_component_type_r_32f, cgltf_type_vec2);
  const int index_accessor =
      builder.addAccessor(std::span<const std::uint32_t>(indices), cgltf_component_type_r_32u, cgltf_type_scalar);
  const int first_texture = builder.addExternalTexture("first", std::move(first_uri));
  const int second_texture =
      reuse_first_image ? first_texture : builder.addExternalTexture("second", std::move(second_uri));
  const int first_material = builder.addMaterial("foo", first_texture, 0, 1.0f);
  const int second_material = builder.addMaterial("foo", second_texture, 0, 1.0f);
  const auto primitive = [&](int material) {
    pistoris::glb::Primitive result;
    result.indices = index_accessor;
    result.material = material;
    result.attributes = {{"POSITION", position_accessor}, {"TEXCOORD_0", texcoord_accessor}};
    return result;
  };
  const int mesh = builder.addMesh("model", {primitive(first_material), primitive(second_material)});
  const int root = builder.addNode("arx_model_origin__origin");
  builder.addChild(root, builder.addNode("mesh", mesh));
  builder.addRoot(root);
  std::vector<std::uint8_t> encoded;
  REQUIRE(builder.write(encoded) == ARX_OK);
  return encoded;
}

std::vector<std::uint8_t> makeSharedSemanticModelGlb() {
  pistoris::glb::Builder builder;
  const std::array<pistoris::glb::Vec3, 3> positions = {{{0.0f, 0.0f, 0.0f}, {1.0f, 0.0f, 0.0f}, {0.0f, 1.0f, 0.0f}}};
  const std::array<std::uint32_t, 3> indices = {0, 1, 2};
  const std::array<std::array<std::uint16_t, 4>, 3> root_joints = {
      std::array<std::uint16_t, 4>{0, 0, 0, 0},
      std::array<std::uint16_t, 4>{0, 0, 0, 0},
      std::array<std::uint16_t, 4>{0, 0, 0, 0},
  };
  const std::array<std::array<std::uint16_t, 4>, 3> child_joints = {
      std::array<std::uint16_t, 4>{1, 0, 0, 0},
      std::array<std::uint16_t, 4>{1, 0, 0, 0},
      std::array<std::uint16_t, 4>{1, 0, 0, 0},
  };
  const std::array<std::array<float, 4>, 3> weights = {
      std::array<float, 4>{1.0f, 0.0f, 0.0f, 0.0f},
      std::array<float, 4>{1.0f, 0.0f, 0.0f, 0.0f},
      std::array<float, 4>{1.0f, 0.0f, 0.0f, 0.0f},
  };
  const std::array<std::array<float, 4>, 3> selected = {
      std::array<float, 4>{1.0f, 1.0f, 1.0f, 1.0f},
      std::array<float, 4>{1.0f, 1.0f, 1.0f, 1.0f},
      std::array<float, 4>{1.0f, 1.0f, 1.0f, 1.0f},
  };
  const std::array<std::array<float, 4>, 3> unselected = {
      std::array<float, 4>{0.0f, 0.0f, 0.0f, 1.0f},
      std::array<float, 4>{0.0f, 0.0f, 0.0f, 1.0f},
      std::array<float, 4>{0.0f, 0.0f, 0.0f, 1.0f},
  };

  const int position_accessor = builder.addVec3Accessor(positions);
  const int index_accessor =
      builder.addAccessor(std::span<const std::uint32_t>(indices), cgltf_component_type_r_32u, cgltf_type_scalar);
  const int root_joint_accessor = builder.addAccessor(
      std::span<const std::array<std::uint16_t, 4>>(root_joints), cgltf_component_type_r_16u, cgltf_type_vec4);
  const int child_joint_accessor = builder.addAccessor(
      std::span<const std::array<std::uint16_t, 4>>(child_joints), cgltf_component_type_r_16u, cgltf_type_vec4);
  const int weight_accessor =
      builder.addAccessor(std::span<const std::array<float, 4>>(weights), cgltf_component_type_r_32f, cgltf_type_vec4);
  const int selected_accessor =
      builder.addAccessor(std::span<const std::array<float, 4>>(selected), cgltf_component_type_r_32f, cgltf_type_vec4);
  const int unselected_accessor = builder.addAccessor(
      std::span<const std::array<float, 4>>(unselected), cgltf_component_type_r_32f, cgltf_type_vec4);
  const auto primitive = [&](int joint_accessor, int selection_accessor) {
    pistoris::glb::Primitive result;
    result.indices = index_accessor;
    result.attributes = {
        {"POSITION", position_accessor},
        {"JOINTS_0", joint_accessor},
        {"WEIGHTS_0", weight_accessor},
        {"_HEAD", selection_accessor},
    };
    return result;
  };
  const int mesh = builder.addMesh("model",
                                   {primitive(root_joint_accessor, selected_accessor),
                                    primitive(child_joint_accessor, selected_accessor),
                                    primitive(root_joint_accessor, unselected_accessor)});
  const int mesh_node = builder.addNode("mesh", mesh);
  const int root = builder.addNode("arx_model_origin__origin");
  const int bone0 = builder.addNode("0__root");
  const int bone1 = builder.addNode("1__child");
  builder.addChild(root, mesh_node);
  builder.addChild(root, bone0);
  builder.addChild(bone0, bone1);
  builder.addRoot(root);

  const std::array<int, 2> joints = {bone0, bone1};
  constexpr std::array<float, 16> kIdentity = {1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1};
  const std::array<std::array<float, 16>, 2> inverse_bind = {kIdentity, kIdentity};
  const int inverse_bind_accessor = builder.addAccessor(
      std::span<const std::array<float, 16>>(inverse_bind), cgltf_component_type_r_32f, cgltf_type_mat4);
  builder.setNodeSkin(mesh_node, builder.addSkin("skeleton", joints, bone0, inverse_bind_accessor));

  std::vector<std::uint8_t> encoded;
  REQUIRE(builder.write(encoded) == ARX_OK);
  return encoded;
}

std::vector<std::uint8_t> makeMultiSkinModelGlb(bool shifted_projection = false) {
  pistoris::glb::Builder builder;
  const std::array<std::uint32_t, 3> indices = {0, 1, 2};
  const std::array<std::array<std::uint16_t, 4>, 3> joints = {
      std::array<std::uint16_t, 4>{0, 0, 0, 0},
      std::array<std::uint16_t, 4>{0, 0, 0, 0},
      std::array<std::uint16_t, 4>{0, 0, 0, 0},
  };
  const std::array<std::array<float, 4>, 3> weights = {
      std::array<float, 4>{1.0f, 0.0f, 0.0f, 0.0f},
      std::array<float, 4>{1.0f, 0.0f, 0.0f, 0.0f},
      std::array<float, 4>{1.0f, 0.0f, 0.0f, 0.0f},
  };
  const auto make_primitive = [&](std::array<pistoris::glb::Vec3, 3> positions) {
    pistoris::glb::Primitive primitive;
    primitive.indices =
        builder.addAccessor(std::span<const std::uint32_t>(indices), cgltf_component_type_r_32u, cgltf_type_scalar);
    primitive.attributes.emplace_back("POSITION", builder.addVec3Accessor(positions));
    primitive.attributes.emplace_back(
        "JOINTS_0",
        builder.addAccessor(
            std::span<const std::array<std::uint16_t, 4>>(joints), cgltf_component_type_r_16u, cgltf_type_vec4));
    primitive.attributes.emplace_back(
        "WEIGHTS_0",
        builder.addAccessor(
            std::span<const std::array<float, 4>>(weights), cgltf_component_type_r_32f, cgltf_type_vec4));
    return primitive;
  };

  const int root = builder.addNode("arx_model_origin__origin");
  builder.addRoot(root);
  const int bone0 = builder.addNode("0__root");
  const int bone1 = builder.addNode("1__child");
  builder.setNodeTranslation(bone1, {0.0f, 1.0f, 0.0f});
  builder.addChild(root, bone0);
  builder.addChild(bone0, bone1);

  const int first_mesh =
      builder.addMesh("first", {make_primitive({{{0.0f, 0.0f, 0.0f}, {1.0f, 0.0f, 0.0f}, {0.0f, 1.0f, 0.0f}}})});
  const int second_mesh =
      builder.addMesh("second", {make_primitive({{{2.0f, 0.0f, 0.0f}, {3.0f, 0.0f, 0.0f}, {2.0f, 1.0f, 0.0f}}})});
  const int first_node = builder.addNode("first", first_mesh);
  const int second_node = builder.addNode("second", second_mesh);
  builder.addChild(root, first_node);
  builder.addChild(root, second_node);

  const std::array<int, 2> skin_joints = {bone0, bone1};
  const std::array<std::array<float, 16>, 2> inverse_bind = {
      std::array<float, 16>{1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1},
      std::array<float, 16>{1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, -1, 0, 1},
  };
  const int inverse_bind_accessor = builder.addAccessor(
      std::span<const std::array<float, 16>>(inverse_bind), cgltf_component_type_r_32f, cgltf_type_mat4);
  std::array<std::array<float, 16>, 2> second_inverse_bind = inverse_bind;
  if (shifted_projection) second_inverse_bind[0][12] = -1.0f;
  const int second_inverse_bind_accessor = builder.addAccessor(
      std::span<const std::array<float, 16>>(second_inverse_bind), cgltf_component_type_r_32f, cgltf_type_mat4);
  builder.setNodeSkin(first_node, builder.addSkin("first", skin_joints, bone0, inverse_bind_accessor));
  builder.setNodeSkin(second_node, builder.addSkin("second", skin_joints, bone0, second_inverse_bind_accessor));

  std::vector<std::uint8_t> encoded;
  REQUIRE(builder.write(encoded) == ARX_OK);
  return encoded;
}

enum class OrdinaryMeshPlacement : std::uint8_t {
  kContent,
  kChildBone,
  kBelowChildBone,
  kChildBoneNode,
};

struct TransformedSkinFixture {
  std::array<float, 3> origin_scale = {1.0f, 1.0f, 1.0f};
  std::array<float, 3> content_scale = {1.0f, 1.0f, 1.0f};
  std::array<float, 3> mesh_scale = {1.0f, 1.0f, 1.0f};
  std::array<float, 3> ordinary_scale = {1.0f, 1.0f, 1.0f};
  ArxQuat root_rotation{};
  OrdinaryMeshPlacement ordinary_mesh_placement = OrdinaryMeshPlacement::kContent;
  bool inverse_binds = true;
  bool identity_inverse_binds = false;
  bool unbound_vertex = false;
  bool asymmetric_rotation_sampling = false;
};

std::vector<std::uint8_t> makeTransformedSkinAnimationGlb(const TransformedSkinFixture& fixture) {
  pistoris::glb::Builder builder;
  const std::array<pistoris::glb::Vec3, 3> positions = {{{0.0f, 0.0f, 0.0f}, {1.0f, 0.0f, 0.0f}, {0.0f, 1.0f, 0.0f}}};
  const std::array<std::uint32_t, 3> indices = {0, 1, 2};
  const std::array<std::array<std::uint16_t, 4>, 3> joints = {
      std::array<std::uint16_t, 4>{0, 0, 0, 0},
      std::array<std::uint16_t, 4>{0, 0, 0, 0},
      std::array<std::uint16_t, 4>{0, 0, 0, 0},
  };
  std::array<std::array<float, 4>, 3> weights = {
      std::array<float, 4>{1.0f, 0.0f, 0.0f, 0.0f},
      std::array<float, 4>{1.0f, 0.0f, 0.0f, 0.0f},
      std::array<float, 4>{1.0f, 0.0f, 0.0f, 0.0f},
  };
  if (fixture.unbound_vertex) weights[1].fill(0.0f);

  pistoris::glb::Primitive primitive;
  primitive.indices =
      builder.addAccessor(std::span<const std::uint32_t>(indices), cgltf_component_type_r_32u, cgltf_type_scalar);
  primitive.attributes.emplace_back("POSITION", builder.addVec3Accessor(positions));
  primitive.attributes.emplace_back(
      "JOINTS_0",
      builder.addAccessor(
          std::span<const std::array<std::uint16_t, 4>>(joints), cgltf_component_type_r_16u, cgltf_type_vec4));
  primitive.attributes.emplace_back(
      "WEIGHTS_0",
      builder.addAccessor(std::span<const std::array<float, 4>>(weights), cgltf_component_type_r_32f, cgltf_type_vec4));

  const int mesh = builder.addMesh("model", {std::move(primitive)});
  pistoris::glb::Primitive ordinary_primitive;
  ordinary_primitive.indices =
      builder.addAccessor(std::span<const std::uint32_t>(indices), cgltf_component_type_r_32u, cgltf_type_scalar);
  const std::array<pistoris::glb::Vec3, 3> ordinary_positions = {
      {{0.0f, 0.0f, 0.0f}, {2.0f, 0.0f, 0.0f}, {0.0f, 2.0f, 0.0f}}};
  ordinary_primitive.attributes.emplace_back("POSITION", builder.addVec3Accessor(ordinary_positions));
  const int ordinary_mesh = builder.addMesh("ordinary", {std::move(ordinary_primitive)});
  const bool ordinary_on_bone = fixture.ordinary_mesh_placement == OrdinaryMeshPlacement::kChildBoneNode;
  const int root = builder.addNode("arx_model_origin__origin");
  const int content = builder.addNode("character");
  const int mesh_node = builder.addNode("mesh", mesh);
  const int ordinary_mesh_node = ordinary_on_bone ? -1 : builder.addNode("ordinary", ordinary_mesh);
  const int bone0 = builder.addNode("0__root");
  const int bone1 = builder.addNode("1__child", ordinary_on_bone ? ordinary_mesh : -1);
  const int ordinary_parent = fixture.ordinary_mesh_placement == OrdinaryMeshPlacement::kBelowChildBone
                                  ? builder.addNode("ordinary_parent")
                                  : -1;
  builder.setNodeTranslation(bone0, {0.0f, 1.0f, 0.0f});
  builder.setNodeRotation(bone0, fixture.root_rotation);
  builder.setNodeTranslation(bone1, {0.0f, 1.0f, 0.0f});
  builder.addChild(root, content);
  builder.addChild(content, mesh_node);
  if (fixture.ordinary_mesh_placement == OrdinaryMeshPlacement::kContent) builder.addChild(content, ordinary_mesh_node);
  builder.addChild(content, bone0);
  builder.addChild(bone0, bone1);
  if (fixture.ordinary_mesh_placement == OrdinaryMeshPlacement::kChildBone) {
    builder.addChild(bone1, ordinary_mesh_node);
  } else if (fixture.ordinary_mesh_placement == OrdinaryMeshPlacement::kBelowChildBone) {
    builder.addChild(bone1, ordinary_parent);
    builder.addChild(ordinary_parent, ordinary_mesh_node);
  }
  builder.addRoot(root);

  const std::array<float, 3> inverse_scene_scale = {
      1.0f / (fixture.origin_scale[0] * fixture.content_scale[0]),
      1.0f / (fixture.origin_scale[1] * fixture.content_scale[1]),
      1.0f / (fixture.origin_scale[2] * fixture.content_scale[2]),
  };
  std::array<std::array<float, 16>, 2> inverse_bind = {
      std::array<float, 16>{inverse_scene_scale[0],
                            0,
                            0,
                            0,
                            0,
                            inverse_scene_scale[1],
                            0,
                            0,
                            0,
                            0,
                            inverse_scene_scale[2],
                            0,
                            0,
                            -1,
                            0,
                            1},
      std::array<float, 16>{inverse_scene_scale[0],
                            0,
                            0,
                            0,
                            0,
                            inverse_scene_scale[1],
                            0,
                            0,
                            0,
                            0,
                            inverse_scene_scale[2],
                            0,
                            0,
                            -2,
                            0,
                            1},
  };
  if (fixture.identity_inverse_binds) {
    constexpr std::array<float, 16> kIdentity = {1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1};
    inverse_bind.fill(kIdentity);
  }
  const std::array<int, 2> skin_joints = {bone0, bone1};
  if (fixture.inverse_binds) {
    const int inverse_bind_accessor = builder.addAccessor(
        std::span<const std::array<float, 16>>(inverse_bind), cgltf_component_type_r_32f, cgltf_type_mat4);
    builder.setNodeSkin(mesh_node, builder.addSkin("skeleton", skin_joints, bone0, inverse_bind_accessor));
  } else {
    builder.setNodeSkin(mesh_node, builder.addSkin("skeleton", skin_joints, bone0));
  }

  std::vector<float> translation_times;
  std::vector<float> rotation_times;
  std::vector<std::array<float, 4>> child_rotations;
  if (fixture.asymmetric_rotation_sampling) {
    translation_times = {0.0f, 2.0f / 24.0f, 8.0f / 24.0f};
    rotation_times = {0.0f, 8.0f / 24.0f};
    child_rotations = {
        std::array<float, 4>{0.0f, 0.0f, 0.0f, 1.0f},
        std::array<float, 4>{0.8660254f, 0.0f, 0.0f, 0.5f},
    };
  } else {
    translation_times = {0.0f, 1.0f / 24.0f};
    rotation_times = translation_times;
    child_rotations = {
        std::array<float, 4>{0.0f, 0.0f, 0.0f, 1.0f},
        std::array<float, 4>{0.0f, 0.0f, 0.70710678f, 0.70710678f},
    };
  }
  const std::vector<pistoris::glb::Vec3> translations(translation_times.size(), pistoris::glb::Vec3{0.0f, 1.0f, 0.0f});
  const int translation_time_accessor = builder.addTimeAccessor(translation_times);
  const int rotation_time_accessor = builder.addTimeAccessor(rotation_times);
  const int translation_accessor = builder.addVec3Accessor(translations);
  const int child_rotation_accessor = builder.addAccessor(
      std::span<const std::array<float, 4>>(child_rotations), cgltf_component_type_r_32f, cgltf_type_vec4);
  builder.addAnimation(
      "pose",
      {
          {translation_time_accessor, translation_accessor, bone0, pistoris::glb::AnimationPath::kTranslation},
          {translation_time_accessor, translation_accessor, bone1, pistoris::glb::AnimationPath::kTranslation},
          {rotation_time_accessor, child_rotation_accessor, bone1, pistoris::glb::AnimationPath::kRotation},
      });

  std::vector<std::uint8_t> encoded;
  REQUIRE(builder.write(encoded) == ARX_OK);
  std::uint32_t json_size = 0;
  std::memcpy(&json_size, encoded.data() + 12U, sizeof(json_size));
  nlohmann::json gltf = nlohmann::json::parse(encoded.begin() + 20, encoded.begin() + 20 + json_size);
  const auto set_scale = [&](std::string_view name, const std::array<float, 3>& scale) {
    const auto found = std::ranges::find_if(
        gltf["nodes"], [name](const nlohmann::json& node) { return node.value("name", std::string{}) == name; });
    REQUIRE(found != gltf["nodes"].end());
    (*found)["scale"] = scale;
  };
  set_scale("arx_model_origin__origin", fixture.origin_scale);
  set_scale("character", fixture.content_scale);
  set_scale("mesh", fixture.mesh_scale);
  if (!ordinary_on_bone) set_scale("ordinary", fixture.ordinary_scale);
  return replaceGlbJson(encoded, gltf);
}

}  // namespace

TEST_SUITE("Model GLB") {
  TEST_CASE("Model equivalence matches approximate vertices one-to-one") {
    pistoris::Model lhs;
    pistoris::Model rhs;
    pistoris::VertexIndex index = pistoris::kInvalidVertexIndex;
    REQUIRE(lhs.addVertex({{0.0f, 1.0f, 0.0f}}, index) == ARX_OK);
    REQUIRE(lhs.addVertex({{0.0005f, 0.0f, 0.0f}}, index) == ARX_OK);
    REQUIRE(rhs.addVertex({{0.0f, 0.0f, 0.0f}}, index) == ARX_OK);
    REQUIRE(rhs.addVertex({{0.0005f, 1.0f, 0.0f}}, index) == ARX_OK);

    test_support::checkModelsEquivalent(lhs, rhs, {.comparison_epsilon = 0.001f});
  }

  TEST_CASE("Roundtrips Model semantics without extras") {
    pistoris::Model source;
    REQUIRE(pistoris::Model::importNative(source, makeSemanticModelFtl()) == ARX_OK);
    REQUIRE(source.setOrigin({1}) == ARX_OK);
    ArxModelSelection empty_selection{};
    empty_selection.name = {"empty", 5};
    pistoris::SelectionId empty_selection_id = pistoris::kInvalidSelectionId;
    REQUIRE(source.addSelection(empty_selection, empty_selection_id) == ARX_OK);
    ArxTextureView unused_texture{};
    unused_texture.path = {"graph/obj3d/textures/unused", 27};
    pistoris::TextureIndex unused_texture_index = pistoris::kNoTexture;
    REQUIRE(source.addTexture(unused_texture, unused_texture_index) == ARX_OK);

    std::vector<std::uint8_t> encoded;
    REQUIRE(source.exportGlb(encoded) == ARX_OK);
    REQUIRE(!encoded.empty());

    pistoris::glb::Asset asset;
    REQUIRE(pistoris::glb::parse(std::span<const std::uint8_t>(encoded), asset) == ARX_OK);
    const cgltf_data& glb = *asset.data();
    REQUIRE(glb.scene != nullptr);
    REQUIRE(glb.scene->nodes_count == 2);
    const cgltf_node* origin = findNode(glb, "arx_model_origin__origin");
    const cgltf_node* bones_parent = findNode(glb, "bones_parent");
    const cgltf_node* root_helper = findNode(glb, "arx_bone__root");
    const cgltf_node* chest_helper = findNode(glb, "arx_bone__chest");
    const cgltf_node* root_settings = findNode(glb, "SETTINGS__BLOB_SHADOW_0.2__settings");
    const cgltf_node* chest_settings = findNode(glb, "SETTINGS__BLOB_SHADOW_0.1__settings");
    const cgltf_node* origin_owner = findNode(glb, "ORIGIN_OWNER__origin");
    REQUIRE(origin != nullptr);
    REQUIRE(bones_parent != nullptr);
    REQUIRE(root_helper != nullptr);
    REQUIRE(chest_helper != nullptr);
    REQUIRE(root_settings != nullptr);
    REQUIRE(chest_settings != nullptr);
    REQUIRE(origin_owner != nullptr);
    CHECK(origin->parent == nullptr);
    CHECK(bones_parent->parent == nullptr);
    CHECK(root_helper->parent == bones_parent);
    CHECK(chest_helper->parent == bones_parent);
    CHECK(root_settings->parent == root_helper);
    CHECK(chest_settings->parent == chest_helper);
    CHECK(origin_owner->parent == chest_helper);
    CHECK(hasNode(glb, "arx_action__view_attach__action_0"));
    CHECK(hasNode(glb, "arx_selection_probe__cut_head__point"));
    CHECK(hasNode(glb, "SETTINGS__BLOB_SHADOW_0.2__settings"));
    CHECK(hasNode(glb, "SETTINGS__BLOB_SHADOW_0.1__settings"));
    REQUIRE(glb.meshes_count == 1);
    CHECK(glb.textures_count == 1);
    CHECK(glb.images_count == 1);
    REQUIRE(glb.meshes[0].primitives_count == 1);
    const cgltf_primitive& primitive = glb.meshes[0].primitives[0];
    const auto has_attribute = [&](std::string_view name) {
      return std::ranges::any_of(std::span<const cgltf_attribute>(primitive.attributes, primitive.attributes_count),
                                 [name](const cgltf_attribute& attribute) {
                                   return attribute.name != nullptr && std::string_view(attribute.name) == name;
                                 });
    };
    CHECK(has_attribute("_HEAD"));
    CHECK(has_attribute("_CHEST"));
    CHECK(has_attribute("_1ST"));
    CHECK(has_attribute("_CUT_HEAD"));
    CHECK(has_attribute("_HEAD_1"));
    CHECK(has_attribute("_UNUSED_AUTHORING_SELECTION"));
    CHECK(has_attribute("_EMPTY"));

    pistoris::Model imported;
    REQUIRE(pistoris::Model::importGlb(imported, encoded) == ARX_OK);
    REQUIRE(imported.validate() == ARX_OK);
    CHECK(imported.vertexCount() == 3);
    CHECK(imported.faceCount() == 1);
    CHECK(imported.boneCount() == 2);
    CHECK(imported.actionPointCount() == 1);
    CHECK(imported.selectionCount() == 7);
    CHECK(imported.origin().bone == 1);

    std::array<ArxModelBone, 2> bones{};
    REQUIRE(imported.copyBones(0, bones.size(), bones.data()) == ARX_OK);
    CHECK((view(bones[0].name) == "root"));
    CHECK(bones[0].blob_shadow_size == doctest::Approx(2.0f));
    CHECK((view(bones[1].name) == "chest"));
    CHECK(bones[1].parent == 0);
    CHECK(bones[1].blob_shadow_size == doctest::Approx(1.0f));

    std::array<ArxModelActionPoint, 1> actions{};
    REQUIRE(imported.copyActionPoints(0, 1, actions.data()) == ARX_OK);
    CHECK((view(actions[0].name) == "view_attach"));
    CHECK(actions[0].bone == 1);

    std::array<pistoris::SelectionId, 6> ids{};
    REQUIRE(imported.copySelectionIds(0, ids.size(), ids.data()) == ARX_OK);
    ArxModelSelection selection{};
    REQUIRE(imported.selection(ids[3], selection) == ARX_OK);
    CHECK((view(selection.name) == "cut_head"));
    CHECK(selection.has_leading_vertex == 1);
    CHECK(selection.leading_bone == 1);
    CHECK(selectionVertices(imported, ids[0]) == std::vector<pistoris::VertexIndex>{0});
    CHECK(selectionVertices(imported, ids[1]) == std::vector<pistoris::VertexIndex>{1});
    std::size_t bone_count = 0;
    REQUIRE(imported.selectionBoneCount(ids[0], bone_count) == ARX_OK);
    REQUIRE(bone_count == 1);
    pistoris::BoneIndex selected_bone = pistoris::kInvalidBoneIndex;
    REQUIRE(imported.copySelectionBones(ids[0], 0, 1, &selected_bone) == ARX_OK);
    CHECK(selected_bone == 0);
    bool includes_origin = false;
    REQUIRE(imported.selectionIncludesOrigin(ids[0], includes_origin) == ARX_OK);
    CHECK(includes_origin);
  }

  TEST_CASE("Roundtrips multiple skeleton roots through native FTL and GLB") {
    pistoris::Model source;
    REQUIRE(pistoris::Model::importNative(source, makeSemanticModelFtl()) == ARX_OK);
    ArxModelBone second_root{};
    REQUIRE(source.copyBones(1, 1, &second_root) == ARX_OK);
    const std::string second_root_name(view(second_root.name));
    second_root.name = {second_root_name.data(), second_root_name.size()};
    second_root.parent = pistoris::kInvalidBoneIndex;
    REQUIRE(source.setBone(1, second_root) == ARX_OK);
    const std::string branch_name = "branch";
    ArxModelBone branch{{branch_name.data(), branch_name.size()}, {7.0f, 8.0f, 9.0f}, 1, 0.0f};
    pistoris::BoneIndex branch_index = pistoris::kInvalidBoneIndex;
    REQUIRE(source.addBone(branch, branch_index) == ARX_OK);
    REQUIRE(branch_index == 2);
    REQUIRE(source.validate() == ARX_OK);

    pistoris::NativeModelBundle native;
    REQUIRE(source.bakeNativeBundle({.include_files = false}, native) == ARX_OK);
    pistoris::Model native_roundtrip;
    REQUIRE(pistoris::Model::importNative(native_roundtrip, native.ftl) == ARX_OK);
    std::array<ArxModelBone, 3> bones{};
    REQUIRE(native_roundtrip.copyBones(0, bones.size(), bones.data()) == ARX_OK);
    CHECK(bones[0].parent == pistoris::kInvalidBoneIndex);
    CHECK(bones[1].parent == pistoris::kInvalidBoneIndex);
    CHECK(bones[2].parent == 1);

    pistoris::Animation animation;
    REQUIRE(animation.setName("forest") == ARX_OK);
    REQUIRE(animation.setResourcePath("anim:npc:forest") == ARX_OK);
    std::array<ArxAnimationGroupTransform, 3> transforms{};
    transforms[0].translation = {1.0f, 2.0f, 3.0f};
    transforms[1].translation = {4.0f, 5.0f, 6.0f};
    transforms[2].translation = {7.0f, 8.0f, 9.0f};
    const ArxAnimationKeyframeInput keyframe{{0, {}, {}, 0, pistoris::kNoSound}, transforms.data(), transforms.size()};
    REQUIRE(animation.replaceKeyframes(1, &keyframe, 1) == ARX_OK);
    const std::array<const pistoris::Animation*, 1> animations = {&animation};

    std::vector<std::uint8_t> encoded;
    REQUIRE(source.exportGlb(encoded, animations) == ARX_OK);
    pistoris::glb::Asset asset;
    REQUIRE(pistoris::glb::parse(encoded, asset) == ARX_OK);
    const cgltf_data& glb = *asset.data();
    REQUIRE(glb.skins_count == 1);
    const cgltf_node* skeleton = findNode(glb, "skeleton");
    const cgltf_node* first = findNode(glb, "000__root");
    const cgltf_node* second = findNode(glb, "001__chest");
    const cgltf_node* branch_node = findNode(glb, "002__branch");
    REQUIRE(skeleton != nullptr);
    REQUIRE(first != nullptr);
    REQUIRE(second != nullptr);
    REQUIRE(branch_node != nullptr);
    CHECK(glb.skins[0].skeleton == skeleton);
    CHECK(first->parent == skeleton);
    CHECK(second->parent == skeleton);
    CHECK(branch_node->parent == second);

    pistoris::Model glb_roundtrip;
    std::vector<std::unique_ptr<pistoris::Animation>> imported_animations;
    REQUIRE(pistoris::Model::importGlb(glb_roundtrip, imported_animations, encoded) == ARX_OK);
    REQUIRE(glb_roundtrip.copyBones(0, bones.size(), bones.data()) == ARX_OK);
    CHECK(bones[0].parent == pistoris::kInvalidBoneIndex);
    CHECK(bones[1].parent == pistoris::kInvalidBoneIndex);
    CHECK(bones[2].parent == 1);
    REQUIRE(imported_animations.size() == 1);
    REQUIRE(imported_animations[0]->groupCount() == 3);
    std::array<ArxAnimationGroupTransform, 3> imported_transforms{};
    REQUIRE(imported_animations[0]->copyGroupTransforms(0, 0, imported_transforms.size(), imported_transforms.data()) ==
            ARX_OK);
    CHECK(imported_transforms[0].translation.x == doctest::Approx(1.0f));
    CHECK(imported_transforms[1].translation.x == doctest::Approx(4.0f));
    CHECK(imported_transforms[2].translation.x == doctest::Approx(7.0f));
  }

  TEST_CASE("GLB name repair preserves duplicate action names and existing suffixed names") {
    pistoris::Model source;
    REQUIRE(pistoris::Model::importNative(source, makeSemanticModelFtl()) == ARX_OK);

    const std::string third_bone_name = "third";
    ArxModelBone third_bone{};
    third_bone.name = {third_bone_name.data(), third_bone_name.size()};
    third_bone.parent = 0;
    pistoris::BoneIndex third_bone_index = pistoris::kInvalidBoneIndex;
    REQUIRE(source.addBone(third_bone, third_bone_index) == ARX_OK);
    const std::array<std::string, 2> extra_action_names = {"view_attach", "view_attach"};
    for (const std::string& name : extra_action_names) {
      ArxModelActionPoint point{};
      point.name = {name.data(), name.size()};
      point.bone = 0;
      pistoris::ActionPointIndex point_index = pistoris::kInvalidActionPointIndex;
      REQUIRE(source.addActionPoint(point, point_index) == ARX_OK);
    }

    std::vector<std::uint8_t> encoded;
    REQUIRE(source.exportGlb(encoded) == ARX_OK);
    std::uint32_t json_size = 0;
    std::memcpy(&json_size, encoded.data() + 12U, sizeof(json_size));
    nlohmann::json gltf = nlohmann::json::parse(encoded.begin() + 20, encoded.begin() + 20 + json_size);

    std::size_t action = 0;
    for (nlohmann::json& node : gltf["nodes"]) {
      const std::string name = node.value("name", std::string{});
      if (name.starts_with("000__")) node["name"] = "000__a";
      if (name.starts_with("001__")) node["name"] = "001__a";
      if (name.starts_with("002__")) node["name"] = "002__a_1";
      if (name.starts_with("arx_action__")) {
        REQUIRE(action < 3U);
        CHECK(name == std::format("arx_action__view_attach__action_{}", action));
        node["name"] = std::array<std::string_view, 3>{
            "arx_action__a__action_0", "arx_action__a__action_1.001", "arx_action__a_1__action_2"}[action];
        ++action;
      }
    }
    REQUIRE(action == 3);

    nlohmann::json& attributes = gltf["meshes"][0]["primitives"][0]["attributes"];
    std::vector<std::string> selection_keys;
    std::vector<nlohmann::json> selection_values;
    for (const auto& [name, value] : attributes.items()) {
      if (!name.empty() && name.front() == '_') {
        selection_keys.push_back(name);
        selection_values.push_back(value);
        if (selection_keys.size() == 3U) break;
      }
    }
    REQUIRE(selection_keys.size() == 3U);
    for (const std::string& name : selection_keys) attributes.erase(name);
    attributes["_a!"] = selection_values[0];
    attributes["_a#"] = selection_values[1];
    attributes["_a-_1"] = selection_values[2];

    pistoris::Model imported;
    REQUIRE(pistoris::Model::importGlb(imported, replaceGlbJson(encoded, gltf)) == ARX_OK);

    std::array<ArxModelBone, 3> bones{};
    REQUIRE(imported.copyBones(0, bones.size(), bones.data()) == ARX_OK);
    CHECK((view(bones[0].name) == "a"));
    CHECK((view(bones[1].name) == "a_2"));
    CHECK((view(bones[2].name) == "a_1"));
    std::array<ArxModelActionPoint, 3> actions{};
    REQUIRE(imported.copyActionPoints(0, actions.size(), actions.data()) == ARX_OK);
    CHECK((view(actions[0].name) == "a"));
    CHECK((view(actions[1].name) == "a"));
    CHECK((view(actions[2].name) == "a_1"));

    std::vector<pistoris::SelectionId> selection_ids(imported.selectionCount());
    REQUIRE(imported.copySelectionIds(0, selection_ids.size(), selection_ids.data()) == ARX_OK);
    std::set<std::string> selection_names;
    for (pistoris::SelectionId id : selection_ids) {
      ArxModelSelection selection{};
      REQUIRE(imported.selection(id, selection) == ARX_OK);
      selection_names.emplace(view(selection.name));
    }
    CHECK(selection_names.contains("a-"));
    CHECK(selection_names.contains("a-_2"));
    CHECK(selection_names.contains("a-_1"));
  }

  TEST_CASE("Rejects structural delimiters in selection attributes") {
    pistoris::Model source;
    REQUIRE(pistoris::Model::importNative(source, makeSemanticModelFtl()) == ARX_OK);

    std::vector<std::uint8_t> encoded;
    REQUIRE(source.exportGlb(encoded) == ARX_OK);
    std::uint32_t json_size = 0;
    std::memcpy(&json_size, encoded.data() + 12U, sizeof(json_size));
    nlohmann::json gltf = nlohmann::json::parse(encoded.begin() + 20, encoded.begin() + 20 + json_size);
    nlohmann::json& attributes = gltf["meshes"][0]["primitives"][0]["attributes"];
    REQUIRE(attributes.contains("_HEAD"));
    attributes["_HEAD__EXTRA"] = attributes["_HEAD"];
    attributes.erase("_HEAD");

    pistoris::Model imported;
    CHECK(pistoris::Model::importGlb(imported, replaceGlbJson(encoded, gltf)) == ARX_GLB_BAD_MODEL_SELECTION);
  }

  TEST_CASE("Accepts an optional single semantic origin and validates units") {
    pistoris::Model model;
    CHECK(pistoris::Model::importGlb(model, {}) != ARX_OK);
    pistoris::Model::GlbImportOptions import_options;
    import_options.arx_units_per_glb_unit = 0.0f;
    CHECK(pistoris::Model::importGlb(model, std::array<std::uint8_t, 1>{0}, import_options) == ARX_INVALID_OPTIONS);

    REQUIRE(pistoris::Model::importNative(model, makeSemanticModelFtl()) == ARX_OK);
    pistoris::Model::GlbExportOptions export_options;
    export_options.arx_units_per_glb_unit = 1001.0f;
    std::vector<std::uint8_t> encoded;
    CHECK(model.exportGlb(encoded, export_options) == ARX_INVALID_OPTIONS);
    const std::size_t faces = model.faceCount();
    CHECK(pistoris::Model::importGlb(model, {}) != ARX_OK);
    CHECK(model.faceCount() == faces);

    REQUIRE(model.exportGlb(encoded) == ARX_OK);
    std::uint32_t json_size = 0;
    std::memcpy(&json_size, encoded.data() + 12U, sizeof(json_size));
    nlohmann::json gltf = nlohmann::json::parse(encoded.begin() + 20, encoded.begin() + 20 + json_size);
    std::size_t model_root = gltf["nodes"].size();
    for (std::size_t index = 0; index < gltf["nodes"].size(); ++index)
      if (gltf["nodes"][index].value("name", std::string{}) == "arx_model_origin__origin") model_root = index;
    REQUIRE(model_root < gltf["nodes"].size());
    gltf["nodes"][model_root]["name"] = "model";
    REQUIRE(pistoris::Model::importGlb(model, replaceGlbJson(encoded, gltf)) == ARX_OK);
    CHECK(model.faceCount() == faces);

    gltf["nodes"][model_root]["name"] = "arx_model_origin__origin__extra";
    CHECK(pistoris::Model::importGlb(model, replaceGlbJson(encoded, gltf)) == ARX_GLB_BAD_MODEL_HIERARCHY);

    const std::size_t duplicate = gltf["nodes"].size();
    gltf["nodes"].push_back({{"name", "arx_model_origin__duplicate"}});
    gltf["scenes"][0]["nodes"].push_back(duplicate);
    gltf["nodes"][model_root]["name"] = "arx_model_origin__origin";
    CHECK(pistoris::Model::importGlb(model, replaceGlbJson(encoded, gltf)) == ARX_GLB_BAD_MODEL_HIERARCHY);
  }

  TEST_CASE("Failed Model GLB conversions preserve Animation reports") {
    ArxAnimationConversionReport report{4, 5};
    pistoris::Model model;
    std::vector<std::unique_ptr<pistoris::Animation>> animations;
    const std::vector<std::uint8_t> invalid;

    CHECK(pistoris::Model::importGlb(model, animations, invalid, &report) != ARX_OK);
    CHECK(report.converted == 4);
    CHECK(report.skipped == 5);

    report = {6, 7};
    std::vector<std::uint8_t> encoded;
    const std::span<const pistoris::Animation* const> no_animations;
    CHECK(model.exportGlb(encoded, no_animations, &report) != ARX_OK);
    CHECK(report.converted == 6);
    CHECK(report.skipped == 7);
  }

  TEST_CASE("Ignores meshes outside an explicit semantic origin") {
    const std::vector<std::uint8_t> encoded = makeSkinnedModelGlb(SkinFixture::kPrimary);
    std::uint32_t json_size = 0;
    std::memcpy(&json_size, encoded.data() + 12U, sizeof(json_size));
    nlohmann::json gltf = nlohmann::json::parse(encoded.begin() + 20, encoded.begin() + 20 + json_size);
    const std::size_t outside = gltf["nodes"].size();
    gltf["nodes"].push_back({{"mesh", 0}, {"name", "outside_mesh"}});
    gltf["scenes"][0]["nodes"].push_back(outside);

    ModelGlbLogCapture logs;
    pistoris::Model model;
    REQUIRE(pistoris::Model::importGlb(model, replaceGlbJson(encoded, gltf)) == ARX_OK);
    CHECK(model.faceCount() == 1);
    CHECK(logs.contains("1 mesh node(s) outside the model origin; ignored"));
  }

  TEST_CASE("Resolves bone hierarchy after all joint ordinals are known") {
    pistoris::Model model;
    CHECK(pistoris::Model::importGlb(model, makeSkinnedModelGlb(SkinFixture::kPrimary, true)) ==
          ARX_GLB_BAD_MODEL_SKELETON);
    CHECK(pistoris::Model::importGlb(model, makeSkinnedModelGlb(SkinFixture::kPrimary, false, true)) ==
          ARX_GLB_BAD_MODEL_SKINNING);
  }

  TEST_CASE("Rejects bone ordinals beyond the intermediate limit") {
    const std::vector<std::uint8_t> encoded = makeSkinnedModelGlb(SkinFixture::kPrimary);
    std::uint32_t json_size = 0;
    std::memcpy(&json_size, encoded.data() + 12U, sizeof(json_size));
    nlohmann::json gltf = nlohmann::json::parse(encoded.begin() + 20, encoded.begin() + 20 + json_size);
    for (nlohmann::json& node : gltf["nodes"])
      if (node.value("name", std::string{}) == "1__child") node["name"] = "1024__child";

    pistoris::Model model;
    CHECK(pistoris::Model::importGlb(model, replaceGlbJson(encoded, gltf)) == ARX_MODEL_TOO_MANY_BONES);
  }

  TEST_CASE("Reads every consecutive skin attribute set") {
    pistoris::Model model;
    REQUIRE(pistoris::Model::importGlb(model, makeSkinnedModelGlb(SkinFixture::kTwoSets)) == ARX_OK);
    std::array<ArxModelVertex, 3> vertices{};
    REQUIRE(model.copyVertices(0, vertices.size(), vertices.data()) == ARX_OK);
    for (const ArxModelVertex& vertex : vertices) CHECK(vertex.bone == 1);

    CHECK(pistoris::Model::importGlb(model, makeSkinnedModelGlb(SkinFixture::kMissingPrimary)) ==
          ARX_GLB_BAD_MODEL_SKINNING);
    CHECK(pistoris::Model::importGlb(model, makeSkinnedModelGlb(SkinFixture::kUnpairedSecondary)) ==
          ARX_GLB_BAD_MODEL_SKINNING);
  }

  TEST_CASE("Ignores meshes attached to terminal semantic nodes") {
    const std::vector<std::uint8_t> base = makeSkinnedModelGlb(SkinFixture::kPrimary);
    for (std::string_view name :
         {"arx_action__preview__action_0", "arx_bone__root", "arx_selection_probe__cut_head__preview"}) {
      CAPTURE(name);
      pistoris::Model model;
      REQUIRE(pistoris::Model::importGlb(model, attachMeshToTerminalNode(base, name)) == ARX_OK);
      CHECK(model.faceCount() == 1);
    }
    pistoris::Model model;
    CHECK(pistoris::Model::importGlb(model, attachMeshToTerminalNode(base, "arx_action__preview")) ==
          ARX_GLB_BAD_MODEL_ACTION_POINT);
  }

  TEST_CASE("Keeps distinct glTF image paths and shares identical external images") {
    pistoris::Model model;
    {
      ModelGlbLogCapture logs;
      REQUIRE(pistoris::Model::importGlb(model, makeTexturedModelGlb("Folder/Foo.PNG", "folder/bar.png")) == ARX_OK);
      std::array<ArxTextureView, 2> textures{};
      REQUIRE(model.copyTextureViews(0, textures.size(), textures.data()) == ARX_OK);
      CHECK((view(textures[0].path) == "folder/foo"));
      CHECK_FALSE(logs.contains("texture path 'Folder/Foo' normalized"));
    }

    REQUIRE(pistoris::Model::importGlb(model, makeTexturedModelGlb("folder1/foo.png", "folder2/foo.png")) == ARX_OK);
    REQUIRE(model.textureCount() == 2);
    std::array<ArxTextureView, 2> textures{};
    REQUIRE(model.copyTextureViews(0, textures.size(), textures.data()) == ARX_OK);
    CHECK((view(textures[0].path) == "folder1/foo"));
    CHECK((view(textures[1].path) == "folder2/foo"));

    REQUIRE(pistoris::Model::importGlb(model, makeTexturedModelGlb("folder/foo.png", "folder/foo.png")) == ARX_OK);
    REQUIRE(model.textureCount() == 1);
    REQUIRE(model.copyTextureViews(0, 1, textures.data()) == ARX_OK);
    CHECK((view(textures[0].path) == "folder/foo"));

    REQUIRE(pistoris::Model::importGlb(model, makeTexturedModelGlb("folder/foo.png", "folder/foo.png", true)) ==
            ARX_OK);
    CHECK(model.textureCount() == 1);

    CHECK(pistoris::Model::importGlb(model, makeTexturedModelGlb("../outside.png", "folder/bar.png")) ==
          ARX_GLB_BAD_MODEL_MATERIAL);
  }

  TEST_CASE("BLEND with base alpha 1 does not infer TRANS") {
    const std::vector<std::uint8_t> encoded = makeTexturedModelGlb("textures/first.png", "textures/second.png");
    nlohmann::json gltf = parseGlbJson(encoded);
    gltf["materials"][0]["alphaMode"] = "BLEND";
    gltf["materials"][0]["pbrMetallicRoughness"]["baseColorFactor"] = {1.0f, 1.0f, 1.0f, 1.0f};

    ModelGlbLogCapture logs;
    pistoris::Model model;
    REQUIRE(pistoris::Model::importGlb(model, replaceGlbJson(encoded, gltf)) == ARX_OK);
    REQUIRE(model.faceCount() == 2);
    std::array<ArxModelFace, 2> faces{};
    REQUIRE(model.copyFaces(0, faces.size(), faces.data()) == ARX_OK);
    CHECK((faces[0].flags & pistoris::kFaceBitTrans) == 0);
    CHECK(logs.contains(
        "BLEND with base alpha 1 imported without TRANS; texture alpha, if present, remains native cutout"));
  }

  TEST_CASE("Preserves shared topology and logical texture dots across material groups") {
    std::array<ArxModelVertex, 4> vertices = {
        ArxModelVertex{{0.0f, 0.0f, 0.0f}},
        ArxModelVertex{{1.0f, 0.0f, 0.0f}},
        ArxModelVertex{{1.0f, 1.0f, 0.0f}},
        ArxModelVertex{{0.0f, 1.0f, 0.0f}},
    };
    const auto corner = [](pistoris::VertexIndex vertex, float u, float v) {
      return ArxModelCorner{vertex, {0.0f, 0.0f, 1.0f}, u, v};
    };
    std::array<ArxModelFace, 2> faces{};
    faces[0].corners[0] = corner(0, 0.0f, 0.0f);
    faces[0].corners[1] = corner(1, 1.0f, 0.0f);
    faces[0].corners[2] = corner(2, 1.0f, 1.0f);
    faces[0].normal = {0.0f, 0.0f, 1.0f};
    faces[0].texture = 0;
    faces[1].corners[0] = corner(0, 0.0f, 0.0f);
    faces[1].corners[1] = corner(2, 1.0f, 1.0f);
    faces[1].corners[2] = corner(3, 0.0f, 1.0f);
    faces[1].normal = {0.0f, 0.0f, 1.0f};
    faces[1].texture = 1;
    const std::array<ArxTextureView, 2> textures = {
        ArxTextureView{.path = {"folder1/item.pie", 16}, .encoded_image = {}},
        ArxTextureView{.path = {"folder2/item.pie", 16}, .encoded_image = {}},
    };

    pistoris::Model source;
    const ArxModelMeshInput mesh = {
        vertices.data(), vertices.size(), faces.data(), faces.size(), textures.data(), textures.size()};
    REQUIRE(source.replaceMesh(mesh) == ARX_OK);
    std::vector<std::uint8_t> encoded;
    REQUIRE(source.exportGlb(encoded) == ARX_OK);

    pistoris::glb::Asset asset;
    REQUIRE(pistoris::glb::parse(encoded, asset) == ARX_OK);
    const cgltf_data& glb = *asset.data();
    REQUIRE(glb.meshes_count == 1);
    REQUIRE(glb.meshes[0].primitives_count == 2);
    const cgltf_accessor* first_positions = positionAccessor(glb.meshes[0].primitives[0]);
    const cgltf_accessor* second_positions = positionAccessor(glb.meshes[0].primitives[1]);
    REQUIRE(first_positions != nullptr);
    CHECK(first_positions == second_positions);
    CHECK(first_positions->count == 4);

    std::set<std::string_view> material_names;
    for (const cgltf_material& material : std::span<const cgltf_material>(glb.materials, glb.materials_count))
      material_names.insert(material.name);
    CHECK(material_names == std::set<std::string_view>{"item.pie", "item.pie_1"});

    pistoris::Model imported;
    REQUIRE(pistoris::Model::importGlb(imported, encoded) == ARX_OK);
    CHECK(imported.vertexCount() == 4);
    CHECK(imported.faceCount() == 2);
    std::array<ArxTextureView, 2> imported_textures{};
    REQUIRE(imported.copyTextureViews(0, imported_textures.size(), imported_textures.data()) == ARX_OK);
    CHECK(view(imported_textures[0].path) == "folder1/item.pie");
    CHECK(view(imported_textures[1].path) == "folder2/item.pie");
  }

  TEST_CASE("Splits shared positions with different bone and selection semantics") {
    pistoris::Model model;
    REQUIRE(pistoris::Model::importGlb(model, makeSharedSemanticModelGlb()) == ARX_OK);
    CHECK(model.vertexCount() == 9);
    CHECK(model.faceCount() == 3);

    std::array<ArxModelVertex, 9> vertices{};
    REQUIRE(model.copyVertices(0, vertices.size(), vertices.data()) == ARX_OK);
    for (std::size_t index = 0; index < 3; ++index) CHECK(vertices[index].bone == 0);
    for (std::size_t index = 3; index < 6; ++index) CHECK(vertices[index].bone == 1);
    for (std::size_t index = 6; index < 9; ++index) CHECK(vertices[index].bone == 0);

    REQUIRE(model.selectionCount() == 1);
    std::array<pistoris::SelectionId, 1> selections{};
    REQUIRE(model.copySelectionIds(0, selections.size(), selections.data()) == ARX_OK);
    CHECK(selectionVertices(model, selections[0]) == std::vector<pistoris::VertexIndex>{0, 1, 2, 3, 4, 5});
  }

  TEST_CASE("Imports separate projections for skins sharing one skeleton") {
    pistoris::Model model;
    REQUIRE(pistoris::Model::importGlb(model, makeMultiSkinModelGlb()) == ARX_OK);
    CHECK(model.vertexCount() == 6);
    CHECK(model.faceCount() == 2);
    CHECK(model.boneCount() == 2);

    REQUIRE(pistoris::Model::importGlb(model, makeMultiSkinModelGlb(true)) == ARX_OK);
    std::array<ArxModelVertex, 6> vertices{};
    REQUIRE(model.copyVertices(0, vertices.size(), vertices.data()) == ARX_OK);
    CHECK(vertices[3].position.x == doctest::Approx(10.0f));
    CHECK(vertices[4].position.x == doctest::Approx(20.0f));
  }

  TEST_CASE("Separates skin bind space from ordinary node transforms") {
    const auto check = [](const TransformedSkinFixture& fixture) {
      pistoris::Model model;
      std::vector<std::unique_ptr<pistoris::Animation>> animations;
      REQUIRE(pistoris::Model::importGlb(model, animations, makeTransformedSkinAnimationGlb(fixture)) == ARX_OK);

      REQUIRE(model.vertexCount() == 6);
      std::array<ArxModelVertex, 6> vertices{};
      REQUIRE(model.copyVertices(0, vertices.size(), vertices.data()) == ARX_OK);
      CHECK(vertices[1].position.x == doctest::Approx(10.0f));
      CHECK(vertices[4].position.x == doctest::Approx(20.0f * fixture.content_scale[0]));
      CHECK(vertices[4].bone == pistoris::kInvalidBoneIndex);

      REQUIRE(model.boneCount() == 2);
      std::array<ArxModelBone, 2> bones{};
      REQUIRE(model.copyBones(0, bones.size(), bones.data()) == ARX_OK);
      CHECK(bones[0].position.y == doctest::Approx(-10.0f * fixture.content_scale[1]));
      CHECK(bones[1].position.y == doctest::Approx(-20.0f * fixture.content_scale[1]));

      REQUIRE(animations.size() == 1);
      REQUIRE(animations[0]->keyframeCount() == 2);
      REQUIRE(animations[0]->groupCount() == 2);
      for (std::size_t keyframe = 0; keyframe < animations[0]->keyframeCount(); ++keyframe) {
        std::array<ArxAnimationGroupTransform, 2> transforms{};
        REQUIRE(animations[0]->copyGroupTransforms(keyframe, 0, transforms.size(), transforms.data()) == ARX_OK);
        for (const ArxAnimationGroupTransform& transform : transforms) {
          CHECK(transform.translation.x == doctest::Approx(0.0f).epsilon(1.0e-4));
          CHECK(transform.translation.y == doctest::Approx(0.0f).epsilon(1.0e-4));
          CHECK(transform.translation.z == doctest::Approx(0.0f).epsilon(1.0e-4));
        }
      }
      std::array<ArxAnimationGroupTransform, 2> animated{};
      REQUIRE(animations[0]->copyGroupTransforms(1, 0, animated.size(), animated.data()) == ARX_OK);
      CHECK(std::abs(animated[1].rotation.w) == doctest::Approx(0.70710678f));
      CHECK(std::abs(animated[1].rotation.z) == doctest::Approx(0.70710678f));
    };

    check({.origin_scale = {2.0f, 2.0f, 2.0f}, .content_scale = {1.5f, 1.5f, 1.5f}});
    check({.origin_scale = {0.5f, 0.5f, 0.5f}, .content_scale = {1.5f, 1.5f, 1.5f}});
    check({.origin_scale = {2.0f, 3.0f, 4.0f}, .content_scale = {0.75f, 0.75f, 0.75f}});
  }

  TEST_CASE("Binds unskinned meshes rigidly through known joint ancestry") {
    for (const OrdinaryMeshPlacement placement : {OrdinaryMeshPlacement::kChildBone,
                                                  OrdinaryMeshPlacement::kBelowChildBone,
                                                  OrdinaryMeshPlacement::kChildBoneNode}) {
      CAPTURE(static_cast<int>(placement));
      pistoris::Model model;
      REQUIRE(pistoris::Model::importGlb(
                  model, makeTransformedSkinAnimationGlb({.ordinary_mesh_placement = placement})) == ARX_OK);

      REQUIRE(model.vertexCount() == 6);
      std::array<ArxModelVertex, 6> vertices{};
      REQUIRE(model.copyVertices(0, vertices.size(), vertices.data()) == ARX_OK);
      for (std::size_t index = 0; index < 3; ++index) CHECK(vertices[index].bone == 0);
      for (std::size_t index = 3; index < vertices.size(); ++index) CHECK(vertices[index].bone == 1);
      CHECK(vertices[4].position.x == doctest::Approx(20.0f));
      CHECK(vertices[4].position.y == doctest::Approx(-20.0f));
    }
  }

  TEST_CASE("Samples LINEAR rotation channels with spherical interpolation") {
    pistoris::Model model;
    std::vector<std::unique_ptr<pistoris::Animation>> animations;
    REQUIRE(pistoris::Model::importGlb(
                model, animations, makeTransformedSkinAnimationGlb({.asymmetric_rotation_sampling = true})) == ARX_OK);
    REQUIRE(animations.size() == 1);
    REQUIRE(animations[0]->keyframeCount() == 3);

    std::array<ArxAnimationKeyframe, 3> keyframes{};
    REQUIRE(animations[0]->copyKeyframes(0, keyframes.size(), keyframes.data()) == ARX_OK);
    CHECK(keyframes[0].frame == 0);
    CHECK(keyframes[1].frame == 2);
    CHECK(keyframes[2].frame == 8);

    std::array<ArxAnimationGroupTransform, 2> transforms{};
    REQUIRE(animations[0]->copyGroupTransforms(1, 0, transforms.size(), transforms.data()) == ARX_OK);
    CHECK(std::abs(transforms[1].rotation.w) == doctest::Approx(0.9659258f).epsilon(1.0e-5));
    CHECK(std::abs(transforms[1].rotation.x) == doctest::Approx(0.2588190f).epsilon(1.0e-5));
    CHECK(transforms[1].rotation.y == doctest::Approx(0.0f).epsilon(1.0e-5));
    CHECK(transforms[1].rotation.z == doctest::Approx(0.0f).epsilon(1.0e-5));
  }

  TEST_CASE("Ignores skinned mesh-node transforms") {
    pistoris::Model model;
    REQUIRE(pistoris::Model::importGlb(model, makeTransformedSkinAnimationGlb({.mesh_scale = {2.0f, 1.0f, 0.5f}})) ==
            ARX_OK);
    std::array<ArxModelVertex, 6> vertices{};
    REQUIRE(model.copyVertices(0, vertices.size(), vertices.data()) == ARX_OK);
    CHECK(vertices[1].position.x == doctest::Approx(10.0f));
  }

  TEST_CASE("Leaves zero-weight skinned vertices unbound in skin space") {
    pistoris::Model model;
    REQUIRE(pistoris::Model::importGlb(
                model, makeTransformedSkinAnimationGlb({.mesh_scale = {2.0f, 1.0f, 0.5f}, .unbound_vertex = true})) ==
            ARX_OK);
    std::array<ArxModelVertex, 6> vertices{};
    REQUIRE(model.copyVertices(0, vertices.size(), vertices.data()) == ARX_OK);
    CHECK(vertices[1].position.x == doctest::Approx(10.0f));
    CHECK(vertices[1].bone == pistoris::kInvalidBoneIndex);
  }

  TEST_CASE("Uses identity inverse binds when the skin omits them") {
    pistoris::Model model;
    const ArxQuat quarter_turn = {0.70710678f, 0.0f, 0.0f, 0.70710678f};
    REQUIRE(pistoris::Model::importGlb(
                model, makeTransformedSkinAnimationGlb({.root_rotation = quarter_turn, .inverse_binds = false})) ==
            ARX_OK);
    std::array<ArxModelVertex, 6> vertices{};
    REQUIRE(model.copyVertices(0, vertices.size(), vertices.data()) == ARX_OK);
    CHECK(vertices[1].position.x == doctest::Approx(0.0f).epsilon(1.0e-5));
    CHECK(vertices[1].position.y == doctest::Approx(-20.0f));
    CHECK(vertices[2].position.x == doctest::Approx(-10.0f));
    CHECK(vertices[2].position.y == doctest::Approx(-10.0f));

    std::array<ArxModelBone, 2> bones{};
    REQUIRE(model.copyBones(0, bones.size(), bones.data()) == ARX_OK);
    CHECK(bones[0].position.y == doctest::Approx(-10.0f));
    CHECK(bones[1].position.x == doctest::Approx(-10.0f));
    CHECK(bones[1].position.y == doctest::Approx(-10.0f));
  }

  TEST_CASE("Treats omitted inverse binds as explicit identity") {
    const ArxQuat quarter_turn = {0.70710678f, 0.0f, 0.0f, 0.70710678f};
    const TransformedSkinFixture explicit_fixture{
        .origin_scale = {2.0f, 2.0f, 2.0f},
        .content_scale = {1.5f, 1.5f, 1.5f},
        .root_rotation = quarter_turn,
        .identity_inverse_binds = true,
    };
    TransformedSkinFixture omitted_fixture = explicit_fixture;
    omitted_fixture.inverse_binds = false;

    pistoris::Model explicit_model;
    pistoris::Model omitted_model;
    std::vector<std::unique_ptr<pistoris::Animation>> explicit_animations;
    std::vector<std::unique_ptr<pistoris::Animation>> omitted_animations;
    REQUIRE(pistoris::Model::importGlb(
                explicit_model, explicit_animations, makeTransformedSkinAnimationGlb(explicit_fixture)) == ARX_OK);
    REQUIRE(pistoris::Model::importGlb(
                omitted_model, omitted_animations, makeTransformedSkinAnimationGlb(omitted_fixture)) == ARX_OK);
    test_support::checkModelsEquivalent(explicit_model, omitted_model);
    REQUIRE(explicit_animations.size() == 1);
    REQUIRE(omitted_animations.size() == 1);
    test_support::checkAnimationsEquivalent(*explicit_animations[0], *omitted_animations[0]);
  }

  TEST_CASE("Validates affine node matrices centrally") {
    const std::vector<std::uint8_t> encoded = makeSkinnedModelGlb(SkinFixture::kPrimary);
    std::uint32_t json_size = 0;
    std::memcpy(&json_size, encoded.data() + 12U, sizeof(json_size));
    const nlohmann::json source = nlohmann::json::parse(encoded.begin() + 20, encoded.begin() + 20 + json_size);
    const auto origin = std::ranges::find_if(source["nodes"], [](const nlohmann::json& node) {
      return node.value("name", std::string{}).starts_with("arx_model_origin__");
    });
    REQUIRE(origin != source["nodes"].end());
    const std::size_t origin_index = static_cast<std::size_t>(origin - source["nodes"].begin());

    nlohmann::json gltf = source;
    gltf["nodes"][origin_index]["matrix"] = {
        1.0f, 0.0f, 0.0f, 5.0e-5f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.00005f};
    pistoris::Model model;
    REQUIRE(pistoris::Model::importGlb(model, replaceGlbJson(encoded, gltf)) == ARX_OK);

    gltf = source;
    gltf["nodes"][origin_index]["matrix"] = {
        1.0f, 0.0f, 0.0f, 2.0e-4f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f};
    CHECK(pistoris::Model::importGlb(model, replaceGlbJson(encoded, gltf)) == ARX_GLB_BAD_FORMAT);
  }

  TEST_CASE("Validates skin pairing and inverse bind accessors") {
    const std::vector<std::uint8_t> encoded = makeSkinnedModelGlb(SkinFixture::kPrimary);
    std::uint32_t json_size = 0;
    std::memcpy(&json_size, encoded.data() + 12U, sizeof(json_size));
    const nlohmann::json source = nlohmann::json::parse(encoded.begin() + 20, encoded.begin() + 20 + json_size);

    nlohmann::json gltf = source;
    const auto skinned_node =
        std::ranges::find_if(gltf["nodes"], [](const nlohmann::json& node) { return node.contains("skin"); });
    REQUIRE(skinned_node != gltf["nodes"].end());
    skinned_node->erase("skin");
    pistoris::Model model;
    CHECK(pistoris::Model::importGlb(model, replaceGlbJson(encoded, gltf)) == ARX_GLB_BAD_MODEL_SKINNING);

    gltf = source;
    gltf["meshes"][0]["primitives"][0]["attributes"].erase("JOINTS_0");
    gltf["meshes"][0]["primitives"][0]["attributes"].erase("WEIGHTS_0");
    CHECK(pistoris::Model::importGlb(model, replaceGlbJson(encoded, gltf)) == ARX_GLB_BAD_MODEL_SKINNING);

    REQUIRE(pistoris::Model::importGlb(model, makeSkinnedModelGlb(SkinFixture::kPrimary, false, false, true)) ==
            ARX_OK);
    CHECK(pistoris::Model::importGlb(model, replaceInverseBindComponent(encoded, 0, 3, 0.25f)) ==
          ARX_GLB_BAD_MODEL_SKINNING);
  }

  TEST_CASE("Accepts multiple ordered skeleton roots") {
    const std::vector<std::uint8_t> encoded = makeSkinnedModelGlb(SkinFixture::kPrimary);
    std::uint32_t json_size = 0;
    std::memcpy(&json_size, encoded.data() + 12U, sizeof(json_size));
    nlohmann::json gltf = nlohmann::json::parse(encoded.begin() + 20, encoded.begin() + 20 + json_size);
    const std::size_t root =
        std::ranges::find_if(
            gltf["nodes"], [](const nlohmann::json& node) { return node.value("name", std::string{}) == "0__root"; }) -
        gltf["nodes"].begin();
    const std::size_t child =
        std::ranges::find_if(
            gltf["nodes"], [](const nlohmann::json& node) { return node.value("name", std::string{}) == "1__child"; }) -
        gltf["nodes"].begin();
    const std::size_t origin =
        std::ranges::find_if(gltf["nodes"],
                             [](const nlohmann::json& node) {
                               return node.value("name", std::string{}).starts_with("arx_model_origin__");
                             }) -
        gltf["nodes"].begin();
    REQUIRE(root < gltf["nodes"].size());
    REQUIRE(child < gltf["nodes"].size());
    REQUIRE(origin < gltf["nodes"].size());
    gltf["nodes"][root].erase("children");
    gltf["nodes"][origin]["children"].push_back(child);
    gltf["skins"][0]["skeleton"] = origin;

    pistoris::Model model;
    REQUIRE(pistoris::Model::importGlb(model, replaceGlbJson(encoded, gltf)) == ARX_OK);
    std::array<ArxModelBone, 2> bones{};
    REQUIRE(model.copyBones(0, bones.size(), bones.data()) == ARX_OK);
    CHECK(bones[0].parent == pistoris::kInvalidBoneIndex);
    CHECK(bones[1].parent == pistoris::kInvalidBoneIndex);
  }

  TEST_CASE("Rejects morph targets") {
    pistoris::Model model;
    const std::vector<std::uint8_t> glb = makeSkinnedModelGlb(SkinFixture::kPrimary);
    CHECK(pistoris::Model::importGlb(model, addMorphTarget(glb)) == ARX_GLB_UNSUPPORTED_FEATURE);
  }

  TEST_CASE("Rejects required extensions") {
    const std::vector<std::uint8_t> source = makeSkinnedModelGlb(SkinFixture::kPrimary);
    std::uint32_t json_size = 0;
    std::memcpy(&json_size, source.data() + 12U, sizeof(json_size));
    nlohmann::json gltf = nlohmann::json::parse(source.begin() + 20, source.begin() + 20 + json_size);
    gltf["extensionsUsed"] = {"ARX_test_extension"};
    gltf["extensionsRequired"] = {"ARX_test_extension"};

    pistoris::Model model;
    CHECK(pistoris::Model::importGlb(model, replaceGlbJson(source, gltf)) == ARX_GLB_UNSUPPORTED_FEATURE);
  }

  TEST_CASE("Rejects non-uniform effective bind transforms") {
    pistoris::Model model;
    CHECK(pistoris::Model::importGlb(model, makeSkinnedModelGlb(SkinFixture::kNonUniformBind)) ==
          ARX_GLB_MODEL_NON_UNIFORM_SCALE);
    CHECK(pistoris::Model::importGlb(
              model, makeTransformedSkinAnimationGlb({.content_scale = {2.0f, 1.0f, 1.0f}, .inverse_binds = false})) ==
          ARX_GLB_MODEL_NON_UNIFORM_SCALE);
  }

  TEST_CASE("Rejects reflected skeletal transforms") {
    pistoris::Model model;
    CHECK(pistoris::Model::importGlb(model, makeTransformedSkinAnimationGlb({.content_scale = {-1.0f, 1.0f, 1.0f}})) ==
          ARX_GLB_MODEL_NON_UNIFORM_SCALE);
    CHECK(pistoris::Model::importGlb(
              model, replaceInverseBindComponent(makeSkinnedModelGlb(SkinFixture::kPrimary), 0, 0, -1.0f)) ==
          ARX_GLB_MODEL_NON_UNIFORM_SCALE);
  }

  TEST_CASE("Accepts reflected unskinned mesh transforms") {
    pistoris::Model model;
    REQUIRE(pistoris::Model::importGlb(
                model, makeTransformedSkinAnimationGlb({.ordinary_scale = {-1.0f, 1.0f, 1.0f}})) == ARX_OK);
    std::array<ArxModelVertex, 6> vertices{};
    REQUIRE(model.copyVertices(0, vertices.size(), vertices.data()) == ARX_OK);
    CHECK(std::ranges::any_of(
        vertices, [](const ArxModelVertex& vertex) { return vertex.position.x == doctest::Approx(-20.0f); }));
  }

  TEST_CASE("Reports the Model selection limit") {
    const std::vector<std::uint8_t> source = makeSkinnedModelGlb(SkinFixture::kPrimary);
    std::uint32_t json_size = 0;
    std::memcpy(&json_size, source.data() + 12U, sizeof(json_size));
    nlohmann::json gltf = nlohmann::json::parse(source.begin() + 20, source.begin() + 20 + json_size);
    nlohmann::json& attributes = gltf["meshes"][0]["primitives"][0]["attributes"];
    const nlohmann::json weights = attributes["WEIGHTS_0"];
    for (std::size_t index = 0; index <= 64U; ++index) attributes["_SELECTION" + std::to_string(index)] = weights;

    pistoris::Model model;
    CHECK(pistoris::Model::importGlb(model, replaceGlbJson(source, gltf)) == ARX_MODEL_TOO_MANY_SELECTIONS);
  }

  TEST_CASE("Round-trips Animation sidecars without synthetic duration keys") {
    pistoris::Model model;
    ArxAnimationConversionReport export_report{};
    const std::vector<std::uint8_t> encoded = makeMotionModelGlb(model, 10.0f, &export_report);
    CHECK(export_report.converted == 1);
    CHECK(export_report.skipped == 0);

    pistoris::glb::Asset asset;
    REQUIRE(pistoris::glb::parse(encoded, asset) == ARX_OK);
    REQUIRE(asset.data()->animations_count == 1);
    const cgltf_node* animation_parent = findNode(*asset.data(), "animations_parent");
    const cgltf_node* animation_helper = findNode(*asset.data(), "arx_animation__walk");
    const cgltf_node* model_origin = findNode(*asset.data(), "arx_model_origin__origin");
    const cgltf_node* motion_node = findNode(*asset.data(), "skeleton");
    const cgltf_node* mesh_node = findNode(*asset.data(), "mesh");
    REQUIRE(animation_parent != nullptr);
    REQUIRE(animation_helper != nullptr);
    REQUIRE(model_origin != nullptr);
    REQUIRE(motion_node != nullptr);
    REQUIRE(mesh_node != nullptr);
    CHECK(animation_parent->parent == nullptr);
    CHECK(animation_helper->parent == animation_parent);
    CHECK(motion_node->parent == model_origin);
    CHECK(mesh_node->parent == motion_node);
    const std::span<const cgltf_animation_channel> exported_channels(asset.data()->animations[0].channels,
                                                                     asset.data()->animations[0].channels_count);
    CHECK(std::ranges::any_of(exported_channels, [motion_node](const cgltf_animation_channel& channel) {
      return channel.target_node == motion_node;
    }));
    CHECK(std::ranges::none_of(exported_channels, [model_origin](const cgltf_animation_channel& channel) {
      return channel.target_node == model_origin;
    }));
    CHECK(hasNode(*asset.data(), "arx_animation__walk"));
    CHECK(hasNode(*asset.data(), "PATH_anim:npc:walk__fast__path"));
    CHECK(hasNode(*asset.data(), "SETTINGS__FRAME_LENGTH_10__STEPS_6__settings"));
    CHECK(hasNode(*asset.data(), "SOUND__FRAMES_6__sound"));
    CHECK(hasNode(*asset.data(), "PATH_sfx/step__hard.wav__path"));

    pistoris::Model imported_model;
    std::vector<std::unique_ptr<pistoris::Animation>> imported_animations;
    ArxAnimationConversionReport import_report{};
    REQUIRE(pistoris::Model::importGlb(imported_model, imported_animations, encoded, &import_report) == ARX_OK);
    REQUIRE(imported_animations.size() == 1);
    CHECK(import_report.converted == 1);
    CHECK(import_report.skipped == 0);
    const pistoris::Animation& imported = *imported_animations.front();
    CHECK((imported.name() == "walk"));
    CHECK((imported.resourcePath() == "graph/obj3d/anims/npc/walk__fast.tea"));
    CHECK(imported.frameLength() == 10);
    CHECK(imported.keyframeCount() == 2);
    CHECK(imported.groupCount() == 2);
    std::array<ArxAnimationKeyframe, 2> keyframes{};
    REQUIRE(imported.copyKeyframes(0, keyframes.size(), keyframes.data()) == ARX_OK);
    CHECK(keyframes[1].frame == 6);
    CHECK(keyframes[1].footstep == 1);
    REQUIRE(keyframes[1].sound != pistoris::kNoSound);
    ArxSoundView imported_sound{};
    REQUIRE(imported.copySoundViews(keyframes[1].sound, 1, &imported_sound) == ARX_OK);
    CHECK(view(imported_sound.path) == "sfx/step__hard.wav");
    CHECK(keyframes[1].root_translation.x == doctest::Approx(3.0f));
    CHECK(keyframes[1].root_rotation.w == doctest::Approx(0.70710678f));
    CHECK(keyframes[1].root_rotation.z == doctest::Approx(0.70710678f));
    std::array<ArxAnimationGroupTransform, 2> imported_transforms{};
    REQUIRE(imported.copyGroupTransforms(1, 0, imported_transforms.size(), imported_transforms.data()) == ARX_OK);
    CHECK(imported_transforms[0].translation.x == doctest::Approx(4.0f));
    CHECK(imported_transforms[0].scale.y == doctest::Approx(3.0f));
    CHECK(imported_transforms[1].translation.y == doctest::Approx(8.0f));
    CHECK(imported_transforms[1].rotation.w == doctest::Approx(0.70710678f));
    CHECK(imported_transforms[1].rotation.z == doctest::Approx(0.70710678f));
    CHECK(imported_transforms[1].scale.x == doctest::Approx(1.5f));
    CHECK(imported_transforms[1].scale.z == doctest::Approx(0.5f));
  }

  TEST_CASE("Requires a semantic origin for propelled Animation motion") {
    pistoris::Model model;
    const std::vector<std::uint8_t> encoded = makeMotionModelGlb(model);
    nlohmann::json no_origin = parseGlbJson(encoded);
    for (nlohmann::json& node : no_origin["nodes"])
      if (node.value("name", std::string{}) == "arx_model_origin__origin") node["name"] = "model";

    pistoris::Model imported_model;
    std::vector<std::unique_ptr<pistoris::Animation>> animations;
    REQUIRE(pistoris::Model::importGlb(imported_model, animations, replaceGlbJson(encoded, no_origin)) == ARX_OK);
    REQUIRE(animations.size() == 1);
    std::array<ArxAnimationKeyframe, 2> keyframes{};
    REQUIRE(animations[0]->copyKeyframes(0, keyframes.size(), keyframes.data()) == ARX_OK);
    CHECK(keyframes[1].root_translation.x == doctest::Approx(0.0f));
    CHECK(keyframes[1].root_translation.y == doctest::Approx(0.0f));
    CHECK(keyframes[1].root_translation.z == doctest::Approx(0.0f));
    CHECK(keyframes[1].root_rotation.w == doctest::Approx(1.0f));
    CHECK(keyframes[1].root_rotation.x == doctest::Approx(0.0f));
    CHECK(keyframes[1].root_rotation.y == doctest::Approx(0.0f));
    CHECK(keyframes[1].root_rotation.z == doctest::Approx(0.0f));
  }

  TEST_CASE("Ignores semantic-origin scale when recovering propelled motion") {
    pistoris::Model model;
    const std::vector<std::uint8_t> encoded = makeMotionModelGlb(model);
    nlohmann::json gltf = parseGlbJson(encoded);
    const std::size_t origin = glbNodeIndex(gltf, "arx_model_origin__origin");
    REQUIRE(origin < gltf["nodes"].size());
    gltf["nodes"][origin]["scale"] = {2.0f, 2.0f, 2.0f};

    pistoris::Model imported_model;
    std::vector<std::unique_ptr<pistoris::Animation>> animations;
    REQUIRE(pistoris::Model::importGlb(imported_model, animations, replaceGlbJson(encoded, gltf)) == ARX_OK);
    REQUIRE(animations.size() == 1);
    std::array<ArxAnimationKeyframe, 2> keyframes{};
    REQUIRE(animations[0]->copyKeyframes(0, keyframes.size(), keyframes.data()) == ARX_OK);
    CHECK(keyframes[1].root_translation.x == doctest::Approx(3.0f));
    CHECK(keyframes[1].root_translation.y == doctest::Approx(4.0f));
    CHECK(keyframes[1].root_translation.z == doctest::Approx(5.0f));
  }

  TEST_CASE("Discovers the propelled-motion carrier structurally") {
    pistoris::Model model;
    const std::vector<std::uint8_t> encoded = makeMotionModelGlb(model);
    nlohmann::json gltf = parseGlbJson(encoded);
    const std::size_t carrier = glbNodeIndex(gltf, "skeleton");
    REQUIRE(carrier < gltf["nodes"].size());
    gltf["nodes"][carrier]["name"] = "Armature";
    gltf["skins"][0].erase("skeleton");

    pistoris::Model imported_model;
    std::vector<std::unique_ptr<pistoris::Animation>> animations;
    REQUIRE(pistoris::Model::importGlb(imported_model, animations, replaceGlbJson(encoded, gltf)) == ARX_OK);
    REQUIRE(animations.size() == 1);
    std::array<ArxAnimationKeyframe, 2> keyframes{};
    REQUIRE(animations[0]->copyKeyframes(0, keyframes.size(), keyframes.data()) == ARX_OK);
    CHECK(keyframes[1].root_translation.x == doctest::Approx(3.0f));
    CHECK(keyframes[1].root_translation.y == doctest::Approx(4.0f));
    CHECK(keyframes[1].root_translation.z == doctest::Approx(5.0f));
  }

  TEST_CASE("Keeps multiple root joints independent from propelled motion") {
    pistoris::Model model;
    const std::vector<std::uint8_t> encoded = makeMotionModelGlb(model);
    nlohmann::json gltf = parseGlbJson(encoded);
    const std::size_t carrier = glbNodeIndex(gltf, "skeleton");
    const std::size_t first_bone = glbNodeIndexWithPrefix(gltf, "000__");
    const std::size_t second_bone = glbNodeIndexWithPrefix(gltf, "001__");
    REQUIRE(carrier < gltf["nodes"].size());
    REQUIRE(first_bone < gltf["nodes"].size());
    REQUIRE(second_bone < gltf["nodes"].size());
    auto& first_children = gltf["nodes"][first_bone]["children"];
    const auto second_child = std::ranges::find_if(
        first_children, [second_bone](const nlohmann::json& child) { return child.get<std::size_t>() == second_bone; });
    REQUIRE(second_child != first_children.end());
    first_children.erase(second_child);
    gltf["nodes"][carrier]["children"].push_back(second_bone);

    pistoris::Model imported_model;
    std::vector<std::unique_ptr<pistoris::Animation>> animations;
    REQUIRE(pistoris::Model::importGlb(imported_model, animations, replaceGlbJson(encoded, gltf)) == ARX_OK);
    REQUIRE(animations.size() == 1);
    std::array<ArxAnimationKeyframe, 2> keyframes{};
    REQUIRE(animations[0]->copyKeyframes(0, keyframes.size(), keyframes.data()) == ARX_OK);
    CHECK(keyframes[1].root_translation.x == doctest::Approx(3.0f));
    CHECK(keyframes[1].root_translation.y == doctest::Approx(4.0f));
    CHECK(keyframes[1].root_translation.z == doctest::Approx(5.0f));
  }

  TEST_CASE("Imports semantic-origin propelled motion from older Model GLB") {
    pistoris::Model model;
    const std::vector<std::uint8_t> encoded = makeMotionModelGlb(model);
    nlohmann::json gltf = parseGlbJson(encoded);
    const std::size_t origin = glbNodeIndex(gltf, "arx_model_origin__origin");
    const std::size_t carrier = glbNodeIndex(gltf, "skeleton");
    const std::size_t mesh = glbNodeIndex(gltf, "mesh");
    REQUIRE(origin < gltf["nodes"].size());
    REQUIRE(carrier < gltf["nodes"].size());
    REQUIRE(mesh < gltf["nodes"].size());
    auto& carrier_children = gltf["nodes"][carrier]["children"];
    const auto mesh_child = std::ranges::find_if(
        carrier_children, [mesh](const nlohmann::json& child) { return child.get<std::size_t>() == mesh; });
    REQUIRE(mesh_child != carrier_children.end());
    carrier_children.erase(mesh_child);
    gltf["nodes"][origin]["children"].push_back(mesh);
    for (nlohmann::json& channel : gltf["animations"][0]["channels"])
      if (channel["target"]["node"] == carrier) channel["target"]["node"] = origin;

    pistoris::Model imported_model;
    std::vector<std::unique_ptr<pistoris::Animation>> animations;
    REQUIRE(pistoris::Model::importGlb(imported_model, animations, replaceGlbJson(encoded, gltf)) == ARX_OK);
    REQUIRE(animations.size() == 1);
    std::array<ArxAnimationKeyframe, 2> keyframes{};
    REQUIRE(animations[0]->copyKeyframes(0, keyframes.size(), keyframes.data()) == ARX_OK);
    CHECK(keyframes[1].root_translation.x == doctest::Approx(3.0f));
    CHECK(keyframes[1].root_translation.y == doctest::Approx(4.0f));
    CHECK(keyframes[1].root_translation.z == doctest::Approx(5.0f));
  }

  TEST_CASE("Composes semantic-origin and carrier motion") {
    pistoris::Model model;
    const std::vector<std::uint8_t> encoded = makeMotionModelGlb(model);
    nlohmann::json gltf = parseGlbJson(encoded);
    const std::size_t origin = glbNodeIndex(gltf, "arx_model_origin__origin");
    const std::size_t carrier = glbNodeIndex(gltf, "skeleton");
    REQUIRE(origin < gltf["nodes"].size());
    REQUIRE(carrier < gltf["nodes"].size());
    for (const nlohmann::json& channel : gltf["animations"][0]["channels"])
      if (channel["target"]["node"] == carrier && channel["target"]["path"] == "translation") {
        nlohmann::json origin_channel = channel;
        origin_channel["target"]["node"] = origin;
        gltf["animations"][0]["channels"].push_back(std::move(origin_channel));
        break;
      }

    pistoris::Model imported_model;
    std::vector<std::unique_ptr<pistoris::Animation>> animations;
    REQUIRE(pistoris::Model::importGlb(imported_model, animations, replaceGlbJson(encoded, gltf)) == ARX_OK);
    REQUIRE(animations.size() == 1);
    std::array<ArxAnimationKeyframe, 2> keyframes{};
    REQUIRE(animations[0]->copyKeyframes(0, keyframes.size(), keyframes.data()) == ARX_OK);
    CHECK(keyframes[1].root_translation.x == doctest::Approx(6.0f));
    CHECK(keyframes[1].root_translation.y == doctest::Approx(8.0f));
    CHECK(keyframes[1].root_translation.z == doctest::Approx(10.0f));
  }

  TEST_CASE("Applies static carrier scale coherently to the bind pose") {
    pistoris::Model model;
    const std::vector<std::uint8_t> encoded = makeMotionModelGlb(model);
    pistoris::Model reference_model;
    std::vector<std::unique_ptr<pistoris::Animation>> reference_animations;
    REQUIRE(pistoris::Model::importGlb(reference_model, reference_animations, encoded) == ARX_OK);
    nlohmann::json gltf = parseGlbJson(encoded);
    const std::size_t carrier = glbNodeIndex(gltf, "skeleton");
    REQUIRE(carrier < gltf["nodes"].size());
    gltf["nodes"][carrier]["scale"] = {2.0f, 2.0f, 2.0f};

    pistoris::Model imported_model;
    std::vector<std::unique_ptr<pistoris::Animation>> animations;
    REQUIRE(pistoris::Model::importGlb(imported_model, animations, replaceGlbJson(encoded, gltf)) == ARX_OK);
    REQUIRE(animations.size() == 1);

    std::vector<ArxModelVertex> source_vertices(reference_model.vertexCount());
    std::vector<ArxModelVertex> imported_vertices(imported_model.vertexCount());
    REQUIRE(imported_vertices.size() == source_vertices.size());
    REQUIRE(reference_model.copyVertices(0, source_vertices.size(), source_vertices.data()) == ARX_OK);
    REQUIRE(imported_model.copyVertices(0, imported_vertices.size(), imported_vertices.data()) == ARX_OK);
    for (std::size_t index = 0; index < source_vertices.size(); ++index) {
      CHECK(imported_vertices[index].position.x == doctest::Approx(source_vertices[index].position.x * 2.0f));
      CHECK(imported_vertices[index].position.y == doctest::Approx(source_vertices[index].position.y * 2.0f));
      CHECK(imported_vertices[index].position.z == doctest::Approx(source_vertices[index].position.z * 2.0f));
    }

    std::vector<ArxModelBone> source_bones(reference_model.boneCount());
    std::vector<ArxModelBone> imported_bones(imported_model.boneCount());
    REQUIRE(imported_bones.size() == source_bones.size());
    REQUIRE(reference_model.copyBones(0, source_bones.size(), source_bones.data()) == ARX_OK);
    REQUIRE(imported_model.copyBones(0, imported_bones.size(), imported_bones.data()) == ARX_OK);
    for (std::size_t index = 0; index < source_bones.size(); ++index) {
      CHECK(imported_bones[index].position.x == doctest::Approx(source_bones[index].position.x * 2.0f));
      CHECK(imported_bones[index].position.y == doctest::Approx(source_bones[index].position.y * 2.0f));
      CHECK(imported_bones[index].position.z == doctest::Approx(source_bones[index].position.z * 2.0f));
    }

    std::array<ArxAnimationKeyframe, 2> keyframes{};
    REQUIRE(animations[0]->copyKeyframes(0, keyframes.size(), keyframes.data()) == ARX_OK);
    CHECK(keyframes[1].root_translation.x == doctest::Approx(3.0f));
    CHECK(keyframes[1].root_translation.y == doctest::Approx(4.0f));
    CHECK(keyframes[1].root_translation.z == doctest::Approx(5.0f));
    std::array<ArxAnimationGroupTransform, 2> transforms{};
    REQUIRE(animations[0]->copyGroupTransforms(1, 0, transforms.size(), transforms.data()) == ARX_OK);
    CHECK(transforms[0].translation.x == doctest::Approx(8.0f));
    CHECK(transforms[0].translation.y == doctest::Approx(10.0f));
    CHECK(transforms[0].translation.z == doctest::Approx(12.0f));
    CHECK(transforms[1].translation.x == doctest::Approx(14.0f));
    CHECK(transforms[1].translation.y == doctest::Approx(16.0f));
    CHECK(transforms[1].translation.z == doctest::Approx(18.0f));
  }

  TEST_CASE("Skips an Animation with animated carrier scale") {
    pistoris::Model model;
    const std::vector<std::uint8_t> encoded = makeMotionModelGlb(model);
    nlohmann::json gltf = parseGlbJson(encoded);
    const std::size_t carrier = glbNodeIndex(gltf, "skeleton");
    REQUIRE(carrier < gltf["nodes"].size());
    for (const nlohmann::json& channel : gltf["animations"][0]["channels"])
      if (channel["target"]["node"] == carrier && channel["target"]["path"] == "translation") {
        nlohmann::json scale_channel = channel;
        scale_channel["target"]["path"] = "scale";
        gltf["animations"][0]["channels"].push_back(std::move(scale_channel));
        break;
      }

    pistoris::Model imported_model;
    std::vector<std::unique_ptr<pistoris::Animation>> animations;
    ArxAnimationConversionReport report{};
    REQUIRE(pistoris::Model::importGlb(imported_model, animations, replaceGlbJson(encoded, gltf), &report) == ARX_OK);
    CHECK(animations.empty());
    CHECK(report.converted == 0);
    CHECK(report.skipped == 1);
  }

  TEST_CASE("Preserves propelled motion across Model GLB unit ratios") {
    constexpr float kUnits = 43.0f;
    pistoris::Model model;
    const std::vector<std::uint8_t> encoded = makeMotionModelGlb(model, kUnits);
    pistoris::Model imported_model;
    std::vector<std::unique_ptr<pistoris::Animation>> animations;
    REQUIRE(pistoris::Model::importGlb(imported_model, animations, encoded, {.arx_units_per_glb_unit = kUnits}) ==
            ARX_OK);
    REQUIRE(animations.size() == 1);
    std::array<ArxAnimationKeyframe, 2> keyframes{};
    REQUIRE(animations[0]->copyKeyframes(0, keyframes.size(), keyframes.data()) == ARX_OK);
    CHECK(keyframes[1].root_translation.x == doctest::Approx(3.0f));
    CHECK(keyframes[1].root_translation.y == doctest::Approx(4.0f));
    CHECK(keyframes[1].root_translation.z == doctest::Approx(5.0f));
  }

  TEST_CASE("Uses the semantic origin as an unrigged motion carrier") {
    pistoris::Model model;
    REQUIRE(pistoris::Model::importNative(model, makeSemanticModelFtl()) == ARX_OK);
    model.clearSkeleton();
    pistoris::Animation animation;
    configureMotionAnimation(animation);
    const std::array<const pistoris::Animation*, 1> source_animations = {&animation};
    std::vector<std::uint8_t> encoded;
    REQUIRE(model.exportGlb(encoded, source_animations) == ARX_OK);

    pistoris::Model imported_model;
    std::vector<std::unique_ptr<pistoris::Animation>> animations;
    REQUIRE(pistoris::Model::importGlb(imported_model, animations, encoded) == ARX_OK);
    REQUIRE(animations.size() == 1);
    std::array<ArxAnimationKeyframe, 2> keyframes{};
    REQUIRE(animations[0]->copyKeyframes(0, keyframes.size(), keyframes.data()) == ARX_OK);
    CHECK(keyframes[1].root_translation.x == doctest::Approx(3.0f));
    CHECK(keyframes[1].root_translation.y == doctest::Approx(4.0f));
    CHECK(keyframes[1].root_translation.z == doctest::Approx(5.0f));
  }

  TEST_CASE("Normalizes imported propelled-motion rotations") {
    pistoris::Model model;
    const std::vector<std::uint8_t> encoded = makeMotionModelGlb(model);
    pistoris::Model imported_model;
    std::vector<std::unique_ptr<pistoris::Animation>> animations;
    REQUIRE(pistoris::Model::importGlb(imported_model, animations, scaleFirstAnimationRotation(encoded, 2.0f)) ==
            ARX_OK);
    REQUIRE(animations.size() == 1);
    std::array<ArxAnimationKeyframe, 2> keyframes{};
    REQUIRE(animations[0]->copyKeyframes(0, keyframes.size(), keyframes.data()) == ARX_OK);
    CHECK(keyframes[0].root_rotation.w == doctest::Approx(1.0f));
    CHECK(keyframes[1].root_rotation.w == doctest::Approx(0.70710678f));
    CHECK(keyframes[1].root_rotation.z == doctest::Approx(0.70710678f));
  }

  TEST_CASE("Resolves intermediary animation nodes into bone groups") {
    pistoris::Model model;
    const std::vector<std::uint8_t> encoded = makeMotionModelGlb(model);
    nlohmann::json gltf = parseGlbJson(encoded);
    const std::size_t parent = glbNodeIndexWithPrefix(gltf, "000__");
    const std::size_t child = glbNodeIndexWithPrefix(gltf, "001__");
    REQUIRE(parent < gltf["nodes"].size());
    REQUIRE(child < gltf["nodes"].size());
    const std::size_t intermediary = gltf["nodes"].size();
    gltf["nodes"].push_back({{"children", nlohmann::json::array({child})}, {"name", "intermediary"}});
    gltf["nodes"][child]["translation"] = {0.0f, 0.0f, 0.0f};
    for (nlohmann::json& node_child : gltf["nodes"][parent]["children"])
      if (node_child == child) node_child = intermediary;
    for (nlohmann::json& channel : gltf["animations"][0]["channels"])
      if (channel["target"]["node"] == child && channel["target"]["path"] != "scale")
        channel["target"]["node"] = intermediary;

    pistoris::Model intermediary_model;
    std::vector<std::unique_ptr<pistoris::Animation>> intermediary_animations;
    REQUIRE(pistoris::Model::importGlb(intermediary_model, intermediary_animations, replaceGlbJson(encoded, gltf)) ==
            ARX_OK);
    REQUIRE(intermediary_animations.size() == 1);
    std::array<ArxAnimationGroupTransform, 2> imported_transforms{};
    REQUIRE(intermediary_animations[0]->copyGroupTransforms(
                1, 0, imported_transforms.size(), imported_transforms.data()) == ARX_OK);
    CHECK(imported_transforms[1].translation.x == doctest::Approx(7.0f));
    CHECK(imported_transforms[1].translation.y == doctest::Approx(9.0f));
    CHECK(imported_transforms[1].translation.z == doctest::Approx(9.0f));
  }

  TEST_CASE("Imports Animation timing helpers and shifts negative source time") {
    pistoris::Model model;
    REQUIRE(pistoris::Model::importNative(model, makeSemanticModelFtl()) == ARX_OK);

    pistoris::Animation animation;
    REQUIRE(animation.setName("timing") == ARX_OK);
    const pistoris::SoundIndex end_sound = addSound(animation, "sfx/end.wav");
    const std::array<ArxAnimationGroupTransform, 2> transforms{};
    const std::array<ArxAnimationKeyframeInput, 2> inputs = {{
        {{0, {}, {}, 0, pistoris::kNoSound}, transforms.data(), transforms.size()},
        {{6, {12.0f, 0.0f, 0.0f}, {}, 1, end_sound}, transforms.data(), transforms.size()},
    }};
    REQUIRE(animation.replaceKeyframes(7, inputs.data(), inputs.size()) == ARX_OK);

    const std::array<const pistoris::Animation*, 1> animations = {&animation};
    std::vector<std::uint8_t> encoded;
    REQUIRE(model.exportGlb(encoded, animations) == ARX_OK);
    pistoris::glb::Asset asset;
    REQUIRE(pistoris::glb::parse(encoded, asset) == ARX_OK);
    CHECK(hasNode(*asset.data(), "SETTINGS__EXTRA_FRAME__STEPS_6__settings"));
    const cgltf_node* first_bone = nullptr;
    for (std::size_t index = 0; index < asset.data()->nodes_count; ++index)
      if (asset.data()->nodes[index].name != nullptr &&
          std::string_view(asset.data()->nodes[index].name).starts_with("000__")) {
        first_bone = &asset.data()->nodes[index];
        break;
      }
    REQUIRE(first_bone != nullptr);
    const std::span<const cgltf_animation_channel> channels(asset.data()->animations[0].channels,
                                                            asset.data()->animations[0].channels_count);
    CHECK(std::ranges::any_of(channels, [first_bone](const cgltf_animation_channel& channel) {
      return channel.target_node == first_bone && channel.target_path == cgltf_animation_path_type_rotation;
    }));

    {
      pistoris::Model imported_model;
      std::vector<std::unique_ptr<pistoris::Animation>> imported;
      REQUIRE(pistoris::Model::importGlb(imported_model, imported, encoded) == ARX_OK);
      REQUIRE(imported.size() == 1);
      CHECK(imported[0]->frameLength() == 7);
    }

    {
      std::uint32_t json_size = 0;
      std::memcpy(&json_size, encoded.data() + 12U, sizeof(json_size));
      nlohmann::json rotated = nlohmann::json::parse(encoded.begin() + 20, encoded.begin() + 20 + json_size);
      const auto carrier = std::ranges::find_if(
          rotated["nodes"], [](const nlohmann::json& node) { return node.value("name", std::string{}) == "skeleton"; });
      REQUIRE(carrier != rotated["nodes"].end());
      (*carrier)["rotation"] = {0.0f, 0.0f, 0.70710678f, 0.70710678f};

      pistoris::Model imported_model;
      std::vector<std::unique_ptr<pistoris::Animation>> imported;
      REQUIRE(pistoris::Model::importGlb(imported_model, imported, replaceGlbJson(encoded, rotated)) == ARX_OK);
      REQUIRE(imported.size() == 1);
      std::array<ArxAnimationKeyframe, 2> rotated_keyframes{};
      REQUIRE(imported[0]->copyKeyframes(0, rotated_keyframes.size(), rotated_keyframes.data()) == ARX_OK);
      CHECK(rotated_keyframes[1].root_translation.x == doctest::Approx(12.0f));
      CHECK(rotated_keyframes[1].root_translation.y == doctest::Approx(0.0f));
      CHECK(rotated_keyframes[1].root_translation.z == doctest::Approx(0.0f));
    }

    {
      std::uint32_t json_size = 0;
      std::memcpy(&json_size, encoded.data() + 12U, sizeof(json_size));
      nlohmann::json gltf = nlohmann::json::parse(encoded.begin() + 20, encoded.begin() + 20 + json_size);
      gltf["animations"][0]["name"] = "timing.invalid";
      pistoris::Model imported_model;
      std::vector<std::unique_ptr<pistoris::Animation>> imported;
      REQUIRE(pistoris::Model::importGlb(imported_model, imported, replaceGlbJson(encoded, gltf)) == ARX_OK);
      REQUIRE(imported.size() == 1);
      CHECK(imported[0]->name() == "timing-invalid");
    }

    {
      const std::vector<std::uint8_t> shortened =
          replaceAnimationSettings(encoded, "SETTINGS__FRAME_LENGTH_3__STEPS_6__settings");
      pistoris::Model imported_model;
      std::vector<std::unique_ptr<pistoris::Animation>> imported;
      REQUIRE(pistoris::Model::importGlb(imported_model, imported, shortened) == ARX_OK);
      REQUIRE(imported.size() == 1);
      CHECK(imported[0]->frameLength() == 3);
      REQUIRE(imported[0]->keyframeCount() == 2);
      std::array<ArxAnimationKeyframe, 2> keyframes{};
      REQUIRE(imported[0]->copyKeyframes(0, keyframes.size(), keyframes.data()) == ARX_OK);
      CHECK(keyframes[0].frame == 0);
      CHECK(keyframes[1].frame == 3);
      CHECK(keyframes[1].root_translation.x == doctest::Approx(6.0f));
      CHECK(keyframes[1].footstep == 0);
      CHECK(keyframes[1].sound == pistoris::kNoSound);
    }

    {
      const std::vector<std::uint8_t> shifted = replaceFirstAnimationTime(encoded, -0.25f);
      ModelGlbLogCapture logs;
      pistoris::Model imported_model;
      std::vector<std::unique_ptr<pistoris::Animation>> imported;
      REQUIRE(pistoris::Model::importGlb(imported_model, imported, shifted) == ARX_OK);
      REQUIRE(imported.size() == 1);
      CHECK(imported[0]->frameLength() == 13);
      REQUIRE(imported[0]->keyframeCount() == 3);
      std::array<ArxAnimationKeyframe, 3> keyframes{};
      REQUIRE(imported[0]->copyKeyframes(0, keyframes.size(), keyframes.data()) == ARX_OK);
      CHECK(keyframes[0].frame == 0);
      CHECK(keyframes[1].frame == 6);
      CHECK(keyframes[2].frame == 12);
      CHECK(keyframes[1].root_translation.x == doctest::Approx(6.0f));
      CHECK(keyframes[1].footstep == 1);
      REQUIRE(keyframes[1].sound != pistoris::kNoSound);
      ArxSoundView imported_sound{};
      REQUIRE(imported[0]->copySoundViews(keyframes[1].sound, 1, &imported_sound) == ARX_OK);
      CHECK(view(imported_sound.path) == "sfx/end.wav");
      CHECK(logs.contains("timeline repaired"));
      CHECK(logs.contains("shifted by"));
    }

    {
      const std::vector<std::uint8_t> rounded = replaceFirstAnimationTime(encoded, 0.01f);
      ModelGlbLogCapture logs;
      pistoris::Model imported_model;
      std::vector<std::unique_ptr<pistoris::Animation>> imported;
      REQUIRE(pistoris::Model::importGlb(imported_model, imported, rounded) == ARX_OK);
      REQUIRE(imported.size() == 1);
      CHECK(logs.contains("timestamp(s) rounded to 24 Hz"));
    }
  }

  TEST_CASE("Accepts mismatched Animation group counts and skips invalid sidecars") {
    pistoris::Model model;
    REQUIRE(pistoris::Model::importNative(model, makeSemanticModelFtl()) == ARX_OK);

    pistoris::Animation invalid;
    pistoris::Animation shorter;
    pistoris::Animation longer;
    pistoris::Animation collapsed_times;
    pistoris::Animation degenerate_rotation;
    REQUIRE(shorter.setName("shorter") == ARX_OK);
    std::array<ArxAnimationGroupTransform, 1> shorter_transforms{};
    shorter_transforms[0].translation.x = 1.0f;
    const ArxAnimationKeyframeInput shorter_input{
        {0, {}, {}, 0, pistoris::kNoSound}, shorter_transforms.data(), shorter_transforms.size()};
    REQUIRE(shorter.replaceKeyframes(0, &shorter_input, 1) == ARX_OK);
    REQUIRE(longer.setName("longer") == ARX_OK);
    std::array<ArxAnimationGroupTransform, 3> longer_transforms{};
    longer_transforms[1].translation.x = 2.0f;
    longer_transforms[2].rotation = {0.0f, 0.0f, 0.0f, 0.0f};
    const ArxAnimationKeyframeInput longer_input{
        {0, {}, {}, 0, pistoris::kNoSound}, longer_transforms.data(), longer_transforms.size()};
    REQUIRE(longer.replaceKeyframes(0, &longer_input, 1) == ARX_OK);
    REQUIRE(collapsed_times.setName("collapsed_times") == ARX_OK);
    const std::array<ArxAnimationGroupTransform, 2> compatible_transforms{};
    const std::array<ArxAnimationKeyframeInput, 2> collapsed_inputs = {{
        {{static_cast<std::uint32_t>(std::numeric_limits<std::int32_t>::max()) - 1U, {}, {}, 0, pistoris::kNoSound},
         compatible_transforms.data(),
         compatible_transforms.size()},
        {{static_cast<std::uint32_t>(std::numeric_limits<std::int32_t>::max()), {}, {}, 0, pistoris::kNoSound},
         compatible_transforms.data(),
         compatible_transforms.size()},
    }};
    REQUIRE(collapsed_times.replaceKeyframes(static_cast<std::uint32_t>(std::numeric_limits<std::int32_t>::max()),
                                             collapsed_inputs.data(),
                                             collapsed_inputs.size()) == ARX_OK);
    REQUIRE(degenerate_rotation.setName("degenerate_rotation") == ARX_OK);
    ArxAnimationKeyframe degenerate_keyframe{};
    degenerate_keyframe.root_rotation = {0.0f, 0.0f, 0.0f, 0.0f};
    const ArxAnimationKeyframeInput degenerate_input{
        degenerate_keyframe, compatible_transforms.data(), compatible_transforms.size()};
    REQUIRE(degenerate_rotation.replaceKeyframes(0, &degenerate_input, 1) == ARX_OK);
    const std::array<const pistoris::Animation*, 5> animations = {
        &invalid, &shorter, &longer, &collapsed_times, &degenerate_rotation};

    ModelGlbLogCapture logs;
    ArxAnimationConversionReport report{};
    std::vector<std::uint8_t> encoded;
    REQUIRE(model.exportGlb(encoded, animations, &report) == ARX_OK);
    CHECK(report.converted == 2);
    CHECK(report.skipped == 3);
    CHECK(logs.contains("1 trailing group(s) discarded"));
    pistoris::glb::Asset asset;
    REQUIRE(pistoris::glb::parse(encoded, asset) == ARX_OK);
    CHECK(asset.data()->animations_count == 2);

    pistoris::Model imported_model;
    std::vector<std::unique_ptr<pistoris::Animation>> imported;
    REQUIRE(pistoris::Model::importGlb(imported_model, imported, encoded) == ARX_OK);
    REQUIRE(imported.size() == 2);
    CHECK(imported[0]->groupCount() == 1);
    CHECK(imported[1]->groupCount() == 2);
  }

  TEST_CASE("GLB Animation group metadata preserves explicit state and inferred motion") {
    pistoris::Model model;
    REQUIRE(pistoris::Model::importNative(model, makeSemanticModelFtl()) == ARX_OK);
    ArxModelBone branch{};
    const std::string branch_name = "branch";
    branch.name = view(branch_name);
    branch.parent = 1;
    pistoris::BoneIndex branch_index = pistoris::kInvalidBoneIndex;
    REQUIRE(model.addBone(branch, branch_index) == ARX_OK);
    REQUIRE(branch_index == 2);
    pistoris::BoneIndex parent = branch_index;
    while (model.boneCount() < 39U) {
      const float ordinal = static_cast<float>(model.boneCount());
      const std::string name = std::format("deep-{}", model.boneCount());
      ArxModelBone bone{};
      bone.name = view(name);
      bone.position = {ordinal * 123.4567f, ordinal * -78.9012f, ordinal * 0.3333f};
      bone.parent = parent;
      pistoris::BoneIndex added = pistoris::kInvalidBoneIndex;
      REQUIRE(model.addBone(bone, added) == ARX_OK);
      parent = added;
    }

    pistoris::Animation trimmed;
    REQUIRE(trimmed.setName("trimmed") == ARX_OK);
    std::array<ArxAnimationGroupTransform, 3> trimmed_transforms{};
    trimmed_transforms[0].translation.x = 1.0f;
    const ArxAnimationKeyframeInput trimmed_input{
        {0, {}, {}, 0, pistoris::kNoSound}, trimmed_transforms.data(), trimmed_transforms.size()};
    REQUIRE(trimmed.replaceKeyframes(0, &trimmed_input, 1) == ARX_OK);

    pistoris::Animation internal_gap;
    REQUIRE(internal_gap.setName("internal-gap") == ARX_OK);
    std::array<ArxAnimationGroupTransform, 3> gap_first_transforms{};
    std::array<ArxAnimationGroupTransform, 3> gap_second_transforms{};
    gap_first_transforms[0].translation.x = 5.0e-5f;
    gap_second_transforms[0].translation.x = 1.0f;
    gap_first_transforms[1].translation.y = 5.0e-5f;
    gap_first_transforms[1].scale.z = 1.00005f;
    gap_second_transforms[1].translation.y = -5.0e-5f;
    gap_second_transforms[1].scale.z = 0.99995f;
    gap_second_transforms[2].translation.z = 2.0f;
    const std::array<ArxAnimationKeyframeInput, 2> gap_inputs = {{
        {{0, {}, {}, 0, pistoris::kNoSound}, gap_first_transforms.data(), gap_first_transforms.size()},
        {{1, {}, {}, 0, pistoris::kNoSound}, gap_second_transforms.data(), gap_second_transforms.size()},
    }};
    REQUIRE(internal_gap.replaceKeyframes(1, gap_inputs.data(), gap_inputs.size()) == ARX_OK);

    pistoris::Animation neutral;
    REQUIRE(neutral.setName("neutral") == ARX_OK);
    const std::array<ArxAnimationGroupTransform, 3> neutral_transforms{};
    const ArxAnimationKeyframeInput neutral_input{
        {0, {}, {}, 0, pistoris::kNoSound}, neutral_transforms.data(), neutral_transforms.size()};
    REQUIRE(neutral.replaceKeyframes(0, &neutral_input, 1) == ARX_OK);

    pistoris::Animation negative_identity;
    REQUIRE(negative_identity.setName("negative-identity") == ARX_OK);
    std::array<ArxAnimationGroupTransform, 3> negative_identity_transforms{};
    negative_identity_transforms[2].rotation.w = -1.0f;
    const ArxAnimationKeyframeInput negative_identity_input{
        {0, {}, {}, 0, pistoris::kNoSound}, negative_identity_transforms.data(), negative_identity_transforms.size()};
    REQUIRE(negative_identity.replaceKeyframes(0, &negative_identity_input, 1) == ARX_OK);
    REQUIRE(negative_identity.claimGroup(2) == ARX_OK);

    const std::array<const pistoris::Animation*, 4> animations = {
        &trimmed, &internal_gap, &neutral, &negative_identity};
    std::vector<std::uint8_t> encoded;
    REQUIRE(model.exportGlb(encoded, animations) == ARX_OK);
    pistoris::glb::Asset asset;
    REQUIRE(pistoris::glb::parse(encoded, asset) == ARX_OK);
    CHECK(hasNode(*asset.data(), "GROUPS__VOID_1-38__groups"));
    CHECK(hasNode(*asset.data(), "GROUPS__VOID_3-38__CLAIM_1__groups"));
    CHECK(hasNode(*asset.data(), "GROUPS__VOID_0-38__groups"));
    CHECK(hasNode(*asset.data(), "GROUPS__VOID_0-1_3-38__CLAIM_2__groups"));

    pistoris::Model imported_model;
    std::vector<std::unique_ptr<pistoris::Animation>> imported;
    REQUIRE(pistoris::Model::importGlb(imported_model, imported, encoded) == ARX_OK);
    REQUIRE(imported.size() == 4);
    CHECK(imported[0]->groupCount() == 1);
    CHECK(imported[1]->groupCount() == 3);
    CHECK(imported[2]->groupCount() == 0);
    CHECK(imported[3]->groupCount() == 3);
    std::array<ArxAnimationGroupTransform, 3> imported_first{};
    std::array<ArxAnimationGroupTransform, 3> imported_second{};
    REQUIRE(imported[1]->copyGroupTransforms(0, 0, imported_first.size(), imported_first.data()) == ARX_OK);
    REQUIRE(imported[1]->copyGroupTransforms(1, 0, imported_second.size(), imported_second.data()) == ARX_OK);
    CHECK(imported_first[0].translation.x == doctest::Approx(5.0e-5f));
    CHECK(imported_first[1].translation.y == doctest::Approx(5.0e-5f));
    CHECK(imported_first[1].scale.z == doctest::Approx(1.00005f));
    CHECK(imported_second[1].translation.y == doctest::Approx(-5.0e-5f));
    CHECK(imported_second[1].scale.z == doctest::Approx(0.99995f));
    bool state = false;
    REQUIRE(imported[1]->isGroupClaimed(1, state) == ARX_OK);
    CHECK(state);
    REQUIRE(imported[3]->isGroupClaimed(2, state) == ARX_OK);
    CHECK(state);

    pistoris::tea::Data baked_gap;
    REQUIRE(imported[1]->bakeNative(baked_gap) == ARX_OK);
    REQUIRE(baked_gap.keyframes.size() == 2);
    for (const pistoris::tea::Keyframe& keyframe : baked_gap.keyframes) {
      REQUIRE(keyframe.groups.size() == 3);
      CHECK(keyframe.groups[1].quat.w == 1.0f);
      CHECK(keyframe.groups[1].quat.x == 0.0f);
      CHECK(keyframe.groups[1].quat.y == 0.0f);
      CHECK(keyframe.groups[1].quat.z == 0.0f);
      CHECK(keyframe.groups[1].translate.y != 0.0f);
    }

    std::uint32_t json_size = 0;
    std::memcpy(&json_size, encoded.data() + 12U, sizeof(json_size));
    const nlohmann::json source_json = nlohmann::json::parse(encoded.begin() + 20, encoded.begin() + 20 + json_size);
    nlohmann::json with_trailing_void = source_json;
    std::size_t trimmed_helper = with_trailing_void["nodes"].size();
    for (std::size_t index = 0; index < with_trailing_void["nodes"].size(); ++index)
      if (with_trailing_void["nodes"][index].value("name", std::string{}) == "arx_animation__trimmed")
        trimmed_helper = index;
    REQUIRE(trimmed_helper < with_trailing_void["nodes"].size());
    const std::size_t trailing_void = with_trailing_void["nodes"].size();
    with_trailing_void["nodes"].push_back({{"name", "GROUPS__VOID_1-2__groups"}});
    with_trailing_void["nodes"][trimmed_helper]["children"].push_back(trailing_void);
    pistoris::Model explicit_void_model;
    std::vector<std::unique_ptr<pistoris::Animation>> explicit_void;
    REQUIRE(pistoris::Model::importGlb(
                explicit_void_model, explicit_void, replaceGlbJson(encoded, with_trailing_void)) == ARX_OK);
    REQUIRE(explicit_void.size() == 4);
    CHECK(explicit_void[0]->groupCount() == 1);

    nlohmann::json without_groups = source_json;
    for (nlohmann::json& node : without_groups["nodes"])
      if (node.value("name", std::string{}).starts_with("GROUPS__")) node["name"] = "groups";
    pistoris::Model inferred_model;
    std::vector<std::unique_ptr<pistoris::Animation>> inferred;
    REQUIRE(pistoris::Model::importGlb(inferred_model, inferred, replaceGlbJson(encoded, without_groups)) == ARX_OK);
    REQUIRE(inferred.size() == 4);
    CHECK(inferred[0]->groupCount() == 1);
    CHECK(inferred[1]->groupCount() == 3);
    CHECK(inferred[2]->groupCount() == 0);
    CHECK(inferred[3]->groupCount() == 0);
  }

  TEST_CASE("GLB Animation projection claims rotations that normalize to identity") {
    pistoris::Model model;
    REQUIRE(pistoris::Model::importNative(model, makeSemanticModelFtl()) == ARX_OK);

    pistoris::Animation animation;
    REQUIRE(animation.setName("normalized-identity") == ARX_OK);
    std::array<ArxAnimationGroupTransform, 2> transforms{};
    transforms[1].rotation.w = 2.0f;
    const ArxAnimationKeyframeInput input{{0, {}, {}, 0, pistoris::kNoSound}, transforms.data(), transforms.size()};
    REQUIRE(animation.replaceKeyframes(0, &input, 1) == ARX_OK);

    const std::array<const pistoris::Animation*, 1> animations = {&animation};
    std::vector<std::uint8_t> encoded;
    REQUIRE(model.exportGlb(encoded, animations) == ARX_OK);
    pistoris::glb::Asset asset;
    REQUIRE(pistoris::glb::parse(encoded, asset) == ARX_OK);
    CHECK(hasNode(*asset.data(), "GROUPS__VOID_0__CLAIM_1__groups"));

    pistoris::Model imported_model;
    std::vector<std::unique_ptr<pistoris::Animation>> imported;
    REQUIRE(pistoris::Model::importGlb(imported_model, imported, encoded) == ARX_OK);
    REQUIRE(imported.size() == 1);
    CHECK(imported.front()->groupCount() == 2);
    bool claimed = false;
    REQUIRE(imported.front()->isGroupClaimed(1, claimed) == ARX_OK);
    CHECK(claimed);
    ArxAnimationGroupTransform transform{};
    REQUIRE(imported.front()->copyGroupTransforms(0, 1, 1, &transform) == ARX_OK);
    CHECK(transform.rotation.w == doctest::Approx(1.0f));
  }

  TEST_CASE("Skips only the Animation when its sound path cannot be repaired") {
    pistoris::Model model;
    REQUIRE(pistoris::Model::importNative(model, makeSemanticModelFtl()) == ARX_OK);

    pistoris::Animation animation;
    REQUIRE(animation.setName("walk") == ARX_OK);
    const pistoris::SoundIndex sound = addSound(animation, "sfx/step.wav");
    const std::array<ArxAnimationGroupTransform, 2> transforms{};
    const ArxAnimationKeyframeInput input{{0, {}, {}, 0, sound}, transforms.data(), transforms.size()};
    REQUIRE(animation.replaceKeyframes(0, &input, 1) == ARX_OK);
    const std::array<const pistoris::Animation*, 1> animations = {&animation};
    std::vector<std::uint8_t> encoded;
    REQUIRE(model.exportGlb(encoded, animations) == ARX_OK);

    std::uint32_t json_size = 0;
    std::memcpy(&json_size, encoded.data() + 12U, sizeof(json_size));
    nlohmann::json gltf = nlohmann::json::parse(encoded.begin() + 20, encoded.begin() + 20 + json_size);
    for (nlohmann::json& node : gltf["nodes"])
      if (node.value("name", std::string{}) == "PATH_sfx/step.wav__path") node["name"] = "PATH_../step.wav__path";

    ModelGlbLogCapture logs;
    pistoris::Model imported_model;
    std::vector<std::unique_ptr<pistoris::Animation>> imported_animations;
    ArxAnimationConversionReport report{};
    REQUIRE(pistoris::Model::importGlb(imported_model, imported_animations, replaceGlbJson(encoded, gltf), &report) ==
            ARX_OK);
    CHECK(imported_animations.empty());
    CHECK(report.converted == 0);
    CHECK(report.skipped == 1);
    CHECK(logs.containsCode(ARX_ANIMATION_BAD_SOUND_PATH));
  }

  TEST_CASE("Skips malformed Animation helpers and channels with focused diagnostics") {
    pistoris::Model model;
    REQUIRE(pistoris::Model::importNative(model, makeSemanticModelFtl()) == ARX_OK);

    pistoris::Animation animation;
    REQUIRE(animation.setName("walk") == ARX_OK);
    REQUIRE(animation.setResourcePath("anim:npc:walk") == ARX_OK);
    const std::array<ArxAnimationGroupTransform, 2> transforms{};
    const ArxAnimationKeyframeInput input{
        {0, {}, {0.9238795f, 0.0f, 0.0f, 0.3826834f}, 0, pistoris::kNoSound}, transforms.data(), transforms.size()};
    REQUIRE(animation.replaceKeyframes(0, &input, 1) == ARX_OK);
    const std::array<const pistoris::Animation*, 1> animations = {&animation};
    std::vector<std::uint8_t> encoded;
    REQUIRE(model.exportGlb(encoded, animations) == ARX_OK);

    std::uint32_t json_size = 0;
    std::memcpy(&json_size, encoded.data() + 12U, sizeof(json_size));
    const nlohmann::json source = nlohmann::json::parse(encoded.begin() + 20, encoded.begin() + 20 + json_size);
    std::size_t helper = source["nodes"].size();
    for (std::size_t index = 0; index < source["nodes"].size(); ++index)
      if (source["nodes"][index].value("name", std::string{}) == "arx_animation__walk") helper = index;
    REQUIRE(helper < source["nodes"].size());

    SUBCASE("animation name without helper") {
      nlohmann::json gltf = source;
      gltf["animations"][0]["name"] = "walk__fast";
      gltf["nodes"][helper]["name"] = "helper";

      ModelGlbLogCapture logs;
      pistoris::Model imported_model;
      std::vector<std::unique_ptr<pistoris::Animation>> imported_animations;
      ArxAnimationConversionReport report{};
      REQUIRE(pistoris::Model::importGlb(imported_model, imported_animations, replaceGlbJson(encoded, gltf), &report) ==
              ARX_OK);
      CHECK(imported_animations.empty());
      CHECK(report.converted == 0);
      CHECK(report.skipped == 1);
      CHECK(logs.containsCode(ARX_GLB_BAD_ANIMATION_NAME));
    }

    SUBCASE("helper") {
      nlohmann::json gltf = source;
      const std::size_t duplicate_path = gltf["nodes"].size();
      gltf["nodes"].push_back({{"name", "PATH_anim:npc:other__path"}});
      gltf["nodes"][helper]["children"].push_back(duplicate_path);

      ModelGlbLogCapture logs;
      pistoris::Model imported_model;
      std::vector<std::unique_ptr<pistoris::Animation>> imported_animations;
      ArxAnimationConversionReport report{};
      REQUIRE(pistoris::Model::importGlb(imported_model, imported_animations, replaceGlbJson(encoded, gltf), &report) ==
              ARX_OK);
      CHECK(imported_animations.empty());
      CHECK(report.converted == 0);
      CHECK(report.skipped == 1);
      CHECK(logs.containsCode(ARX_GLB_BAD_ANIMATION_HELPER));
    }

    SUBCASE("path label") {
      nlohmann::json gltf = source;
      for (nlohmann::json& node : gltf["nodes"])
        if (node.value("name", std::string{}) == "PATH_anim:npc:walk__path") node["name"] = "PATH_anim:npc:walk";

      ModelGlbLogCapture logs;
      pistoris::Model imported_model;
      std::vector<std::unique_ptr<pistoris::Animation>> imported_animations;
      ArxAnimationConversionReport report{};
      REQUIRE(pistoris::Model::importGlb(imported_model, imported_animations, replaceGlbJson(encoded, gltf), &report) ==
              ARX_OK);
      CHECK(imported_animations.empty());
      CHECK(report.converted == 0);
      CHECK(report.skipped == 1);
      CHECK(logs.containsCode(ARX_GLB_BAD_ANIMATION_HELPER));
    }

    SUBCASE("conflicting group state") {
      nlohmann::json gltf = source;
      const std::size_t conflicting_groups = gltf["nodes"].size();
      gltf["nodes"].push_back({{"name", "GROUPS__VOID_0__CLAIM_0__groups"}});
      gltf["nodes"][helper]["children"].push_back(conflicting_groups);

      ModelGlbLogCapture logs;
      pistoris::Model imported_model;
      std::vector<std::unique_ptr<pistoris::Animation>> imported_animations;
      ArxAnimationConversionReport report{};
      REQUIRE(pistoris::Model::importGlb(imported_model, imported_animations, replaceGlbJson(encoded, gltf), &report) ==
              ARX_OK);
      CHECK(imported_animations.empty());
      CHECK(report.skipped == 1);
      CHECK(logs.containsCode(ARX_GLB_BAD_ANIMATION_HELPER));
    }

    SUBCASE("channel") {
      nlohmann::json gltf = source;
      std::size_t mesh = gltf["nodes"].size();
      for (std::size_t index = 0; index < gltf["nodes"].size(); ++index)
        if (gltf["nodes"][index].contains("mesh")) mesh = index;
      REQUIRE(mesh < gltf["nodes"].size());
      gltf["animations"][0]["channels"][0]["target"]["node"] = mesh;

      ModelGlbLogCapture logs;
      pistoris::Model imported_model;
      std::vector<std::unique_ptr<pistoris::Animation>> imported_animations;
      ArxAnimationConversionReport report{};
      REQUIRE(pistoris::Model::importGlb(imported_model, imported_animations, replaceGlbJson(encoded, gltf), &report) ==
              ARX_OK);
      CHECK(imported_animations.empty());
      CHECK(report.converted == 0);
      CHECK(report.skipped == 1);
      CHECK(logs.containsCode(ARX_GLB_BAD_ANIMATION_BINDING));
    }

    SUBCASE("zero-length rotation") {
      ModelGlbLogCapture logs;
      pistoris::Model imported_model;
      std::vector<std::unique_ptr<pistoris::Animation>> imported_animations;
      ArxAnimationConversionReport report{};
      REQUIRE(pistoris::Model::importGlb(
                  imported_model, imported_animations, scaleFirstAnimationRotation(encoded, 0.0f), &report) == ARX_OK);
      CHECK(imported_animations.empty());
      CHECK(report.converted == 0);
      CHECK(report.skipped == 1);
      CHECK(logs.containsCode(ARX_GLB_BAD_ANIMATION_SAMPLER));
    }

    SUBCASE("non-finite time") {
      ModelGlbLogCapture logs;
      pistoris::Model imported_model;
      std::vector<std::unique_ptr<pistoris::Animation>> imported_animations;
      ArxAnimationConversionReport report{};
      REQUIRE(pistoris::Model::importGlb(imported_model,
                                         imported_animations,
                                         replaceFirstAnimationTime(encoded, std::numeric_limits<float>::quiet_NaN()),
                                         &report) == ARX_OK);
      CHECK(imported_animations.empty());
      CHECK(report.converted == 0);
      CHECK(report.skipped == 1);
      CHECK(logs.containsCode(ARX_GLB_BAD_FORMAT));
    }

    SUBCASE("channel outside Model root") {
      nlohmann::json gltf = source;
      const std::size_t outside_mesh = gltf["meshes"].size();
      nlohmann::json mesh = gltf["meshes"][0];
      for (nlohmann::json& primitive : mesh["primitives"])
        primitive["targets"] = nlohmann::json::array({{{"POSITION", primitive["attributes"]["POSITION"]}}});
      gltf["meshes"].push_back(std::move(mesh));
      const std::size_t outside = gltf["nodes"].size();
      gltf["nodes"].push_back({{"mesh", outside_mesh}, {"name", "outside"}});
      gltf["scenes"][static_cast<std::size_t>(gltf.value("scene", 0))]["nodes"].push_back(outside);
      const std::size_t sampler = gltf["animations"][0]["samplers"].size();
      const std::size_t time_accessor = gltf["animations"][0]["samplers"][0]["input"];
      gltf["animations"][0]["samplers"].push_back({{"input", time_accessor}, {"output", time_accessor}});
      gltf["animations"][0]["channels"].push_back(
          {{"sampler", sampler}, {"target", {{"node", outside}, {"path", "weights"}}}});

      pistoris::Model imported_model;
      std::vector<std::unique_ptr<pistoris::Animation>> imported_animations;
      ArxAnimationConversionReport report{};
      REQUIRE(pistoris::Model::importGlb(imported_model, imported_animations, replaceGlbJson(encoded, gltf), &report) ==
              ARX_OK);
      CHECK(imported_animations.size() == 1);
      CHECK(report.converted == 1);
      CHECK(report.skipped == 0);
    }

    SUBCASE("animation helper below terminal node") {
      nlohmann::json gltf = source;
      const std::size_t nested = gltf["nodes"].size();
      gltf["nodes"].push_back({{"name", "arx_animation__walk"}});
      gltf["nodes"][helper]["children"].push_back(nested);

      pistoris::Model imported_model;
      std::vector<std::unique_ptr<pistoris::Animation>> imported_animations;
      ArxAnimationConversionReport report{};
      REQUIRE(pistoris::Model::importGlb(imported_model, imported_animations, replaceGlbJson(encoded, gltf), &report) ==
              ARX_OK);
      CHECK(imported_animations.size() == 1);
      CHECK(report.converted == 1);
      CHECK(report.skipped == 0);
    }
  }
}
