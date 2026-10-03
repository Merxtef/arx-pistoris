// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#include "native/tea.h"

#include "arx_pistoris/base/location.hpp"
#include "arx_pistoris/base/math.hpp"
#include "arx_pistoris/base/status.h"
#include "arx_pistoris/native/location.hpp"
#include "arx_pistoris/native/tea.hpp"
#include "arx_pistoris/runtime/types.h"

#include "api/result_failure.h"
#include "native/binary_location.h"
#include "native/fixed_string.h"
#include "native/resource_lookup.h"
#include "native/resource_path.h"
#include "utils/container_allocation.h"
#include "utils/cursor.h"
#include "utils/log.h"
#include "utils/math/finite.h"
#include "utils/math/quat.h"
#include "utils/native_text.h"
#include "utils/return_code.h"

#include <cstdint>
#include <cstring>
#include <format>
#include <string>
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

bool canonicalSamplePath(std::string_view path) {
  std::string canonical;
  return normalizeNativeResourcePath(path, canonical) && canonical == path;
}

bool encodeSamplePath(std::string_view path, tea::Sample& out) {
  std::string encoded;
  if (!encodeNativeResourceStem(path, sizeof(out.name), encoded)) return false;
  std::memset(out.name, 0, sizeof(out.name));
  std::memcpy(out.name, encoded.data(), encoded.size());
  return true;
}

}  // namespace

static ArxReturnCode readKeyframe(tea::Keyframe* kf, ReadCursor& c, uint32_t version, int32_t num_groups,
                                  int frame_idx) {
  int32_t key_move = 0;
  int32_t key_orient = 0;
  int32_t key_morph = 0;

  c.locate(TeaElement::kKeyframe, "keyframe", static_cast<std::size_t>(frame_idx));
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
  for (std::size_t group_index = 0; group_index < kf->groups.size(); ++group_index) {
    auto& group = kf->groups[group_index];
    c.locate(TeaElement::kGroupTransform, "group_transform", static_cast<std::size_t>(frame_idx), group_index);
    c.read(group.key_group);
    c.skip(8);  // angle
    c.read(group.quat);
    c.read(group.translate);
    c.read(group.zoom);
  }

  int32_t num_sample = 0;
  c.locate(TeaElement::kSound, "sample_presence", static_cast<std::size_t>(frame_idx));
  c.read(num_sample);
  if (!c) return ARX_UNEXPECTED_EOF;

  if (num_sample != -1) {
    auto& sample = kf->sample.emplace();
    c.locate(TeaElement::kSound, "sample", static_cast<std::size_t>(frame_idx));
    c.read(sample);
    if (!c) return ARX_UNEXPECTED_EOF;
    canonicalizeFixedString(sample.name, "TEA: sample.name", frame_idx);

    int32_t sample_size = 0;
    c.locate(TeaElement::kSound, "sample_size", static_cast<std::size_t>(frame_idx));
    c.read(sample_size);
    if (!c) return ARX_UNEXPECTED_EOF;
    if (sample_size < 0) return ARX_TEA_BAD_SAMPLE_SIZE;
    c.locate(TeaElement::kSound, "sample_data", static_cast<std::size_t>(frame_idx));
    c.skip(static_cast<std::size_t>(sample_size));  // audio
  }

  c.skip(4);  // num_sfx

  if (!c) return ARX_UNEXPECTED_EOF;
  return ARX_OK;
}

