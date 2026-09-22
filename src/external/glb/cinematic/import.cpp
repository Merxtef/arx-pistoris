// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#include "arx_pistoris/base/indices.h"
#include "arx_pistoris/base/math.hpp"
#include "arx_pistoris/base/status.h"
#include "arx_pistoris/cinematic/sound.hpp"
#include "arx_pistoris/runtime/types.h"
#include "arx_pistoris/sound.hpp"

#include "api.h"
#include "cgltf/cgltf.h"
#include "cinematic/data.h"
#include "cinematic/internal.h"
#include "coordinates.h"
#include "external/glb/accessor.h"
#include "external/glb/container.h"
#include "external/glb/node_graph.h"
#include "external/glb/primitive_indices.h"
#include "external/glb/utils/image.h"
#include "external/glb/utils/names.h"
#include "external/glb/utils/node.h"
#include "external/glb/utils/sound.h"
#include "external/glb/utils/texture.h"
#include "external/glb/utils/transform.h"
#include "modules/cinematic.h"
#include "modules/sounds.h"
#include "modules/textures.h"
#include "names.h"
#include "surface.h"
#include "utils/encoded_image.h"
#include "utils/log.h"
#include "utils/math/geometry_algorithms.h"
#include "utils/math/mat4.h"
#include "utils/math/quat.h"
#include "utils/name_tokens.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <numbers>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <unordered_set>
#include <utility>
#include <vector>

