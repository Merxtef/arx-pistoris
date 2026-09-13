// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#include "utils/audio.h"

#define DR_WAV_NO_STDIO
#include "dr_libs/dr_wav.h"

#define DR_MP3_NO_STDIO
#include "dr_libs/dr_mp3.h"

#define STB_VORBIS_HEADER_ONLY
#define STB_VORBIS_NO_STDIO
#include "stb/stb_vorbis.c"  // NOLINT(bugprone-suspicious-include)

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <limits>
#include <new>
#include <span>
#include <utility>
#include <vector>

namespace pistoris::audio {
namespace {

struct Decoded {
  Info info;
  std::vector<std::int16_t> samples;
};

Error sampleCount(std::uint64_t frames, std::uint32_t channels, std::uint32_t sample_rate, std::size_t& out) noexcept {
  if (channels == 0 || channels > 2) return Error::kUnsupportedChannels;
  if (frames == 0 || sample_rate == 0) return Error::kMalformed;
  if (frames > kMaxDecodedBytes / sizeof(std::int16_t) / channels) return Error::kTooLarge;
  const std::uint64_t count = frames * channels;
  if (!std::in_range<std::size_t>(count)) return Error::kTooLarge;
  out = static_cast<std::size_t>(count);
  return Error::kNone;
}

Error decodeWav(std::span<const std::uint8_t> encoded, Decoded& out) {
  drwav decoder{};
  if (!drwav_init_memory(&decoder, encoded.data(), encoded.size(), nullptr)) return Error::kMalformed;

  const Info info{Format::kWav, decoder.channels, decoder.sampleRate, decoder.totalPCMFrameCount};
  std::size_t count = 0;
  Error error = sampleCount(info.frame_count, info.channels, info.sample_rate, count);
  if (error != Error::kNone) {
    drwav_uninit(&decoder);
    return error;
  }

  std::vector<std::int16_t> samples(count);
  const drwav_uint64 read = drwav_read_pcm_frames_s16(&decoder, info.frame_count, samples.data());
  drwav_uninit(&decoder);
  if (read != info.frame_count) return Error::kMalformed;
  out.info = info;
  out.samples = std::move(samples);
  return Error::kNone;
}

Error decodeMp3(std::span<const std::uint8_t> encoded, Decoded& out) {
  drmp3 decoder{};
  if (!drmp3_init_memory(&decoder, encoded.data(), encoded.size(), nullptr)) return Error::kMalformed;

  const drmp3_uint64 frames = drmp3_get_pcm_frame_count(&decoder);
  const Info info{Format::kMp3, decoder.channels, decoder.sampleRate, frames};
  std::size_t count = 0;
  Error error = sampleCount(info.frame_count, info.channels, info.sample_rate, count);
  if (error != Error::kNone || !drmp3_seek_to_pcm_frame(&decoder, 0)) {
    drmp3_uninit(&decoder);
    return error == Error::kNone ? Error::kMalformed : error;
  }

  std::vector<std::int16_t> samples(count);
  const drmp3_uint64 read = drmp3_read_pcm_frames_s16(&decoder, info.frame_count, samples.data());
  drmp3_uninit(&decoder);
  if (read != info.frame_count) return Error::kMalformed;
  out.info = info;
  out.samples = std::move(samples);
  return Error::kNone;
}

Error decodeVorbis(std::span<const std::uint8_t> encoded, Decoded& out) {
  if (encoded.size() > static_cast<std::size_t>(std::numeric_limits<int>::max())) return Error::kTooLarge;
  int open_error = VORBIS__no_error;
  stb_vorbis* decoder = stb_vorbis_open_memory(encoded.data(), static_cast<int>(encoded.size()), &open_error, nullptr);
  if (decoder == nullptr) return Error::kMalformed;

  const stb_vorbis_info decoder_info = stb_vorbis_get_info(decoder);
  Info info{Format::kOggVorbis,
            static_cast<std::uint32_t>(decoder_info.channels),
            decoder_info.sample_rate,
            stb_vorbis_stream_length_in_samples(decoder)};
  std::size_t count = 0;
  Error error = sampleCount(info.frame_count, info.channels, info.sample_rate, count);
  if (error != Error::kNone) {
    stb_vorbis_close(decoder);
    return error;
  }

  std::vector<std::int16_t> samples(count);
  std::size_t frame_offset = 0;
  while (frame_offset < info.frame_count) {
    const std::size_t remaining_samples = (static_cast<std::size_t>(info.frame_count) - frame_offset) * info.channels;
    const int request = static_cast<int>(std::min<std::size_t>(remaining_samples, std::numeric_limits<int>::max()));
    const int read = stb_vorbis_get_samples_short_interleaved(
        decoder, static_cast<int>(info.channels), samples.data() + frame_offset * info.channels, request);
    if (read <= 0) break;
    frame_offset += static_cast<std::size_t>(read);
  }
  const int decode_error = stb_vorbis_get_error(decoder);
  stb_vorbis_close(decoder);
  if (decode_error != VORBIS__no_error || frame_offset != info.frame_count) return Error::kMalformed;
  out.info = info;
  out.samples = std::move(samples);
  return Error::kNone;
}

bool startsWith(std::span<const std::uint8_t> encoded, std::span<const std::uint8_t> signature) noexcept {
  return encoded.size() >= signature.size() && std::equal(signature.begin(), signature.end(), encoded.begin());
}

Error inspectWav(std::span<const std::uint8_t> encoded, Info& out) {
  drwav decoder{};
  if (!drwav_init_memory(&decoder, encoded.data(), encoded.size(), nullptr)) return Error::kMalformed;
  const Info info{Format::kWav, decoder.channels, decoder.sampleRate, decoder.totalPCMFrameCount};
  std::size_t count = 0;
  const Error error = sampleCount(info.frame_count, info.channels, info.sample_rate, count);
  drwav_uninit(&decoder);
  if (error != Error::kNone) return error;
  out = info;
  return Error::kNone;
}

Error inspectMp3(std::span<const std::uint8_t> encoded, Info& out) {
  drmp3 decoder{};
  if (!drmp3_init_memory(&decoder, encoded.data(), encoded.size(), nullptr)) return Error::kMalformed;
  const Info info{Format::kMp3, decoder.channels, decoder.sampleRate, drmp3_get_pcm_frame_count(&decoder)};
  std::size_t count = 0;
  const Error error = sampleCount(info.frame_count, info.channels, info.sample_rate, count);
  drmp3_uninit(&decoder);
  if (error != Error::kNone) return error;
  out = info;
  return Error::kNone;
}

Error inspectVorbis(std::span<const std::uint8_t> encoded, Info& out) {
  if (encoded.size() > static_cast<std::size_t>(std::numeric_limits<int>::max())) return Error::kTooLarge;
  int open_error = VORBIS__no_error;
  stb_vorbis* decoder = stb_vorbis_open_memory(encoded.data(), static_cast<int>(encoded.size()), &open_error, nullptr);
  if (decoder == nullptr) return Error::kMalformed;
  const stb_vorbis_info decoder_info = stb_vorbis_get_info(decoder);
  const Info info{Format::kOggVorbis,
                  static_cast<std::uint32_t>(decoder_info.channels),
                  decoder_info.sample_rate,
                  stb_vorbis_stream_length_in_samples(decoder)};
  std::size_t count = 0;
  const Error error = sampleCount(info.frame_count, info.channels, info.sample_rate, count);
  stb_vorbis_close(decoder);
  if (error != Error::kNone) return error;
  out = info;
  return Error::kNone;
}

Error inspectEncoded(std::span<const std::uint8_t> encoded, Info& out) {
  if (encoded.empty()) return Error::kMalformed;
  static constexpr std::array<std::uint8_t, 4> kOgg = {'O', 'g', 'g', 'S'};
  static constexpr std::array<std::uint8_t, 4> kRiff = {'R', 'I', 'F', 'F'};
  static constexpr std::array<std::uint8_t, 4> kRifx = {'R', 'I', 'F', 'X'};
  static constexpr std::array<std::uint8_t, 4> kRf64 = {'R', 'F', '6', '4'};

  if (startsWith(encoded, kRiff) || startsWith(encoded, kRifx) || startsWith(encoded, kRf64))
    return inspectWav(encoded, out);
  if (startsWith(encoded, kOgg)) return inspectVorbis(encoded, out);
  return inspectMp3(encoded, out);
}

Error validateWav(std::span<const std::uint8_t> encoded, Info* out) {
  drwav decoder{};
  if (!drwav_init_memory(&decoder, encoded.data(), encoded.size(), nullptr)) return Error::kMalformed;
  const Info info{Format::kWav, decoder.channels, decoder.sampleRate, decoder.totalPCMFrameCount};
  std::size_t count = 0;
  Error error = sampleCount(info.frame_count, info.channels, info.sample_rate, count);
  std::array<std::int16_t, 8192> samples{};
  std::uint64_t read_total = 0;
  while (error == Error::kNone && read_total < info.frame_count) {
    const std::uint64_t request =
        std::min<std::uint64_t>(info.frame_count - read_total, samples.size() / info.channels);
    const std::uint64_t read = drwav_read_pcm_frames_s16(&decoder, request, samples.data());
    read_total += read;
    if (read != request) error = Error::kMalformed;
  }
  drwav_uninit(&decoder);
  if (error == Error::kNone && out != nullptr) *out = info;
  return error;
}

Error validateMp3(std::span<const std::uint8_t> encoded, Info* out) {
  drmp3 decoder{};
  if (!drmp3_init_memory(&decoder, encoded.data(), encoded.size(), nullptr)) return Error::kMalformed;
  const Info info{Format::kMp3, decoder.channels, decoder.sampleRate, drmp3_get_pcm_frame_count(&decoder)};
  std::size_t count = 0;
  Error error = sampleCount(info.frame_count, info.channels, info.sample_rate, count);
  if (error == Error::kNone && !drmp3_seek_to_pcm_frame(&decoder, 0)) error = Error::kMalformed;
  std::array<std::int16_t, 8192> samples{};
  std::uint64_t read_total = 0;
  while (error == Error::kNone && read_total < info.frame_count) {
    const std::uint64_t request =
        std::min<std::uint64_t>(info.frame_count - read_total, samples.size() / info.channels);
    const std::uint64_t read = drmp3_read_pcm_frames_s16(&decoder, request, samples.data());
    read_total += read;
    if (read != request) error = Error::kMalformed;
  }
  drmp3_uninit(&decoder);
  if (error == Error::kNone && out != nullptr) *out = info;
  return error;
}

Error validateVorbis(std::span<const std::uint8_t> encoded, Info* out) {
  if (encoded.size() > static_cast<std::size_t>(std::numeric_limits<int>::max())) return Error::kTooLarge;
  int open_error = VORBIS__no_error;
  stb_vorbis* decoder = stb_vorbis_open_memory(encoded.data(), static_cast<int>(encoded.size()), &open_error, nullptr);
  if (decoder == nullptr) return Error::kMalformed;
  const stb_vorbis_info decoder_info = stb_vorbis_get_info(decoder);
  const Info info{Format::kOggVorbis,
                  static_cast<std::uint32_t>(decoder_info.channels),
                  decoder_info.sample_rate,
                  stb_vorbis_stream_length_in_samples(decoder)};
  std::size_t count = 0;
  Error error = sampleCount(info.frame_count, info.channels, info.sample_rate, count);
  if (error != Error::kNone) {
    stb_vorbis_close(decoder);
    return error;
  }
  std::array<std::int16_t, 8192> samples{};
  std::uint64_t read_total = 0;
  while (read_total < info.frame_count) {
    const int read = stb_vorbis_get_samples_short_interleaved(
        decoder, static_cast<int>(info.channels), samples.data(), static_cast<int>(samples.size()));
    if (read <= 0) break;
    read_total += static_cast<std::uint64_t>(read);
  }
  const int decode_error = stb_vorbis_get_error(decoder);
  stb_vorbis_close(decoder);
  if (decode_error != VORBIS__no_error || read_total != info.frame_count) return Error::kMalformed;
  if (out != nullptr) *out = info;
  return Error::kNone;
}

Error validateEncoded(std::span<const std::uint8_t> encoded, Info* out) {
  if (encoded.empty()) return Error::kMalformed;
  static constexpr std::array<std::uint8_t, 4> kOgg = {'O', 'g', 'g', 'S'};
  static constexpr std::array<std::uint8_t, 4> kRiff = {'R', 'I', 'F', 'F'};
  static constexpr std::array<std::uint8_t, 4> kRifx = {'R', 'I', 'F', 'X'};
  static constexpr std::array<std::uint8_t, 4> kRf64 = {'R', 'F', '6', '4'};

  if (startsWith(encoded, kRiff) || startsWith(encoded, kRifx) || startsWith(encoded, kRf64))
    return validateWav(encoded, out);
  if (startsWith(encoded, kOgg)) return validateVorbis(encoded, out);
  return validateMp3(encoded, out);
}

Error decode(std::span<const std::uint8_t> encoded, Decoded& out) {
  if (encoded.empty()) return Error::kMalformed;
  static constexpr std::array<std::uint8_t, 4> kOgg = {'O', 'g', 'g', 'S'};
  static constexpr std::array<std::uint8_t, 4> kRiff = {'R', 'I', 'F', 'F'};
  static constexpr std::array<std::uint8_t, 4> kRifx = {'R', 'I', 'F', 'X'};
  static constexpr std::array<std::uint8_t, 4> kRf64 = {'R', 'F', '6', '4'};

  if (startsWith(encoded, kRiff) || startsWith(encoded, kRifx) || startsWith(encoded, kRf64))
    return decodeWav(encoded, out);
  if (startsWith(encoded, kOgg)) return decodeVorbis(encoded, out);
  return decodeMp3(encoded, out);
}

struct WriteContext {
  std::vector<std::uint8_t> bytes;
  bool failed = false;
  bool bad_alloc = false;
};

std::size_t writeBytes(void* user, const void* data, std::size_t size) noexcept {
  auto& context = *static_cast<WriteContext*>(user);
  if (context.failed) return 0;
  try {
    const auto* begin = static_cast<const std::uint8_t*>(data);
    context.bytes.insert(context.bytes.end(), begin, begin + size);
  } catch (const std::bad_alloc&) {
    context.failed = true;
    context.bad_alloc = true;
    return 0;
  } catch (...) {
    context.failed = true;
    return 0;
  }
  return size;
}

Error encodeWav(const Decoded& decoded, std::vector<std::uint8_t>& out) {
  const std::uint64_t pcm_bytes = static_cast<std::uint64_t>(decoded.samples.size()) * sizeof(std::int16_t);
  if (pcm_bytes > kMaxDecodedBytes) return Error::kTooLarge;

  WriteContext context;
  context.bytes.reserve(static_cast<std::size_t>(pcm_bytes) + 128U);
  const drwav_data_format format{
      drwav_container_riff, DR_WAVE_FORMAT_PCM, decoded.info.channels, decoded.info.sample_rate, 16};
  drwav writer{};
  if (!drwav_init_write_sequential_pcm_frames(
          &writer, &format, decoded.info.frame_count, writeBytes, &context, nullptr))
    return context.bad_alloc ? Error::kOutOfMemory : Error::kMalformed;
  const drwav_uint64 written = drwav_write_pcm_frames(&writer, decoded.info.frame_count, decoded.samples.data());
  drwav_uninit(&writer);
  if (written != decoded.info.frame_count || context.failed || context.bytes.empty())
    return context.bad_alloc ? Error::kOutOfMemory : Error::kMalformed;
  out = std::move(context.bytes);
  return Error::kNone;
}

void downmixMono(Decoded& decoded) {
  if (decoded.info.channels != 2) return;
  for (std::size_t frame = 0; frame < decoded.info.frame_count; ++frame) {
    const std::int32_t left = decoded.samples[frame * 2U];
    const std::int32_t right = decoded.samples[frame * 2U + 1U];
    decoded.samples[frame] = static_cast<std::int16_t>((left + right) / 2);
  }
  decoded.samples.resize(static_cast<std::size_t>(decoded.info.frame_count));
  decoded.info.channels = 1;
}

}  // namespace

Error inspect(std::span<const std::uint8_t> encoded, Info* out) noexcept {
  try {
    Info info;
    const Error error = inspectEncoded(encoded, info);
    if (error == Error::kNone && out != nullptr) *out = info;
    return error;
  } catch (const std::bad_alloc&) {
    return Error::kOutOfMemory;
  } catch (...) {
    return Error::kMalformed;
  }
}

Error validate(std::span<const std::uint8_t> encoded, Info* out) noexcept {
  try {
    return validateEncoded(encoded, out);
  } catch (const std::bad_alloc&) {
    return Error::kOutOfMemory;
  } catch (...) {
    return Error::kMalformed;
  }
}

Error transcodeToPcm16Wav(std::span<const std::uint8_t> encoded, bool mono, std::vector<std::uint8_t>& out,
                          Info* out_source) {
  return transcodeToPcm16WavVariants(encoded, !mono, mono, mono ? nullptr : &out, mono ? &out : nullptr, out_source);
}

Error transcodeToPcm16WavVariants(std::span<const std::uint8_t> encoded, bool include_preserved, bool include_mono,
                                  std::vector<std::uint8_t>* out_preserved, std::vector<std::uint8_t>* out_mono,
                                  Info* out_source) {
  if (!include_preserved && !include_mono) return Error::kMalformed;
  if ((include_preserved && out_preserved == nullptr) || (include_mono && out_mono == nullptr) ||
      (include_preserved && include_mono && out_preserved == out_mono))
    return Error::kMalformed;
  try {
    Decoded decoded;
    Error error = decode(encoded, decoded);
    if (error != Error::kNone) return error;
    const Info source = decoded.info;

    std::vector<std::uint8_t> preserved;
    std::vector<std::uint8_t> mono;
    if (include_preserved) {
      error = encodeWav(decoded, preserved);
      if (error != Error::kNone) return error;
    }
    if (include_mono) {
      if (source.channels == 1 && include_preserved) {
        mono = preserved;
      } else {
        downmixMono(decoded);
        error = encodeWav(decoded, mono);
        if (error != Error::kNone) return error;
      }
    }

    if (include_preserved) *out_preserved = std::move(preserved);
    if (include_mono) *out_mono = std::move(mono);
    if (out_source != nullptr) *out_source = source;
    return Error::kNone;
  } catch (const std::bad_alloc&) {
    return Error::kOutOfMemory;
  }
}

}  // namespace pistoris::audio