TeaBinaryResult<tea::Data> loadTea(ReadCursor& c, NativeBinaryRegion region) {
  auto fail = [&](ArxReturnCode code, const CursorLocation& location) {
    return api_detail::teaBinaryFailure<tea::Data>(code, native_binary::location<TeaElement>(location, region));
  };
  auto fail_cursor = [&](ArxReturnCode code) {
    return api_detail::teaBinaryFailure<tea::Data>(code, native_binary::location<TeaElement>(c, region));
  };

  tea::Data result;
  char identity[20] = {};
  const CursorLocation identity_location = c.mark(TeaElement::kHeader, "identity");
  c.read(identity);
  if (!c) return fail_cursor(ARX_UNEXPECTED_EOF);
  if (std::memcmp(identity, kTeaMagic, sizeof(kTeaMagic)) != 0) return fail(ARX_INVALID_IDENTIFIER, identity_location);

  uint32_t version = 0;
  const CursorLocation version_location = c.mark(TeaElement::kHeader, "version");
  c.read(version);
  if (!c) return fail_cursor(ARX_UNEXPECTED_EOF);
  if (version < kTeaVersion) return fail(ARX_TEA_BAD_VERSION, version_location);

  c.locate(TeaElement::kHeader, "name").read(result.name);
  canonicalizeFixedString(result.name, "TEA: anim_name", 0);
  const CursorLocation frame_count_location = c.mark(TeaElement::kHeader, "frame_count");
  c.read(result.num_frames);
  const CursorLocation group_count_location = c.mark(TeaElement::kHeader, "group_count");
  c.read(result.num_groups);
  int32_t num_key_frames = 0;
  const CursorLocation keyframe_count_location = c.mark(TeaElement::kHeader, "keyframe_count");
  c.read(num_key_frames);
  if (!c) return fail_cursor(ARX_UNEXPECTED_EOF);

  if (result.num_frames < 0) return fail(ARX_TEA_BAD_FRAMES_N, frame_count_location);
  if (result.num_groups < 0 || static_cast<std::size_t>(result.num_groups) > kTeaMaxGroups)
    return fail(ARX_TEA_BAD_GROUPS_N, group_count_location);
  if (num_key_frames < 0 || static_cast<std::size_t>(num_key_frames) > kTeaMaxKeyframes)
    return fail(ARX_TEA_BAD_KEYFRAMES_N, keyframe_count_location);

  if (!tryResize(result.keyframes, static_cast<std::size_t>(num_key_frames)))
    return fail(ARX_BAD_ALLOC, keyframe_count_location);
  for (int32_t i = 0; i < num_key_frames; ++i) {
    if (const ArxReturnCode code = readKeyframe(&result.keyframes[i], c, version, result.num_groups, i); code != ARX_OK)
      return fail_cursor(code);
  }

  TeaLocation semantic_location;
  if (const ArxReturnCode code = canonicalizeTea(&result, &semantic_location); code != ARX_OK)
    return api_detail::teaBinaryFailure<tea::Data>(code, native_binary::semanticLocation(semantic_location, region));
  TeaLocation validation_location;
  if (const ArxReturnCode code = validateTea(&result, &validation_location); code != ARX_OK) {
    return api_detail::teaBinaryFailure<tea::Data>(code, native_binary::semanticLocation(validation_location, region));
  }

  log(ARX_LOG_INFO,
      "TEA loaded: {} keyframes, {} groups ({} active), timeline {} frames at {} fps ({:.3f} s)",
      result.keyframes.size(),
      result.num_groups,
      activeGroupCount(result),
      result.num_frames,
      static_cast<int>(kTeaFps),
      static_cast<double>(result.num_frames) / static_cast<double>(kTeaFps));

  return TeaBinaryResult<tea::Data>::success(std::move(result));
}

ArxReturnCode writeKeyframe(const tea::Keyframe& kf, WriteCursor& c) {
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
    tea::Sample encoded;
    if (!encodeSamplePath(sample->name, encoded)) return ARX_TEA_BAD_SAMPLE_PATH;
    c.write(encoded);
    c.write(static_cast<int32_t>(0));  // sample_size; audio dropped
  } else {
    c.write(static_cast<int32_t>(-1));
  }

  c.pad(4);  // num_sfx

  return c ? ARX_OK : ARX_BAD_ALLOC;
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
          native_text::diagnostic(path));
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

  for (const auto& kf : d->keyframes) ARX_RETURN_IF_ERR(writeKeyframe(kf, c));

  return c ? ARX_OK : ARX_BAD_ALLOC;
}

ArxReturnCode canonicalizeTea(tea::Data* d, TeaLocation* failure_location) {
  auto fail = [failure_location](ArxReturnCode code,
                                 TeaElement element = TeaElement::kHeader,
                                 std::size_t index = kNoElementIndex,
                                 std::string field = {}) {
    if (failure_location) *failure_location = {.element = element, .index = index, .field = std::move(field)};
    return code;
  };
  if (!d) return fail(ARX_INVALID_DATA_POINTER);
  for (std::size_t index = 0; index < d->keyframes.size(); ++index) {
    tea::Keyframe& keyframe = d->keyframes[index];
    if (!keyframe.sample) continue;
    tea::Sample& sample = *keyframe.sample;
    std::string canonical;
    if (!normalizeNativeResourceStem(sample.name, canonical) || canonical.size() >= sizeof(sample.name))
      return fail(ARX_TEA_BAD_SAMPLE_PATH, TeaElement::kSound, index, "sample.name");
    std::memset(sample.name, 0, sizeof(sample.name));
    std::memcpy(sample.name, canonical.data(), canonical.size());
  }
  return ARX_OK;
}

