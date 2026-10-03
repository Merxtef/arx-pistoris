// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#include "native/ftl.h"

#include "arx_pistoris/base/location.hpp"
#include "arx_pistoris/base/math.h"
#include "arx_pistoris/base/status.h"
#include "arx_pistoris/json/location.hpp"
#include "arx_pistoris/native/ftl.hpp"
#include "arx_pistoris/native/location.hpp"
#include "arx_pistoris/native/text.hpp"
#include "arx_pistoris/runtime/types.h"

#include "api/result_failure.h"
#include "external/json.h"
#include "external/json/native_common.h"
#include "utils/log.h"
#include "utils/math/finite.h"
#include "utils/native_text.h"
#include "utils/return_code.h"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace pistoris {
namespace {

using Json = json_detail::Json;

constexpr std::string_view kFtlSchema = "https://arx-tools.github.io/schemas/ftl.schema.json";

std::string_view jsonField(FtlElement element, std::string_view field) {
  if (element == FtlElement::kFace) {
    if (field == "type") return "faceType";
    if (field == "vertex_idx") return "vertexIdx";
    if (field == "texture_id") return "textureIdx";
  }
  if (element == FtlElement::kBone && field == "blob_shadow_size") return "blobShadowSize";
  if (element == FtlElement::kActionPoint && field == "vertex_idx") return "vertexIdx";
  return field;
}

std::string jsonPointer(const FtlLocation& location) {
  if (location.element == FtlElement::kHeader) {
    if (location.field == "vertices") return "/vertices";
    if (location.field == "faces") return "/faces";
    if (location.field == "texture_containers") return "/textureContainers";
    if (location.field == "groups") return "/groups";
    if (location.field == "actions") return "/actions";
    if (location.field == "selections") return "/selections";
    return location.field.empty() ? "/header" : "/header/" + std::string(jsonField(location.element, location.field));
  }
  if (location.index == kNoElementIndex) return {};
  std::string_view array;
  switch (location.element) {
    case FtlElement::kVertex:
      array = "/vertices";
      break;
    case FtlElement::kFace:
      array = "/faces";
      break;
    case FtlElement::kTexture:
      array = "/textureContainers";
      break;
    case FtlElement::kBone:
      array = "/groups";
      break;
    case FtlElement::kActionPoint:
      array = "/actions";
      break;
    case FtlElement::kSelection:
      array = "/selections";
      break;
    case FtlElement::kHeader:
      return "/header";
  }
  std::string result = json_detail::indexedPointer(array, location.index);
  if (location.element == FtlElement::kVertex && location.field == "position")
    result += "/vector";
  else if (location.element == FtlElement::kVertex && location.field == "normal")
    result += "/norm";
  else if (!location.field.empty())
    result += "/" + std::string(jsonField(location.element, location.field));
  if (location.subindex != kNoElementIndex) result += "/" + std::to_string(location.subindex);
  return result;
}

Json vectorArray(const ArxVector3& value) { return Json::array({value.x, value.y, value.z}); }

bool getVectorArray(const Json& json, ArxVector3& out) {
  return json.is_array() && json.size() == 3 && json_detail::getFloat(json[0], out.x) &&
         json_detail::getFloat(json[1], out.y) && json_detail::getFloat(json[2], out.z);
}

ArxReturnCode getInt32Array(const Json& json, std::vector<std::int32_t>& out, std::size_t limit) {
  if (!json.is_array()) return ARX_JSON_BAD_SCHEMA;
  if (json.size() > limit) return ARX_JSON_LIMIT_EXCEEDED;
  out.clear();
  out.reserve(json.size());
  for (const Json& item : json) {
    std::int32_t value = 0;
    if (!json_detail::getSigned(item, value)) return ARX_JSON_BAD_SCHEMA;
    out.push_back(value);
  }
  return ARX_OK;
}

ArxReturnCode importFtl(std::string_view text, NativeTextMode text_mode, ftl::Data& out,
                        std::optional<JsonLocation>& failure_location) {
  Json root;
  ARX_RETURN_IF_ERR(json_detail::parse(text, root, failure_location));
  if (!root.is_object()) return json_detail::schemaFailure(failure_location, {});
  if (!json_detail::validSchema(root, kFtlSchema)) return json_detail::schemaFailure(failure_location, "/$schema");

  const Json* header = json_detail::member(root, "header");
  if (!header) return json_detail::schemaFailure(failure_location, "/header");
  const Json* origin = json_detail::member(*header, "origin");
  const Json* name = json_detail::member(*header, "name");
  std::string text_value;
  if (!origin || !name || !json_detail::getUnsigned(*origin, out.header.origin) ||
      !json_detail::getString(*name, text_value)) {
    return json_detail::schemaFailure(failure_location, "/header");
  }
  if (!json_detail::encodeTruncated(text_value, text_mode, out.header.name))
    return json_detail::schemaFailure(failure_location, "/header/name");

  const Json* vertices = nullptr;
  ArxReturnCode code = json_detail::arrayMember(root, "vertices", vertices, kFtlMaxVertices);
  if (code != ARX_OK) return json_detail::schemaFailure(failure_location, "/vertices", code);
  out.vertices.reserve(vertices->size());
  for (std::size_t index = 0; index < vertices->size(); ++index) {
    const Json& json = (*vertices)[index];
    ftl::Vertex vertex;
    const Json* vector = json_detail::member(json, "vector");
    const Json* normal = json_detail::member(json, "norm");
    if (!vector || !normal || !json_detail::getVector(*vector, vertex.position) ||
        !json_detail::getVector(*normal, vertex.normal)) {
      return json_detail::schemaFailure(failure_location, json_detail::indexedPointer("/vertices", index));
    }
    out.vertices.push_back(vertex);
  }

  const Json* faces = nullptr;
  code = json_detail::arrayMember(root, "faces", faces, kFtlMaxFaces);
  if (code != ARX_OK) return json_detail::schemaFailure(failure_location, "/faces", code);
  out.faces.reserve(faces->size());
  for (std::size_t index = 0; index < faces->size(); ++index) {
    const Json& json = (*faces)[index];
    const std::string pointer = json_detail::indexedPointer("/faces", index);
    ftl::Face face;
    const Json* face_type = json_detail::member(json, "faceType");
    const Json* vertex_indices = json_detail::member(json, "vertexIdx");
    const Json* texture_index = json_detail::member(json, "textureIdx");
    const Json* u = json_detail::member(json, "u");
    const Json* v = json_detail::member(json, "v");
    const Json* normal = json_detail::member(json, "norm");
    if (!face_type || !vertex_indices || !texture_index || !u || !v || !normal ||
        !json_detail::getUnsigned(*face_type, face.type) || !vertex_indices->is_array() ||
        vertex_indices->size() != 3 || !json_detail::getUnsigned((*vertex_indices)[0], face.vertex_idx.x) ||
        !json_detail::getUnsigned((*vertex_indices)[1], face.vertex_idx.y) ||
        !json_detail::getUnsigned((*vertex_indices)[2], face.vertex_idx.z) ||
        !json_detail::getSigned(*texture_index, face.texture_id) || !getVectorArray(*u, face.u) ||
        !getVectorArray(*v, face.v) || !json_detail::getVector(*normal, face.norm)) {
      return json_detail::schemaFailure(failure_location, pointer);
    }
    const Json* transparency = json_detail::member(json, "transval");
    if (transparency && !json_detail::getFloat(*transparency, face.transval))
      return json_detail::schemaFailure(failure_location, pointer + "/transval");
    out.faces.push_back(face);
  }

  const Json* texture_containers = nullptr;
  code = json_detail::arrayMember(root, "textureContainers", texture_containers, kFtlMaxTextures);
  if (code != ARX_OK) return json_detail::schemaFailure(failure_location, "/textureContainers", code);
  out.texture_containers.reserve(texture_containers->size());
  for (std::size_t index = 0; index < texture_containers->size(); ++index) {
    const Json& json = (*texture_containers)[index];
    const std::string pointer = json_detail::indexedPointer("/textureContainers", index);
    ftl::TextureContainer texture{};
    const Json* filename = json_detail::member(json, "filename");
    if (!filename || !json_detail::getString(*filename, text_value))
      return json_detail::schemaFailure(failure_location, pointer + "/filename");
    if (!json_detail::encodeTruncated(text_value, text_mode, texture.filename))
      return json_detail::schemaFailure(failure_location, pointer + "/filename");
    out.texture_containers.push_back(texture);
  }

  const Json* groups = nullptr;
  code = json_detail::arrayMember(root, "groups", groups, kFtlMaxGroups);
  if (code != ARX_OK) return json_detail::schemaFailure(failure_location, "/groups", code);
  out.groups.reserve(groups->size());
  for (std::size_t index = 0; index < groups->size(); ++index) {
    const Json& json = (*groups)[index];
    const std::string pointer = json_detail::indexedPointer("/groups", index);
    ftl::Group group;
    name = json_detail::member(json, "name");
    origin = json_detail::member(json, "origin");
    const Json* indices = json_detail::member(json, "indices");
    const Json* blob_shadow_size = json_detail::member(json, "blobShadowSize");
    if (!name || !origin || !indices || !blob_shadow_size || !json_detail::getString(*name, text_value) ||
        !json_detail::getUnsigned(*origin, group.origin) ||
        !json_detail::getFloat(*blob_shadow_size, group.blob_shadow_size)) {
      return json_detail::schemaFailure(failure_location, pointer);
    }
    code = getInt32Array(*indices, group.indices, out.vertices.size());
    if (code != ARX_OK) return json_detail::schemaFailure(failure_location, pointer + "/indices", code);
    if (!json_detail::encodeTruncated(text_value, text_mode, group.name))
      return json_detail::schemaFailure(failure_location, pointer + "/name");
    out.groups.push_back(std::move(group));
  }

  const Json* actions = nullptr;
  code = json_detail::arrayMember(root, "actions", actions, kFtlMaxActions);
  if (code != ARX_OK) return json_detail::schemaFailure(failure_location, "/actions", code);
  out.actions.reserve(actions->size());
  for (std::size_t index = 0; index < actions->size(); ++index) {
    const Json& json = (*actions)[index];
    const std::string pointer = json_detail::indexedPointer("/actions", index);
    ftl::Action action{};
    name = json_detail::member(json, "name");
    const Json* vertex_index = json_detail::member(json, "vertexIdx");
    const Json* action_type = json_detail::member(json, "action");
    const Json* sfx = json_detail::member(json, "sfx");
    if (!name || !vertex_index || !action_type || !sfx || !json_detail::getString(*name, text_value) ||
        !json_detail::getSigned(*vertex_index, action.vertex_idx) ||
        !json_detail::getSigned(*action_type, action.action) || !json_detail::getSigned(*sfx, action.sfx)) {
      return json_detail::schemaFailure(failure_location, pointer);
    }
    if (!json_detail::encodeTruncated(text_value, text_mode, action.name))
      return json_detail::schemaFailure(failure_location, pointer + "/name");
    out.actions.push_back(action);
  }

  const Json* selections = nullptr;
  code = json_detail::arrayMember(root, "selections", selections, kFtlMaxSelections);
  if (code != ARX_OK) return json_detail::schemaFailure(failure_location, "/selections", code);
  out.selections.reserve(selections->size());
  for (std::size_t index = 0; index < selections->size(); ++index) {
    const Json& json = (*selections)[index];
    const std::string pointer = json_detail::indexedPointer("/selections", index);
    ftl::Selection selection;
    name = json_detail::member(json, "name");
    const Json* selected = json_detail::member(json, "selected");
    if (!name || !selected || !json_detail::getString(*name, text_value))
      return json_detail::schemaFailure(failure_location, pointer);
    code = getInt32Array(*selected, selection.selected, out.vertices.size());
    if (code != ARX_OK) return json_detail::schemaFailure(failure_location, pointer + "/selected", code);
    if (!json_detail::encodeTruncated(text_value, text_mode, selection.name))
      return json_detail::schemaFailure(failure_location, pointer + "/name");
    out.selections.push_back(std::move(selection));
  }

  FtlLocation native_location;
  if (const ArxReturnCode rc = canonicalizeFtl(&out, &native_location); rc != ARX_OK)
    return json_detail::schemaFailureIfUnknown(failure_location, jsonPointer(native_location), rc);
  if (const ArxReturnCode rc = validateFtl(&out, &native_location); rc != ARX_OK)
    return json_detail::schemaFailureIfUnknown(failure_location, jsonPointer(native_location), rc);
  log(ARX_LOG_INFO,
      "FTL JSON loaded: {} vertices, {} faces, {} textures, {} groups, {} actions, {} selections",
      out.vertices.size(),
      out.faces.size(),
      out.texture_containers.size(),
      out.groups.size(),
      out.actions.size(),
      out.selections.size());
  return ARX_OK;
}

}  // namespace

