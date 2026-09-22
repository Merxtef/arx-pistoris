// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#pragma once

#include "arx_pistoris/base/math.hpp"
#include "arx_pistoris/native/ftl.hpp"
#include "arx_pistoris/pistoris.hpp"

#include <algorithm>
#include <cstdint>
#include <cstring>
#include <string_view>
#include <vector>

inline void setFtlName(std::string_view name, char* out, std::size_t capacity) {
  const std::size_t size = std::min(name.size(), capacity - 1);
  if (size != 0) std::memcpy(out, name.data(), size);
  out[size] = '\0';
}

inline pistoris::Ftl makeSemanticModelFtl() {
  using namespace pistoris;

  Ftl result;
  result.header.origin = 0;
  result.vertices = {
      {{10.0f, 20.0f, 30.0f}, {0.0f, 0.0f, 1.0f}},
      {{10.0f, 20.0f, 30.0f}, {0.0f, 0.0f, 1.0f}},
      {{11.0f, 20.0f, 30.0f}, {0.0f, 0.0f, 1.0f}},
      {{10.0f, 21.0f, 30.0f}, {0.0f, 0.0f, 1.0f}},
      {{10.0f, 20.0f, 30.0f}, {0.0f, 0.0f, 1.0f}},
      {{10.0f, 21.0f, 30.0f}, {0.0f, 0.0f, 1.0f}},
      {{11.0f, 20.0f, 30.0f}, {0.0f, 0.0f, 1.0f}},
      {{10.5f, 20.5f, 30.0f}, {0.0f, 0.0f, 1.0f}},
  };

  ftl::Face face;
  face.vertex_idx = {1, 2, 3};
  face.texture_id = 0;
  face.u = {0.0f, 1.0f, 0.0f};
  face.v = {0.0f, 0.0f, 1.0f};
  face.norm = {1.0f, 0.0f, 0.0f};
  result.faces.push_back(face);

  result.texture_containers.emplace_back();
  setFtlName("graph/obj3d/textures/my_tex",
             result.texture_containers[0].filename,
             sizeof(result.texture_containers[0].filename));

  result.groups.resize(2);
  setFtlName("root", result.groups[0].name, sizeof(result.groups[0].name));
  result.groups[0].origin = 4;
  result.groups[0].indices = {0, 1, 2, 3, 4, 5, 6, 7};
  result.groups[0].blob_shadow_size = 2.0f;
  setFtlName("chest", result.groups[1].name, sizeof(result.groups[1].name));
  result.groups[1].origin = 5;
  result.groups[1].indices = {2, 5, 6, 7};
  result.groups[1].blob_shadow_size = 1.0f;

  result.actions.emplace_back();
  setFtlName("view_attach", result.actions[0].name, sizeof(result.actions[0].name));
  result.actions[0].vertex_idx = 6;

  const auto add_selection = [&](std::string_view name, std::vector<std::int32_t> selected) {
    result.selections.emplace_back();
    setFtlName(name, result.selections.back().name, sizeof(result.selections.back().name));
    result.selections.back().selected = std::move(selected);
  };
  add_selection("head", {0, 1, 4, 6});
  add_selection("chest", {2, 5});
  add_selection("1st", {1, 2});
  add_selection("cut_head", {7, 0, 1, 4, 6});
  add_selection("head", {2});
  add_selection("unused_authoring_selection", {3});

  return result;
}