ArxReturnCode validateTea(const tea::Data* d, TeaLocation* failure_location) {
  auto fail = [failure_location](ArxReturnCode code,
                                 TeaElement element = TeaElement::kHeader,
                                 std::size_t index = kNoElementIndex,
                                 std::size_t subindex = kNoElementIndex,
                                 std::string field = {}) {
    if (failure_location)
      *failure_location = {.element = element, .index = index, .subindex = subindex, .field = std::move(field)};
    return code;
  };
  if (!d) return fail(ARX_INVALID_DATA_POINTER);
  if (!isNullTerminated(d->name))
    return fail(ARX_TEA_BAD_NAME, TeaElement::kHeader, kNoElementIndex, kNoElementIndex, "name");
  if (d->num_frames < 0)
    return fail(ARX_TEA_BAD_FRAMES_N, TeaElement::kHeader, kNoElementIndex, kNoElementIndex, "num_frames");
  if (d->num_groups < 0 || static_cast<std::size_t>(d->num_groups) > kTeaMaxGroups)
    return fail(ARX_TEA_BAD_GROUPS_N, TeaElement::kHeader, kNoElementIndex, kNoElementIndex, "num_groups");
  if (d->keyframes.empty() || d->keyframes.size() > kTeaMaxKeyframes)
    return fail(ARX_TEA_BAD_KEYFRAMES_N, TeaElement::kHeader, kNoElementIndex, kNoElementIndex, "keyframes");

  int32_t prev_frame = -1;
  for (std::size_t frame = 0; frame < d->keyframes.size(); ++frame) {
    const tea::Keyframe& kf = d->keyframes[frame];
    if (kf.flag_frame != kTeaFlagFrameNone && kf.flag_frame != kTeaFlagFrameStep)
      return fail(ARX_TEA_BAD_FLAG_FRAME, TeaElement::kKeyframe, frame, kNoElementIndex, "flag_frame");
    if (kf.groups.size() != static_cast<std::size_t>(d->num_groups))
      return fail(ARX_TEA_BAD_GROUPS_N, TeaElement::kKeyframe, frame, kNoElementIndex, "groups");
    if (kf.num_frame < 0 || kf.num_frame <= prev_frame)
      return fail(ARX_TEA_NON_MONOTONIC_FRAMES, TeaElement::kKeyframe, frame, kNoElementIndex, "num_frame");
    if (kf.translate && !math::finite(*kf.translate))
      return fail(ARX_TEA_BAD_ROOT_TRANSFORM, TeaElement::kKeyframe, frame, kNoElementIndex, "translate");
    if (kf.quat && !math::finite(*kf.quat))
      return fail(ARX_TEA_BAD_ROOT_TRANSFORM, TeaElement::kKeyframe, frame, kNoElementIndex, "quat");
    for (std::size_t group = 0; group < kf.groups.size(); ++group) {
      const tea::GroupAnim& transform = kf.groups[group];
      if (!math::finite(transform.quat))
        return fail(ARX_TEA_BAD_GROUP_TRANSFORM, TeaElement::kGroupTransform, frame, group, "quat");
      if (!math::finite(transform.translate))
        return fail(ARX_TEA_BAD_GROUP_TRANSFORM, TeaElement::kGroupTransform, frame, group, "translate");
      if (!math::finite(transform.zoom))
        return fail(ARX_TEA_BAD_GROUP_TRANSFORM, TeaElement::kGroupTransform, frame, group, "zoom");
    }
    if (kf.sample) {
      // NOLINTNEXTLINE(bugprone-unchecked-optional-access): checked above
      const tea::Sample& sample = *kf.sample;
      if (!isNullTerminated(sample.name))
        return fail(ARX_TEA_BAD_SAMPLE_PATH, TeaElement::kSound, frame, kNoElementIndex, "sample.name");
      if (!canonicalSamplePath(sample.name))
        return fail(ARX_TEA_BAD_SAMPLE_PATH, TeaElement::kSound, frame, kNoElementIndex, "sample.name");
      tea::Sample encoded;
      if (!encodeSamplePath(sample.name, encoded))
        return fail(ARX_TEA_BAD_SAMPLE_PATH, TeaElement::kSound, frame, kNoElementIndex, "sample.name");
    }
    prev_frame = kf.num_frame;
  }
  if (prev_frame > d->num_frames)
    return fail(ARX_TEA_BAD_FRAMES_N, TeaElement::kHeader, kNoElementIndex, kNoElementIndex, "num_frames");

  return ARX_OK;
}

}  // namespace pistoris
