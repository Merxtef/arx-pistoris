// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#include "native/cin.h"

#include "arx_pistoris/base/status.h"
#include "arx_pistoris/native/cin.hpp"
#include "arx_pistoris/runtime/types.h"

#include "native/c_string.h"
#include "native/cin_resource_path.h"
#include "native/resource_lookup.h"
#include "native/resource_path.h"
#include "utils/cinematic_constraints.h"
#include "utils/container_allocation.h"
#include "utils/cursor.h"
#include "utils/log.h"
#include "utils/math/finite.h"
#include "utils/native_text.h"
#include "utils/return_code.h"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <limits>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace pistoris {
namespace {

constexpr std::size_t kTrackSize = 24;
constexpr std::size_t kKey175Size = 120;
constexpr std::size_t kKey176Size = 180;

struct PhysicalKey {
  cin::Keyframe value;
  std::size_t order = 0;
};

ArxReturnCode readLight(cin::Light& light, ReadCursor& cursor) {
  cursor.read(light.position);
  cursor.read(light.fall_in);
  cursor.read(light.fall_out);
  cursor.read(light.color);
  cursor.read(light.intensity);
  cursor.read(light.random_intensity);
  cursor.skip(8);  // runtime links
  return cursor ? ARX_OK : ARX_UNEXPECTED_EOF;
}

WriteCursor& writeLight(const cin::Light& light, WriteCursor& cursor) {
  cursor.write(light.position);
  cursor.write(light.fall_in);
  cursor.write(light.fall_out);
  cursor.write(light.color);
  cursor.write(light.intensity);
  cursor.write(light.random_intensity);
  return cursor.pad(8);  // runtime links
}

void normalizeInactiveFields(cin::Keyframe& key) noexcept {
  if (((key.effects >> 16U) & 0xffU) != 1U) key.flash_decay = 0.0f;
  if (((key.effects >> 24U) & 0xffU) != 1U || (math::finite(key.light.intensity) && key.light.intensity < 0.0f))
    key.light = {};
}

ArxReturnCode readKey(cin::Keyframe& key, ReadCursor& cursor, std::int32_t version) {
  cursor.read(key.frame);
  cursor.read(key.bitmap);
  cursor.read(key.effects);
  cursor.read(key.interpolation);
  cursor.read(key.crossfade);
  cursor.read(key.camera_position);
  cursor.read(key.camera_roll);
  cursor.read(key.color);
  cursor.read(key.secondary_color);
  cursor.read(key.flash_color);

  if (version == kCinVersion175) cursor.skip(4);  // sound ignored by the engine

  cursor.read(key.flash_decay);
  ARX_RETURN_IF_ERR(readLight(key.light, cursor), cursor);
  cursor.read(key.bitmap_position);
  cursor.read(key.bitmap_roll);
  cursor.read(key.outgoing_speed);

  if (version == kCinVersion) {
    std::array<std::int32_t, 16> sounds = {};
    cursor.read(sounds);
    key.sound = sounds[3];
  }

  if (!cursor) return ARX_UNEXPECTED_EOF;
  if (key.crossfade < 0) {
    key.crossfade = 1;
  } else if (key.crossfade > 1) {
    const std::int16_t original = key.crossfade;
    key.crossfade = static_cast<std::int16_t>(key.crossfade & 1);
    log(ARX_LOG_WARN,
        "CIN: key at frame {} has inconsistent crossfade {}; using {}",
        key.frame,
        original,
        key.crossfade);
  }
  if (key.interpolation < -1 || key.interpolation > 1) {
    log(ARX_LOG_WARN,
        "CIN: key at frame {} has unsupported interpolation {}; using linear",
        key.frame,
        key.interpolation);
    key.interpolation = 1;
  }
  normalizeInactiveFields(key);
  return ARX_OK;
}

WriteCursor& writeKey(const cin::Keyframe& key, WriteCursor& cursor) {
  cin::Keyframe effective = key;
  normalizeInactiveFields(effective);
  cursor.write(effective.frame);
  cursor.write(effective.bitmap);
  cursor.write(effective.effects);
  cursor.write(effective.interpolation);
  cursor.write(effective.crossfade);
  cursor.write(effective.camera_position);
  cursor.write(effective.camera_roll);
  cursor.write(effective.color);
  cursor.write(effective.secondary_color);
  cursor.write(effective.flash_color);
  cursor.write(effective.flash_decay);
  writeLight(effective.light, cursor);
  cursor.write(effective.bitmap_position);
  cursor.write(effective.bitmap_roll);
  cursor.write(effective.outgoing_speed);
  std::array<std::int32_t, 16> sounds;
  sounds.fill(-1);
  sounds[3] = effective.sound;
  return cursor.write(sounds);
}

bool finite(const cin::Light& light) {
  return math::finite(light.position) && math::finite(light.fall_in) && math::finite(light.fall_out) &&
         math::finite(light.color) && math::finite(light.intensity) && math::finite(light.random_intensity);
}

bool finite(const cin::Keyframe& key) {
  if (!math::finite(key.camera_position) || !math::finite(key.camera_roll) || !math::finite(key.bitmap_position) ||
      !math::finite(key.bitmap_roll) || !math::finite(key.outgoing_speed))
    return false;
  if (((key.effects >> 16U) & 0xffU) == 1U && !math::finite(key.flash_decay)) return false;
  if (((key.effects >> 24U) & 0xffU) != 1U) return true;
  if (!math::finite(key.light.intensity)) return false;
  return key.light.intensity < 0.0f || finite(key.light);
}

}  // namespace

