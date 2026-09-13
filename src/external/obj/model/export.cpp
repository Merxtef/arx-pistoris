
// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#include "arx_pistoris/base/flags.h"
#include "arx_pistoris/base/indices.h"
#include "arx_pistoris/base/math.hpp"
#include "arx_pistoris/base/status.h"
#include "arx_pistoris/model/obj.hpp"
#include "arx_pistoris/runtime/types.h"

#include "external/material_name.h"
#include "external/obj/coordinates.h"
#include "external/obj/model.h"
#include "model/data.h"
#include "modules/action_points.h"
#include "modules/geometry.h"
#include "modules/textures.h"
#include "utils/encoded_image.h"
#include "utils/log.h"
#include "utils/resource_path.h"

#include <array>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <format>
#include <iomanip>
#include <limits>
#include <locale>
#include <map>
#include <optional>
#include <span>
#include <sstream>
#include <string>
#include <string_view>
#include <tuple>
#include <utility>
#include <vector>

namespace pistoris {
namespace {

struct MaterialState {
  TextureIndex texture = kNoTexture;
  FaceType flags = 0;
  float transval = 0.0f;
};

struct MaterialOutput {
  TextureIndex texture = kNoTexture;
  std::string name;
  bool has_opacity = false;
  float transval = 0.0f;
};

struct MaterialKey {
  TextureIndex texture = kNoTexture;
  FaceType flags = 0;
  std::uint32_t transval = 0;

  friend bool operator==(const MaterialKey&, const MaterialKey&) noexcept = default;

