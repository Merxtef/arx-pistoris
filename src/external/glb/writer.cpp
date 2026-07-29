// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#include "writer.h"

#include "arx_pistoris/arx_math.h"
#include "arx_pistoris/flags.h"
#include "arx_pistoris/pistoris_types.h"

#include "cgltf/cgltf_write.h"
#include "external/glb/accessor.h"
#include "external/glb/container.h"
#include "utils/math/quat.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <limits>
#include <span>
#include <string>
#include <utility>
#include <vector>

namespace pistoris::glb {
namespace {

constexpr std::uint32_t kGlbMagic = 0x46546c67;
constexpr std::uint32_t kGlbV2 = 2;
constexpr std::uint32_t kJsonChunk = 0x4e4f534a;
constexpr std::uint32_t kBinChunk = 0x004e4942;

void append32(std::vector<std::uint8_t>& out, std::uint32_t value) {
  std::size_t offset = out.size();
  out.resize(offset + sizeof(value));
  std::memcpy(out.data() + offset, &value, sizeof(value));
}

char* strPtr(const std::string& value) { return value.empty() ? nullptr : const_cast<char*>(value.c_str()); }

bool validIndex(int value, std::size_t size) { return value >= 0 && static_cast<std::size_t>(value) < size; }

std::string slashPath(std::string path) {
  std::replace(path.begin(), path.end(), '\\', '/');
  return path;
}

}  // namespace

Builder::Builder() = default;

int Builder::addAccessorBytes(std::span<const std::uint8_t> bytes, std::size_t count,
                              cgltf_component_type component_type, cgltf_type type) {
  while (bin_.size() % 4 != 0) bin_.push_back(0);
  AccessorDesc desc;
  desc.offset = bin_.size();
  desc.size = bytes.size();
  desc.count = count;
  desc.component_type = component_type;
  desc.type = type;
  bin_.insert(bin_.end(), bytes.begin(), bytes.end());
  int index = static_cast<int>(accessors_.size());
  accessors_.push_back(desc);
  return index;
}

int Builder::addVec3Accessor(std::span<const Vec3> values) {
  if (content_basis_rotation_ == math::kIdentityQuat)
    return addAccessor(values, cgltf_component_type_r_32f, cgltf_type_vec3);

  std::vector<Vec3> converted;
  converted.reserve(values.size());
  for (const Vec3& value : values) {
    const ArxVector3 rotated = math::rotate(content_basis_rotation_, {value.x, value.y, value.z});
    converted.push_back({rotated.x, rotated.y, rotated.z});
  }
  return addAccessor(std::span<const Vec3>(converted), cgltf_component_type_r_32f, cgltf_type_vec3);
}

int Builder::addExternalTexture(std::string name, std::string uri) {
  int index = static_cast<int>(textures_.size());
  TextureDesc desc;
  desc.name = std::move(name);
  desc.uri = slashPath(std::move(uri));
  textures_.push_back(std::move(desc));
  return index;
}

int Builder::addEmbeddedTexture(std::string name, std::string mime_type, std::span<const std::uint8_t> encoded) {
  while (bin_.size() % 4 != 0) bin_.push_back(0);
  TextureDesc desc;
  desc.name = std::move(name);
  desc.mime_type = std::move(mime_type);
  desc.offset = bin_.size();
  desc.size = encoded.size();
  bin_.insert(bin_.end(), encoded.begin(), encoded.end());
  int index = static_cast<int>(textures_.size());
  textures_.push_back(std::move(desc));
  return index;
}

int Builder::addMaterial(std::string name, int texture, FaceType flags, float alpha, bool alpha_cutout) {
  MaterialDesc desc;
  desc.name = std::move(name);
  desc.texture = texture;
  desc.color = {1.0f, 1.0f, 1.0f, alpha};
  desc.double_sided = (flags & kFaceBitDoublesided) != 0;
  desc.alpha_mode = (flags & kFaceBitTrans) != 0 ? cgltf_alpha_mode_blend
                    : alpha_cutout               ? cgltf_alpha_mode_mask
                                                 : cgltf_alpha_mode_opaque;
  int index = static_cast<int>(materials_.size());
  materials_.push_back(std::move(desc));
  return index;
}

int Builder::addColorMaterial(std::string name, std::array<float, 4> color, bool double_sided) {
  MaterialDesc desc;
  desc.name = std::move(name);
  desc.color = color;
  desc.double_sided = double_sided;
  desc.alpha_mode = color[3] < 1.0f ? cgltf_alpha_mode_blend : cgltf_alpha_mode_opaque;
  int index = static_cast<int>(materials_.size());
  materials_.push_back(std::move(desc));
  return index;
}

int Builder::addPointLight(std::string name, Vec3 color, float intensity, float range) {
  int index = static_cast<int>(lights_.size());
  lights_.push_back({std::move(name), color, intensity, range});
  return index;
}

int Builder::addMesh(std::string name, std::vector<Primitive> primitives) {
  int index = static_cast<int>(meshes_.size());
  meshes_.push_back({std::move(name), std::move(primitives)});
  return index;
}

int Builder::addNode(std::string name, int mesh) {
  int index = static_cast<int>(nodes_.size());
  NodeDesc desc;
  desc.name = std::move(name);
  desc.mesh = mesh;
  nodes_.push_back(std::move(desc));
  return index;
}

void Builder::setContentBasisRotation(const ArxQuat& rotation) { content_basis_rotation_ = math::normalize(rotation); }

void Builder::setNodeTranslation(int node, Vec3 translation) {
  if (!validIndex(node, nodes_.size())) return;
  const ArxVector3 converted = math::rotate(content_basis_rotation_, {translation.x, translation.y, translation.z});
  nodes_[static_cast<std::size_t>(node)].has_translation = true;
  nodes_[static_cast<std::size_t>(node)].translation = {converted.x, converted.y, converted.z};
}

void Builder::setNodeRotation(int node, const ArxQuat& rotation) {
  if (!validIndex(node, nodes_.size())) return;
  nodes_[static_cast<std::size_t>(node)].has_rotation = true;
  nodes_[static_cast<std::size_t>(node)].rotation =
      math::normalize(content_basis_rotation_ * rotation * math::conjugate(content_basis_rotation_));
}

void Builder::setNodeLight(int node, int light) {
  if (!validIndex(node, nodes_.size()) || !validIndex(light, lights_.size())) return;
  nodes_[static_cast<std::size_t>(node)].light = light;
}

void Builder::addChild(int parent, int child) {
  if (!validIndex(parent, nodes_.size()) || !validIndex(child, nodes_.size())) return;
  nodes_[static_cast<std::size_t>(parent)].children.push_back(child);
}

void Builder::addRoot(int node) {
  if (validIndex(node, nodes_.size())) roots_.push_back(node);
}

void Builder::setRootTransform(std::string name, Vec3 translation, const ArxQuat& rotation, float uniform_scale) {
  root_transform_enabled_ = true;
  root_transform_name_ = std::move(name);
  root_translation_ = translation;
  root_rotation_ = rotation;
  root_scale_ = uniform_scale;
}

int Builder::addDebugMeshNode(std::string name, std::span<const Vec3> positions, std::span<const std::uint32_t> indices,
                              int material) {
  Primitive primitive;
  primitive.indices = addAccessor(indices, cgltf_component_type_r_32u, cgltf_type_scalar);
  primitive.material = material;
  primitive.attributes.emplace_back("POSITION", addVec3Accessor(positions));
  int mesh = addMesh(name, {std::move(primitive)});
  int node = addNode(std::move(name), mesh);
  addRoot(node);
  return node;
}

ArxReturnCode Builder::write(std::vector<std::uint8_t>& out) const {
  if (bin_.size() > std::numeric_limits<std::uint32_t>::max()) return ARX_GLB_BAD_FORMAT;

  std::vector<cgltf_buffer> buffers(1);
  buffers[0].size = bin_.size();
  buffers[0].data = const_cast<std::uint8_t*>(bin_.data());

  std::size_t embedded_texture_count = 0;
  for (const TextureDesc& texture : textures_)
    if (texture.uri.empty()) ++embedded_texture_count;
  std::vector<cgltf_buffer_view> views(accessors_.size() + embedded_texture_count);
  std::vector<cgltf_accessor> accessors(accessors_.size());
  for (std::size_t i = 0; i < accessors_.size(); ++i) {
    const AccessorDesc& source = accessors_[i];
    cgltf_size stride = cgltf_calc_size(source.type, source.component_type);
    if (source.count == 0 || stride == 0 || source.count > std::numeric_limits<std::size_t>::max() / stride ||
        source.size != source.count * stride) {
      return ARX_GLB_BAD_FORMAT;
    }
    views[i].buffer = &buffers[0];
    views[i].offset = source.offset;
    views[i].size = source.size;
    accessors[i].buffer_view = &views[i];
    accessors[i].component_type = source.component_type;
    accessors[i].type = source.type;
    accessors[i].count = source.count;
    accessors[i].stride = stride;
  }

  std::vector<cgltf_image> images(textures_.size());
  std::vector<cgltf_texture> textures(textures_.size());
  std::size_t next_view = accessors_.size();
  for (std::size_t i = 0; i < textures_.size(); ++i) {
    const TextureDesc& source = textures_[i];
    if (source.name.empty()) return ARX_GLB_BAD_FORMAT;
    images[i].name = strPtr(source.name);
    if (!source.uri.empty()) {
      images[i].uri = strPtr(source.uri);
    } else {
      if (source.mime_type.empty() || source.size == 0 || source.offset > bin_.size() ||
          source.size > bin_.size() - source.offset) {
        return ARX_GLB_BAD_FORMAT;
      }
      views[next_view].buffer = &buffers[0];
      views[next_view].offset = source.offset;
      views[next_view].size = source.size;
      images[i].buffer_view = &views[next_view];
      images[i].mime_type = strPtr(source.mime_type);
      ++next_view;
    }
    textures[i].name = strPtr(source.name);
    textures[i].image = &images[i];
  }

  std::vector<cgltf_material> materials(materials_.size());
  for (std::size_t i = 0; i < materials_.size(); ++i) {
    const MaterialDesc& source = materials_[i];
    cgltf_material& material = materials[i];
    material.name = strPtr(source.name);
    material.has_pbr_metallic_roughness = 1;
    std::copy(source.color.begin(), source.color.end(), material.pbr_metallic_roughness.base_color_factor);
    material.pbr_metallic_roughness.metallic_factor = 0.0f;
    material.pbr_metallic_roughness.roughness_factor = 1.0f;
    material.double_sided = source.double_sided;
    material.alpha_mode = source.alpha_mode;
    material.alpha_cutoff = 0.5f;
    if (source.texture >= 0) {
      if (!validIndex(source.texture, textures.size())) return ARX_GLB_BAD_FORMAT;
      material.pbr_metallic_roughness.base_color_texture.texture = &textures[static_cast<std::size_t>(source.texture)];
    }
  }

  std::vector<cgltf_mesh> meshes(meshes_.size());
  std::vector<std::vector<cgltf_primitive>> primitives(meshes_.size());
  std::vector<std::vector<std::vector<cgltf_attribute>>> attributes(meshes_.size());
  for (std::size_t i = 0; i < meshes_.size(); ++i) {
    meshes[i].name = strPtr(meshes_[i].name);
    primitives[i].resize(meshes_[i].primitives.size());
    attributes[i].resize(meshes_[i].primitives.size());
    for (std::size_t j = 0; j < meshes_[i].primitives.size(); ++j) {
      const Primitive& source = meshes_[i].primitives[j];
      cgltf_primitive& primitive = primitives[i][j];
      primitive.type = cgltf_primitive_type_triangles;
      if (!validIndex(source.indices, accessors.size())) return ARX_GLB_BAD_FORMAT;
      primitive.indices = &accessors[static_cast<std::size_t>(source.indices)];
      if (source.material >= 0) {
        if (!validIndex(source.material, materials.size())) return ARX_GLB_BAD_FORMAT;
        primitive.material = &materials[static_cast<std::size_t>(source.material)];
      }
      attributes[i][j].resize(source.attributes.size());
      for (std::size_t k = 0; k < source.attributes.size(); ++k) {
        const auto& [name, accessor] = source.attributes[k];
        if (!validIndex(accessor, accessors.size())) return ARX_GLB_BAD_FORMAT;
        cgltf_attribute& attribute = attributes[i][j][k];
        attribute.name = strPtr(name);
        attribute.data = &accessors[static_cast<std::size_t>(accessor)];
        attribute.type = cgltf_attribute_type_custom;
        if (name == "POSITION") {
          attribute.type = cgltf_attribute_type_position;
          cgltf_accessor& position = accessors[static_cast<std::size_t>(accessor)];
          if (position.type != cgltf_type_vec3 || position.component_type != cgltf_component_type_r_32f) {
            return ARX_GLB_BAD_FORMAT;
          }
          const AccessorDesc& desc = accessors_[static_cast<std::size_t>(accessor)];
          auto component_value = [&](std::size_t value, std::size_t component) {
            float result = 0.0f;
            std::size_t offset = desc.offset + (value * 3 + component) * sizeof(float);
            std::memcpy(&result, bin_.data() + offset, sizeof(result));
            return result;
          };
          for (std::size_t component = 0; component < 3; ++component) {
            float current = component_value(0, component);
            if (!std::isfinite(current)) return ARX_GLB_BAD_FORMAT;
            position.min[component] = current;
            position.max[component] = current;
          }
          for (std::size_t value = 1; value < desc.count; ++value) {
            for (std::size_t component = 0; component < 3; ++component) {
              float current = component_value(value, component);
              if (!std::isfinite(current)) return ARX_GLB_BAD_FORMAT;
              position.min[component] = std::min(position.min[component], current);
              position.max[component] = std::max(position.max[component], current);
            }
          }
          position.has_min = 1;
          position.has_max = 1;
        } else if (name == "NORMAL")
          attribute.type = cgltf_attribute_type_normal;
        else if (name == "TEXCOORD_0")
          attribute.type = cgltf_attribute_type_texcoord;
        else if (name == "COLOR_0")
          attribute.type = cgltf_attribute_type_color;
      }
      primitive.attributes = attributes[i][j].data();
      primitive.attributes_count = attributes[i][j].size();
    }
    meshes[i].primitives = primitives[i].data();
    meshes[i].primitives_count = primitives[i].size();
  }

  std::vector<cgltf_light> lights(lights_.size());
  for (std::size_t i = 0; i < lights_.size(); ++i) {
    const LightDesc& source = lights_[i];
    cgltf_light& light = lights[i];
    light.name = strPtr(source.name);
    light.color[0] = source.color.x;
    light.color[1] = source.color.y;
    light.color[2] = source.color.z;
    light.intensity = source.intensity;
    light.type = cgltf_light_type_point;
    light.range = source.range;
  }

  const bool rotated_roots =
      root_rotation_.w != 1.0f || root_rotation_.x != 0.0f || root_rotation_.y != 0.0f || root_rotation_.z != 0.0f;
  const bool transformed_roots = root_transform_enabled_;
  const std::size_t root_transform_index = nodes_.size();
  std::vector<cgltf_node> nodes(nodes_.size() + (transformed_roots ? 1U : 0U));
  std::vector<std::vector<cgltf_node*>> children(nodes.size());
  for (std::size_t i = 0; i < nodes_.size(); ++i) {
    const NodeDesc& source = nodes_[i];
    nodes[i].name = strPtr(source.name);
    if (source.mesh >= 0) {
      if (!validIndex(source.mesh, meshes.size())) return ARX_GLB_BAD_FORMAT;
      nodes[i].mesh = &meshes[static_cast<std::size_t>(source.mesh)];
    }
    if (source.light >= 0) {
      if (!validIndex(source.light, lights.size())) return ARX_GLB_BAD_FORMAT;
      nodes[i].light = &lights[static_cast<std::size_t>(source.light)];
    }
    if (source.has_translation) {
      nodes[i].has_translation = 1;
      nodes[i].translation[0] = source.translation.x;
      nodes[i].translation[1] = source.translation.y;
      nodes[i].translation[2] = source.translation.z;
    }
    if (source.has_rotation) {
      nodes[i].has_rotation = 1;
      nodes[i].rotation[0] = source.rotation.x;
      nodes[i].rotation[1] = source.rotation.y;
      nodes[i].rotation[2] = source.rotation.z;
      nodes[i].rotation[3] = source.rotation.w;
    }
    children[i].reserve(source.children.size());
    for (int child : source.children) {
      if (!validIndex(child, nodes_.size())) return ARX_GLB_BAD_FORMAT;
      children[i].push_back(&nodes[static_cast<std::size_t>(child)]);
    }
    nodes[i].children = children[i].data();
    nodes[i].children_count = children[i].size();
  }

  std::vector<cgltf_node*> roots;
  if (transformed_roots) {
    cgltf_node& transform = nodes[root_transform_index];
    transform.name = strPtr(root_transform_name_);
    transform.has_translation = 1;
    transform.translation[0] = root_translation_.x;
    transform.translation[1] = root_translation_.y;
    transform.translation[2] = root_translation_.z;
    if (rotated_roots) {
      transform.has_rotation = 1;
      transform.rotation[0] = root_rotation_.x;
      transform.rotation[1] = root_rotation_.y;
      transform.rotation[2] = root_rotation_.z;
      transform.rotation[3] = root_rotation_.w;
    }
    transform.has_scale = 1;
    transform.scale[0] = root_scale_;
    transform.scale[1] = root_scale_;
    transform.scale[2] = root_scale_;
    children[root_transform_index].reserve(roots_.size());
    for (int root : roots_) {
      if (!validIndex(root, nodes_.size())) return ARX_GLB_BAD_FORMAT;
      cgltf_node* child = &nodes[static_cast<std::size_t>(root)];
      child->parent = &transform;
      children[root_transform_index].push_back(child);
    }
    transform.children = children[root_transform_index].data();
    transform.children_count = children[root_transform_index].size();
    roots.push_back(&transform);
  } else {
    roots.reserve(roots_.size());
    for (int root : roots_) {
      if (!validIndex(root, nodes_.size())) return ARX_GLB_BAD_FORMAT;
      roots.push_back(&nodes[static_cast<std::size_t>(root)]);
    }
  }
  cgltf_scene scene{};
  scene.nodes = roots.data();
  scene.nodes_count = roots.size();

  std::string version = "2.0";
  std::string generator = "arx-pistoris";
  cgltf_data data{};
  data.file_type = cgltf_file_type_glb;
  data.asset.version = strPtr(version);
  data.asset.generator = strPtr(generator);
  data.buffers = buffers.data();
  data.buffers_count = buffers.size();
  data.buffer_views = views.data();
  data.buffer_views_count = views.size();
  data.accessors = accessors.data();
  data.accessors_count = accessors.size();
  data.images = images.data();
  data.images_count = images.size();
  data.textures = textures.data();
  data.textures_count = textures.size();
  data.materials = materials.data();
  data.materials_count = materials.size();
  data.meshes = meshes.data();
  data.meshes_count = meshes.size();
  data.lights = lights.data();
  data.lights_count = lights.size();
  data.nodes = nodes.data();
  data.nodes_count = nodes.size();
  data.scenes = &scene;
  data.scenes_count = 1;
  data.scene = &scene;
  data.bin = bin_.data();
  data.bin_size = bin_.size();

  if (cgltf_validate(&data) != cgltf_result_success) return ARX_GLB_BAD_FORMAT;

  cgltf_options options{};
  options.type = cgltf_file_type_glb;
  cgltf_size json_with_null = cgltf_write(&options, nullptr, 0, &data);
  if (json_with_null == 0) return ARX_GLB_BAD_FORMAT;
  std::vector<char> json(json_with_null);
  if (cgltf_write(&options, json.data(), json.size(), &data) != json_with_null) return ARX_GLB_BAD_FORMAT;
  std::size_t json_size = json_with_null - 1;
  std::size_t json_padded = (json_size + 3U) & ~std::size_t{3U};
  std::size_t bin_padded = (bin_.size() + 3U) & ~std::size_t{3U};
  std::size_t total = 12U + 8U + json_padded + (bin_.empty() ? 0U : 8U + bin_padded);
  if (json_padded > std::numeric_limits<std::uint32_t>::max() ||
      bin_padded > std::numeric_limits<std::uint32_t>::max() || total > std::numeric_limits<std::uint32_t>::max()) {
    return ARX_GLB_BAD_FORMAT;
  }

  std::vector<std::uint8_t> result;
  result.reserve(total);
  append32(result, kGlbMagic);
  append32(result, kGlbV2);
  append32(result, static_cast<std::uint32_t>(total));
  append32(result, static_cast<std::uint32_t>(json_padded));
  append32(result, kJsonChunk);
  result.insert(result.end(),
                reinterpret_cast<const std::uint8_t*>(json.data()),
                reinterpret_cast<const std::uint8_t*>(json.data()) + json_size);
  result.resize(20U + json_padded, 0x20);
  if (!bin_.empty()) {
    append32(result, static_cast<std::uint32_t>(bin_padded));
    append32(result, kBinChunk);
    result.insert(result.end(), bin_.begin(), bin_.end());
    result.resize(total, 0);
  }
  out = std::move(result);
  return ARX_OK;
}

}  // namespace pistoris::glb