ArxReturnCode loadCin(cin::Data* data, ReadCursor& cursor) {
  if (!data) return ARX_INVALID_DATA_POINTER;

  std::array<char, 4> magic = {};
  std::int32_t version = 0;
  cursor.read(magic);
  cursor.read(version);
  if (!cursor) return ARX_UNEXPECTED_EOF;
  if (magic != kCinMagic) return ARX_INVALID_IDENTIFIER;
  if (version != kCinVersion175 && version != kCinVersion) return ARX_CIN_BAD_VERSION;

  std::string discarded;
  ARX_RETURN_IF_ERR(native_io::readCString(discarded, cursor));

  cin::Data result;
  std::int32_t bitmap_count = 0;
  cursor.read(bitmap_count);
  if (!cursor) return ARX_UNEXPECTED_EOF;
  if (bitmap_count <= 0) return ARX_CIN_BAD_BITMAP_COUNT;
  if (static_cast<std::size_t>(bitmap_count) > cursor.remaining() / 5U) return ARX_UNEXPECTED_EOF;
  if (!tryResize(result.bitmaps, static_cast<std::size_t>(bitmap_count))) return ARX_BAD_ALLOC;
  for (cin::Bitmap& bitmap : result.bitmaps) {
    cursor.read(bitmap.subdivision_scale);
    std::string stored_path;
    ARX_RETURN_IF_ERR(native_io::readCString(stored_path, cursor), cursor);
    if (!decodeCinIllustrationPath(stored_path, bitmap.path)) return ARX_CIN_BAD_BITMAP_PATH;
  }

  std::int32_t sound_count = 0;
  cursor.read(sound_count);
  if (!cursor) return ARX_UNEXPECTED_EOF;
  if (sound_count < 0 || static_cast<std::size_t>(sound_count) > kCinMaxSounds) return ARX_CIN_BAD_SOUND_COUNT;
  const std::size_t minimum_sound_size = version == kCinVersion ? 3U : 1U;
  if (static_cast<std::size_t>(sound_count) > cursor.remaining() / minimum_sound_size) return ARX_UNEXPECTED_EOF;
  if (!tryResize(result.sounds, static_cast<std::size_t>(sound_count))) return ARX_BAD_ALLOC;
  for (cin::Sound& sound : result.sounds) {
    if (version == kCinVersion) cursor.skip(2);  // unused language tag
    std::string stored_path;
    ARX_RETURN_IF_ERR(native_io::readCString(stored_path, cursor), cursor);
    if (!decodeCinSoundPath(stored_path, sound)) return ARX_CIN_BAD_SOUND_PATH;
  }

  std::int32_t start_frame = 0;
  float current_frame = 0.0f;
  std::int32_t key_count = 0;
  std::int32_t pause = 0;
  cursor.read(start_frame);
  cursor.read(result.end_frame);
  cursor.read(current_frame);
  cursor.read(result.fps);
  cursor.read(key_count);
  cursor.read(pause);
  if (!cursor) return ARX_UNEXPECTED_EOF;
  if (key_count < 0) return ARX_CIN_BAD_KEY_COUNT;
  const std::size_t key_size = version == kCinVersion ? kKey176Size : kKey175Size;
  if (static_cast<std::size_t>(key_count) > cursor.remaining() / key_size) return ARX_UNEXPECTED_EOF;

  std::vector<PhysicalKey> physical;
  if (!tryResize(physical, static_cast<std::size_t>(key_count))) return ARX_BAD_ALLOC;
  for (std::size_t index = 0; index < physical.size(); ++index) {
    physical[index].order = index;
    ARX_RETURN_IF_ERR(readKey(physical[index].value, cursor, version), cursor);
  }

  std::sort(physical.begin(), physical.end(), [](const PhysicalKey& left, const PhysicalKey& right) {
    if (left.value.frame != right.value.frame) return left.value.frame < right.value.frame;
    return left.order < right.order;
  });
  if (!tryResize(result.keyframes, physical.size())) return ARX_BAD_ALLOC;
  std::size_t accepted = 0;
  for (PhysicalKey& item : physical) {
    if (item.value.frame < 0 || item.value.frame > result.end_frame) continue;
    if (accepted != 0 && result.keyframes[accepted - 1].frame == item.value.frame) {
      result.keyframes[accepted - 1] = item.value;
    } else {
      result.keyframes[accepted++] = item.value;
    }
  }
  result.keyframes.resize(accepted);

  ARX_RETURN_IF_ERR(validateCin(&result));
  if (start_frame != 0) log(ARX_LOG_WARN, "CIN: start frame {} discarded; playback starts at 0", start_frame);
  *data = std::move(result);
  log(ARX_LOG_INFO,
      "CIN loaded: {} keyframes, {} illustrations, {} sounds, timeline {} frames at {:.3g} fps",
      data->keyframes.size(),
      data->bitmaps.size(),
      data->sounds.size(),
      data->end_frame,
      data->fps);
  return ARX_OK;
}

