// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#include "native/tea.h"

#include "arx_pistoris/base/math.hpp"
#include "arx_pistoris/base/status.h"
#include "arx_pistoris/native/tea.hpp"
#include "arx_pistoris/runtime/types.h"

#include "native/fixed_string.h"
#include "native/resource_lookup.h"
#include "utils/container_allocation.h"
#include "utils/cursor.h"
#include "utils/log.h"
#include "utils/math/finite.h"
#include "utils/math/quat.h"
#include "utils/return_code.h"

#include <cstdint>
#include <cstring>
#include <format>
#include <string_view>
#include <utility>

namespace pistoris {

namespace {

std::size_t activeGroupCount(const tea::Data& animation) noexcept {
  std::size_t active = 0;
  for (std::size_t group = 0; group < static_cast<std::size_t>(animation.num_groups); ++group) {
    for (const tea::Keyframe& keyframe : animation.keyframes) {
      const tea::GroupAnim& transform = keyframe.groups[group];
      if (transform.quat == math::kIdentityQuat && transform.translate == ArxVector3{} &&
          transform.zoom == ArxVector3{}) {
        continue;
      }
      ++active;
      break;
    }
  }
  return active;
}

}  // namespace

static ArxReturnCode readKeyframe(tea::Keyframe* kf, ReadCursor& c, uint32_t version, int32_t num_groups,
                                  int frame_idx) {
  int32_t key_move = 0;
  int32_t key_orient = 0;
  int32_t key_morph = 0;

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
  if (!d) return ARX_INVALID_DATA_POINTER;

  tea::Data result;
  char identity[20] = {};
  c.read(identity);
  if (!c) return ARX_UNEXPECTED_EOF;
  if (std::memcmp(identity, kTeaMagic, sizeof(kTeaMagic)) != 0) return ARX_INVALID_IDENTIFIER;

  uint32_t version = 0;
  c.read(version);
  if (!c) return ARX_UNEXPECTED_EOF;
  if (version < kTeaVersion) return ARX_TEA_BAD_VERSION;

  c.read(result.name);
  clampStr(result.name, "TEA: anim_name", 0);
  c.read(result.num_frames);
  c.read(result.num_groups);
  int32_t num_key_frames = 0;
  c.read(num_key_frames);
  if (!c) return ARX_UNEXPECTED_EOF;

  if (result.num_groups < 0 || static_cast<std::size_t>(result.num_groups) > kTeaMaxGroups) return ARX_TEA_BAD_GROUPS_N;
  if (num_key_frames < 0 || static_cast<std::size_t>(num_key_frames) > kTeaMaxKeyframes) return ARX_TEA_BAD_KEYFRAMES_N;

  if (!tryResize(result.keyframes, static_cast<std::size_t>(num_key_frames))) return ARX_BAD_ALLOC;
  for (int32_t i = 0; i < num_key_frames; ++i) {
    ARX_RETURN_IF_ERR(readKeyframe(&result.keyframes[i], c, version, result.num_groups, i), c);
  }

  ARX_RETURN_IF_ERR(validateTea(&result));

  *d = std::move(result);

  log(ARX_LOG_INFO,
      "TEA loaded: {} keyframes, {} groups ({} active), timeline {} frames at {} fps ({:.3f} s)",
      d->keyframes.size(),
      d->num_groups,
      activeGroupCount(*d),
      d->num_frames,
      static_cast<int>(kTeaFps),
      static_cast<double>(d->num_frames) / static_cast<double>(kTeaFps));

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

  const auto& translate = kf.translate;
  if (translate) c.write(*translate);

  const auto& quat = kf.quat;
  if (quat) {
    c.pad(8);  // THEO_ANGLE
    c.write(*quat);
  }

  for (const auto& group : kf.groups) {
    c.write(group.key_group);
    c.pad(8);  // angle
    c.write(group.quat);
    c.write(group.translate);
    c.write(group.zoom);
  }

  const auto& sample = kf.sample;
  if (sample) {
    c.write(static_cast<int32_t>(0));  // num_sample (unused by readers)
    c.write(*sample);
    c.write(static_cast<int32_t>(0));  // sample_size; audio dropped
  } else {
    c.write(static_cast<int32_t>(-1));
  }

  c.pad(4);  // num_sfx

  return c;
}

ArxReturnCode saveTea(const tea::Data* d, WriteCursor& c) {
  ARX_RETURN_IF_ERR(validateTea(d));

  for (std::size_t index = 0; index < d->keyframes.size(); ++index) {
    const auto& sample = d->keyframes[index].sample;
    if (!sample) continue;
    const std::string_view path = sample->name;
    if (!path.empty() && !resolvesThroughDefaultLooseRoot("sfx", path)) {
      log(ARX_LOG_WARN,
          "TEA saving: keyframe[{}] sample path '{}' resolves outside Libertatis default loose roots; "
          "it may not be discovered",
          index,
          path);
    }
  }

  log(ARX_LOG_INFO,
      "TEA saving: {} keyframes, {} groups ({} active), timeline {} frames at {} fps ({:.3f} s)",
      d->keyframes.size(),
      d->num_groups,
      activeGroupCount(*d),
      d->num_frames,
      static_cast<int>(kTeaFps),
      static_cast<double>(d->num_frames) / static_cast<double>(kTeaFps));

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
  if (!d) return ARX_INVALID_DATA_POINTER;
  if (!isNullTerminated(d->name)) return ARX_TEA_BAD_NAME;
  if (d->num_frames < 0) return ARX_TEA_BAD_FRAMES_N;
  if (d->num_groups < 0 || static_cast<std::size_t>(d->num_groups) > kTeaMaxGroups) return ARX_TEA_BAD_GROUPS_N;
  if (d->keyframes.empty() || d->keyframes.size() > kTeaMaxKeyframes) return ARX_TEA_BAD_KEYFRAMES_N;

  int32_t prev_frame = -1;
  for (const auto& kf : d->keyframes) {
    if (kf.flag_frame != kTeaFlagFrameNone && kf.flag_frame != kTeaFlagFrameStep) return ARX_TEA_BAD_FLAG_FRAME;
    if (kf.groups.size() != static_cast<std::size_t>(d->num_groups)) return ARX_TEA_BAD_GROUPS_N;
    if (kf.num_frame < 0 || kf.num_frame <= prev_frame) return ARX_TEA_NON_MONOTONIC_FRAMES;
    if ((kf.translate && !math::finite(*kf.translate)) || (kf.quat && !math::finite(*kf.quat)))
      return ARX_TEA_BAD_ROOT_TRANSFORM;
    for (const tea::GroupAnim& group : kf.groups)
      if (!math::finite(group.quat) || !math::finite(group.translate) || !math::finite(group.zoom))
        return ARX_TEA_BAD_GROUP_TRANSFORM;
    if (kf.sample) {
      // NOLINTNEXTLINE(bugprone-unchecked-optional-access): checked above
      const tea::Sample& sample = *kf.sample;
      if (!isNullTerminated(sample.name)) return ARX_TEA_BAD_SAMPLE_PATH;
    }
    prev_frame = kf.num_frame;
  }
  if (prev_frame > d->num_frames) return ARX_TEA_BAD_FRAMES_N;

  return ARX_OK;
}

}  // namespace pistoris
