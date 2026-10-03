// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#include "native/cin.h"

#include "arx_pistoris/base/location.hpp"
#include "arx_pistoris/base/status.h"
#include "arx_pistoris/native/cin.hpp"
#include "arx_pistoris/native/location.hpp"
#include "arx_pistoris/runtime/types.h"

#include "api/result_failure.h"
#include "native/binary_location.h"
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

ArxReturnCode readLight(cin::Light& light, ReadCursor& cursor, std::size_t key_index) {
  cursor.locate(CinElement::kKeyframe, "light_position", key_index).read(light.position);
  cursor.locate(CinElement::kKeyframe, "light_fall_in", key_index).read(light.fall_in);
  cursor.locate(CinElement::kKeyframe, "light_fall_out", key_index).read(light.fall_out);
  cursor.locate(CinElement::kKeyframe, "light_color", key_index).read(light.color);
  cursor.locate(CinElement::kKeyframe, "light_intensity", key_index).read(light.intensity);
  cursor.locate(CinElement::kKeyframe, "light_random_intensity", key_index).read(light.random_intensity);
  cursor.locate(CinElement::kKeyframe, "light_runtime_links", key_index).skip(8);
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

ArxReturnCode readKey(cin::Keyframe& key, ReadCursor& cursor, std::int32_t version, std::size_t key_index) {
  cursor.locate(CinElement::kKeyframe, "frame", key_index).read(key.frame);
  cursor.locate(CinElement::kKeyframe, "illustration", key_index).read(key.bitmap);
  cursor.locate(CinElement::kKeyframe, "effects", key_index).read(key.effects);
  cursor.locate(CinElement::kKeyframe, "interpolation", key_index).read(key.interpolation);
  cursor.locate(CinElement::kKeyframe, "crossfade", key_index).read(key.crossfade);
  cursor.locate(CinElement::kKeyframe, "camera_position", key_index).read(key.camera_position);
  cursor.locate(CinElement::kKeyframe, "camera_roll", key_index).read(key.camera_roll);
  cursor.locate(CinElement::kKeyframe, "color", key_index).read(key.color);
  cursor.locate(CinElement::kKeyframe, "secondary_color", key_index).read(key.secondary_color);
  cursor.locate(CinElement::kKeyframe, "flash_color", key_index).read(key.flash_color);

  if (version == kCinVersion175)
    cursor.locate(CinElement::kKeyframe, "unused_sound", key_index).skip(4);  // ignored by the engine

  cursor.locate(CinElement::kKeyframe, "flash_decay", key_index).read(key.flash_decay);
  ARX_RETURN_IF_ERR(readLight(key.light, cursor, key_index), cursor);
  cursor.locate(CinElement::kKeyframe, "illustration_position", key_index).read(key.bitmap_position);
  cursor.locate(CinElement::kKeyframe, "illustration_roll", key_index).read(key.bitmap_roll);
  cursor.locate(CinElement::kKeyframe, "outgoing_speed", key_index).read(key.outgoing_speed);

  if (version == kCinVersion) {
    std::array<std::int32_t, 16> sounds = {};
    cursor.locate(CinElement::kKeyframe, "sounds", key_index).read(sounds);
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

CinBinaryResult<cin::Data> loadCin(ReadCursor& cursor, NativeBinaryRegion region) {
  auto fail = [&](ArxReturnCode code, const CursorLocation& location) {
    return api_detail::cinBinaryFailure<cin::Data>(code, native_binary::location<CinElement>(location, region));
  };
  auto fail_cursor = [&](ArxReturnCode code) {
    return api_detail::cinBinaryFailure<cin::Data>(code, native_binary::location<CinElement>(cursor, region));
  };

  std::array<char, 4> magic = {};
  std::int32_t version = 0;
  const CursorLocation magic_location = cursor.mark(CinElement::kHeader, "magic");
  cursor.read(magic);
  const CursorLocation version_location = cursor.mark(CinElement::kHeader, "version");
  cursor.read(version);
  if (!cursor) return fail_cursor(ARX_UNEXPECTED_EOF);
  if (magic != kCinMagic) return fail(ARX_INVALID_IDENTIFIER, magic_location);
  if (version != kCinVersion175 && version != kCinVersion) return fail(ARX_CIN_BAD_VERSION, version_location);

  std::string discarded;
  cursor.locate(CinElement::kHeader, "legacy_name");
  if (const ArxReturnCode code = native_io::readCString(discarded, cursor); code != ARX_OK) return fail_cursor(code);

  cin::Data result;
  std::int32_t bitmap_count = 0;
  const CursorLocation bitmap_count_location = cursor.mark(CinElement::kHeader, "illustration_count");
  cursor.read(bitmap_count);
  if (!cursor) return fail_cursor(ARX_UNEXPECTED_EOF);
  if (bitmap_count <= 0) return fail(ARX_CIN_BAD_BITMAP_COUNT, bitmap_count_location);
  if (static_cast<std::size_t>(bitmap_count) > cursor.remaining() / 5U) {
    cursor.locate(CinElement::kIllustration, "illustrations").skip(static_cast<std::size_t>(bitmap_count) * 5U);
    return fail_cursor(ARX_UNEXPECTED_EOF);
  }
  if (!tryResize(result.bitmaps, static_cast<std::size_t>(bitmap_count)))
    return fail(ARX_BAD_ALLOC, bitmap_count_location);
  for (std::size_t index = 0; index < result.bitmaps.size(); ++index) {
    cin::Bitmap& bitmap = result.bitmaps[index];
    cursor.locate(CinElement::kIllustration, "subdivision_scale", index);
    cursor.read(bitmap.subdivision_scale);
    std::string stored_path;
    cursor.locate(CinElement::kIllustration, "path", index);
    if (const ArxReturnCode code = native_io::readCString(stored_path, cursor); code != ARX_OK)
      return fail_cursor(code);
    if (!decodeCinIllustrationPath(stored_path, bitmap.path)) return fail_cursor(ARX_CIN_BAD_BITMAP_PATH);
  }

  std::int32_t sound_count = 0;
  const CursorLocation sound_count_location = cursor.mark(CinElement::kHeader, "sound_count");
  cursor.read(sound_count);
  if (!cursor) return fail_cursor(ARX_UNEXPECTED_EOF);
  if (sound_count < 0 || static_cast<std::size_t>(sound_count) > kCinMaxSounds)
    return fail(ARX_CIN_BAD_SOUND_COUNT, sound_count_location);
  const std::size_t minimum_sound_size = version == kCinVersion ? 3U : 1U;
  if (static_cast<std::size_t>(sound_count) > cursor.remaining() / minimum_sound_size) {
    cursor.locate(CinElement::kSound, "sounds").skip(static_cast<std::size_t>(sound_count) * minimum_sound_size);
    return fail_cursor(ARX_UNEXPECTED_EOF);
  }
  if (!tryResize(result.sounds, static_cast<std::size_t>(sound_count)))
    return fail(ARX_BAD_ALLOC, sound_count_location);
  for (std::size_t index = 0; index < result.sounds.size(); ++index) {
    cin::Sound& sound = result.sounds[index];
    cursor.locate(CinElement::kSound, "language_tag", index);
    if (version == kCinVersion) cursor.skip(2);  // unused language tag
    std::string stored_path;
    cursor.locate(CinElement::kSound, "path", index);
    if (const ArxReturnCode code = native_io::readCString(stored_path, cursor); code != ARX_OK)
      return fail_cursor(code);
    if (!decodeCinSoundPath(stored_path, sound)) return fail_cursor(ARX_CIN_BAD_SOUND_PATH);
  }

  std::int32_t start_frame = 0;
  float current_frame = 0.0f;
  std::int32_t key_count = 0;
  std::int32_t pause = 0;
  cursor.locate(CinElement::kHeader, "start_frame").read(start_frame);
  cursor.locate(CinElement::kHeader, "end_frame").read(result.end_frame);
  cursor.locate(CinElement::kHeader, "current_frame").read(current_frame);
  cursor.locate(CinElement::kHeader, "fps").read(result.fps);
  const CursorLocation key_count_location = cursor.mark(CinElement::kHeader, "keyframe_count");
  cursor.read(key_count);
  cursor.locate(CinElement::kHeader, "pause").read(pause);
  if (!cursor) return fail_cursor(ARX_UNEXPECTED_EOF);
  if (key_count < 0) return fail(ARX_CIN_BAD_KEY_COUNT, key_count_location);
  const std::size_t key_size = version == kCinVersion ? kKey176Size : kKey175Size;
  if (static_cast<std::size_t>(key_count) > cursor.remaining() / key_size) {
    cursor.locate(CinElement::kKeyframe, "keyframes").skip(static_cast<std::size_t>(key_count) * key_size);
    return fail_cursor(ARX_UNEXPECTED_EOF);
  }

  std::vector<PhysicalKey> physical;
  if (!tryResize(physical, static_cast<std::size_t>(key_count))) return fail(ARX_BAD_ALLOC, key_count_location);
  for (std::size_t index = 0; index < physical.size(); ++index) {
    physical[index].order = index;
    if (const ArxReturnCode code = readKey(physical[index].value, cursor, version, index); code != ARX_OK)
      return fail_cursor(code);
  }

  std::sort(physical.begin(), physical.end(), [](const PhysicalKey& left, const PhysicalKey& right) {
    if (left.value.frame != right.value.frame) return left.value.frame < right.value.frame;
    return left.order < right.order;
  });
  if (!tryResize(result.keyframes, physical.size())) return fail(ARX_BAD_ALLOC, key_count_location);
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

  CinLocation validation_location;
  if (const ArxReturnCode code = validateCin(&result, &validation_location); code != ARX_OK)
    return api_detail::cinBinaryFailure<cin::Data>(code, native_binary::semanticLocation(validation_location, region));
  if (start_frame != 0) log(ARX_LOG_WARN, "CIN: start frame {} discarded; playback starts at 0", start_frame);
  log(ARX_LOG_INFO,
      "CIN loaded: {} keyframes, {} illustrations, {} sounds, timeline {} frames at {:.3g} fps",
      result.keyframes.size(),
      result.bitmaps.size(),
      result.sounds.size(),
      result.end_frame,
      result.fps);
  return CinBinaryResult<cin::Data>::success(std::move(result));
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

ArxReturnCode validateCin(const cin::Data* data, CinLocation* failure_location) {
  auto fail = [failure_location](ArxReturnCode code,
                                 CinElement element = CinElement::kHeader,
                                 std::size_t index = kNoElementIndex,
                                 std::string field = {}) {
    if (failure_location) *failure_location = {.element = element, .index = index, .field = std::move(field)};
    return code;
  };
  if (!data) return fail(ARX_INVALID_DATA_POINTER);
  if (data->bitmaps.empty() ||
      data->bitmaps.size() > static_cast<std::size_t>(std::numeric_limits<std::int32_t>::max()))
    return fail(ARX_CIN_BAD_BITMAP_COUNT, CinElement::kHeader, kNoElementIndex, "bitmaps");
  for (std::size_t index = 0; index < data->bitmaps.size(); ++index) {
    const cin::Bitmap& bitmap = data->bitmaps[index];
    if (!cinematic_constraints::safeGrid(1, 1, bitmap.subdivision_scale, false))
      return fail(ARX_CIN_BAD_BITMAP_SCALE, CinElement::kIllustration, index, "subdivision_scale");
    if (bitmap.path.empty() || native_io::containsNull(bitmap.path))
      return fail(ARX_CIN_BAD_BITMAP_PATH, CinElement::kIllustration, index, "path");
    std::string canonical;
    std::string encoded;
    if (!normalizeNativeResourcePath(bitmap.path, canonical) || canonical != bitmap.path ||
        !encodeCinIllustrationPath(bitmap.path, encoded))
      return fail(ARX_CIN_BAD_BITMAP_PATH, CinElement::kIllustration, index, "path");
  }

  if (data->sounds.size() > kCinMaxSounds)
    return fail(ARX_CIN_BAD_SOUND_COUNT, CinElement::kHeader, kNoElementIndex, "sounds");
  for (std::size_t index = 0; index < data->sounds.size(); ++index) {
    const cin::Sound& sound = data->sounds[index];
    if (sound.path.empty() || native_io::containsNull(sound.path))
      return fail(ARX_CIN_BAD_SOUND_PATH, CinElement::kSound, index, "path");
    std::string canonical;
    std::string encoded;
    if (!normalizeNativeResourcePath(sound.path, canonical) || canonical != sound.path ||
        !encodeCinSoundPath(sound, encoded))
      return fail(ARX_CIN_BAD_SOUND_PATH, CinElement::kSound, index, "path");
  }

  if (data->end_frame <= 0) return fail(ARX_CIN_BAD_TRACK, CinElement::kHeader, kNoElementIndex, "end_frame");
  if (!math::finite(data->fps) || data->fps <= 0.0f)
    return fail(ARX_CIN_BAD_TRACK, CinElement::kHeader, kNoElementIndex, "fps");
  if (data->keyframes.size() < 2 ||
      data->keyframes.size() > static_cast<std::size_t>(std::numeric_limits<std::int32_t>::max()))
    return fail(ARX_CIN_BAD_KEY_COUNT, CinElement::kHeader, kNoElementIndex, "keyframes");
  if (data->keyframes.front().frame != 0) return fail(ARX_CIN_BAD_KEY_FRAME, CinElement::kKeyframe, 0, "frame");

  std::int32_t previous = -1;
  for (std::size_t index = 0; index < data->keyframes.size(); ++index) {
    const cin::Keyframe& key = data->keyframes[index];
    if (key.frame <= previous || key.frame > data->end_frame)
      return fail(ARX_CIN_BAD_KEY_FRAME, CinElement::kKeyframe, index, "frame");
    if (key.bitmap < 0 || static_cast<std::size_t>(key.bitmap) >= data->bitmaps.size())
      return fail(ARX_CIN_BAD_KEY_BITMAP, CinElement::kKeyframe, index, "bitmap");
    const bool dream_draw =
        ((key.effects >> 8U) & 0xffU) == 1U || (index != 0 && data->keyframes[index - 1U].crossfade == 1 &&
                                                ((data->keyframes[index - 1U].effects >> 8U) & 0xffU) == 1U);
    if (dream_draw && !cinematic_constraints::safeGrid(1, 1, data->bitmaps[key.bitmap].subdivision_scale, true))
      return fail(ARX_CIN_BAD_BITMAP_SCALE,
                  CinElement::kIllustration,
                  static_cast<std::size_t>(key.bitmap),
                  "subdivision_scale");
    if (key.sound < -1 || (key.sound >= 0 && static_cast<std::size_t>(key.sound) >= data->sounds.size()))
      return fail(ARX_CIN_BAD_KEY_SOUND, CinElement::kKeyframe, index, "sound");
    if (key.interpolation < -1 || key.interpolation > 1)
      return fail(ARX_CIN_BAD_KEY_INTERPOLATION, CinElement::kKeyframe, index, "interpolation");
    if (key.crossfade != 0 && key.crossfade != 1)
      return fail(ARX_CIN_BAD_KEY_CROSSFADE, CinElement::kKeyframe, index, "crossfade");
    if (!finite(key)) {
      std::string_view field;
      if (!math::finite(key.camera_position))
        field = "camera_position";
      else if (!math::finite(key.camera_roll))
        field = "camera_roll";
      else if (!math::finite(key.bitmap_position))
        field = "bitmap_position";
      else if (!math::finite(key.bitmap_roll))
        field = "bitmap_roll";
      else if (!math::finite(key.outgoing_speed))
        field = "outgoing_speed";
      else if (((key.effects >> 16U) & 0xffU) == 1U && !math::finite(key.flash_decay))
        field = "flash_decay";
      else if (!math::finite(key.light.intensity))
        field = "light.intensity";
      else if (!math::finite(key.light.position))
        field = "light.position";
      else if (!math::finite(key.light.fall_in))
        field = "light.fall_in";
      else if (!math::finite(key.light.fall_out))
        field = "light.fall_out";
      else if (!math::finite(key.light.color))
        field = "light.color";
      else if (!math::finite(key.light.random_intensity))
        field = "light.random_intensity";
      return fail(ARX_CIN_BAD_KEY_TRANSFORM, CinElement::kKeyframe, index, std::string(field));
    }
    if (index + 1U < data->keyframes.size() && key.outgoing_speed <= 0.0f)
      return fail(ARX_CIN_BAD_KEY_TIMING, CinElement::kKeyframe, index, "outgoing_speed");
    previous = key.frame;
  }
  return cinematic_constraints::validDuration(data->fps, std::span<const cin::Keyframe>(data->keyframes))
             ? ARX_OK
             : fail(ARX_CIN_BAD_KEY_TIMING, CinElement::kHeader, kNoElementIndex, "keyframes");
}

static_assert(kTrackSize == sizeof(std::int32_t) * 4U + sizeof(float) * 2U);

}  // namespace pistoris
