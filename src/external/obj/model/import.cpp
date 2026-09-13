
// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#include "arx_pistoris/base/flags.h"
#include "arx_pistoris/base/indices.h"
#include "arx_pistoris/base/math.hpp"
#include "arx_pistoris/base/status.h"
#include "arx_pistoris/model/obj.hpp"
#include "arx_pistoris/runtime/types.h"

#include "api/status_boundary.h"
#include "external/material_name.h"
#include "external/obj/coordinates.h"
#include "external/obj/model.h"
#include "model/data.h"
#include "modules/action_points.h"
#include "modules/geometry.h"
#include "modules/textures.h"
#include "utils/identifier.h"
#include "utils/log.h"
#include "utils/math/triangulation.h"
#include "utils/resource_path.h"
#include "utils/text_cursor.h"

#include <algorithm>
#include <array>
#include <charconv>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <system_error>
#include <unordered_map>
#include <utility>
#include <vector>

namespace pistoris {
namespace {

struct MtlEntry {
  std::string image_path;
  float transval = 0.0f;
  bool has_opacity = false;
};

struct FaceCorner {
  std::size_t position = 0;
  std::size_t texcoord = 0;
  std::size_t normal = 0;
  bool has_texcoord = false;
  bool has_normal = false;
};

struct MaterialState {
  TextureIndex texture = kNoTexture;
  FaceType flags = 0;
  float transval = 0.0f;
};

std::string_view nextLine(std::string_view text, std::size_t& position) noexcept {
  const std::size_t start = position;
  const std::size_t newline = text.find('\n', position);
  if (newline == std::string_view::npos) {
    position = text.size();
    std::string_view line = text.substr(start);
    if (!line.empty() && line.back() == '\r') line.remove_suffix(1);
    return line;
  }
  position = newline + 1;
  std::string_view line = text.substr(start, newline - start);
  if (!line.empty() && line.back() == '\r') line.remove_suffix(1);
  return line;
}

std::optional<float> parseFloat(std::string_view text) noexcept {
  float result = 0.0f;
  const auto [end, error] = std::from_chars(text.data(), text.data() + text.size(), result);
  if (error != std::errc{} || end != text.data() + text.size() || !std::isfinite(result)) return std::nullopt;
  return result;
}

std::string_view lineValue(TextCursor& line) noexcept {
  const Token value = line.restOfLine();
  if (value.kind != TokenKind::kString) return {};
  std::string_view result = value.text;
  const std::size_t comment = result.find('#');
  if (comment != std::string_view::npos) result = result.substr(0, comment);
  while (!result.empty() && (result.back() == ' ' || result.back() == '\t')) result.remove_suffix(1);
  return result;
}

ArxReturnCode objLineError(ArxReturnCode code, std::size_t line, std::string_view message) {
  log(ARX_LOG_ERROR, "OBJ line {}: {}", line, message);
  return code;
}

ArxReturnCode mtlLineError(std::string_view path, std::size_t line, std::string_view message) {
  log(ARX_LOG_ERROR, "OBJ MTL '{}' line {}: {}", path.empty() ? std::string_view("<inline>") : path, line, message);
  return ARX_OBJ_BAD_MTL;
}

bool resolveIndex(std::string_view text, std::size_t count, std::size_t& out) noexcept {
  int parsed = 0;
  const auto [end, error] = std::from_chars(text.data(), text.data() + text.size(), parsed);
  if (error != std::errc{} || end != text.data() + text.size() || parsed == 0) return false;
  if (parsed > 0) {
    if (static_cast<std::size_t>(parsed) > count) return false;
    out = static_cast<std::size_t>(parsed) - 1U;
    return true;
  }
  const std::size_t distance = static_cast<std::size_t>(-static_cast<long long>(parsed));
  if (distance > count) return false;
  out = count - distance;
  return true;
}

ArxReturnCode parseVector3(TextCursor& line, ArxVector3& out) noexcept {
  const Token x_token = line.next();
  const Token y_token = line.next();
  const Token z_token = line.next();
  if (x_token.kind != TokenKind::kString || y_token.kind != TokenKind::kString || z_token.kind != TokenKind::kString)
    return ARX_OBJ_BAD_FORMAT;
  const std::optional<float> x = parseFloat(x_token.text);
  const std::optional<float> y = parseFloat(y_token.text);
  const std::optional<float> z = parseFloat(z_token.text);
  if (!x || !y || !z) return ARX_OBJ_BAD_FORMAT;
  out = {*x, *y, *z};
  return ARX_OK;
}

bool fixedMapOptionArity(std::string_view option, std::size_t& out) noexcept {
  if (option.compare("-mm") == 0) {
    out = 2;
    return true;
  }
  if (option.compare("-blendu") == 0 || option.compare("-blendv") == 0 || option.compare("-boost") == 0 ||
      option.compare("-bm") == 0 || option.compare("-cc") == 0 || option.compare("-clamp") == 0 ||
      option.compare("-imfchan") == 0 || option.compare("-texres") == 0 || option.compare("-type") == 0) {
    out = 1;
    return true;
  }
  return false;
}

ArxReturnCode parseMapPath(std::string_view value, std::string_view library_path, std::size_t line_number,
                           std::string& out) {
  TextCursor cursor(value, "#");
  Token token = cursor.next();
  while (token.kind == TokenKind::kString && token.text.starts_with('-')) {
    const std::string_view option = token.text;
    if (option == "-o" || option == "-s" || option == "-t") {
      std::size_t values = 0;
      token = cursor.next();
      while (token.kind == TokenKind::kString && values < 3U && parseFloat(token.text)) {
        ++values;
        token = cursor.next();
      }
      if (values == 0) return mtlLineError(library_path, line_number, "map_Kd option has no numeric value");
      continue;
    }
    std::size_t arity = 0;
    if (!fixedMapOptionArity(option, arity))
      return mtlLineError(library_path, line_number, "map_Kd uses an unsupported option");
    for (std::size_t index = 0; index < arity; ++index) {
      token = cursor.next();
      if (token.kind != TokenKind::kString)
        return mtlLineError(library_path, line_number, "map_Kd option is missing a value");
    }
    token = cursor.next();
  }
  if (token.kind != TokenKind::kString) return mtlLineError(library_path, line_number, "map_Kd has no texture path");
  std::string_view path = value.substr(token.col - 1U);
  const std::size_t comment = path.find('#');
  if (comment != std::string_view::npos) path = path.substr(0, comment);
  while (!path.empty() && (path.back() == ' ' || path.back() == '\t')) path.remove_suffix(1);
  if (path.empty()) return mtlLineError(library_path, line_number, "map_Kd has no texture path");
  out = path;
  return ARX_OK;
}

ArxReturnCode parseMtl(const ObjMaterialLibraryView& library, std::unordered_map<std::string, MtlEntry>& out) {
  MtlEntry* current = nullptr;
  std::size_t position = 0;
  std::size_t line_number = 0;
  while (position < library.text.size()) {
    ++line_number;
    TextCursor line(nextLine(library.text, position), "#");
    const Token keyword = line.next();
    if (keyword.kind == TokenKind::kSpecialChar) {
      const Token name = line.next();
      if (name.kind != TokenKind::kString) continue;
      if (name.text.starts_with("arx_"))
        log(ARX_LOG_WARN,
            "OBJ MTL '{}' line {}: unknown reserved directive '{}' ignored",
            library.path.empty() ? std::string_view("<inline>") : library.path,
            line_number,
            name.text);
      continue;
    }
    if (keyword.kind != TokenKind::kString) continue;
    if (keyword.text == "newmtl") {
      const std::string_view name = lineValue(line);
      if (name.empty()) return mtlLineError(library.path, line_number, "newmtl has no material name");
      current = &out[std::string(name)];
      *current = {};
      continue;
    }
    if (!current) continue;
    if (keyword.text == "map_Kd") {
      const std::string_view value = lineValue(line);
      if (value.empty()) return mtlLineError(library.path, line_number, "map_Kd has no texture path");
      const ArxReturnCode rc = parseMapPath(value, library.path, line_number, current->image_path);
      if (rc != ARX_OK) return rc;
    } else if (keyword.text == "d") {
      const Token value = line.next();
      const std::optional<float> opacity = value.kind == TokenKind::kString ? parseFloat(value.text) : std::nullopt;
      if (!opacity) return mtlLineError(library.path, line_number, "d has an invalid opacity");
      current->transval = 1.0f - *opacity;
      current->has_opacity = true;
    }
  }
  return ARX_OK;
}

ArxReturnCode parseFaceCorner(std::string_view token, std::size_t position_count, std::size_t texcoord_count,
                              std::size_t normal_count, FaceCorner& out) noexcept {
  const std::size_t first = token.find('/');
  if (first == std::string_view::npos)
    return resolveIndex(token, position_count, out.position) ? ARX_OK : ARX_OBJ_BAD_POSITION_INDEX;
  if (!resolveIndex(token.substr(0, first), position_count, out.position)) return ARX_OBJ_BAD_POSITION_INDEX;

  const std::size_t second = token.find('/', first + 1U);
  const std::string_view texcoord =
      token.substr(first + 1U, second == std::string_view::npos ? std::string_view::npos : second - first - 1U);
  if (!texcoord.empty()) {
    if (!resolveIndex(texcoord, texcoord_count, out.texcoord)) return ARX_OBJ_BAD_TEXCOORD_INDEX;
    out.has_texcoord = true;
  }
  if (second == std::string_view::npos) return ARX_OK;

  const std::string_view normal = token.substr(second + 1U);
  if (!normal.empty()) {
    if (!resolveIndex(normal, normal_count, out.normal)) return ARX_OBJ_BAD_NORMAL_INDEX;
    out.has_normal = true;
  }
  return ARX_OK;
}

ArxVector3 polygonNormal(std::span<const FaceCorner> corners, std::span<const ArxVector3> positions) noexcept {
  ArxVector3 result{};
  for (std::size_t index = 0; index < corners.size(); ++index) {
    const ArxVector3& current = positions[corners[index].position];
    const ArxVector3& next = positions[corners[(index + 1U) % corners.size()].position];
    result.x += (current.y - next.y) * (current.z + next.z);
    result.y += (current.z - next.z) * (current.x + next.x);
    result.z += (current.x - next.x) * (current.y + next.y);
  }
  return math::normalizeFiniteOr(result, {});
}

ArxVector2 projectPoint(const ArxVector3& point, const ArxVector3& normal) noexcept {
  const float x = std::abs(normal.x);
  const float y = std::abs(normal.y);
  const float z = std::abs(normal.z);
  if (x >= y && x >= z) return {point.y, point.z};
  if (y >= z) return {point.x, point.z};
  return {point.x, point.y};
}

ArxReturnCode triangulate(std::span<const FaceCorner> corners, std::span<const ArxVector3> positions,
                          std::vector<ArxVector2>& projected, std::vector<std::uint32_t>& out, ArxVector3& out_normal) {
  if (corners.size() < 3) return ARX_OBJ_BAD_FACE;
  out_normal = polygonNormal(corners, positions);
  if (math::lengthSquared(out_normal) == 0.0) return ARX_OBJ_BAD_FACE;
  if (corners.size() == 3) {
    out.assign({0, 1, 2});
    return ARX_OK;
  }

  projected.clear();
  projected.reserve(corners.size());
  for (const FaceCorner& corner : corners) projected.push_back(projectPoint(positions[corner.position], out_normal));
  if (math::triangulateSimplePolygon(projected, out) == math::TriangulationResult::kSuccess) return ARX_OK;
  if (corners.size() != 4) return ARX_OBJ_BAD_FACE;
  out.assign({0, 1, 2, 0, 2, 3});
  return ARX_OK;
}

using TextureSourceMap =
    std::unordered_map<std::string, TextureIndex, ResourcePathIdentityHash, ResourcePathIdentityEqual>;

struct TextureImportRegistry {
  TextureSourceMap external;
  TextureSourceMap fallbacks;
};

ArxReturnCode resolveTexture(std::string_view material_name, const MtlEntry* material,
                             TextureImportRegistry& texture_registry, ModelModules& out, MaterialState& state,
                             std::vector<std::string>* texture_source_paths) {
  const bool has_source = material && !material->image_path.empty();
  material_names::Decoded decoded;
  material_names::DecodeInfo decode_info;
  const material_names::DecodeError decode_error =
      material_names::decode(material_name, has_source, decoded, &decode_info);
  if (decode_error != material_names::DecodeError::kNone) return ARX_OBJ_BAD_MATERIAL_NAME;
  if (decode_info.duplicate_flags != 0)
    log(ARX_LOG_WARN,
        "OBJ -> Model: material '{}' repeats {} face flag token(s); duplicates ignored",
        material_name,
        decode_info.duplicate_flags);
  if (decode_info.unknown_tokens != 0)
    log(ARX_LOG_WARN,
        "OBJ -> Model: material '{}' ignores {} unknown token(s), first '{}'; texture identity comes from map_Kd",
        material_name,
        decode_info.unknown_tokens,
        decode_info.first_unknown_token);

  state.flags = decoded.flags;
  state.transval = material && material->has_opacity ? material->transval : 0.0f;
  if (material && material->has_opacity && material->transval != 0.0f) state.flags |= kFaceBitTrans;
  if (decoded.transval) {
    if ((state.flags & kFaceBitTrans) == 0) return ARX_OBJ_BAD_MATERIAL_NAME;
    state.transval = *decoded.transval;
  }
  state.texture = kNoTexture;
  if (decoded.fallback_stem == "no_tex" && !has_source) return ARX_OK;
  if (decoded.fallback_stem == "no_tex")
    log(ARX_LOG_WARN, "OBJ -> Model: no_tex material '{}' uses its map_Kd texture", material_name);

  const std::string_view source = has_source ? std::string_view(material->image_path) : decoded.fallback_stem;
  TextureSourceMap& textures_by_source = has_source ? texture_registry.external : texture_registry.fallbacks;
  Texture texture = has_source ? textures::fromImagePath(source) : Texture(source);
  const auto found = textures_by_source.find(source);
  if (found != textures_by_source.end()) {
    Texture& existing = out.textures.textures[found->second];
    if (existing.external_image_extension.empty())
      existing.external_image_extension = std::move(texture.external_image_extension);
    state.texture = found->second;
    return ARX_OK;
  }
  if (out.textures.textures.size() >= static_cast<std::size_t>(kNoTexture)) return ARX_OBJ_TOO_MANY_TEXTURES;
  const TextureIndex index = static_cast<TextureIndex>(out.textures.textures.size());
  out.textures.textures.push_back(std::move(texture));
  textures_by_source.emplace(source, index);
  state.texture = index;
  if (texture_source_paths) texture_source_paths->push_back(has_source ? material->image_path : std::string{});
  return ARX_OK;
}

}  // namespace

ArxReturnCode importObjToModel(std::string_view obj, std::span<const ObjMaterialLibraryView> material_libraries,
                               ModelModules& out, std::vector<std::string>* texture_source_paths) {
  std::unordered_map<std::string, MtlEntry> materials;
  for (const ObjMaterialLibraryView& library : material_libraries) {
    const ArxReturnCode rc = parseMtl(library, materials);
    if (rc != ARX_OK) return rc;
  }

  std::vector<ArxVector3> positions;
  std::vector<ArxVector3> normals;
  std::vector<ArxVector2> texcoords;
  std::vector<VertexIndex> vertex_map;
  TextureImportRegistry texture_registry;
  texture_registry.external.reserve(materials.size());
  texture_registry.fallbacks.reserve(materials.size());
  if (texture_source_paths) texture_source_paths->reserve(materials.size());
  std::unordered_map<std::string, MaterialState> material_states;
  std::unordered_map<std::string, std::uint32_t> smoothing_groups;
  std::unordered_map<std::uint64_t, ArxVector3> smoothed_normals;
  std::vector<std::uint32_t> face_smoothing_groups;
  std::vector<FaceCorner> corners;
  std::vector<ArxVector2> projected;
  std::vector<std::uint32_t> triangles;
  std::string current_material;
  std::uint32_t current_smoothing_group = 0;
  std::uint32_t next_smoothing_group = 1;
  std::size_t stripped_quad_materials = 0;

  std::size_t position = 0;
  std::size_t line_number = 0;
  while (position < obj.size()) {
    ++line_number;
    TextCursor line(nextLine(obj, position), "#");
    const Token keyword = line.next();
    if (keyword.kind == TokenKind::kSpecialChar) {
      const Token directive = line.next();
      if (directive.kind != TokenKind::kString) continue;
      if (directive.text != "arx_action") {
        if (directive.text.starts_with("arx_"))
          log(ARX_LOG_WARN, "OBJ: unknown reserved directive '{}' ignored", directive.text);
        continue;
      }
      const Token name = line.next();
      if (name.kind != TokenKind::kString)
        return objLineError(ARX_OBJ_BAD_ACTION_POINT, line_number, "arx_action has no name");
      ArxVector3 point{};
      const ArxReturnCode rc = parseVector3(line, point);
      if (rc != ARX_OK)
        return objLineError(ARX_OBJ_BAD_ACTION_POINT, line_number, "arx_action has an invalid position");
      if (out.action_points.points.size() >= static_cast<std::size_t>(kInvalidActionPointIndex))
        return objLineError(ARX_OBJ_TOO_MANY_ACTION_POINTS, line_number, "arx_action exceeds the action-point limit");
      out.action_points.points.push_back(
          {std::string(name.text), obj_coordinates::toModelPoint(point), kInvalidBoneIndex});
      out.selections.action_point_masks.push_back(0);
      continue;
    }
    if (keyword.kind != TokenKind::kString) continue;

    if (keyword.text == "v") {
      ArxVector3 value{};
      const ArxReturnCode rc = parseVector3(line, value);
      if (rc != ARX_OK) return objLineError(rc, line_number, "vertex position is invalid");
      positions.push_back(obj_coordinates::toModelPoint(value));
      continue;
    }
    if (keyword.text == "vn") {
      ArxVector3 value{};
      const ArxReturnCode rc = parseVector3(line, value);
      if (rc != ARX_OK) return objLineError(rc, line_number, "vertex normal is invalid");
      value = math::normalizeFiniteOr(obj_coordinates::toModelDirection(value), {});
      if (math::lengthSquared(value) == 0.0)
        return objLineError(ARX_OBJ_BAD_FORMAT, line_number, "vertex normal is zero");
      normals.push_back(value);
      continue;
    }
    if (keyword.text == "vt") {
      const Token u_token = line.next();
      if (u_token.kind != TokenKind::kString)
        return objLineError(ARX_OBJ_BAD_FORMAT, line_number, "texture coordinate has no U component");
      const std::optional<float> u = parseFloat(u_token.text);
      if (!u) return objLineError(ARX_OBJ_BAD_FORMAT, line_number, "texture coordinate U component is invalid");
      const Token v_token = line.next();
      std::optional<float> v = 0.0f;
      if (v_token.kind == TokenKind::kString) v = parseFloat(v_token.text);
      if (!v) return objLineError(ARX_OBJ_BAD_FORMAT, line_number, "texture coordinate V component is invalid");
      texcoords.push_back(obj_coordinates::toModelTexcoord({*u, *v}));
      continue;
    }
    if (keyword.text == "usemtl") {
      current_material = lineValue(line);
      continue;
    }
    if (keyword.text == "s") {
      const Token group = line.next();
      if (group.kind != TokenKind::kString)
        return objLineError(ARX_OBJ_BAD_FORMAT, line_number, "smoothing group has no value");
      if (group.text == "off" || group.text == "0") {
        current_smoothing_group = 0;
      } else {
        const std::string name(group.text == "on" ? std::string_view("1") : group.text);
        const auto found = smoothing_groups.find(name);
        if (found != smoothing_groups.end()) {
          current_smoothing_group = found->second;
        } else {
          if (next_smoothing_group == 0)
            return objLineError(ARX_OBJ_BAD_FORMAT, line_number, "too many smoothing groups");
          current_smoothing_group = next_smoothing_group++;
          smoothing_groups.emplace(name, current_smoothing_group);
        }
      }
      continue;
    }
    if (keyword.text != "f") continue;

    corners.clear();
    for (Token token = line.next(); token.kind == TokenKind::kString; token = line.next()) {
      FaceCorner corner;
      const ArxReturnCode rc = parseFaceCorner(token.text, positions.size(), texcoords.size(), normals.size(), corner);
      if (rc != ARX_OK) return objLineError(rc, line_number, "face references an invalid attribute index");
      corners.push_back(corner);
    }
    if (corners.size() >= 3U) {
      const std::size_t face_limit = static_cast<std::size_t>(kInvalidFaceIndex);
      if (out.geometry.faces.size() > face_limit || corners.size() - 2U > face_limit - out.geometry.faces.size())
        return objLineError(ARX_OBJ_TOO_MANY_FACES, line_number, "face exceeds the Model face limit");
    }
    ArxVector3 polygon_normal{};
    ArxReturnCode rc = triangulate(corners, positions, projected, triangles, polygon_normal);
    if (rc != ARX_OK) return objLineError(rc, line_number, "face cannot be triangulated");

    MaterialState current;
    if (!current_material.empty()) {
      const auto cached = material_states.find(current_material);
      if (cached != material_states.end()) {
        current = cached->second;
      } else {
        const auto found = materials.find(current_material);
        const MtlEntry* material = found == materials.end() ? nullptr : &found->second;
        rc = resolveTexture(current_material, material, texture_registry, out, current, texture_source_paths);
        if (rc != ARX_OK)
          return objLineError(rc,
                              line_number,
                              rc == ARX_OBJ_TOO_MANY_TEXTURES ? "material exceeds the Model texture limit"
                                                              : "material name is invalid");
        if ((current.flags & kFaceBitQuad) != 0) {
          current.flags &= ~kFaceBitQuad;
          ++stripped_quad_materials;
        }
        material_states.emplace(current_material, current);
      }
    }

    if (vertex_map.empty()) {
      vertex_map.assign(positions.size(), kInvalidVertexIndex);
      const std::size_t vertex_capacity = std::min(positions.size(), static_cast<std::size_t>(kInvalidVertexIndex));
      geometry::reserveVertexCapacity(out.geometry, vertex_capacity);
      out.skeleton.vertex_bones.reserve(vertex_capacity);
      out.selections.vertex_masks.reserve(vertex_capacity);
    } else if (vertex_map.size() < positions.size()) {
      vertex_map.resize(positions.size(), kInvalidVertexIndex);
    }
    for (std::size_t offset = 0; offset < triangles.size(); offset += 3U) {
      std::array<std::uint32_t, 3> indices = {triangles[offset], triangles[offset + 1U], triangles[offset + 2U]};
      const ArxVector3& a = positions[corners[indices[0]].position];
      const ArxVector3& b = positions[corners[indices[1]].position];
      const ArxVector3& c = positions[corners[indices[2]].position];
      if (math::dot(math::cross(b - a, c - a), polygon_normal) < 0.0) std::swap(indices[1], indices[2]);
      const ArxVector3 face_normal = math::normalizeFiniteOr(
          math::cross(positions[corners[indices[1]].position] - a, positions[corners[indices[2]].position] - a), {});
      if (math::lengthSquared(face_normal) == 0.0)
        return objLineError(ARX_OBJ_BAD_FACE, line_number, "triangulated face is degenerate");

      Face face;
      face.texture = current.texture;
      face.flags = current.flags;
      face.transval = current.transval;
      face.normal = face_normal;
      for (std::size_t corner_index = 0; corner_index < face.corners.size(); ++corner_index) {
        const FaceCorner& source = corners[indices[corner_index]];
        VertexIndex& vertex = vertex_map[source.position];
        if (vertex == kInvalidVertexIndex) {
          if (out.geometry.vertices.size() >= static_cast<std::size_t>(kInvalidVertexIndex))
            return objLineError(ARX_OBJ_TOO_MANY_VERTICES, line_number, "face exceeds the Model vertex limit");
          vertex = static_cast<VertexIndex>(out.geometry.vertices.size());
          out.geometry.vertices.push_back({positions[source.position]});
          out.skeleton.vertex_bones.push_back(kInvalidBoneIndex);
          out.selections.vertex_masks.push_back(0);
        }
        const ArxVector2 uv = source.has_texcoord ? texcoords[source.texcoord] : ArxVector2{};
        face.corners[corner_index] = {
            .vertex = vertex,
            .normal = source.has_normal              ? normals[source.normal]
                      : current_smoothing_group == 0 ? polygon_normal
                                                     : ArxVector3{},
            .u = uv.x,
            .v = uv.y,
        };
      }
      out.geometry.faces.push_back(face);
      face_smoothing_groups.push_back(current_smoothing_group);
    }
    if (current_smoothing_group != 0) {
      for (const FaceCorner& source : corners) {
        const VertexIndex vertex = vertex_map[source.position];
        const std::uint64_t key = (static_cast<std::uint64_t>(current_smoothing_group) << 32U) | vertex;
        ArxVector3& normal = smoothed_normals[key];
        normal = normal + polygon_normal;
      }
    }
  }

  if (out.geometry.faces.empty()) return ARX_OBJ_NO_GEOMETRY;
  for (std::size_t face_index = 0; face_index < out.geometry.faces.size(); ++face_index) {
    Face& face = out.geometry.faces[face_index];
    const std::uint32_t smoothing_group = face_smoothing_groups[face_index];
    if (smoothing_group == 0) continue;
    for (Corner& corner : face.corners) {
      if (math::lengthSquared(corner.normal) != 0.0) continue;
      const std::uint64_t key = (static_cast<std::uint64_t>(smoothing_group) << 32U) | corner.vertex;
      const auto accumulated = smoothed_normals.find(key);
      corner.normal = accumulated == smoothed_normals.end() ? face.normal
                                                            : math::normalizeFiniteOr(accumulated->second, face.normal);
    }
  }

  textures::PathRepairInfo texture_repairs;
  if (textures::repairPaths(out.textures.textures, &texture_repairs) != textures::Error::kNone)
    return ARX_MODEL_BAD_TEXTURE_PATH;
  for (const textures::PathRepairInfo::Repair& repair : texture_repairs.repairs)
    log(ARX_LOG_WARN, "OBJ -> Model: texture path '{}' normalized to '{}'", repair.original, repair.repaired);

  for (ActionPoint& point : out.action_points.points) {
    const std::string original = point.name;
    const IdentifierRepair repair = action_points::repairName(point.name);
    if (repair != IdentifierRepair::kNone && repair != IdentifierRepair::kCase)
      log(ARX_LOG_INFO, "OBJ -> Model: action point '{}' normalized to '{}'", original, point.name);
  }

  if (stripped_quad_materials != 0)
    log(ARX_LOG_WARN, "OBJ -> Model: stripped QUAD flag from {} used material(s)", stripped_quad_materials);
  return ARX_OK;
}

ArxReturnCode objMaterialLibraryPaths(std::string_view obj, std::vector<std::string>& out) noexcept {
  return api_detail::statusBoundary([&]() -> ArxReturnCode {
    std::vector<std::string> result;
    std::size_t position = 0;
    std::size_t line_number = 0;
    while (position < obj.size()) {
      ++line_number;
      TextCursor line(nextLine(obj, position), "#");
      const Token keyword = line.next();
      if (keyword.kind != TokenKind::kString || keyword.text != "mtllib") continue;
      const std::size_t begin = result.size();
      for (Token path = line.next(); path.kind == TokenKind::kString; path = line.next())
        result.emplace_back(path.text);
      if (result.size() == begin)
        return objLineError(ARX_OBJ_BAD_MTL, line_number, "mtllib has no material-library path");
    }
    out = std::move(result);
    return ARX_OK;
  });
}

}  // namespace pistoris
