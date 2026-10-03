// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#include "native/amb.h"

#include "arx_pistoris/base/location.hpp"
#include "arx_pistoris/base/status.h"
#include "arx_pistoris/native/amb.hpp"
#include "arx_pistoris/native/location.hpp"
#include "arx_pistoris/runtime/types.h"

#include "api/result_failure.h"
#include "native/binary_location.h"
#include "native/c_string.h"
#include "native/resource_lookup.h"
#include "native/resource_path.h"
#include "utils/container_allocation.h"
#include "utils/cursor.h"
#include "utils/log.h"
#include "utils/native_text.h"
#include "utils/return_code.h"

#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <string>
#include <string_view>
#include <utility>

namespace pistoris {
namespace {

constexpr std::size_t kAmbKeySize = 116;
constexpr amb::TrackFlags kTrackFlagMask = amb::kTrackPosition | amb::kTrackMaster;
constexpr amb::SettingFlags kSettingFlagMask = amb::kSettingRandom | amb::kSettingInterpolate;

ReadCursor& readSetting(amb::Setting& setting, ReadCursor& cursor, std::size_t track_index, std::size_t key_index,
                        std::string_view min_field, std::string_view max_field, std::string_view interval_field,
                        std::string_view flags_field) {
  cursor.locate(AmbElement::kKey, min_field, track_index, key_index).read(setting.min);
  cursor.locate(AmbElement::kKey, max_field, track_index, key_index).read(setting.max);
  cursor.locate(AmbElement::kKey, interval_field, track_index, key_index).read(setting.interval_ms);
  cursor.locate(AmbElement::kKey, flags_field, track_index, key_index).read(setting.flags);
  setting.flags &= kSettingFlagMask;
  return cursor;
}

ReadCursor& readKey(amb::Key& key, ReadCursor& cursor, bool has_padding, std::size_t track_index,
                    std::size_t key_index) {
  if (has_padding) cursor.locate(AmbElement::kKey, "padding", track_index, key_index).skip(sizeof(std::uint32_t));
  cursor.locate(AmbElement::kKey, "start_ms", track_index, key_index).read(key.start_ms);
  cursor.locate(AmbElement::kKey, "loop_count", track_index, key_index).read(key.loop_minus_one);
  cursor.locate(AmbElement::kKey, "delay_min_ms", track_index, key_index).read(key.delay_min_ms);
  cursor.locate(AmbElement::kKey, "delay_max_ms", track_index, key_index).read(key.delay_max_ms);
  readSetting(
      key.volume, cursor, track_index, key_index, "volume_min", "volume_max", "volume_interval_ms", "volume_flags");
  readSetting(key.pitch, cursor, track_index, key_index, "pitch_min", "pitch_max", "pitch_interval_ms", "pitch_flags");
  readSetting(key.pan, cursor, track_index, key_index, "pan_min", "pan_max", "pan_interval_ms", "pan_flags");
  readSetting(key.x, cursor, track_index, key_index, "x_min", "x_max", "x_interval_ms", "x_flags");
  readSetting(key.y, cursor, track_index, key_index, "y_min", "y_max", "y_interval_ms", "y_flags");
  readSetting(key.z, cursor, track_index, key_index, "z_min", "z_max", "z_interval_ms", "z_flags");
  return cursor;
}

WriteCursor& writeSetting(const amb::Setting& setting, WriteCursor& cursor) {
  cursor.write(setting.min);
  cursor.write(setting.max);
  cursor.write(setting.interval_ms);
  cursor.write(setting.flags & kSettingFlagMask);
  return cursor;
}

WriteCursor& writeKey(const amb::Key& key, WriteCursor& cursor) {
  cursor.pad(sizeof(std::uint32_t));
  cursor.write(key.start_ms);
  cursor.write(key.loop_minus_one);
  cursor.write(key.delay_min_ms);
  cursor.write(key.delay_max_ms);
  writeSetting(key.volume, cursor);
  writeSetting(key.pitch, cursor);
  writeSetting(key.pan, cursor);
  writeSetting(key.x, cursor);
  writeSetting(key.y, cursor);
  writeSetting(key.z, cursor);
  return cursor;
}

bool isZeroSetting(const amb::Setting& setting) {
  return setting.min == 0.0f && setting.max == 0.0f && setting.interval_ms == 0 && setting.flags == 0;
}

void canonicalizeActiveSetting(amb::Setting& setting) {
  setting.flags &= kSettingFlagMask;
  if (setting.min == setting.max) {
    setting.interval_ms = 0;
    setting.flags = 0;
  }
}

void canonicalizeKey(amb::Key& key, bool positioned) {
  canonicalizeActiveSetting(key.volume);
  canonicalizeActiveSetting(key.pitch);
  if (positioned) {
    key.pan = {};
    canonicalizeActiveSetting(key.x);
    canonicalizeActiveSetting(key.y);
    canonicalizeActiveSetting(key.z);
  } else {
    canonicalizeActiveSetting(key.pan);
    key.x = {};
    key.y = {};
    key.z = {};
  }
}

bool settingSpanFitsFloat(const amb::Setting& setting) {
  const double span = std::abs(static_cast<double>(setting.max) - static_cast<double>(setting.min));
  return span <= std::numeric_limits<float>::max();
}

ArxReturnCode validateSettingReset(const amb::Setting& setting) {
  if ((setting.flags & amb::kSettingRandom) == 0 || setting.min == setting.max) return ARX_OK;
  if (!std::isfinite(setting.min) || !std::isfinite(setting.max) || setting.min > setting.max ||
      !settingSpanFitsFloat(setting))
    return ARX_AMB_BAD_SETTING;
  return ARX_OK;
}

ArxReturnCode validateActiveSetting(const amb::Setting& setting) {
  if (!std::isfinite(setting.min) || !std::isfinite(setting.max)) return ARX_AMB_BAD_SETTING;
  if (setting.min == setting.max)
    return setting.interval_ms == 0 && setting.flags == 0 ? ARX_OK : ARX_AMB_UNUSED_SETTING_DATA;
  if ((setting.flags & amb::kSettingInterpolate) != 0 && (setting.interval_ms == 0 || !settingSpanFitsFloat(setting)))
    return ARX_AMB_BAD_SETTING;
  return ARX_OK;
}

ArxReturnCode validateKey(const amb::Key& key, bool positioned, std::string_view& failure_field) {
  const auto validate_setting = [&failure_field](const amb::Setting& setting, std::string_view field, bool active) {
    const ArxReturnCode code = active ? validateActiveSetting(setting) : validateSettingReset(setting);
    if (code != ARX_OK) failure_field = field;
    return code;
  };
  if (key.delay_min_ms > key.delay_max_ms) {
    failure_field = "delay_min_ms";
    return ARX_AMB_BAD_KEY_TIMING;
  }
  ARX_RETURN_IF_ERR(validate_setting(key.volume, "volume", false));
  ARX_RETURN_IF_ERR(validate_setting(key.pitch, "pitch", false));
  ARX_RETURN_IF_ERR(validate_setting(key.pan, "pan", false));
  ARX_RETURN_IF_ERR(validate_setting(key.x, "x", false));
  ARX_RETURN_IF_ERR(validate_setting(key.y, "y", false));
  ARX_RETURN_IF_ERR(validate_setting(key.z, "z", false));

  ARX_RETURN_IF_ERR(validate_setting(key.volume, "volume", true));
  ARX_RETURN_IF_ERR(validate_setting(key.pitch, "pitch", true));
  if (positioned) {
    if (!isZeroSetting(key.pan)) {
      failure_field = "pan";
      return ARX_AMB_UNUSED_SETTING_DATA;
    }
    ARX_RETURN_IF_ERR(validate_setting(key.x, "x", true));
    ARX_RETURN_IF_ERR(validate_setting(key.y, "y", true));
    ARX_RETURN_IF_ERR(validate_setting(key.z, "z", true));
  } else {
    ARX_RETURN_IF_ERR(validate_setting(key.pan, "pan", true));
    if (!isZeroSetting(key.x)) {
      failure_field = "x";
      return ARX_AMB_UNUSED_SETTING_DATA;
    }
    if (!isZeroSetting(key.y)) {
      failure_field = "y";
      return ARX_AMB_UNUSED_SETTING_DATA;
    }
    if (!isZeroSetting(key.z)) {
      failure_field = "z";
      return ARX_AMB_UNUSED_SETTING_DATA;
    }
  }
  return ARX_OK;
}

}  // namespace

AmbBinaryResult<amb::Data> loadAmb(ReadCursor& cursor, NativeBinaryRegion region) {
  auto fail = [&](ArxReturnCode code, const CursorLocation& location) {
    return api_detail::ambBinaryFailure<amb::Data>(code, native_binary::location<AmbElement>(location, region));
  };
  auto fail_cursor = [&](ArxReturnCode code) {
    return api_detail::ambBinaryFailure<amb::Data>(code, native_binary::location<AmbElement>(cursor, region));
  };

  amb::Data result;
  std::uint32_t magic = 0;
  std::uint32_t version = 0;
  std::uint32_t track_count = 0;
  const CursorLocation magic_location = cursor.mark(AmbElement::kHeader, "magic");
  cursor.read(magic);
  const CursorLocation version_location = cursor.mark(AmbElement::kHeader, "version");
  cursor.read(version);
  const CursorLocation track_count_location = cursor.mark(AmbElement::kHeader, "track_count");
  cursor.read(track_count);
  if (!cursor) return fail_cursor(ARX_UNEXPECTED_EOF);
  if (magic != kAmbMagic) return fail(ARX_INVALID_IDENTIFIER, magic_location);
  if (version != kAmbVersion1000 && version != kAmbVersion && version != kAmbVersion1002 && version != kAmbVersion1003)
    return fail(ARX_AMB_BAD_VERSION, version_location);
  if (track_count == 0) return fail(ARX_AMB_BAD_TRACK_COUNT, track_count_location);

  const std::size_t minimum_track_size =
      version == kAmbVersion1000 ? 1 + kAmbKeySize : (version >= kAmbVersion1002 ? 2 : 1) + 8;
  if (track_count > cursor.remaining() / minimum_track_size) {
    cursor.locate(AmbElement::kTrack, "tracks").skip(static_cast<std::size_t>(track_count) * minimum_track_size);
    return fail_cursor(ARX_UNEXPECTED_EOF);
  }

  if (!tryResize(result.tracks, track_count)) return fail(ARX_BAD_ALLOC, track_count_location);

  for (std::uint32_t track_index = 0; track_index < track_count; ++track_index) {
    amb::Track& track = result.tracks[track_index];
    cursor.locate(AmbElement::kTrack, "sample_path", track_index);
    if (const ArxReturnCode code = native_io::readCString(track.sample_path, cursor); code != ARX_OK)
      return fail_cursor(code);

    if (version >= kAmbVersion1002) {
      std::string discarded_name;
      cursor.locate(AmbElement::kTrack, "legacy_name", track_index);
      if (const ArxReturnCode code = native_io::readCString(discarded_name, cursor); code != ARX_OK)
        return fail_cursor(code);
      if (!discarded_name.empty()) {
        log(ARX_LOG_WARN,
            "AMB: track {} name '{}' is unused and was discarded",
            track_index,
            native_text::diagnostic(discarded_name));
      }
    }

    if (version == kAmbVersion1000) {
      if (!tryResize(track.keys, 1)) return fail_cursor(ARX_BAD_ALLOC);
      readKey(track.keys.front(), cursor, false, track_index, 0);
      cursor.locate(AmbElement::kTrack, "flags", track_index);
      cursor.read(track.flags);
      track.flags &= kTrackFlagMask;
      if (!cursor) return fail_cursor(ARX_UNEXPECTED_EOF);
      continue;
    }

    std::uint32_t key_count = 0;
    cursor.locate(AmbElement::kTrack, "flags", track_index);
    cursor.read(track.flags);
    track.flags &= kTrackFlagMask;
    const CursorLocation key_count_location = cursor.mark(AmbElement::kTrack, "key_count", track_index);
    cursor.read(key_count);
    if (!cursor) return fail_cursor(ARX_UNEXPECTED_EOF);
    if (key_count == 0) return fail(ARX_AMB_BAD_KEY_COUNT, key_count_location);
    if (key_count > cursor.remaining() / kAmbKeySize) {
      cursor.locate(AmbElement::kKey, "keys", track_index).skip(static_cast<std::size_t>(key_count) * kAmbKeySize);
      return fail_cursor(ARX_UNEXPECTED_EOF);
    }
    if (!tryResize(track.keys, key_count)) return fail(ARX_BAD_ALLOC, key_count_location);

    for (std::uint32_t physical_index = 0; physical_index < key_count; ++physical_index) {
      const std::uint32_t logical_index = version == kAmbVersion1003 ? key_count - physical_index - 1 : physical_index;
      readKey(track.keys[logical_index], cursor, true, track_index, logical_index);
      if (!cursor) return fail_cursor(ARX_UNEXPECTED_EOF);
    }
  }

  AmbLocation semantic_location;
  if (const ArxReturnCode code = canonicalizeAmb(&result, &semantic_location); code != ARX_OK)
    return api_detail::ambBinaryFailure<amb::Data>(code, native_binary::semanticLocation(semantic_location, region));
  AmbLocation validation_location;
  if (const ArxReturnCode code = validateAmb(&result, &validation_location); code != ARX_OK) {
    return api_detail::ambBinaryFailure<amb::Data>(code, native_binary::semanticLocation(validation_location, region));
  }
  log(ARX_LOG_INFO, "AMB loaded: {} tracks", result.tracks.size());
  return AmbBinaryResult<amb::Data>::success(std::move(result));
}

ArxReturnCode canonicalizeAmb(amb::Data* data, AmbLocation* failure_location) {
  auto fail = [failure_location](ArxReturnCode code,
                                 AmbElement element = AmbElement::kHeader,
                                 std::size_t index = kNoElementIndex,
                                 std::string field = {}) {
    if (failure_location) *failure_location = {.element = element, .index = index, .field = std::move(field)};
    return code;
  };
  if (!data) return fail(ARX_INVALID_DATA_POINTER);
  for (std::size_t index = 0; index < data->tracks.size(); ++index) {
    amb::Track& track = data->tracks[index];
    std::string canonical_path;
    if (!normalizeNativeResourcePath(track.sample_path, canonical_path))
      return fail(ARX_AMB_BAD_SAMPLE_PATH, AmbElement::kTrack, index, "sample_path");
    track.sample_path = std::move(canonical_path);
    track.flags &= kTrackFlagMask;
    const bool positioned = (track.flags & amb::kTrackPosition) != 0;
    for (amb::Key& key : track.keys) canonicalizeKey(key, positioned);
  }
  return ARX_OK;
}

ArxReturnCode saveAmb(const amb::Data* data, WriteCursor& cursor) {
  ARX_RETURN_IF_ERR(validateAmb(data));

  for (std::size_t index = 0; index < data->tracks.size(); ++index) {
    const std::string& path = data->tracks[index].sample_path;
    if (!resolvesThroughDefaultLooseRoot({}, path)) {
      log(ARX_LOG_WARN,
          "AMB saving: track[{}] sample path '{}' resolves outside Libertatis default loose roots; "
          "it may not be discovered",
          index,
          native_text::diagnostic(path));
    }
  }

  cursor.write(kAmbMagic);
  cursor.write(kAmbVersion);
  cursor.write(static_cast<std::uint32_t>(data->tracks.size()));
  for (const amb::Track& track : data->tracks) {
    native_io::writeCString(track.sample_path, cursor);
    cursor.write(track.flags & kTrackFlagMask);
    cursor.write(static_cast<std::uint32_t>(track.keys.size()));
    for (const amb::Key& key : track.keys) writeKey(key, cursor);
  }

  if (!cursor) return ARX_BAD_ALLOC;
  log(ARX_LOG_INFO, "AMB saving: {} tracks", data->tracks.size());
  return ARX_OK;
}

ArxReturnCode validateAmb(const amb::Data* data, AmbLocation* failure_location) {
  auto fail = [failure_location](ArxReturnCode code,
                                 AmbElement element = AmbElement::kHeader,
                                 std::size_t index = kNoElementIndex,
                                 std::size_t subindex = kNoElementIndex,
                                 std::string field = {}) {
    if (failure_location)
      *failure_location = {.element = element, .index = index, .subindex = subindex, .field = std::move(field)};
    return code;
  };
  if (!data) return fail(ARX_INVALID_DATA_POINTER);
  if (data->tracks.empty() || data->tracks.size() > std::numeric_limits<std::uint32_t>::max())
    return fail(ARX_AMB_BAD_TRACK_COUNT, AmbElement::kHeader, kNoElementIndex, kNoElementIndex, "tracks");

  std::size_t master_count = 0;
  for (std::size_t track_index = 0; track_index < data->tracks.size(); ++track_index) {
    const amb::Track& track = data->tracks[track_index];
    if (track.sample_path.empty() || native_io::containsNull(track.sample_path))
      return fail(ARX_AMB_BAD_SAMPLE_PATH, AmbElement::kTrack, track_index, kNoElementIndex, "sample_path");
    std::string canonical_path;
    if (!normalizeNativeResourcePath(track.sample_path, canonical_path) || canonical_path != track.sample_path)
      return fail(ARX_AMB_BAD_SAMPLE_PATH, AmbElement::kTrack, track_index, kNoElementIndex, "sample_path");
    if (track.keys.empty() || track.keys.size() > std::numeric_limits<std::uint32_t>::max())
      return fail(ARX_AMB_BAD_KEY_COUNT, AmbElement::kTrack, track_index, kNoElementIndex, "keys");
    if ((track.flags & amb::kTrackMaster) != 0) ++master_count;

    const bool positioned = (track.flags & amb::kTrackPosition) != 0;
    for (std::size_t key_index = 0; key_index < track.keys.size(); ++key_index) {
      std::string_view field;
      const ArxReturnCode rc = validateKey(track.keys[key_index], positioned, field);
      if (rc != ARX_OK) return fail(rc, AmbElement::kKey, track_index, key_index, std::string(field));
    }
  }

  if (master_count != 1)
    return fail(ARX_AMB_BAD_MASTER_COUNT, AmbElement::kHeader, kNoElementIndex, kNoElementIndex, "tracks");
  return ARX_OK;
}

}  // namespace pistoris
