// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#include "arx_pistoris/base/indices.h"
#include "arx_pistoris/base/status.h"
#include "arx_pistoris/runtime/types.h"
#include "arx_pistoris/sound.hpp"

#include "api.h"
#include "cgltf/cgltf.h"
#include "cinematic/data.h"
#include "coordinates.h"
#include "external/glb/accessor.h"
#include "external/glb/utils/image.h"
#include "external/glb/writer.h"
#include "modules/cinematic.h"
#include "modules/sounds.h"
#include "modules/textures.h"
#include "names.h"
#include "utils/encoded_image.h"
#include "utils/log.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace pistoris {
namespace {

ArxReturnCode textureError(textures::Error error) noexcept {
  if (error == textures::Error::kOutOfMemory) return ARX_BAD_ALLOC;
  return error == textures::Error::kNone ? ARX_OK : ARX_CINEMATIC_BAD_TEXTURE_IMAGE;
}

std::span<const std::uint8_t> preparedBytes(const textures::PreparedImage& prepared) noexcept {
  return prepared.bytes.data();
}

void addKey(glb::Builder& builder, int illustration_node, int camera, const CinematicKeyframe& key,
            std::string_view sound_path, SoundKind sound_kind) {
  const int key_node = builder.addNode(glb_cinematic::keyName(key));
  builder.setNodeTranslation(key_node, glb_cinematic::toGlbPoint(key.camera_position));
  builder.setNodeRotation(key_node, glb_cinematic::toGlbRotation(key.camera_roll));
  builder.setNodeCamera(key_node, camera);
  builder.addChild(illustration_node, key_node);

  if (key.post_effect != CinematicPostEffect::kNone)
    builder.addChild(key_node, builder.addNode(glb_cinematic::flashName(key)));

  if (key.light_active) {
    const int light = builder.addNode(glb_cinematic::lightName(key.light));
    if (key.light.intensity >= 0.0f)
      builder.setNodeTranslation(light, glb_cinematic::toGlbLightPoint(key.light.position));
    builder.addChild(key_node, light);
  }

  if (!sound_path.empty()) {
    const int sound = builder.addNode(glb_cinematic::soundName(sound_kind));
    builder.addChild(sound, builder.addNode(glb_cinematic::soundPathName(sound_path)));
    builder.addChild(key_node, sound);
  }
}

}  // namespace