ArxReturnCode saveCin(const cin::Data* data, WriteCursor& cursor) {
  ARX_RETURN_IF_ERR(validateCin(data));

  for (std::size_t index = 0; index < data->bitmaps.size(); ++index) {
    const std::string& path = data->bitmaps[index].path;
    if (!resolvesThroughDefaultLooseRoot({}, path)) {
      log(ARX_LOG_WARN,
          "CIN saving: illustration[{}] path '{}' resolves outside Libertatis default loose roots; "
          "it may not be discovered",
          index,
          native_text::diagnostic(data->bitmaps[index].path));
    }
  }
  for (std::size_t index = 0; index < data->sounds.size(); ++index) {
    const cin::Sound& sound = data->sounds[index];
    const std::string_view base = sound.speech ? "speech" : "sfx";
    if (!resolvesThroughDefaultLooseRoot(base, sound.path)) {
      log(ARX_LOG_WARN,
          "CIN saving: sound[{}] path '{}' resolves outside Libertatis default loose roots; "
          "it may not be discovered",
          index,
          native_text::diagnostic(data->sounds[index].path));
    }
  }

  log(ARX_LOG_INFO,
      "CIN saving: {} keyframes, {} illustrations, {} sounds, timeline {} frames at {:.3g} fps",
      data->keyframes.size(),
      data->bitmaps.size(),
      data->sounds.size(),
      data->end_frame,
      data->fps);

  cursor.write(kCinMagic);
  cursor.write(kCinVersion);
  native_io::writeCString({}, cursor);  // legacy authoring metadata
  cursor.write(static_cast<std::int32_t>(data->bitmaps.size()));
  for (const cin::Bitmap& bitmap : data->bitmaps) {
    cursor.write(bitmap.subdivision_scale);
    std::string stored_path;
    if (!encodeCinIllustrationPath(bitmap.path, stored_path)) return ARX_CIN_BAD_BITMAP_PATH;
    native_io::writeCString(stored_path, cursor);
  }

  cursor.write(static_cast<std::int32_t>(data->sounds.size()));
  for (const cin::Sound& sound : data->sounds) {
    cursor.pad(2);  // unused language tag
    std::string stored_path;
    if (!encodeCinSoundPath(sound, stored_path)) return ARX_CIN_BAD_SOUND_PATH;
    native_io::writeCString(stored_path, cursor);
  }

  cursor.write(std::int32_t{0});
  cursor.write(data->end_frame);
  cursor.write(0.0f);
  cursor.write(data->fps);
  cursor.write(static_cast<std::int32_t>(data->keyframes.size()));
  cursor.write(std::int32_t{1});
  for (const cin::Keyframe& key : data->keyframes) writeKey(key, cursor);
  return cursor ? ARX_OK : ARX_BAD_ALLOC;
}