namespace pistoris {
namespace {

constexpr float kTransformTolerance = 1.0e-4f;

std::string_view nodeName(const cgltf_node& node) noexcept { return node.name != nullptr ? node.name : ""; }

std::string_view mappingName(glb_cinematic::SurfaceMappingMode mode) noexcept {
  switch (mode) {
    case glb_cinematic::SurfaceMappingMode::kBounds:
      return "bounds";
    case glb_cinematic::SurfaceMappingMode::kAffineUv:
      return "affine UV";
    case glb_cinematic::SurfaceMappingMode::kPiecewiseUv:
      return "piecewise UV";
  }
  return "unknown";
}

math::Mat4 localTransform(const cgltf_node& node) noexcept {
  cgltf_float values[16];
  cgltf_node_transform_local(&node, values);
  math::Mat4 result{};
  for (std::size_t index = 0; index < 16; ++index) result.m[index] = values[index];
  return result;
}

bool nodeTransform(const cgltf_node& node, glb::DecomposedTransform& out) noexcept {
  if (node.has_scale)
    for (float component : node.scale)
      if (!std::isfinite(component) || component <= 0.0f) return false;
  return glb::decomposeTransform(localTransform(node), out);
}

bool cameraFov(const cgltf_node& node, float& out) noexcept {
  out = glb_cinematic::gameVerticalFov();
  if (node.camera == nullptr) return true;
  const cgltf_camera& camera = *node.camera;
  if (camera.type != cgltf_camera_type_perspective || camera.extensions_count != 0) return false;
  const cgltf_camera_perspective& perspective = camera.data.perspective;
  if (!std::isfinite(perspective.yfov) || perspective.yfov <= 0.0f || perspective.yfov >= std::numbers::pi_v<float> ||
      !std::isfinite(perspective.znear) || perspective.znear <= 0.0f ||
      (perspective.has_zfar && (!std::isfinite(perspective.zfar) || perspective.zfar <= perspective.znear)) ||
      (perspective.has_aspect_ratio && (!std::isfinite(perspective.aspect_ratio) || perspective.aspect_ratio <= 0.0f)))
    return false;
  out = perspective.yfov;
  return true;
}

ArxReturnCode textureImportError(glb::TextureImportError error) noexcept {
  switch (error) {
    case glb::TextureImportError::kNone:
      return ARX_OK;
    case glb::TextureImportError::kOutOfMemory:
      return ARX_BAD_ALLOC;
    case glb::TextureImportError::kTooManyTextures:
      return ARX_CINEMATIC_TOO_MANY_TEXTURES;
    case glb::TextureImportError::kBadPath:
      return ARX_CINEMATIC_BAD_TEXTURE_PATH;
    case glb::TextureImportError::kBadImage:
      return ARX_GLB_BAD_CINEMATIC_IMAGE;
  }
  return ARX_INTERNAL_ERROR;
}

ArxReturnCode soundPathImportError(glb::SoundPathImportError error) noexcept {
  switch (error) {
    case glb::SoundPathImportError::kNone:
      return ARX_OK;
    case glb::SoundPathImportError::kBadPath:
      return ARX_CINEMATIC_BAD_SOUND_PATH;
    case glb::SoundPathImportError::kTooManySounds:
      return ARX_CINEMATIC_TOO_MANY_SOUNDS;
    case glb::SoundPathImportError::kBadKind:
      return ARX_INTERNAL_ERROR;
  }
  return ARX_INTERNAL_ERROR;
}

ArxReturnCode failNode(ArxReturnCode code, std::string_view kind, std::string_view name, std::string_view reason) {
  log(ARX_LOG_ERROR, "GLB -> Cinematic: {} '{}' {}", kind, name, reason);
  return code;
}

bool materialAppearanceIgnored(const cgltf_material& material) noexcept {
  for (std::size_t index = 0; index < 3; ++index)
    if (std::abs(material.pbr_metallic_roughness.base_color_factor[index] - 1.0f) > kTransformTolerance) return true;
  if (material.alpha_mode != cgltf_alpha_mode_opaque &&
      std::abs(material.pbr_metallic_roughness.base_color_factor[3] - 1.0f) > kTransformTolerance)
    return true;
  for (float factor : material.emissive_factor)
    if (std::abs(factor) > kTransformTolerance) return true;
  return false;
}

void warnIgnoredAnimationChannels(const cgltf_data& data, const glb::NodeGraph& graph, std::size_t root) {
  std::size_t count = 0;
  for (std::size_t animation_index = 0; animation_index < data.animations_count; ++animation_index)
    for (std::size_t channel_index = 0; channel_index < data.animations[animation_index].channels_count;
         ++channel_index) {
      const cgltf_node* target = data.animations[animation_index].channels[channel_index].target_node;
      if (target == nullptr) continue;
      const std::size_t node = cgltf_node_index(&data, target);
      if (node < data.nodes_count && glb::isDescendantOrSelf(graph, node, root)) ++count;
    }
  if (count != 0)
    log(ARX_LOG_WARN,
        "GLB -> Cinematic: {} animation channel(s) target Cinematic nodes; GLB animation is ignored, use KEY frames",
        count);
}

std::optional<std::int32_t> synthesizeInitialHold(CinematicData& cinematic) {
  const CinematicKeyframe& first = cinematic.keyframes.front();
  if (first.frame <= 0) return std::nullopt;
  CinematicKeyframe hold;
  hold.illustration = first.illustration;
  hold.camera_position = first.camera_position;
  hold.camera_roll = first.camera_roll;
  hold.light = first.light;
  hold.light_active = first.light_active;
  hold.interpolation = CinematicInterpolation::kNone;
  const std::int32_t first_frame = first.frame;
  cinematic.keyframes.insert(cinematic.keyframes.begin(), hold);
  return first_frame;
}

struct PendingIllustration {
  const cgltf_node* node = nullptr;
  glb_cinematic::IllustrationName name;
  CinematicIllustration illustration;
  image::Info image_info;
  glb_cinematic::IllustrationSurface surface;
  ArxVector3 scale{1.0f, 1.0f, 1.0f};
  float pixels_per_unit = glb_cinematic::kUnitsPerGlbUnit;
  bool outside_sample = false;
  bool ambiguous_sample = false;
  bool non_blend_material = false;
};

const cgltf_accessor* attribute(const cgltf_primitive& primitive, cgltf_attribute_type type, int index) noexcept {
  return cgltf_find_accessor(&primitive, type, index);
}

ArxReturnCode addPrimitiveSurface(const glb::Asset& asset, const cgltf_primitive& primitive, const ArxVector3& scale,
                                  const cgltf_image*& source_image, std::vector<std::uint8_t>& source_image_bytes,
                                  glb_cinematic::IllustrationSurface& surface) {
  if (primitive.type != cgltf_primitive_type_triangles || primitive.targets_count != 0 ||
      primitive.material == nullptr || !primitive.material->has_pbr_metallic_roughness) {
    log(ARX_LOG_ERROR, "GLB -> Cinematic: illustration primitive needs triangles and a textured PBR material");
    return ARX_GLB_BAD_CINEMATIC_ILLUSTRATION;
  }

  glb::TextureBinding binding;
  const glb::TextureBindingError binding_error =
      glb::decodeTextureBinding(primitive.material->pbr_metallic_roughness.base_color_texture, binding);
  if (binding_error == glb::TextureBindingError::kUnsupportedFeature) return ARX_GLB_UNSUPPORTED_FEATURE;
  if (binding_error != glb::TextureBindingError::kNone || binding.image == nullptr) {
    log(ARX_LOG_ERROR, "GLB -> Cinematic: illustration primitive has no usable base-color image");
    return ARX_GLB_BAD_CINEMATIC_IMAGE;
  }
  if ((binding.image->uri != nullptr && !glb::isDataUri(binding.image->uri)) ||
      (binding.image->uri == nullptr && binding.image->buffer_view == nullptr)) {
    log(ARX_LOG_ERROR, "GLB -> Cinematic: illustration primitive has an external or missing image");
    return ARX_GLB_BAD_CINEMATIC_IMAGE;
  }
  if (source_image != nullptr && source_image != binding.image) {
    image::Format format = image::Format::kUnknown;
    if (source_image_bytes.empty()) {
      const ArxReturnCode rc = glb::readEmbeddedImage(*source_image, source_image_bytes, format);
      if (rc != ARX_OK) return rc == ARX_BAD_ALLOC ? rc : ARX_GLB_BAD_CINEMATIC_IMAGE;
    }
    std::vector<std::uint8_t> image_bytes;
    const ArxReturnCode rc = glb::readEmbeddedImage(*binding.image, image_bytes, format);
    if (rc != ARX_OK) return rc == ARX_BAD_ALLOC ? rc : ARX_GLB_BAD_CINEMATIC_IMAGE;
    if (source_image_bytes != image_bytes) {
      log(ARX_LOG_ERROR, "GLB -> Cinematic: illustration primitives reference different embedded images");
      return ARX_GLB_BAD_CINEMATIC_IMAGE;
    }
  }
  if (source_image == nullptr) source_image = binding.image;

  const cgltf_accessor* position_source = attribute(primitive, cgltf_attribute_type_position, 0);
  if (position_source == nullptr) {
    log(ARX_LOG_ERROR, "GLB -> Cinematic: illustration primitive has no positions");
    return ARX_GLB_BAD_CINEMATIC_ILLUSTRATION;
  }
  const cgltf_accessor* texcoord_source = attribute(primitive, cgltf_attribute_type_texcoord, binding.texcoord);
  glb::AccessorCache accessors(asset, 3);
  const glb::AccessorView* positions = nullptr;
  ArxReturnCode rc = accessors.get(position_source, positions);
  if (rc != ARX_OK) return rc;
  if (glb::validatePositionAccessor(*positions) != ARX_OK || positions->count < 3) {
    log(ARX_LOG_ERROR, "GLB -> Cinematic: illustration primitive has an invalid position accessor");
    return ARX_GLB_BAD_CINEMATIC_ILLUSTRATION;
  }

  const glb::AccessorView* texcoords = nullptr;
  bool usable_texcoords = texcoord_source != nullptr;
  if (usable_texcoords) {
    rc = accessors.get(texcoord_source, texcoords);
    if (rc != ARX_OK) return rc;
    usable_texcoords = glb::validateTexcoordAccessor(*texcoords) == ARX_OK && texcoords->count == positions->count;
  }
  if (!usable_texcoords) surface.noteMissingTexcoords();

  glb::PrimitiveIndices indices;
  rc = glb::readPrimitiveIndices(accessors, primitive, positions->count, false, false, indices);
  if (rc != ARX_OK) return rc;
  if (indices.empty() || indices.size() % 3 != 0) {
    log(ARX_LOG_ERROR, "GLB -> Cinematic: illustration primitive has no complete triangles");
    return ARX_GLB_BAD_CINEMATIC_ILLUSTRATION;
  }

  std::unordered_set<std::uint32_t> height_vertices;
  for (std::size_t offset = 0; offset < indices.size(); offset += 3) {
    glb_cinematic::SurfaceTriangle triangle;
    bool valid_triangle = true;
    for (std::size_t corner = 0; corner < 3; ++corner) {
      const std::uint32_t index = indices[offset + corner];
      if (index >= positions->count) return ARX_GLB_BAD_CINEMATIC_ILLUSTRATION;
      const glb::Vec3 position = glb::readVec3(*positions, index);
      const ArxVector3 scaled{position.x * scale.x, position.y * scale.y, position.z * scale.z};
      if (!std::isfinite(scaled.x) || !std::isfinite(scaled.y) || !std::isfinite(scaled.z))
        return ARX_GLB_BAD_CINEMATIC_ILLUSTRATION;
      triangle.position[corner] = {scaled.x, scaled.z};
      surface.includePosition(triangle.position[corner]);
      if (height_vertices.insert(index).second) surface.noteHeight(scaled.y);
      if (usable_texcoords) {
        const glb::Vec2 source = glb::readVec2(*texcoords, index);
        triangle.texcoord[corner] = glb::transformTexcoord(binding, source);
        valid_triangle =
            valid_triangle && std::isfinite(triangle.texcoord[corner].x) && std::isfinite(triangle.texcoord[corner].y);
      }
    }
    std::array<double, 3> area_probe{};
    const bool nondegenerate = math::barycentricXz({triangle.position[0].x, 0.0f, triangle.position[0].y},
                                                   {triangle.position[1].x, 0.0f, triangle.position[1].y},
                                                   {triangle.position[2].x, 0.0f, triangle.position[2].y},
                                                   triangle.position[0].x,
                                                   triangle.position[0].y,
                                                   area_probe);
    if (usable_texcoords && valid_triangle && nondegenerate)
      surface.addTriangle(triangle);
    else
      surface.noteMissingTexcoords();
  }
  return ARX_OK;
}

ArxReturnCode importIllustration(const glb::Asset& asset, const cgltf_node& node,
                                 const glb_cinematic::IllustrationName& name, glb::TextureImporter& textures,
                                 PendingIllustration& out) {
  if (node.mesh == nullptr || node.camera != nullptr || node.light != nullptr || node.skin != nullptr ||
      node.extensions_count != 0 || node.mesh->primitives_count == 0)
    return failNode(ARX_GLB_BAD_CINEMATIC_ILLUSTRATION,
                    "illustration",
                    nodeName(node),
                    "needs a mesh and cannot also be a camera, light, or skin");

  PendingIllustration result;
  result.node = &node;
  result.name = name;
  glb::DecomposedTransform transform;
  if (!nodeTransform(node, transform)) {
    log(ARX_LOG_ERROR,
        "GLB -> Cinematic: illustration '{}' has an invalid transform or nonpositive scale",
        nodeName(node));
    return ARX_GLB_BAD_CINEMATIC_ILLUSTRATION;
  }
  result.scale = transform.scale;
  const cgltf_image* image = nullptr;
  std::vector<std::uint8_t> image_bytes;
  bool ignored_appearance = false;
  for (std::size_t index = 0; index < node.mesh->primitives_count; ++index) {
    const cgltf_primitive& primitive = node.mesh->primitives[index];
    if (primitive.material != nullptr && materialAppearanceIgnored(*primitive.material)) ignored_appearance = true;
    if (primitive.material != nullptr && primitive.material->alpha_mode != cgltf_alpha_mode_blend)
      result.non_blend_material = true;
    const ArxReturnCode rc = addPrimitiveSurface(asset, primitive, result.scale, image, image_bytes, result.surface);
    if (rc != ARX_OK) return rc;
  }
  if (image == nullptr || !result.surface.finalize())
    return failNode(
        ARX_GLB_BAD_CINEMATIC_ILLUSTRATION, "illustration", nodeName(node), "has no usable horizontal surface");
  if (ignored_appearance)
    log(ARX_LOG_WARN,
        "GLB -> Cinematic: illustration '{}' has material tint or emission; only its image is imported",
        nodeName(node));

  TextureIndex texture = kNoTexture;
  const ArxReturnCode rc = textureImportError(textures.import({image, "illustration"}, texture));
  if (rc != ARX_OK) return rc;
  result.illustration = {.texture = texture, .subdivision_scale = name.subdivision_scale};
  out = std::move(result);
  return ARX_OK;
}

ArxReturnCode sampledPoint(PendingIllustration& illustration, const ArxVector3& point, float vertical_fov,
                           ArxVector3& out) {
  glb_cinematic::SurfaceSample sample;
  if (!illustration.surface.sample({point.x, point.z}, sample)) return ARX_GLB_BAD_CINEMATIC_ILLUSTRATION;
  illustration.outside_sample = illustration.outside_sample || sample.outside;
  illustration.ambiguous_sample = illustration.ambiguous_sample || sample.ambiguous;
  const float depth_scale =
      illustration.pixels_per_unit * std::tan(vertical_fov * 0.5f) / std::tan(glb_cinematic::gameVerticalFov() * 0.5f);
  const float height_above_plane = point.y - illustration.surface.averageHeight();
  out = {
      (sample.texcoord.x - 0.5f) * static_cast<float>(illustration.image_info.width),
      (sample.texcoord.y - 0.5f) * static_cast<float>(illustration.image_info.height),
      -height_above_plane * depth_scale,
  };
  return std::isfinite(out.x) && std::isfinite(out.y) && std::isfinite(out.z) ? ARX_OK
                                                                              : ARX_GLB_BAD_CINEMATIC_KEY_PLACEMENT;
}

ArxReturnCode parseSound(const cgltf_node& node, glb::SoundPathImporter& sounds, SoundHandle& out) {
  SoundKind kind = SoundKind::kEffect;
  glb::ParsedLabel label;
  if (!glb_cinematic::parseSoundName(nodeName(node), kind, &label) || !glb::simpleEmptyNode(node))
    return failNode(
        ARX_GLB_BAD_CINEMATIC_HELPER, "SOUND", nodeName(node), "needs a valid EFFECT or SPEECH name and an empty node");
  glb::reportConventionLabel("GLB -> Cinematic", nodeName(node), label);
  const cgltf_node* path_node = nullptr;
  std::vector<std::string_view> child_tokens;
  for (std::size_t index = 0; index < node.children_count; ++index) {
    const cgltf_node* child = node.children[index];
    if (child == nullptr) return ARX_GLB_BAD_FORMAT;
    const std::string_view child_name = nodeName(*child);
    log(ARX_LOG_DEBUG, "GLB -> Cinematic: processing SOUND child node '{}'", child_name);
    splitDoubleUnderscore(child_name, child_tokens);
    if (child_tokens.front().starts_with("PATH_")) {
      if (path_node != nullptr) {
        log(ARX_LOG_ERROR, "GLB -> Cinematic: SOUND '{}' has multiple PATH children", nodeName(node));
        return ARX_GLB_BAD_CINEMATIC_HELPER;
      }
      path_node = child;
    } else {
      log(ARX_LOG_WARN, "GLB -> Cinematic: unexpected SOUND child '{}' ignored", child_name);
    }
  }
  if (path_node == nullptr) {
    log(ARX_LOG_ERROR, "GLB -> Cinematic: SOUND '{}' requires one PATH child", nodeName(node));
    return ARX_GLB_BAD_CINEMATIC_HELPER;
  }
  std::string path;
  if (!glb_cinematic::parseSoundPathName(nodeName(*path_node), path) || !glb::simpleEmptyNode(*path_node) ||
      path_node->children_count != 0)
    return failNode(
        ARX_GLB_BAD_CINEMATIC_HELPER, "PATH", nodeName(*path_node), "needs a path label and an empty leaf node");
  return soundPathImportError(sounds.import(kind, path, out));
}

ArxReturnCode parseKey(const cgltf_node& node, PendingIllustration& illustration, glb::SoundPathImporter& sounds,
                       CinematicKeyframe& out) {
  CinematicKeyframe result;
  glb::ParsedLabel label;
  if (!glb_cinematic::parseKeyName(nodeName(node), result, &label)) {
    return failNode(ARX_GLB_BAD_CINEMATIC_KEY_NAME, "KEY", nodeName(node), "has an invalid name");
  }
  glb::reportConventionLabel("GLB -> Cinematic", nodeName(node), label);
  if (node.mesh != nullptr || node.light != nullptr || node.skin != nullptr || node.extensions_count != 0 ||
      node.has_mesh_gpu_instancing)
    return failNode(ARX_GLB_BAD_CINEMATIC_KEY_TRANSFORM,
                    "KEY",
                    nodeName(node),
                    "must be a camera or empty node without mesh, light, skin, or extensions");
  glb::DecomposedTransform key_transform;
  if (!nodeTransform(node, key_transform)) {
    return failNode(
        ARX_GLB_BAD_CINEMATIC_KEY_TRANSFORM, "KEY", nodeName(node), "has an invalid transform or nonpositive scale");
  }
  float vertical_fov = 0.0f;
  if (!cameraFov(node, vertical_fov)) {
    return failNode(
        ARX_GLB_BAD_CINEMATIC_CAMERA, "KEY", nodeName(node), "has an unsupported or invalid perspective camera");
  }
  result.illustration = static_cast<CinematicIllustrationIndex>(illustration.name.ordinal);
  const ArxVector3 camera_point{illustration.scale.x * key_transform.translation.x,
                                illustration.scale.y * key_transform.translation.y,
                                illustration.scale.z * key_transform.translation.z};
  if (!std::isfinite(camera_point.x) || !std::isfinite(camera_point.y) || !std::isfinite(camera_point.z))
    return failNode(
        ARX_GLB_BAD_CINEMATIC_KEY_TRANSFORM, "KEY", nodeName(node), "has a non-finite transformed position");
  ArxReturnCode rc = sampledPoint(illustration, camera_point, vertical_fov, result.camera_position);
  if (rc != ARX_OK)
    return failNode(
        ARX_GLB_BAD_CINEMATIC_KEY_PLACEMENT, "KEY", nodeName(node), "cannot be mapped onto its illustration");
  glb_cinematic::KeyOrientation orientation;
  if (!glb_cinematic::resolveKeyOrientation(key_transform.rotation, orientation))
    return failNode(
        ARX_GLB_BAD_CINEMATIC_KEY_TRANSFORM, "KEY", nodeName(node), "has an orientation that cannot be leveled");
  result.camera_roll = orientation.roll;
  if (orientation.corrected)
    log(ARX_LOG_WARN, "GLB -> Cinematic: KEY '{}' is not facing the illustration; orientation leveled", nodeName(node));

  bool flash_found = false;
  bool light_found = false;
  bool sound_found = false;
  std::vector<std::string_view> child_tokens;
  for (std::size_t index = 0; index < node.children_count; ++index) {
    const cgltf_node* child = node.children[index];
    if (child == nullptr) return ARX_GLB_BAD_FORMAT;
    const std::string_view child_name = nodeName(*child);
    log(ARX_LOG_DEBUG, "GLB -> Cinematic: processing key child node '{}' under '{}'", child_name, nodeName(node));
    splitDoubleUnderscore(child_name, child_tokens);
    const std::string_view child_kind = child_tokens.front();
    if (child_kind == "PRESENTATION") {
      return failNode(ARX_GLB_BAD_CINEMATIC_HELPER, "helper", child_name, "uses the removed PRESENTATION convention");
    } else if (child_kind == "FLASH") {
      glb::ParsedLabel helper_label;
      if (flash_found || !glb_cinematic::parseFlashName(child_name, result, &helper_label) ||
          !glb::simpleEmptyNode(*child) || child->children_count != 0)
        return failNode(
            ARX_GLB_BAD_CINEMATIC_HELPER, "FLASH", child_name, "is duplicate, malformed, or not an empty leaf node");
      glb::reportConventionLabel("GLB -> Cinematic", child_name, helper_label);
      flash_found = true;
    } else if (child_kind == "LIGHT") {
      glb::ParsedLabel helper_label;
      if (light_found || !glb_cinematic::parseLightName(child_name, result.light, &helper_label) ||
          !glb::simpleEmptyNode(*child) || child->children_count != 0)
        return failNode(
            ARX_GLB_BAD_CINEMATIC_HELPER, "LIGHT", child_name, "is duplicate, malformed, or not an empty leaf node");
      glb::reportConventionLabel("GLB -> Cinematic", child_name, helper_label);
      if (result.light.intensity >= 0.0f) {
        const math::Mat4 illustration_scale =
            math::fromTrs({0.0f, 0.0f, 0.0f}, math::kIdentityQuat, illustration.scale);
        const math::Mat4 camera_frame = math::fromTrs(camera_point, orientation.rotation, {1.0f, 1.0f, 1.0f});
        const std::optional<math::Mat4> inverse_camera = math::inverseAffine(camera_frame);
        if (!inverse_camera)
          return failNode(ARX_GLB_BAD_CINEMATIC_HELPER, "LIGHT", child_name, "cannot use this KEY transform");
        const math::Mat4 effective_key =
            orientation.corrected ? math::fromTrs(key_transform.translation, orientation.rotation, key_transform.scale)
                                  : localTransform(node);
        const ArxVector3 light_point =
            math::xformPoint(illustration_scale * effective_key, math::translation(localTransform(*child)));
        const ArxVector3 camera_local = math::xformPoint(*inverse_camera, light_point);
        if (!glb_cinematic::toArxLightPoint(camera_local, vertical_fov, result.light.position))
          return failNode(
              ARX_GLB_BAD_CINEMATIC_HELPER, "LIGHT", child_name, "has a position that cannot be represented");
      }
      result.light_active = true;
      light_found = true;
    } else if (child_kind == "SOUND") {
      if (sound_found)
        return failNode(ARX_GLB_BAD_CINEMATIC_HELPER, "SOUND", child_name, "duplicates another SOUND on this KEY");
      rc = parseSound(*child, sounds, result.sound);
      if (rc != ARX_OK) return rc;
      sound_found = true;
    } else {
      log(ARX_LOG_WARN, "GLB -> Cinematic: unexpected key helper '{}' ignored", child_name);
    }
  }
  out = result;
  return ARX_OK;
}

}  // namespace

ArxReturnCode importCinematicFromGlb(std::span<const std::uint8_t> bytes, CinematicModules& out,
                                     std::vector<CinematicSoundSourceReference>* sound_sources) {
  glb::Asset asset;
  ArxReturnCode rc = glb::parse(bytes, asset);
  if (rc != ARX_OK) return rc;
  cgltf_data& data = *asset.data();
  constexpr std::array<std::string_view, 2> kExtensions = {"KHR_materials_unlit", "KHR_texture_transform"};
  rc = glb::validateRequiredExtensions(data, kExtensions);
  if (rc != ARX_OK) return rc;

  glb::NodeGraph graph;
  rc = glb::buildNodeGraph(data, graph);
  if (rc != ARX_OK) return rc;
  std::vector<std::size_t> roots;
  for (std::size_t index : graph.preorder)
    if (glb_cinematic::rootNameCandidate(nodeName(data.nodes[index]))) roots.push_back(index);
  if (roots.empty()) {
    log(ARX_LOG_DEBUG, "GLB -> Cinematic: no Cinematic root found");
    return ARX_GLB_NO_CINEMATIC;
  }
  if (roots.size() != 1) {
    log(ARX_LOG_DEBUG, "GLB -> Cinematic: {} candidate root nodes found", roots.size());
    for (std::size_t candidate : roots)
      for (std::size_t ancestor : roots)
        if (candidate != ancestor && glb::isDescendantOrSelf(graph, candidate, ancestor))
          return ARX_GLB_BAD_CINEMATIC_ROOT;
    return ARX_GLB_AMBIGUOUS_CINEMATIC;
  }

  const std::size_t root_index = roots.front();
  const cgltf_node& root = data.nodes[root_index];
  log(ARX_LOG_DEBUG, "GLB -> Cinematic: importing root node {} '{}'", root_index, nodeName(root));
  glb_cinematic::RootName parsed_root;
  glb::ParsedLabel root_label;
  if (!glb_cinematic::parseRootName(nodeName(root), parsed_root, &root_label)) {
    log(ARX_LOG_ERROR, "GLB -> Cinematic: invalid root name '{}'", nodeName(root));
    return ARX_GLB_BAD_CINEMATIC_ROOT;
  }
  if (!glb::simpleEmptyNode(root))
    return failNode(ARX_GLB_BAD_CINEMATIC_ROOT, "root", nodeName(root), "must be an empty node");
  glb::reportConventionLabel("GLB -> Cinematic", nodeName(root), root_label);
  warnIgnoredAnimationChannels(data, graph, root_index);

  CinematicModules result;
  std::vector<CinematicSoundSourceReference> sources;
  std::vector<glb::ImportedSoundSource> imported_sound_sources;
  glb::TextureImporter texture_importer(result.textures, nullptr, "GLB -> Cinematic");
  std::vector<PendingIllustration> illustrations;
  illustrations.reserve(root.children_count);
  for (std::size_t index = 0; index < root.children_count; ++index) {
    const cgltf_node* child = root.children[index];
    if (child == nullptr) return ARX_GLB_BAD_FORMAT;
    const std::size_t node_index = cgltf_node_index(&data, child);
    log(ARX_LOG_DEBUG, "GLB -> Cinematic: processing root child node {} '{}'", node_index, nodeName(*child));
    glb_cinematic::IllustrationName illustration_name;
    glb::ParsedLabel illustration_label;
    if (!glb_cinematic::parseIllustrationName(nodeName(*child), illustration_name, &illustration_label)) {
      if (glb_cinematic::illustrationNameCandidate(nodeName(*child))) {
        log(ARX_LOG_ERROR, "GLB -> Cinematic: invalid illustration name '{}'", nodeName(*child));
        return ARX_GLB_BAD_CINEMATIC_ILLUSTRATION;
      }
      log(ARX_LOG_WARN, "GLB -> Cinematic: unexpected root child '{}' ignored", nodeName(*child));
      continue;
    }
    glb::reportConventionLabel("GLB -> Cinematic", nodeName(*child), illustration_label);
    PendingIllustration illustration;
    rc = importIllustration(asset, *child, illustration_name, texture_importer, illustration);
    if (rc != ARX_OK) {
      log(ARX_LOG_DEBUG,
          "GLB -> Cinematic: illustration node {} '{}' failed with code {}",
          node_index,
          nodeName(*child),
          rc);
      return rc;
    }
    const Texture& texture = result.textures.textures[illustration.illustration.texture];
    const image::Error image_error = image::inspectMetadata(texture.encoded_image, illustration.image_info);
    if (image_error == image::Error::kOutOfMemory) return ARX_BAD_ALLOC;
    if (image_error != image::Error::kNone)
      return failNode(
          ARX_GLB_BAD_CINEMATIC_IMAGE, "illustration", nodeName(*child), "has an invalid or unreadable embedded image");
    if (illustration.non_blend_material && image::hasAlpha(illustration.image_info))
      log(ARX_LOG_WARN,
          "GLB -> Cinematic: illustration {} image has an alpha channel but its material is not BLEND; image alpha "
          "is imported",
          illustration.name.ordinal);
    illustration.pixels_per_unit = illustration.surface.pixelsPerUnit(
        {static_cast<float>(illustration.image_info.width), static_cast<float>(illustration.image_info.height)});
    if (!std::isfinite(illustration.pixels_per_unit) || illustration.pixels_per_unit <= 0.0f) {
      illustration.pixels_per_unit = glb_cinematic::kUnitsPerGlbUnit;
      log(ARX_LOG_WARN,
          "GLB -> Cinematic: illustration {} has no usable pixel scale; 100 pixels per GLB unit used for depth",
          illustration.name.ordinal);
    }
    log(ARX_LOG_DEBUG,
        "GLB -> Cinematic: illustration {} image {}x{}, {} chart, {} pixels/unit, average Y {}",
        illustration.name.ordinal,
        illustration.image_info.width,
        illustration.image_info.height,
        mappingName(illustration.surface.mappingMode()),
        illustration.pixels_per_unit,
        illustration.surface.averageHeight());
    illustrations.push_back(std::move(illustration));
  }
  if (illustrations.empty()) return ARX_CINEMATIC_NO_ILLUSTRATIONS;
  if (!glb::makeTexturePathsUnique(result.textures.textures, "GLB -> Cinematic")) return ARX_CINEMATIC_BAD_TEXTURE_PATH;
  std::ranges::sort(illustrations, {}, [](const PendingIllustration& value) { return value.name.ordinal; });
  for (std::size_t index = 0; index < illustrations.size(); ++index) {
    if (index != 0 && illustrations[index - 1].name.ordinal == illustrations[index].name.ordinal) {
      log(ARX_LOG_ERROR, "GLB -> Cinematic: duplicate illustration ordinal {}", illustrations[index].name.ordinal);
      return ARX_GLB_BAD_CINEMATIC_ILLUSTRATION;
    }
    result.cinematic.illustrations.push_back(illustrations[index].illustration);
  }

  glb::SoundPathImporter sound_importer(
      result.sounds, sound_sources != nullptr ? &imported_sound_sources : nullptr, "GLB -> Cinematic");
  std::vector<std::string_view> child_tokens;
  for (std::size_t illustration_index = 0; illustration_index < illustrations.size(); ++illustration_index) {
    PendingIllustration& illustration = illustrations[illustration_index];
    for (std::size_t index = 0; index < illustration.node->children_count; ++index) {
      const cgltf_node* child = illustration.node->children[index];
      if (child == nullptr) return ARX_GLB_BAD_FORMAT;
      const std::size_t node_index = cgltf_node_index(&data, child);
      log(ARX_LOG_DEBUG,
          "GLB -> Cinematic: processing illustration child node {} '{}' under illustration {}",
          node_index,
          nodeName(*child),
          illustration.name.ordinal);
      splitDoubleUnderscore(nodeName(*child), child_tokens);
      if (!child_tokens.front().starts_with("KEY_")) {
        log(ARX_LOG_WARN, "GLB -> Cinematic: unexpected illustration child '{}' ignored", nodeName(*child));
        continue;
      }
      CinematicKeyframe key;
      rc = parseKey(*child, illustration, sound_importer, key);
      if (rc != ARX_OK) {
        log(ARX_LOG_DEBUG, "GLB -> Cinematic: key node {} '{}' failed with code {}", node_index, nodeName(*child), rc);
        return rc;
      }
      key.illustration = static_cast<CinematicIllustrationIndex>(illustration_index);
      result.cinematic.keyframes.push_back(key);
    }
    if (illustration.surface.missingTexcoords()) {
      if (illustration.surface.mappingMode() != glb_cinematic::SurfaceMappingMode::kBounds)
        log(ARX_LOG_WARN,
            "GLB -> Cinematic: illustration {} has triangles without a usable UV mapping; those triangles were "
            "excluded from its UV chart",
            illustration.name.ordinal);
      else
        log(ARX_LOG_WARN,
            "GLB -> Cinematic: illustration {} has no usable texture coordinate chart; implicit X/Z bounds chart "
            "used",
            illustration.name.ordinal);
    }
    if (illustration.surface.varyingHeight())
      log(ARX_LOG_WARN,
          "GLB -> Cinematic: illustration {} is not planar; height variation ignored",
          illustration.name.ordinal);
    if (illustration.surface.mappingMode() == glb_cinematic::SurfaceMappingMode::kPiecewiseUv)
      log(ARX_LOG_WARN,
          "GLB -> Cinematic: illustration {} has a non-affine UV chart; triangle-local mapping determines key "
          "positions",
          illustration.name.ordinal);
    if (illustration.surface.collapsedAxis())
      log(ARX_LOG_WARN,
          "GLB -> Cinematic: illustration {} has a collapsed geometry axis; that image coordinate uses its center",
          illustration.name.ordinal);
    if (illustration.outside_sample) {
      if (illustration.surface.mappingMode() == glb_cinematic::SurfaceMappingMode::kBounds)
        log(ARX_LOG_WARN,
            "GLB -> Cinematic: illustration {} has keys outside its X/Z bounds; implicit bounds-chart "
            "extrapolation used",
            illustration.name.ordinal);
      else
        log(ARX_LOG_WARN,
            "GLB -> Cinematic: illustration {} has keys outside its mesh; nearest-triangle UV "
            "extrapolation used",
            illustration.name.ordinal);
    }
    if (illustration.ambiguous_sample)
      log(ARX_LOG_WARN,
          "GLB -> Cinematic: illustration {} has overlapping UV-mapped geometry; one matching triangle was used",
          illustration.name.ordinal);
  }
  std::ranges::sort(result.cinematic.keyframes, {}, &CinematicKeyframe::frame);
  if (result.cinematic.keyframes.empty()) {
    log(ARX_LOG_ERROR, "GLB -> Cinematic: no KEY nodes found");
    return ARX_CINEMATIC_BAD_KEY_COUNT;
  }
  if (result.cinematic.keyframes.front().frame < 0) {
    log(ARX_LOG_ERROR, "GLB -> Cinematic: KEY frame {} is negative", result.cinematic.keyframes.front().frame);
    return ARX_CINEMATIC_BAD_KEY_FRAME;
  }
  for (std::size_t index = 1; index < result.cinematic.keyframes.size(); ++index)
    if (result.cinematic.keyframes[index - 1].frame == result.cinematic.keyframes[index].frame) {
      log(ARX_LOG_ERROR, "GLB -> Cinematic: duplicate KEY frame {}", result.cinematic.keyframes[index].frame);
      return ARX_CINEMATIC_BAD_KEY_FRAME;
    }
  rc = soundPathImportError(sound_importer.finish());
  if (rc != ARX_OK) {
    log(ARX_LOG_DEBUG, "GLB -> Cinematic: sound path finalization failed with code {}", rc);
    return rc;
  }
  sources.reserve(imported_sound_sources.size());
  for (glb::ImportedSoundSource& source : imported_sound_sources)
    sources.push_back({source.sound, std::move(source.path)});
  result.cinematic.fps = parsed_root.fps.value_or(25.0f);
  result.cinematic.end_frame = parsed_root.end_frame.value_or(result.cinematic.keyframes.back().frame);

  const std::optional<std::int32_t> hold_end = synthesizeInitialHold(result.cinematic);

  rc = cinematic_detail::validateStructure(result);
  if (rc != ARX_OK) {
    log(ARX_LOG_DEBUG, "GLB -> Cinematic: constructed Cinematic validation failed with code {}", rc);
    return rc;
  }
  rc = cinematic_detail::soundError(sounds::validateAudio(result.sounds));
  if (rc != ARX_OK) {
    log(ARX_LOG_DEBUG, "GLB -> Cinematic: sound validation failed with code {}", rc);
    return rc;
  }
  if (hold_end)
    log(ARX_LOG_WARN,
        "GLB -> Cinematic: KEY_0 missing; inserted a static hold through frame {} (sound and effects remain at that "
        "key)",
        *hold_end);
  out = std::move(result);
  if (sound_sources != nullptr) *sound_sources = std::move(sources);
  return ARX_OK;
}

}  // namespace pistoris
