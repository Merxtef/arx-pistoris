// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#include "arx/tea.h"

#include "arx_pistoris/pistoris_types.h"
#include "arx_pistoris/tea_data.hpp"

#include "utils/cursor.h"
#include "utils/log.h"
#include "utils/mem_utils.h"
#include "utils/parse_utils.h"

#include <cstdint>
#include <cstring>
#include <format>

namespace pistoris {

static ArxReturnCode readKeyframe(tea::Keyframe* kf, ReadCursor& c, uint32_t version, int32_t num_groups,
                                  int frame_idx) {
  int32_t key_move   = 0;
  int32_t key_orient = 0;
  int32_t key_morph  = 0;

  c.read(kf->num_frame);
  c.read(kf->flag_frame);
  if (version >= kTeaVersionAlt) c.skip(256);  // info_frame
  c.skip(4);                                   // master_key_frame
  c.skip(4);                                   // key_frame
  c.read(key_move);
  c.read(key_orient);
  c.read(key_morph);
  c.skip(4);  // time_frame
  if (!c) return ARX_UNEXPECTED_EOF;

  if (key_move != 0) {
    c.read(kf->translate.emplace());
  }

  if (key_orient != 0) {
    c.skip(8);  // THEO_ANGLE
    c.read(kf->quat.emplace());
  }

  if (key_morph != 0) c.skip(16);  // THEA_MORPH

  if (!tryResize(kf->groups, static_cast<std::size_t>(num_groups))) return ARX_BAD_ALLOC;
  for (auto& group : kf->groups) {
    c.read(group.key_group);
    c.skip(8);  // angle
    c.read(group.quat);
    c.read(group.translate);
    c.read(group.zoom);
  }

  int32_t num_sample = 0;
  c.read(num_sample);
  if (!c) return ARX_UNEXPECTED_EOF;

  if (num_sample != -1) {
    auto& sample = kf->sample.emplace();
    c.read(sample);
    if (!c) return ARX_UNEXPECTED_EOF;
    clampStr(sample.name, "TEA: sample.name", frame_idx);

    int32_t sample_size = 0;
    c.read(sample_size);
    if (!c) return ARX_UNEXPECTED_EOF;
    if (sample_size < 0) return ARX_TEA_BAD_SAMPLE_SIZE;
    c.skip(static_cast<std::size_t>(sample_size));  // audio
  }

  c.skip(4);  // num_sfx

  if (!c) return ARX_UNEXPECTED_EOF;
  return ARX_OK;
}

ArxReturnCode loadTea(tea::Data* d, ReadCursor& c) {
  char identity[20] = {};
  c.read(identity);
  if (!c) return ARX_UNEXPECTED_EOF;
  if (std::memcmp(identity, kTeaMagic, sizeof(kTeaMagic)) != 0) return ARX_INVALID_IDENTIFIER;

  uint32_t version = 0;
  c.read(version);
  if (!c) return ARX_UNEXPECTED_EOF;
  if (version < kTeaVersion) return ARX_TEA_BAD_VERSION;

  c.read(d->name);
  clampStr(d->name, "TEA: anim_name", 0);
  c.read(d->num_frames);
  c.read(d->num_groups);
  int32_t num_key_frames = 0;
  c.read(num_key_frames);
  if (!c) return ARX_UNEXPECTED_EOF;

  if (d->num_groups < 0 || static_cast<std::size_t>(d->num_groups) > kTeaMaxGroups) return ARX_TEA_BAD_GROUPS_N;
  if (num_key_frames < 0 || static_cast<std::size_t>(num_key_frames) > kTeaMaxKeyframes) return ARX_TEA_BAD_KEYFRAMES_N;

  if (!tryResize(d->keyframes, static_cast<std::size_t>(num_key_frames))) return ARX_BAD_ALLOC;
  for (int32_t i = 0; i < num_key_frames; ++i) {
    ARX_RETURN_IF_ERR(readKeyframe(&d->keyframes[i], c, version, d->num_groups, i), c);
  }

  ARX_RETURN_IF_ERR(validateTea(d));

  log(ARX_LOG_INFO, std::format("TEA loaded: {} keyframes, {} groups, num_frames={}", d->keyframes.size(),
                                d->num_groups, d->num_frames));

  return ARX_OK;
}

static WriteCursor& writeKeyframe(const tea::Keyframe& kf, WriteCursor& c) {
  c.write(kf.num_frame);
  c.write(kf.flag_frame);
  c.pad(4);                                                         // master_key_frame
  c.pad(4);                                                         // key_frame
  c.write(static_cast<int32_t>(kf.translate.has_value() ? 1 : 0));  // key_move
  c.write(static_cast<int32_t>(kf.quat.has_value() ? 1 : 0));       // key_orient
  c.pad(4);                                                         // key_morph
  c.pad(4);                                                         // time_frame

  if (kf.translate) c.write(*kf.translate);

  if (kf.quat) {
    c.pad(8);  // THEO_ANGLE
    c.write(*kf.quat);
  }

  for (const auto& group : kf.groups) {
    c.write(group.key_group);
    c.pad(8);  // angle
    c.write(group.quat);
    c.write(group.translate);
    c.write(group.zoom);
  }

  if (kf.sample) {
    c.write(static_cast<int32_t>(0));  // num_sample (unused by readers)
    c.write(*kf.sample);
    c.write(static_cast<int32_t>(0));  // sample_size; audio dropped
  } else {
    c.write(static_cast<int32_t>(-1));
  }

  c.pad(4);  // num_sfx

  return c;
}

ArxReturnCode saveTea(const tea::Data* d, WriteCursor& c) {
  ARX_RETURN_IF_ERR(validateTea(d));

  log(ARX_LOG_INFO, std::format("TEA saving: {} keyframes, {} groups, num_frames={}", d->keyframes.size(),
                                d->num_groups, d->num_frames));

  char identity[20] = {};
  std::memcpy(identity, kTeaMagic, sizeof(kTeaMagic) - 1);
  c.write(identity);
  c.write(kTeaVersion);  // always v2014
  c.write(d->name);
  c.write(d->num_frames);
  c.write(d->num_groups);
  c.write(static_cast<int32_t>(d->keyframes.size()));

  for (const auto& kf : d->keyframes) writeKeyframe(kf, c);

  return c ? ARX_OK : ARX_BAD_ALLOC;
}

ArxReturnCode validateTea(const tea::Data* d) {
  if (d->num_frames < 0) return ARX_TEA_BAD_FRAMES_N;
  if (d->num_groups < 0 || static_cast<std::size_t>(d->num_groups) > kTeaMaxGroups) return ARX_TEA_BAD_GROUPS_N;
  if (d->keyframes.empty() || d->keyframes.size() > kTeaMaxKeyframes) return ARX_TEA_BAD_KEYFRAMES_N;

  int32_t prev_frame = -1;
  for (const auto& kf : d->keyframes) {
    if (kf.flag_frame != kTeaFlagFrameNone && kf.flag_frame != kTeaFlagFrameStep) return ARX_TEA_BAD_FLAG_FRAME;
    if (kf.groups.size() != static_cast<std::size_t>(d->num_groups)) return ARX_TEA_BAD_GROUPS_N;
    if (kf.num_frame < 0 || kf.num_frame <= prev_frame) return ARX_TEA_NON_MONOTONIC_FRAMES;
    prev_frame = kf.num_frame;
  }
  if (prev_frame > d->num_frames) return ARX_TEA_BAD_FRAMES_N;

  return ARX_OK;
}

}  // namespace pistoris