ArxReturnCode validateCin(const cin::Data* data) {
  if (!data) return ARX_INVALID_DATA_POINTER;
  if (data->bitmaps.empty() ||
      data->bitmaps.size() > static_cast<std::size_t>(std::numeric_limits<std::int32_t>::max()))
    return ARX_CIN_BAD_BITMAP_COUNT;
  for (const cin::Bitmap& bitmap : data->bitmaps) {
    if (!cinematic_constraints::safeGrid(1, 1, bitmap.subdivision_scale, false)) return ARX_CIN_BAD_BITMAP_SCALE;
    if (bitmap.path.empty() || native_io::containsNull(bitmap.path)) return ARX_CIN_BAD_BITMAP_PATH;
    std::string canonical;
    std::string encoded;
    if (!normalizeNativeResourcePath(bitmap.path, canonical) || canonical != bitmap.path ||
        !encodeCinIllustrationPath(bitmap.path, encoded))
      return ARX_CIN_BAD_BITMAP_PATH;
  }

  if (data->sounds.size() > kCinMaxSounds) return ARX_CIN_BAD_SOUND_COUNT;
  for (const cin::Sound& sound : data->sounds) {
    if (sound.path.empty() || native_io::containsNull(sound.path)) return ARX_CIN_BAD_SOUND_PATH;
    std::string canonical;
    std::string encoded;
    if (!normalizeNativeResourcePath(sound.path, canonical) || canonical != sound.path ||
        !encodeCinSoundPath(sound, encoded))
      return ARX_CIN_BAD_SOUND_PATH;
  }

  if (data->end_frame <= 0 || !math::finite(data->fps) || data->fps <= 0.0f) return ARX_CIN_BAD_TRACK;
  if (data->keyframes.size() < 2 ||
      data->keyframes.size() > static_cast<std::size_t>(std::numeric_limits<std::int32_t>::max()))
    return ARX_CIN_BAD_KEY_COUNT;
  if (data->keyframes.front().frame != 0) return ARX_CIN_BAD_KEY_FRAME;

  std::int32_t previous = -1;
  for (std::size_t index = 0; index < data->keyframes.size(); ++index) {
    const cin::Keyframe& key = data->keyframes[index];
    if (key.frame <= previous || key.frame > data->end_frame) return ARX_CIN_BAD_KEY_FRAME;
    if (key.bitmap < 0 || static_cast<std::size_t>(key.bitmap) >= data->bitmaps.size()) return ARX_CIN_BAD_KEY_BITMAP;
    const bool dream_draw =
        ((key.effects >> 8U) & 0xffU) == 1U || (index != 0 && data->keyframes[index - 1U].crossfade == 1 &&
                                                ((data->keyframes[index - 1U].effects >> 8U) & 0xffU) == 1U);
    if (dream_draw && !cinematic_constraints::safeGrid(1, 1, data->bitmaps[key.bitmap].subdivision_scale, true))
      return ARX_CIN_BAD_BITMAP_SCALE;
    if (key.sound < -1 || (key.sound >= 0 && static_cast<std::size_t>(key.sound) >= data->sounds.size()))
      return ARX_CIN_BAD_KEY_SOUND;
    if (key.interpolation < -1 || key.interpolation > 1) return ARX_CIN_BAD_KEY_INTERPOLATION;
    if (key.crossfade != 0 && key.crossfade != 1) return ARX_CIN_BAD_KEY_CROSSFADE;
    if (!finite(key)) return ARX_CIN_BAD_KEY_TRANSFORM;
    if (index + 1U < data->keyframes.size() && key.outgoing_speed <= 0.0f) return ARX_CIN_BAD_KEY_TIMING;
    previous = key.frame;
  }
  return cinematic_constraints::validDuration(data->fps, std::span<const cin::Keyframe>(data->keyframes))
             ? ARX_OK
             : ARX_CIN_BAD_KEY_TIMING;
}

static_assert(kTrackSize == sizeof(std::int32_t) * 4U + sizeof(float) * 2U);

}  // namespace pistoris
