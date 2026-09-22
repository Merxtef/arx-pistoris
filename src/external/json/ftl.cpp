// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#include "native/ftl.h"

#include "arx_pistoris/base/math.h"
#include "arx_pistoris/base/status.h"
#include "arx_pistoris/native/ftl.hpp"
#include "arx_pistoris/native/text.hpp"
#include "arx_pistoris/runtime/types.h"

#include "external/json.h"
#include "external/json/native_common.h"
#include "utils/log.h"
#include "utils/native_text.h"
#include "utils/return_code.h"

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace pistoris {
namespace {

using Json = json_detail::Json;

constexpr std::string_view kFtlSchema = "https://arx-tools.github.io/schemas/ftl.schema.json";

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

ArxReturnCode importFtl(std::string_view text, NativeTextMode text_mode, ftl::Data& out) {
  Json root;
  ARX_RETURN_IF_ERR(json_detail::parse(text, root));
  if (!root.is_object() || !json_detail::validSchema(root, kFtlSchema)) return ARX_JSON_BAD_SCHEMA;

  const Json* header = json_detail::member(root, "header");
  if (!header) return ARX_JSON_BAD_SCHEMA;
  const Json* origin = json_detail::member(*header, "origin");
  const Json* name = json_detail::member(*header, "name");
  std::string text_value;
  if (!origin || !name || !json_detail::getUnsigned(*origin, out.header.origin) ||
      !json_detail::getString(*name, text_value)) {
    return ARX_JSON_BAD_SCHEMA;
  }
  if (!json_detail::encodeTruncated(text_value, text_mode, out.header.name)) return ARX_JSON_BAD_SCHEMA;

  const Json* vertices = nullptr;
  ARX_RETURN_IF_ERR(json_detail::arrayMember(root, "vertices", vertices, kFtlMaxVertices));
  out.vertices.reserve(vertices->size());
  for (const Json& json : *vertices) {
    ftl::Vertex vertex;
    const Json* vector = json_detail::member(json, "vector");
    const Json* normal = json_detail::member(json, "norm");
    if (!vector || !normal || !json_detail::getVector(*vector, vertex.position) ||
        !json_detail::getVector(*normal, vertex.normal)) {
      return ARX_JSON_BAD_SCHEMA;
    }
    out.vertices.push_back(vertex);
  }

  const Json* faces = nullptr;
  ARX_RETURN_IF_ERR(json_detail::arrayMember(root, "faces", faces, kFtlMaxFaces));
  out.faces.reserve(faces->size());
  for (const Json& json : *faces) {
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
      return ARX_JSON_BAD_SCHEMA;
    }
    const Json* transparency = json_detail::member(json, "transval");
    if (transparency && !json_detail::getFloat(*transparency, face.transval)) return ARX_JSON_BAD_SCHEMA;
    out.faces.push_back(face);
  }

  const Json* texture_containers = nullptr;
  ARX_RETURN_IF_ERR(json_detail::arrayMember(root, "textureContainers", texture_containers, kFtlMaxTextures));
  out.texture_containers.reserve(texture_containers->size());
  for (const Json& json : *texture_containers) {
    ftl::TextureContainer texture{};
    const Json* filename = json_detail::member(json, "filename");
    if (!filename || !json_detail::getString(*filename, text_value)) return ARX_JSON_BAD_SCHEMA;
    if (!json_detail::encodeTruncated(text_value, text_mode, texture.filename)) return ARX_JSON_BAD_SCHEMA;
    out.texture_containers.push_back(texture);
  }

  const Json* groups = nullptr;
  ARX_RETURN_IF_ERR(json_detail::arrayMember(root, "groups", groups, kFtlMaxGroups));
  out.groups.reserve(groups->size());
  for (const Json& json : *groups) {
    ftl::Group group;
    name = json_detail::member(json, "name");
    origin = json_detail::member(json, "origin");
    const Json* indices = json_detail::member(json, "indices");
    const Json* blob_shadow_size = json_detail::member(json, "blobShadowSize");
    if (!name || !origin || !indices || !blob_shadow_size || !json_detail::getString(*name, text_value) ||
        !json_detail::getUnsigned(*origin, group.origin) ||
        !json_detail::getFloat(*blob_shadow_size, group.blob_shadow_size)) {
      return ARX_JSON_BAD_SCHEMA;
    }
    ARX_RETURN_IF_ERR(getInt32Array(*indices, group.indices, out.vertices.size()));
    if (!json_detail::encodeTruncated(text_value, text_mode, group.name)) return ARX_JSON_BAD_SCHEMA;
    out.groups.push_back(std::move(group));
  }

  const Json* actions = nullptr;
  ARX_RETURN_IF_ERR(json_detail::arrayMember(root, "actions", actions, kFtlMaxActions));
  out.actions.reserve(actions->size());
  for (const Json& json : *actions) {
    ftl::Action action{};
    name = json_detail::member(json, "name");
    const Json* vertex_index = json_detail::member(json, "vertexIdx");
    const Json* action_type = json_detail::member(json, "action");
    const Json* sfx = json_detail::member(json, "sfx");
    if (!name || !vertex_index || !action_type || !sfx || !json_detail::getString(*name, text_value) ||
        !json_detail::getSigned(*vertex_index, action.vertex_idx) ||
        !json_detail::getSigned(*action_type, action.action) || !json_detail::getSigned(*sfx, action.sfx)) {
      return ARX_JSON_BAD_SCHEMA;
    }
    if (!json_detail::encodeTruncated(text_value, text_mode, action.name)) return ARX_JSON_BAD_SCHEMA;
    out.actions.push_back(action);
  }

  const Json* selections = nullptr;
  ARX_RETURN_IF_ERR(json_detail::arrayMember(root, "selections", selections, kFtlMaxSelections));
  out.selections.reserve(selections->size());
  for (const Json& json : *selections) {
    ftl::Selection selection;
    name = json_detail::member(json, "name");
    const Json* selected = json_detail::member(json, "selected");
    if (!name || !selected || !json_detail::getString(*name, text_value)) return ARX_JSON_BAD_SCHEMA;
    ARX_RETURN_IF_ERR(getInt32Array(*selected, selection.selected, out.vertices.size()));
    if (!json_detail::encodeTruncated(text_value, text_mode, selection.name)) return ARX_JSON_BAD_SCHEMA;
    out.selections.push_back(std::move(selection));
  }

  ARX_RETURN_IF_ERR(canonicalizeFtl(&out));
  ARX_RETURN_IF_ERR(validateFtl(&out));
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

ArxReturnCode exportFtlToJson(const ftl::Data& data, bool pretty, NativeTextMode text_mode, std::string& out) {
  return json_detail::guarded("FTL export", [&]() -> ArxReturnCode {
    if (!native_text::validMode(text_mode)) return ARX_INVALID_OPTIONS;
    ARX_RETURN_IF_ERR(validateFtl(&data));

    std::string decoded;
    if (!json_detail::decodeFixed(data.header.name, text_mode, decoded)) return ARX_JSON_BAD_SCHEMA;
    Json root;
    root["$schema"] = kFtlSchema;
    root["header"] = {{"origin", data.header.origin}, {"name", decoded}};

    root["vertices"] = Json::array();
    for (const ftl::Vertex& vertex : data.vertices) {
      root["vertices"].push_back(
          {{"vector", json_detail::vector(vertex.position)}, {"norm", json_detail::vector(vertex.normal)}});
    }

    root["faces"] = Json::array();
    for (const ftl::Face& face : data.faces) {
      root["faces"].push_back({{"faceType", face.type},
                               {"vertexIdx", {face.vertex_idx.x, face.vertex_idx.y, face.vertex_idx.z}},
                               {"textureIdx", face.texture_id},
                               {"u", vectorArray(face.u)},
                               {"v", vectorArray(face.v)},
                               {"transval", face.transval},
                               {"norm", json_detail::vector(face.norm)}});
    }

    root["textureContainers"] = Json::array();
    for (const ftl::TextureContainer& texture : data.texture_containers) {
      if (!json_detail::decodeFixed(texture.filename, text_mode, decoded)) return ARX_FTL_BAD_TEXTURE_PATH;
      root["textureContainers"].push_back({{"filename", decoded}});
    }

    root["groups"] = Json::array();
    for (const ftl::Group& group : data.groups) {
      if (!json_detail::decodeFixed(group.name, text_mode, decoded)) return ARX_FTL_BAD_GROUP_NAME;
      root["groups"].push_back({{"name", decoded},
                                {"origin", group.origin},
                                {"indices", group.indices},
                                {"blobShadowSize", group.blob_shadow_size}});
    }

    root["actions"] = Json::array();
    for (const ftl::Action& action : data.actions) {
      if (!json_detail::decodeFixed(action.name, text_mode, decoded)) return ARX_FTL_BAD_ACTION_NAME;
      root["actions"].push_back(
          {{"name", decoded}, {"vertexIdx", action.vertex_idx}, {"action", action.action}, {"sfx", action.sfx}});
    }

    root["selections"] = Json::array();
    for (const ftl::Selection& selection : data.selections) {
      if (!json_detail::decodeFixed(selection.name, text_mode, decoded)) return ARX_FTL_BAD_SELECTION_NAME;
      root["selections"].push_back({{"name", decoded}, {"selected", selection.selected}});
    }

    return json_detail::dump(root, pretty, out);
  });
}

ArxReturnCode importJsonToFtl(std::string_view text, NativeTextMode text_mode, ftl::Data* out) {
  if (!out) return ARX_INVALID_DATA_POINTER;
  return json_detail::guarded("FTL import", [&]() -> ArxReturnCode {
    if (!native_text::validMode(text_mode)) return ARX_INVALID_OPTIONS;
    ftl::Data temporary;
    const ArxReturnCode rc = importFtl(text, text_mode, temporary);
    if (rc == ARX_OK) *out = std::move(temporary);
    return rc;
  });
}

}  // namespace pistoris
