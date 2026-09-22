// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#include "native/amb.h"

#include "arx_pistoris/base/status.h"
#include "arx_pistoris/native/amb.hpp"
#include "arx_pistoris/runtime/types.h"

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
#include <utility>

namespace pistoris {
namespace {

constexpr std::size_t kAmbKeySize = 116;
constexpr amb::TrackFlags kTrackFlagMask = amb::kTrackPosition | amb::kTrackMaster;
constexpr amb::SettingFlags kSettingFlagMask = amb::kSettingRandom | amb::kSettingInterpolate;

ReadCursor& readSetting(amb::Setting& setting, ReadCursor& cursor) {
  cursor.read(setting.min);
  cursor.read(setting.max);
  cursor.read(setting.interval_ms);
  cursor.read(setting.flags);
  setting.flags &= kSettingFlagMask;
  return cursor;
}

ReadCursor& readKey(amb::Key& key, ReadCursor& cursor, bool has_padding) {
  if (has_padding) cursor.skip(sizeof(std::uint32_t));
  cursor.read(key.start_ms);
  cursor.read(key.loop_minus_one);
  cursor.read(key.delay_min_ms);
  cursor.read(key.delay_max_ms);
  readSetting(key.volume, cursor);
  readSetting(key.pitch, cursor);
  readSetting(key.pan, cursor);
  readSetting(key.x, cursor);
  readSetting(key.y, cursor);
  readSetting(key.z, cursor);
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

ArxReturnCode validateKey(const amb::Key& key, bool positioned) {
  if (key.delay_min_ms > key.delay_max_ms) return ARX_AMB_BAD_KEY_TIMING;
  ARX_RETURN_IF_ERR(validateSettingReset(key.volume));
  ARX_RETURN_IF_ERR(validateSettingReset(key.pitch));
  ARX_RETURN_IF_ERR(validateSettingReset(key.pan));
  ARX_RETURN_IF_ERR(validateSettingReset(key.x));
  ARX_RETURN_IF_ERR(validateSettingReset(key.y));
  ARX_RETURN_IF_ERR(validateSettingReset(key.z));

  ARX_RETURN_IF_ERR(validateActiveSetting(key.volume));
  ARX_RETURN_IF_ERR(validateActiveSetting(key.pitch));
  if (positioned) {
    if (!isZeroSetting(key.pan)) return ARX_AMB_UNUSED_SETTING_DATA;
    ARX_RETURN_IF_ERR(validateActiveSetting(key.x));
    ARX_RETURN_IF_ERR(validateActiveSetting(key.y));
    ARX_RETURN_IF_ERR(validateActiveSetting(key.z));
  } else {
    ARX_RETURN_IF_ERR(validateActiveSetting(key.pan));
    if (!isZeroSetting(key.x) || !isZeroSetting(key.y) || !isZeroSetting(key.z)) return ARX_AMB_UNUSED_SETTING_DATA;
  }
  return ARX_OK;
}

}  // namespace

ArxReturnCode loadAmb(amb::Data* data, ReadCursor& cursor) {
  if (!data) return ARX_INVALID_DATA_POINTER;

  std::uint32_t magic = 0;
  std::uint32_t version = 0;
  std::uint32_t track_count = 0;
  cursor.read(magic);
  cursor.read(version);
  cursor.read(track_count);
  if (!cursor) return ARX_UNEXPECTED_EOF;
  if (magic != kAmbMagic) return ARX_INVALID_IDENTIFIER;
  if (version != kAmbVersion1000 && version != kAmbVersion && version != kAmbVersion1002 && version != kAmbVersion1003)
    return ARX_AMB_BAD_VERSION;
  if (track_count == 0) return ARX_AMB_BAD_TRACK_COUNT;

  const std::size_t minimum_track_size =
      version == kAmbVersion1000 ? 1 + kAmbKeySize : (version >= kAmbVersion1002 ? 2 : 1) + 8;
  if (track_count > cursor.remaining() / minimum_track_size) return ARX_UNEXPECTED_EOF;

  amb::Data result;
  if (!tryResize(result.tracks, track_count)) return ARX_BAD_ALLOC;

  for (std::uint32_t track_index = 0; track_index < track_count; ++track_index) {
    amb::Track& track = result.tracks[track_index];
    ARX_RETURN_IF_ERR(native_io::readCString(track.sample_path, cursor));

    if (version >= kAmbVersion1002) {
      std::string discarded_name;
      ARX_RETURN_IF_ERR(native_io::readCString(discarded_name, cursor));
      if (!discarded_name.empty()) {
        log(ARX_LOG_WARN,
            "AMB: track {} name '{}' is unused and was discarded",
            track_index,
            native_text::diagnostic(discarded_name));
      }
    }

    if (version == kAmbVersion1000) {
      if (!tryResize(track.keys, 1)) return ARX_BAD_ALLOC;
      readKey(track.keys.front(), cursor, false);
      cursor.read(track.flags);
      track.flags &= kTrackFlagMask;
      if (!cursor) return ARX_UNEXPECTED_EOF;
      continue;
    }

    std::uint32_t key_count = 0;
    cursor.read(track.flags);
    track.flags &= kTrackFlagMask;
    cursor.read(key_count);
    if (!cursor) return ARX_UNEXPECTED_EOF;
    if (key_count == 0) return ARX_AMB_BAD_KEY_COUNT;
    if (key_count > cursor.remaining() / kAmbKeySize) return ARX_UNEXPECTED_EOF;
    if (!tryResize(track.keys, key_count)) return ARX_BAD_ALLOC;

    for (std::uint32_t physical_index = 0; physical_index < key_count; ++physical_index) {
      const std::uint32_t logical_index = version == kAmbVersion1003 ? key_count - physical_index - 1 : physical_index;
      readKey(track.keys[logical_index], cursor, true);
      if (!cursor) return ARX_UNEXPECTED_EOF;
    }
  }

  ARX_RETURN_IF_ERR(canonicalizeAmb(&result));
  ARX_RETURN_IF_ERR(validateAmb(&result));
  *data = std::move(result);
  log(ARX_LOG_INFO, "AMB loaded: {} tracks", data->tracks.size());
  return ARX_OK;
}

ArxReturnCode canonicalizeAmb(amb::Data* data) {
  if (!data) return ARX_INVALID_DATA_POINTER;
  for (amb::Track& track : data->tracks) {
    std::string canonical_path;
    if (!normalizeNativeResourcePath(track.sample_path, canonical_path)) return ARX_AMB_BAD_SAMPLE_PATH;
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

ArxReturnCode validateAmb(const amb::Data* data) {
  if (!data) return ARX_INVALID_DATA_POINTER;
  if (data->tracks.empty() || data->tracks.size() > std::numeric_limits<std::uint32_t>::max())
    return ARX_AMB_BAD_TRACK_COUNT;

  std::size_t master_count = 0;
  for (const amb::Track& track : data->tracks) {
    if (track.sample_path.empty() || native_io::containsNull(track.sample_path)) return ARX_AMB_BAD_SAMPLE_PATH;
    std::string canonical_path;
    if (!normalizeNativeResourcePath(track.sample_path, canonical_path) || canonical_path != track.sample_path)
      return ARX_AMB_BAD_SAMPLE_PATH;
    if (track.keys.empty() || track.keys.size() > std::numeric_limits<std::uint32_t>::max())
      return ARX_AMB_BAD_KEY_COUNT;
    if ((track.flags & amb::kTrackMaster) != 0) ++master_count;

    const bool positioned = (track.flags & amb::kTrackPosition) != 0;
    for (const amb::Key& key : track.keys) ARX_RETURN_IF_ERR(validateKey(key, positioned));
  }

  if (master_count != 1) return ARX_AMB_BAD_MASTER_COUNT;
  return ARX_OK;
}

}  // namespace pistoris
