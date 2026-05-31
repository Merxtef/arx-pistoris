// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#pragma once

#include "doctest/doctest.h"

#include "arx_pistoris/ftl_data.hpp"
#include "arx_pistoris/tea_data.hpp"

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <vector>

// tests assume default offset_3d_data = kFtlNVertsOff (544)
constexpr int32_t kFtlSectionPtrsOff = 520;  // int32[6]
constexpr int32_t kFtlNVertsOff      = 544;  // default offset_3d_data
constexpr int32_t kFtlNFacesOff      = 548;
constexpr int32_t kFtlNTexOff        = 552;
constexpr int32_t kFtlNGroupsOff     = 556;
constexpr int32_t kFtlNActionsOff    = 560;
constexpr int32_t kFtlNSelsOff       = 564;
constexpr int32_t kFtlHeaderOff      = 568;
constexpr int32_t kFtlDataOff        = 828;

constexpr std::size_t kFtlVertexSize      = 56;
constexpr std::size_t kFtlFaceSize        = 116;
constexpr std::size_t kFtlTextureSize     = 256;
constexpr std::size_t kFtlGroupHeaderSize = 272;
constexpr std::size_t kFtlSelHeaderSize   = 72;
constexpr std::size_t kFtlActionSize      = 268;

constexpr std::size_t kFtlFaceOffVertIdx   = 16;
constexpr std::size_t kFtlFaceOffTexId     = 22;
constexpr std::size_t kFtlGroupOffOrigin   = 256;
constexpr std::size_t kFtlGroupOffIdxCount = 260;
constexpr std::size_t kFtlSelOffIdxCount   = 64;
constexpr std::size_t kFtlActionOffVertIdx = 256;

inline pistoris::ftl::Data makeData(int n = 1) {
  pistoris::ftl::Data d;
  d.header.origin = 0;
  for (int i = 0; i < n; ++i) d.vertices.push_back({{float(i), 0.0f, 0.0f}, {0.0f, 0.0f, 1.0f}});
  return d;
}

inline pistoris::ftl::Face makeFace(std::uint16_t v0, std::uint16_t v1, std::uint16_t v2,
                                    std::int16_t tex_id = pistoris::kFtlTextureNone) {
  pistoris::ftl::Face f;
  f.vertex_idx = {v0, v1, v2};
  f.texture_id = tex_id;
  return f;
}

// minimal valid: 1 zeroed vertex, no faces
inline std::vector<uint8_t> makeMinimalFtl() {
  std::vector<uint8_t> buf(kFtlDataOff + kFtlVertexSize, 0);
  std::memcpy(buf.data(), pistoris::kFtlMagic, 4);
  std::memcpy(buf.data() + 4, &pistoris::kFtlVersion, 4);
  int32_t sec0 = kFtlNVertsOff, sec_n = -1;
  std::memcpy(buf.data() + kFtlSectionPtrsOff, &sec0, 4);
  for (int i = 1; i < 6; ++i) std::memcpy(buf.data() + kFtlSectionPtrsOff + i * 4, &sec_n, 4);
  int32_t one = 1;
  std::memcpy(buf.data() + kFtlNVertsOff, &one, 4);
  return buf;
}

inline std::vector<uint8_t> makeTriangleFtlWithTexture() {
  std::vector<uint8_t> buf = makeMinimalFtl();
  buf.resize(kFtlDataOff + 3 * kFtlVertexSize + kFtlFaceSize + kFtlTextureSize, 0);

  int32_t three = 3, one = 1;
  std::memcpy(buf.data() + kFtlNVertsOff, &three, 4);
  std::memcpy(buf.data() + kFtlNFacesOff, &one, 4);
  std::memcpy(buf.data() + kFtlNTexOff, &one, 4);

  std::size_t fbase = kFtlDataOff + 3 * kFtlVertexSize;

  uint16_t idx[3] = {0, 1, 2};
  std::memcpy(buf.data() + fbase + kFtlFaceOffVertIdx, idx, sizeof(idx));

  uint16_t tex_id = 0;
  std::memcpy(buf.data() + fbase + kFtlFaceOffTexId, &tex_id, sizeof(tex_id));

  const char* filename = "GRAPH\\OBJ3D\\BODY.BMP";
  std::memcpy(buf.data() + fbase + kFtlFaceSize, filename, std::strlen(filename) + 1);

  return buf;
}

