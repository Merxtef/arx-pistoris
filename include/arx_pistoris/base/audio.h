// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#ifndef ARX_PISTORIS_BASE_AUDIO_H
#define ARX_PISTORIS_BASE_AUDIO_H

#include <stddef.h>
#include <stdint.h>

// NOLINTBEGIN(readability-identifier-naming, performance-enum-size)

/* Non-owning encoded audio; {NULL, 0} empty */
typedef struct ArxEncodedAudioView {
  const uint8_t* data;
  size_t size;
} ArxEncodedAudioView;

typedef uint8_t ArxAudioFormat;
enum {
  ARX_AUDIO_FORMAT_UNKNOWN = 0,
  ARX_AUDIO_FORMAT_WAV,
  ARX_AUDIO_FORMAT_MP3,
  ARX_AUDIO_FORMAT_OGG_VORBIS,
};

typedef struct ArxAudioInfo {
  ArxAudioFormat format;
  uint32_t channels;
  uint32_t sample_rate;
  uint64_t frame_count;
} ArxAudioInfo;

// NOLINTEND(readability-identifier-naming, performance-enum-size)

#endif /* ARX_PISTORIS_BASE_AUDIO_H */
