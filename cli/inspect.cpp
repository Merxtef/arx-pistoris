// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#include "inspect.h"

#include <cstddef>
#include <cstdint>
#include <cstdio>

static void printVertex(const pistoris::ftl::Vertex& v, std::size_t i) {
  std::printf("    [%zu] pos=%f,%f,%f norm=%f,%f,%f\n", i, v.position.x, v.position.y, v.position.z, v.normal.x,
              v.normal.y, v.normal.z);
}

static void printFace(const pistoris::ftl::Face& f, std::size_t i) {
  std::printf("    [%zu] type=%u (%hu,%hu,%hu) tex=%hu u=%f,%f,%f v=%f,%f,%f\n", i, f.type, f.vertex_idx.x,
              f.vertex_idx.y, f.vertex_idx.z, f.texture_id, f.u.x, f.u.y, f.u.z, f.v.x, f.v.y, f.v.z);
  std::printf("         transval=%f norm=%f,%f,%f\n", f.transval, f.norm.x, f.norm.y, f.norm.z);
}

static void printTexture(const pistoris::ftl::TextureContainer& tc, std::size_t i) {
  std::printf("    [%zu] %s\n", i, tc.filename);
}

static void printGroup(const pistoris::ftl::Group& g, std::size_t i) {
  std::printf("    [%zu] %s origin=%u blobShadowSize=%f\n", i, g.name, g.origin, g.blob_shadow_size);
  constexpr std::size_t kHalf = 3;
  std::printf("      (%zu) ", g.indices.size());
  if (g.indices.size() <= 2 * kHalf) {
    for (std::int32_t idx : g.indices) std::printf("%i ", idx);
  } else {
    for (std::size_t j = 0; j < kHalf; ++j) std::printf("%i ", g.indices[j]);
    std::printf("... ");
    for (std::size_t j = g.indices.size() - kHalf; j < g.indices.size(); ++j) std::printf("%i ", g.indices[j]);
  }
  std::printf("\n");
}

static void printAction(const pistoris::ftl::Action& a, std::size_t i) {
  std::printf("    [%zu] %s vertexIdx=%i action=%i sfx=%i\n", i, a.name, a.vertex_idx, a.action, a.sfx);
}

static void printSelection(const pistoris::ftl::Selection& s, std::size_t i) {
  std::printf("    [%zu] %s\n", i, s.name);
  constexpr std::size_t kHalf = 3;
  std::printf("      (%zu) ", s.selected.size());
  if (s.selected.size() <= 2 * kHalf) {
    for (std::int32_t idx : s.selected) std::printf("%i ", idx);
  } else {
    for (std::size_t j = 0; j < kHalf; ++j) std::printf("%i ", s.selected[j]);
    std::printf("... ");
    for (std::size_t j = s.selected.size() - kHalf; j < s.selected.size(); ++j) std::printf("%i ", s.selected[j]);
  }
  std::printf("\n");
}

#define PRINT_ARRAY(SINGLE, MULTIPLE, NUMBER)                                                  \
  if (data.MULTIPLE.size() <= 2 * (NUMBER))                                                    \
    for (std::size_t i = 0; i < data.MULTIPLE.size(); ++i) print##SINGLE(data.MULTIPLE[i], i); \
  else {                                                                                       \
    for (std::size_t i = 0; i < (NUMBER); ++i) print##SINGLE(data.MULTIPLE[i], i);             \
    std::printf("    ...\n");                                                                  \
    for (std::size_t i = data.MULTIPLE.size() - (NUMBER); i < data.MULTIPLE.size(); ++i)       \
      print##SINGLE(data.MULTIPLE[i], i);                                                      \
  }

static void printFtlData(const pistoris::ftl::Data& data) {
  std::printf("  header\n");
  std::printf("    origin = %u\n", data.header.origin);
  std::printf("    name   = '%s'\n", data.header.name);
  std::printf("  vertices:   %zu\n", data.vertices.size());
  PRINT_ARRAY(Vertex, vertices, 3)
  std::printf("  faces:      %zu\n", data.faces.size());
  PRINT_ARRAY(Face, faces, 2)
  std::printf("  textures:   %zu\n", data.texture_containers.size());
  PRINT_ARRAY(Texture, texture_containers, 3)
  std::printf("  groups:     %zu\n", data.groups.size());
  PRINT_ARRAY(Group, groups, 2)
  std::printf("  actions:    %zu\n", data.actions.size());
  PRINT_ARRAY(Action, actions, 2)
  std::printf("  selections: %zu\n", data.selections.size());
  PRINT_ARRAY(Selection, selections, 2)
}

static void printGroupAnim(const pistoris::tea::GroupAnim& g, std::size_t i) {
  std::printf("      [%zu] key=%i quat=%f,%f,%f,%f translate=%f,%f,%f zoom=%f,%f,%f\n", i, g.key_group, g.quat.w,
              g.quat.x, g.quat.y, g.quat.z, g.translate.x, g.translate.y, g.translate.z, g.zoom.x, g.zoom.y, g.zoom.z);
}

static void printKeyframe(const pistoris::tea::Keyframe& kf, std::size_t i) {
  std::printf("    [%zu] frame=%i flag=%i", i, kf.num_frame, kf.flag_frame);
  if (kf.translate) std::printf(" move=%f,%f,%f", kf.translate->x, kf.translate->y, kf.translate->z);
  if (kf.quat) std::printf(" quat=%f,%f,%f,%f", kf.quat->w, kf.quat->x, kf.quat->y, kf.quat->z);
  if (kf.sample) std::printf(" sample='%s'", kf.sample->name);
  std::printf("\n");

  constexpr std::size_t kHalf = 2;
  std::printf("      groups: %zu\n", kf.groups.size());
  if (kf.groups.size() <= 2 * kHalf) {
    for (std::size_t j = 0; j < kf.groups.size(); ++j) printGroupAnim(kf.groups[j], j);
  } else {
    for (std::size_t j = 0; j < kHalf; ++j) printGroupAnim(kf.groups[j], j);
    std::printf("      ...\n");
    for (std::size_t j = kf.groups.size() - kHalf; j < kf.groups.size(); ++j) printGroupAnim(kf.groups[j], j);
  }
}

static void printTeaData(const pistoris::tea::Data& data) {
  std::printf("  num_frames = %i\n", data.num_frames);
  std::printf("  num_groups = %i\n", data.num_groups);
  std::printf("  keyframes: %zu\n", data.keyframes.size());
  PRINT_ARRAY(Keyframe, keyframes, 2)
}

#undef PRINT_ARRAY

int inspectContext(const cli::model::Context& ctx) {
  std::printf("FTL\n");
  printFtlData(ctx.ftl);

  for (std::size_t i = 0; i < ctx.teas.size(); ++i) {
    std::printf("TEA[%zu]\n", i);
    printTeaData(ctx.teas[i]);
  }

  return 0;
}

int inspectContext(const cli::animation::Context& ctx) {
  std::printf("TEA\n");
  printTeaData(ctx.tea);
  return 0;
}