inline std::vector<uint8_t> makeTriangleFtlWithFlags(uint32_t face_type) {
  std::vector<uint8_t> buf = makeTriangleFtlWithTexture();
  std::size_t fbase        = kFtlDataOff + 3 * kFtlVertexSize;
  std::memcpy(buf.data() + fbase, &face_type, 4);
  return buf;
}

constexpr int32_t kTeaVersionOff   = 20;
constexpr int32_t kTeaNumFramesOff = 280;
constexpr int32_t kTeaNumGroupsOff = 284;
constexpr int32_t kTeaNumKfOff     = 288;
constexpr int32_t kTeaHeaderSize   = 292;

// v2014 kf header: 32 | v2015: +info_frame[256] -> 288
constexpr std::size_t kTeaKf2014Size     = 32;
constexpr std::size_t kTeaKf2015Size     = 288;
constexpr std::size_t kTeaKfFlagFrameOff = 4;
constexpr std::size_t kTeaKfKeyMoveOff   = 16;
constexpr std::size_t kTeaKfKeyOrientOff = 20;
constexpr std::size_t kTeaKfKeyMorphOff  = 24;

constexpr std::size_t kTeaGroupAnimSize   = 52;
constexpr std::size_t kTeaSampleBlockSize = 260;

inline std::vector<uint8_t> makeMinimalTea(uint32_t version = pistoris::kTeaVersion) {
  std::vector<uint8_t> buf(static_cast<std::size_t>(kTeaHeaderSize), 0);
  std::memcpy(buf.data(), pistoris::kTeaMagic, sizeof(pistoris::kTeaMagic));
  std::memcpy(buf.data() + kTeaVersionOff, &version, 4);
  return buf;
}

inline void setNumKeyframes(std::vector<uint8_t>& buf, int32_t n) { std::memcpy(buf.data() + kTeaNumKfOff, &n, 4); }

// optional payload sections are appended when key_move/key_orient/key_morph != 0
inline void appendKeyframe2014(std::vector<uint8_t>& buf, int32_t num_groups = 0,
                               int32_t flag_frame = pistoris::kTeaFlagFrameNone, int32_t key_move = 0,
                               int32_t key_orient = 0, int32_t key_morph = 0, int32_t num_frame = 0) {
  std::size_t base = buf.size();
  buf.insert(buf.end(), kTeaKf2014Size, 0);
  std::memcpy(buf.data() + base + 0, &num_frame, 4);
  std::memcpy(buf.data() + base + kTeaKfFlagFrameOff, &flag_frame, 4);
  std::memcpy(buf.data() + base + kTeaKfKeyMoveOff, &key_move, 4);
  std::memcpy(buf.data() + base + kTeaKfKeyOrientOff, &key_orient, 4);
  std::memcpy(buf.data() + base + kTeaKfKeyMorphOff, &key_morph, 4);

  if (key_move != 0) buf.insert(buf.end(), 12, 0);        // ArxVector3
  if (key_orient != 0) buf.insert(buf.end(), 8 + 16, 0);  // THEO_ANGLE + ArxQuat
  if (key_morph != 0) buf.insert(buf.end(), 16, 0);       // THEA_MORPH

  buf.insert(buf.end(), static_cast<std::size_t>(num_groups) * kTeaGroupAnimSize, 0);

  int32_t no_sample = -1;
  auto* p           = reinterpret_cast<const uint8_t*>(&no_sample);
  buf.insert(buf.end(), p, p + 4);
  buf.insert(buf.end(), 4, 0);  // num_sfx
}