FtlResult<std::string> exportFtlToJson(const ftl::Data& data, bool pretty, NativeTextMode text_mode) {
  std::string output;
  std::optional<FtlLocation> location;
  const ArxReturnCode code = json_detail::guarded([&]() -> ArxReturnCode {
    if (!native_text::validMode(text_mode)) return ARX_INVALID_OPTIONS;
    FtlLocation validation_location;
    if (const ArxReturnCode rc = validateFtl(&data, &validation_location); rc != ARX_OK) {
      location = std::move(validation_location);
      return rc;
    }

    std::string decoded;
    if (!json_detail::decodeFixed(data.header.name, text_mode, decoded)) {
      location = FtlLocation{.element = FtlElement::kHeader, .field = "name"};
      return ARX_JSON_BAD_SCHEMA;
    }
    Json root;
    root["$schema"] = kFtlSchema;
    root["header"] = {{"origin", data.header.origin}, {"name", decoded}};

    root["vertices"] = Json::array();
    for (std::size_t vertex_index = 0; vertex_index < data.vertices.size(); ++vertex_index) {
      const ftl::Vertex& vertex = data.vertices[vertex_index];
      if (!math::finite(vertex.position)) {
        location = FtlLocation{.element = FtlElement::kVertex, .index = vertex_index, .field = "position"};
        return ARX_JSON_UNREPRESENTABLE_VALUE;
      }
      if (!math::finite(vertex.normal)) {
        location = FtlLocation{.element = FtlElement::kVertex, .index = vertex_index, .field = "normal"};
        return ARX_JSON_UNREPRESENTABLE_VALUE;
      }
      root["vertices"].push_back(
          {{"vector", json_detail::vector(vertex.position)}, {"norm", json_detail::vector(vertex.normal)}});
    }

    root["faces"] = Json::array();
    for (std::size_t face_index = 0; face_index < data.faces.size(); ++face_index) {
      const ftl::Face& face = data.faces[face_index];
      if (!math::finite(face.u) || !math::finite(face.v) || !math::finite(face.transval) || !math::finite(face.norm)) {
        std::string field = !math::finite(face.u)      ? "u"
                            : !math::finite(face.v)    ? "v"
                            : !math::finite(face.norm) ? "norm"
                                                       : "transval";
        location = FtlLocation{.element = FtlElement::kFace, .index = face_index, .field = std::move(field)};
        return ARX_JSON_UNREPRESENTABLE_VALUE;
      }
      root["faces"].push_back({{"faceType", face.type},
                               {"vertexIdx", {face.vertex_idx.x, face.vertex_idx.y, face.vertex_idx.z}},
                               {"textureIdx", face.texture_id},
                               {"u", vectorArray(face.u)},
                               {"v", vectorArray(face.v)},
                               {"transval", face.transval},
                               {"norm", json_detail::vector(face.norm)}});
    }

    root["textureContainers"] = Json::array();
    for (std::size_t texture_index = 0; texture_index < data.texture_containers.size(); ++texture_index) {
      const ftl::TextureContainer& texture = data.texture_containers[texture_index];
      if (!json_detail::decodeFixed(texture.filename, text_mode, decoded)) {
        location = FtlLocation{.element = FtlElement::kTexture, .index = texture_index, .field = "filename"};
        return ARX_FTL_BAD_TEXTURE_PATH;
      }
      root["textureContainers"].push_back({{"filename", decoded}});
    }

    root["groups"] = Json::array();
    for (std::size_t group_index = 0; group_index < data.groups.size(); ++group_index) {
      const ftl::Group& group = data.groups[group_index];
      if (!json_detail::decodeFixed(group.name, text_mode, decoded)) {
        location = FtlLocation{.element = FtlElement::kBone, .index = group_index, .field = "name"};
        return ARX_FTL_BAD_GROUP_NAME;
      }
      if (!math::finite(group.blob_shadow_size)) {
        location = FtlLocation{.element = FtlElement::kBone, .index = group_index, .field = "blob_shadow_size"};
        return ARX_JSON_UNREPRESENTABLE_VALUE;
      }
      root["groups"].push_back({{"name", decoded},
                                {"origin", group.origin},
                                {"indices", group.indices},
                                {"blobShadowSize", group.blob_shadow_size}});
    }

    root["actions"] = Json::array();
    for (std::size_t action_index = 0; action_index < data.actions.size(); ++action_index) {
      const ftl::Action& action = data.actions[action_index];
      if (!json_detail::decodeFixed(action.name, text_mode, decoded)) {
        location = FtlLocation{.element = FtlElement::kActionPoint, .index = action_index, .field = "name"};
        return ARX_FTL_BAD_ACTION_NAME;
      }
      root["actions"].push_back(
          {{"name", decoded}, {"vertexIdx", action.vertex_idx}, {"action", action.action}, {"sfx", action.sfx}});
    }

    root["selections"] = Json::array();
    for (std::size_t selection_index = 0; selection_index < data.selections.size(); ++selection_index) {
      const ftl::Selection& selection = data.selections[selection_index];
      if (!json_detail::decodeFixed(selection.name, text_mode, decoded)) {
        location = FtlLocation{.element = FtlElement::kSelection, .index = selection_index, .field = "name"};
        return ARX_FTL_BAD_SELECTION_NAME;
      }
      root["selections"].push_back({{"name", decoded}, {"selected", selection.selected}});
    }

    return json_detail::dump(root, pretty, output);
  });
  if (code != ARX_OK) return api_detail::ftlFailure<std::string>(code, std::move(location));
  return FtlResult<std::string>::success(std::move(output));
}

JsonResult<ftl::Data> importJsonToFtl(std::string_view text, NativeTextMode text_mode) {
  ftl::Data result;
  std::optional<JsonLocation> location;
  const ArxReturnCode code = json_detail::guarded([&]() -> ArxReturnCode {
    if (!native_text::validMode(text_mode)) return ARX_INVALID_OPTIONS;
    return importFtl(text, text_mode, result, location);
  });
  if (code != ARX_OK) return api_detail::jsonFailure<ftl::Data>(code, std::move(location));
  return JsonResult<ftl::Data>::success(std::move(result));
}

}  // namespace pistoris