  friend bool operator<(const MaterialKey& left, const MaterialKey& right) noexcept {
    return std::tie(left.texture, left.flags, left.transval) < std::tie(right.texture, right.flags, right.transval);
  }
};

bool validMaterialLibraryName(std::string_view value) noexcept {
  if (value.empty()) return false;
  for (char character : value) {
    const auto byte = static_cast<unsigned char>(character);
    if (byte <= 0x20U || byte == 0x7fU || character == '#') return false;
  }
  return true;
}

std::string imageExtension(const Texture& texture) {
  if (!texture.external_image_extension.empty()) return texture.external_image_extension;
  if (!texture.encoded_image.empty()) {
    const std::string_view extension = image::extension(image::detectFormat(texture.encoded_image));
    if (!extension.empty()) return std::string(extension);
  }
  return ".png";
}

}  // namespace

ArxReturnCode exportModelToObj(const ModelModules& model, std::string_view stem, const ObjExportOptions& options,
                               ObjBundle& out) {
  if (!validMaterialLibraryName(stem)) {
    log(ARX_LOG_ERROR, "Model -> OBJ: material-library name is empty or contains OBJ token separators");
    return ARX_OBJ_BAD_MATERIAL_LIBRARY_NAME;
  }
  std::map<MaterialKey, MaterialOutput> materials;
  std::vector<std::uint8_t> used_textures(model.textures.textures.size(), 0);
  for (const Face& face : model.geometry.faces) {
    const bool transparent = (face.flags & kFaceBitTrans) != 0;
    const float transval = transparent && face.transval != 0.0f ? face.transval : 0.0f;
    const MaterialKey key{face.texture, face.flags, std::bit_cast<std::uint32_t>(transval)};
    MaterialOutput& material = materials[key];
    material.texture = face.texture;
    if (face.texture != kNoTexture) used_textures[face.texture] = 1;
    if (transparent) {
      material.has_opacity = face.transval >= 0.0f && face.transval <= 1.0f;
      material.transval = face.transval;
    }
  }

  std::vector<std::string> texture_paths(model.textures.textures.size());
  ResourcePathUniquifier texture_path_uniquifier;
  texture_path_uniquifier.reserve(model.textures.textures.size());
  for (std::size_t index = 0; index < model.textures.textures.size(); ++index) {
    if (used_textures[index] == 0) continue;
    const Texture& texture = model.textures.textures[index];
    texture_paths[index] = texture.path + imageExtension(texture);
    texture_path_uniquifier.add(texture_paths[index]);
  }
  if (texture_path_uniquifier.apply() != ResourcePathError::kNone) return ARX_MODEL_BAD_TEXTURE_PATH;

  constexpr std::array<std::string_view, 1> kReservedStems = {"no_tex"};
  const std::vector<std::string> texture_stems =
      material_names::fallbackStems(model.textures.textures, used_textures, kReservedStems, "Model -> OBJ");
  for (auto& [key, material] : materials) {
    const std::string_view texture_stem =
        key.texture == kNoTexture ? std::string_view{} : std::string_view(texture_stems[key.texture]);
    material.name = material_names::encode(texture_stem, key.flags, material.transval);
  }

  std::ostringstream object;
  object.imbue(std::locale::classic());
  object << std::setprecision(std::numeric_limits<float>::max_digits10);
  object << "# arx-pistoris OBJ export\n";
  if (!materials.empty()) object << "mtllib " << stem << ".mtl\n";
  for (const ActionPoint& point : model.action_points.points) {
    const ArxVector3 position = obj_coordinates::toObjPoint(point.position);
    object << "# arx_action " << point.name << ' ' << position.x << ' ' << position.y << ' ' << position.z << '\n';
  }
  object << '\n';
  for (const Vertex& vertex : model.geometry.vertices) {
    const ArxVector3 position = obj_coordinates::toObjPoint(vertex.position);
    object << "v " << position.x << ' ' << position.y << ' ' << position.z << '\n';
  }
  object << '\n';
  for (const Face& face : model.geometry.faces)
    for (const Corner& corner : face.corners) {
      const ArxVector3 normal = obj_coordinates::toObjDirection(corner.normal);
      object << "vn " << normal.x << ' ' << normal.y << ' ' << normal.z << '\n';
    }
  object << '\n';
  for (const Face& face : model.geometry.faces)
    for (const Corner& corner : face.corners) {
      const ArxVector2 texcoord = obj_coordinates::toObjTexcoord({corner.u, corner.v});
      object << "vt " << texcoord.x << ' ' << texcoord.y << '\n';
    }
  object << '\n';

  std::optional<MaterialKey> current_material;
  for (std::size_t face_index = 0; face_index < model.geometry.faces.size(); ++face_index) {
    const Face& face = model.geometry.faces[face_index];
    const bool transparent = (face.flags & kFaceBitTrans) != 0;
    const float transval = transparent && face.transval != 0.0f ? face.transval : 0.0f;
    const MaterialKey key{face.texture, face.flags, std::bit_cast<std::uint32_t>(transval)};
    if (!current_material || *current_material != key) {
      object << "usemtl " << materials.at(key).name << '\n';
      current_material = key;
    }
    object << 'f';
    for (std::size_t corner_index = 0; corner_index < face.corners.size(); ++corner_index) {
      const std::size_t vertex = static_cast<std::size_t>(face.corners[corner_index].vertex) + 1U;
      const std::size_t attribute = face_index * 3U + corner_index + 1U;
      object << ' ' << vertex << '/' << attribute << '/' << attribute;
    }
    object << '\n';
  }

  std::ostringstream material_text;
  material_text.imbue(std::locale::classic());
  material_text << std::setprecision(std::numeric_limits<float>::max_digits10);
  material_text << "# arx-pistoris MTL export\n";
  for (const auto& [key, material] : materials) {
    (void)key;
    material_text << "\nnewmtl " << material.name << '\n';
    if (material.texture != kNoTexture) {
      material_text << "map_Kd " << texture_paths[material.texture] << '\n';
    }
    if (material.has_opacity) material_text << "d " << (1.0f - material.transval) << '\n';
  }

  std::size_t bound_actions = 0;
  std::size_t selected_actions = 0;
  for (std::size_t index = 0; index < model.action_points.points.size(); ++index) {
    bound_actions += static_cast<std::size_t>(model.action_points.points[index].bone != kInvalidBoneIndex);
    selected_actions += static_cast<std::size_t>(model.selections.action_point_masks[index] != 0);
  }
  if (!model.skeleton.bones.empty() || model.selections.occupied != 0 || bound_actions != 0 || selected_actions != 0)
    log(ARX_LOG_WARN,
        "Model -> OBJ: skeletal data and selection membership are not representable; {} bones, {} "
        "selections, {} bound action points, {} selected action points ignored",
        model.skeleton.bones.size(),
        std::popcount(model.selections.occupied),
        bound_actions,
        selected_actions);

  ObjBundle result;
  result.text = std::move(object).str();
  result.mtl = std::move(material_text).str();
  if (options.include_files) {
    result.texture_files.reserve(model.textures.textures.size());
    for (std::size_t index = 0; index < model.textures.textures.size(); ++index) {
      const Texture& texture = model.textures.textures[index];
      if (used_textures[index] == 0 || texture.encoded_image.empty()) continue;
      result.texture_files.push_back({static_cast<TextureIndex>(index), texture_paths[index], texture.encoded_image});
    }
  }
  out = std::move(result);
  return ARX_OK;
}

}  // namespace pistoris