inline std::vector<uint8_t> makeKeyframeTea() {
  std::vector<uint8_t> buf = makeMinimalTea();

  int32_t nf = 24, ng = 1, nkf = 1;
  std::memcpy(buf.data() + kTeaNumFramesOff, &nf, 4);
  std::memcpy(buf.data() + kTeaNumGroupsOff, &ng, 4);
  std::memcpy(buf.data() + kTeaNumKfOff, &nkf, 4);

  std::size_t kf = buf.size();
  buf.insert(buf.end(), kTeaKf2014Size, 0);
  int32_t num_frame = 24, flag = pistoris::kTeaFlagFrameStep, one = 1;
  std::memcpy(buf.data() + kf + 0, &num_frame, 4);
  std::memcpy(buf.data() + kf + kTeaKfFlagFrameOff, &flag, 4);
  std::memcpy(buf.data() + kf + kTeaKfKeyMoveOff, &one, 4);
  std::memcpy(buf.data() + kf + kTeaKfKeyOrientOff, &one, 4);

  float tr[3] = {1.f, 2.f, 3.f};
  buf.insert(buf.end(), reinterpret_cast<uint8_t*>(tr), reinterpret_cast<uint8_t*>(tr) + 12);

  buf.insert(buf.end(), 8, 0);  // THEO_ANGLE
  float q[4] = {0.5f, 0.5f, 0.5f, 0.5f};
  buf.insert(buf.end(), reinterpret_cast<uint8_t*>(q), reinterpret_cast<uint8_t*>(q) + 16);

  std::size_t gr = buf.size();
  buf.insert(buf.end(), kTeaGroupAnimSize, 0);
  int32_t kg  = 1;
  float gq[4] = {1.f, 0.f, 0.f, 0.f};
  float gt[3] = {4.f, 5.f, 6.f};
  float gz[3] = {1.f, 1.f, 1.f};
  std::memcpy(buf.data() + gr + 0, &kg, 4);
  std::memcpy(buf.data() + gr + 12, gq, 16);  // after key_group + angle
  std::memcpy(buf.data() + gr + 28, gt, 12);
  std::memcpy(buf.data() + gr + 40, gz, 12);

  int32_t no_sample = -1;
  auto* p           = reinterpret_cast<const uint8_t*>(&no_sample);
  buf.insert(buf.end(), p, p + 4);
  buf.insert(buf.end(), 4, 0);

  return buf;
}

inline void checkEq(const pistoris::ArxVector3& a, const pistoris::ArxVector3& b) {
  CHECK(a.x == b.x);
  CHECK(a.y == b.y);
  CHECK(a.z == b.z);
}

inline void checkEq(const pistoris::ftl::Vertex& a, const pistoris::ftl::Vertex& b) {
  checkEq(a.position, b.position);
  checkEq(a.normal, b.normal);
}

inline void checkEq(const pistoris::ftl::Face& a, const pistoris::ftl::Face& b) {
  CHECK(a.type == b.type);
  CHECK(a.vertex_idx.x == b.vertex_idx.x);
  CHECK(a.vertex_idx.y == b.vertex_idx.y);
  CHECK(a.vertex_idx.z == b.vertex_idx.z);
  CHECK(a.texture_id == b.texture_id);
  checkEq(a.u, b.u);
  checkEq(a.v, b.v);
  CHECK(a.transval == b.transval);
  checkEq(a.norm, b.norm);
}

inline void checkEq(const pistoris::ftl::Header& a, const pistoris::ftl::Header& b) {
  CHECK(a.origin == b.origin);
  CHECK(std::memcmp(a.name, b.name, sizeof(a.name)) == 0);
}

inline void checkEq(const pistoris::ftl::TextureContainer& a, const pistoris::ftl::TextureContainer& b) {
  CHECK(std::memcmp(a.filename, b.filename, sizeof(a.filename)) == 0);
}

inline void checkEq(const pistoris::ftl::Group& a, const pistoris::ftl::Group& b) {
  CHECK(std::memcmp(a.name, b.name, sizeof(a.name)) == 0);
  CHECK(a.origin == b.origin);
  CHECK(a.indices == b.indices);
  CHECK(a.blob_shadow_size == b.blob_shadow_size);
}