ArxReturnCode exportCinematicToGlb(const CinematicModules& modules, std::vector<std::uint8_t>& out) {
  std::vector<textures::ImagePreparationRequest> requests;
  requests.reserve(modules.cinematic.illustrations.size());
  std::vector<std::size_t> prepared_by_texture(modules.textures.textures.size(),
                                               std::numeric_limits<std::size_t>::max());
  for (const CinematicIllustration& illustration : modules.cinematic.illustrations) {
    std::size_t& prepared_index = prepared_by_texture[illustration.texture];
    if (prepared_index != std::numeric_limits<std::size_t>::max()) continue;
    prepared_index = requests.size();
    requests.push_back(
        {illustration.texture,
         {.accepted_formats = image::formatFlag(image::Format::kPng) | image::formatFlag(image::Format::kJpeg),
          .fallback_format = image::Format::kPng,
          .require_power_of_two = false,
          .bmp_color_key = image::BmpColorKey::kNone}});
  }
  std::vector<textures::PreparedImage> prepared;
  log(ARX_LOG_DEBUG, "Cinematic -> GLB: preparing {} unique illustration image(s)", requests.size());
  ArxReturnCode rc = textureError(textures::prepareImages(modules.textures, requests, prepared));
  if (rc != ARX_OK) {
    log(ARX_LOG_DEBUG, "Cinematic -> GLB: illustration image preparation failed with code {}", rc);
    return rc;
  }
  std::vector<int> material_by_prepared(prepared.size(), -1);

  glb::Builder builder;
  builder.setContentBasisRotation(glb_cinematic::kContentBasis);
  const int root = builder.addNode(glb_cinematic::rootName(modules.cinematic));
  builder.addRoot(root);
  const int camera = builder.addPerspectiveCamera(
      "cinematic_camera",
      {.vertical_fov = glb_cinematic::gameVerticalFov(), .aspect_ratio = 4.0f / 3.0f, .znear = 0.01f, .zfar = {}});

  float cell_width = 0.0f;
  float cell_height = 0.0f;
  for (const CinematicIllustration& illustration : modules.cinematic.illustrations) {
    const textures::PreparedImage& image = prepared[prepared_by_texture[illustration.texture]];
    cell_width = std::max(cell_width, static_cast<float>(image.info.width) / glb_cinematic::kUnitsPerGlbUnit);
    cell_height = std::max(cell_height, static_cast<float>(image.info.height) / glb_cinematic::kUnitsPerGlbUnit);
  }
  const std::size_t columns =
      static_cast<std::size_t>(std::ceil(std::sqrt(static_cast<double>(modules.cinematic.illustrations.size()))));

  std::vector<int> illustration_nodes;
  illustration_nodes.reserve(modules.cinematic.illustrations.size());
  for (std::size_t index = 0; index < modules.cinematic.illustrations.size(); ++index) {
    log(ARX_LOG_DEBUG, "Cinematic -> GLB: exporting illustration node {}", index);
    const std::size_t column = index % columns;
    const std::size_t row = index / columns;
    const CinematicIllustration& illustration = modules.cinematic.illustrations[index];
    const std::size_t prepared_index = prepared_by_texture[illustration.texture];
    const textures::PreparedImage& image = prepared[prepared_index];
    const float half_width = static_cast<float>(image.info.width) / (2.0f * glb_cinematic::kUnitsPerGlbUnit);
    const float half_height = static_cast<float>(image.info.height) / (2.0f * glb_cinematic::kUnitsPerGlbUnit);
    const std::array<glb::Vec3, 4> positions = {{
        {-half_width, -half_height, 0.0f},
        {half_width, -half_height, 0.0f},
        {half_width, half_height, 0.0f},
        {-half_width, half_height, 0.0f},
    }};
    const std::array<glb::Vec2, 4> texcoords = {{{0.0f, 0.0f}, {1.0f, 0.0f}, {1.0f, 1.0f}, {0.0f, 1.0f}}};
    const std::array<std::uint32_t, 6> indices = {0, 2, 1, 0, 3, 2};

    int& material = material_by_prepared[prepared_index];
    if (material < 0) {
      const Texture& source = modules.textures.textures[illustration.texture];
      std::string image_name = source.path;
      image_name += image::extension(image.info.format);
      const int texture = builder.addEmbeddedTexture(
          std::move(image_name), std::string(glb::imageMimeType(image.info.format)), preparedBytes(image));
      const glb::MaterialOptions material_options{
          .alpha_mode = image::hasAlpha(image.info) ? glb::MaterialAlphaMode::kBlend : glb::MaterialAlphaMode::kOpaque,
          .double_sided = true,
          .unlit = true,
      };
      material = builder.addMaterial("illustration", texture, material_options);
    }
    glb::Primitive primitive;
    primitive.indices =
        builder.addAccessor(std::span<const std::uint32_t>(indices), cgltf_component_type_r_32u, cgltf_type_scalar);
    primitive.material = material;
    primitive.attributes = {
        {"POSITION", builder.addVec3Accessor(positions)},
        {"TEXCOORD_0",
         builder.addAccessor(std::span<const glb::Vec2>(texcoords), cgltf_component_type_r_32f, cgltf_type_vec2)},
    };
    const int mesh = builder.addMesh("illustration", {std::move(primitive)});
    const int node =
        builder.addNode(glb_cinematic::illustrationName(static_cast<std::uint32_t>(index), illustration), mesh);
    builder.setNodeTranslation(
        node, {static_cast<float>(column) * (cell_width + 0.5f), static_cast<float>(row) * (cell_height + 0.5f), 0.0f});
    builder.addChild(root, node);
    illustration_nodes.push_back(node);
  }

  for (const CinematicKeyframe& key : modules.cinematic.keyframes) {
    log(ARX_LOG_DEBUG, "Cinematic -> GLB: exporting key frame {} under illustration {}", key.frame, key.illustration);
    std::string_view sound_path;
    SoundKind sound_kind = SoundKind::kEffect;
    if (key.sound != kNoSoundHandle) {
      if (soundHandleKind(key.sound, sound_kind) != ARX_OK) return ARX_CINEMATIC_BAD_KEY_SOUND;
      sound_path = sounds::path(modules.sounds, key.sound);
      if (sound_path.empty()) return ARX_CINEMATIC_BAD_KEY_SOUND;
    }
    addKey(builder, illustration_nodes[key.illustration], camera, key, sound_path, sound_kind);
  }
  rc = builder.write(out);
  if (rc != ARX_OK) log(ARX_LOG_DEBUG, "Cinematic -> GLB: GLB encoding failed with code {}", rc);
  return rc;
}

}  // namespace pistoris
