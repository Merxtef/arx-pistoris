// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#pragma once

#include "arx_pistoris/base/flags.h"
#include "arx_pistoris/base/math.hpp"

#include "accessor.h"

#include <array>
#include <cstdint>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace pistoris::glb {

struct Primitive {
  int indices = -1;
  int material = -1;
  std::vector<std::pair<std::string, int>> attributes;
};

enum class AnimationPath : std::uint8_t {
  kTranslation,
  kRotation,
  kScale,
};

struct AnimationChannel {
  int input = -1;
  int output = -1;
  int node = -1;
  AnimationPath path = AnimationPath::kTranslation;
};

class Builder {
 public:
  Builder();

  template <class T>
  int addAccessor(std::span<const T> values, cgltf_component_type component_type, cgltf_type type) {
    std::span<const std::uint8_t> bytes(reinterpret_cast<const std::uint8_t*>(values.data()), values.size_bytes());
    return addAccessorBytes(bytes, values.size(), component_type, type);
  }

  int addVec3Accessor(std::span<const Vec3> values);
  int addTimeAccessor(std::span<const float> values);
  int addExternalTexture(std::string name, std::string uri);
  int addEmbeddedTexture(std::string name, std::string mime_type, std::span<const std::uint8_t> encoded);
  int addMaterial(std::string name, int texture, FaceType flags, float alpha, bool alpha_cutout = false);
  int addColorMaterial(std::string name, std::array<float, 4> color, bool double_sided = false);
  int addPointLight(std::string name, Vec3 color, float intensity, float range);
  int addMesh(std::string name, std::vector<Primitive> primitives);
  int addSkin(std::string name, std::span<const int> joints, int skeleton, int inverse_bind_matrices = -1);
  int addAnimation(std::string name, std::vector<AnimationChannel> channels);
  int addNode(std::string name, int mesh = -1);
  void setNodeExtrasJson(int node, std::string json);
  void setContentBasisRotation(const ArxQuat& rotation);
  void setNodeTranslation(int node, Vec3 translation);
  void setNodeRotation(int node, const ArxQuat& rotation);
  void setNodeLight(int node, int light);
  void setNodeSkin(int node, int skin);
  void addChild(int parent, int child);
  void addRoot(int node);
  void setRootTransform(std::string name, Vec3 translation, const ArxQuat& rotation, float uniform_scale);
  int addDebugMeshNode(std::string name, std::span<const Vec3> positions, std::span<const std::uint32_t> indices,
                       int material);

  ArxReturnCode write(std::vector<std::uint8_t>& out) const;

 private:
  struct AccessorDesc {
    std::size_t offset = 0;
    std::size_t size = 0;
    std::size_t count = 0;
    cgltf_component_type component_type = cgltf_component_type_invalid;
    cgltf_type type = cgltf_type_invalid;
    bool has_min_max = false;
    float minimum = 0.0f;
    float maximum = 0.0f;
  };

  struct MaterialDesc {
    std::string name;
    int texture = -1;
    std::array<float, 4> color{1.0f, 1.0f, 1.0f, 1.0f};
    bool double_sided = false;
    cgltf_alpha_mode alpha_mode = cgltf_alpha_mode_opaque;
  };

  struct TextureDesc {
    std::string name;
    std::string uri;
    std::string mime_type;
    std::size_t offset = 0;
    std::size_t size = 0;
  };

  struct MeshDesc {
    std::string name;
    std::vector<Primitive> primitives;
  };

  struct LightDesc {
    std::string name;
    Vec3 color{};
    float intensity = 0.0f;
    float range = 0.0f;
  };

  struct SkinDesc {
    std::string name;
    std::vector<int> joints;
    int skeleton = -1;
    int inverse_bind_matrices = -1;
  };

  struct NodeDesc {
    std::string name;
    std::string extras_json;
    int mesh = -1;
    int light = -1;
    int skin = -1;
    bool has_translation = false;
    bool has_rotation = false;
    Vec3 translation{};
    ArxQuat rotation{};
    std::vector<int> children;
  };

  struct AnimationDesc {
    std::string name;
    std::vector<AnimationChannel> channels;
  };

  int addAccessorBytes(std::span<const std::uint8_t> bytes, std::size_t count, cgltf_component_type component_type,
                       cgltf_type type);

  std::vector<std::uint8_t> bin_;
  std::vector<AccessorDesc> accessors_;
  std::vector<TextureDesc> textures_;
  std::vector<MaterialDesc> materials_;
  std::vector<MeshDesc> meshes_;
  std::vector<LightDesc> lights_;
  std::vector<SkinDesc> skins_;
  std::vector<NodeDesc> nodes_;
  std::vector<AnimationDesc> animations_;
  std::vector<int> roots_;
  ArxQuat content_basis_rotation_ = {1.0f, 0.0f, 0.0f, 0.0f};
  bool root_transform_enabled_ = false;
  std::string root_transform_name_;
  Vec3 root_translation_{};
  ArxQuat root_rotation_ = {1.0f, 0.0f, 0.0f, 0.0f};
  float root_scale_ = 1.0f;
};

}  // namespace pistoris::glb