inline void checkEq(const pistoris::ftl::Action& a, const pistoris::ftl::Action& b) {
  CHECK(std::memcmp(a.name, b.name, sizeof(a.name)) == 0);
  CHECK(a.vertex_idx == b.vertex_idx);
  CHECK(a.action == b.action);
  CHECK(a.sfx == b.sfx);
}

inline void checkEq(const pistoris::ftl::Selection& a, const pistoris::ftl::Selection& b) {
  CHECK(std::memcmp(a.name, b.name, sizeof(a.name)) == 0);
  CHECK(a.selected == b.selected);
}

inline void checkEq(const pistoris::ftl::Data& a, const pistoris::ftl::Data& b) {
  checkEq(a.header, b.header);
  REQUIRE(a.vertices.size() == b.vertices.size());
  for (std::size_t i = 0; i < a.vertices.size(); ++i) checkEq(a.vertices[i], b.vertices[i]);
  REQUIRE(a.faces.size() == b.faces.size());
  for (std::size_t i = 0; i < a.faces.size(); ++i) checkEq(a.faces[i], b.faces[i]);
  REQUIRE(a.texture_containers.size() == b.texture_containers.size());
  for (std::size_t i = 0; i < a.texture_containers.size(); ++i)
    checkEq(a.texture_containers[i], b.texture_containers[i]);
  REQUIRE(a.groups.size() == b.groups.size());
  for (std::size_t i = 0; i < a.groups.size(); ++i) checkEq(a.groups[i], b.groups[i]);
  REQUIRE(a.actions.size() == b.actions.size());
  for (std::size_t i = 0; i < a.actions.size(); ++i) checkEq(a.actions[i], b.actions[i]);
  REQUIRE(a.selections.size() == b.selections.size());
  for (std::size_t i = 0; i < a.selections.size(); ++i) checkEq(a.selections[i], b.selections[i]);
}

inline void checkEq(const pistoris::ArxQuat& a, const pistoris::ArxQuat& b) {
  CHECK(a.w == b.w);
  CHECK(a.x == b.x);
  CHECK(a.y == b.y);
  CHECK(a.z == b.z);
}

inline void checkEq(const pistoris::tea::Sample& a, const pistoris::tea::Sample& b) {
  CHECK(std::memcmp(a.name, b.name, sizeof(a.name)) == 0);
}

inline void checkEq(const pistoris::tea::GroupAnim& a, const pistoris::tea::GroupAnim& b) {
  CHECK(a.key_group == b.key_group);
  checkEq(a.quat, b.quat);
  checkEq(a.translate, b.translate);
  checkEq(a.zoom, b.zoom);
}

inline void checkEq(const pistoris::tea::Keyframe& a, const pistoris::tea::Keyframe& b) {
  CHECK(a.num_frame == b.num_frame);
  CHECK(a.flag_frame == b.flag_frame);
  CHECK(a.translate.has_value() == b.translate.has_value());
  if (a.translate && b.translate) checkEq(*a.translate, *b.translate);
  CHECK(a.quat.has_value() == b.quat.has_value());
  if (a.quat && b.quat) checkEq(*a.quat, *b.quat);
  REQUIRE(a.groups.size() == b.groups.size());
  for (std::size_t i = 0; i < a.groups.size(); ++i) checkEq(a.groups[i], b.groups[i]);
  CHECK(a.sample.has_value() == b.sample.has_value());
  if (a.sample && b.sample) checkEq(*a.sample, *b.sample);
}

inline void checkEq(const pistoris::tea::Data& a, const pistoris::tea::Data& b) {
  CHECK(a.num_frames == b.num_frames);
  CHECK(a.num_groups == b.num_groups);
  CHECK(std::memcmp(a.name, b.name, sizeof(a.name)) == 0);
  REQUIRE(a.keyframes.size() == b.keyframes.size());
  for (std::size_t i = 0; i < a.keyframes.size(); ++i) checkEq(a.keyframes[i], b.keyframes[i]);
}
