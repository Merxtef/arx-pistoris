// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#pragma once

#include "arx_pistoris/arx_pistoris.h"

#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <ios>
#include <iterator>
#include <utility>
#include <vector>

namespace arx_fuzz {

inline void discardLog(ArxLogLevel, const char*, void*) {}

inline void silenceLogs() {
  static const bool done = [] {
    arx_pistoris_set_log_callback(discardLog, nullptr);
    return true;
  }();
  (void)done;
}

struct ByteBuffer {
  std::uint8_t* value = nullptr;
  std::size_t byte_count = 0;

  ByteBuffer() = default;
  ByteBuffer(const ByteBuffer&) = delete;
  ByteBuffer& operator=(const ByteBuffer&) = delete;
  ByteBuffer(ByteBuffer&& other) noexcept
      : value(std::exchange(other.value, nullptr)), byte_count(std::exchange(other.byte_count, 0)) {}
  ByteBuffer& operator=(ByteBuffer&& other) noexcept {
    if (this == &other) return *this;
    reset();
    value = std::exchange(other.value, nullptr);
    byte_count = std::exchange(other.byte_count, 0);
    return *this;
  }
  ~ByteBuffer() { reset(); }

  void reset() noexcept {
    arx_pistoris_free_bytes(value);
    value = nullptr;
    byte_count = 0;
  }

  [[nodiscard]] std::uint8_t* get() const noexcept { return value; }
  [[nodiscard]] std::size_t size() const noexcept { return byte_count; }
};

struct FtlHandle {
  ArxFtlHandle value = nullptr;

  FtlHandle() = default;
  explicit FtlHandle(ArxFtlHandle handle) : value(handle) {}
  FtlHandle(const FtlHandle&) = delete;
  FtlHandle& operator=(const FtlHandle&) = delete;
  FtlHandle(FtlHandle&& other) noexcept : value(std::exchange(other.value, nullptr)) {}
  FtlHandle& operator=(FtlHandle&& other) noexcept {
    if (this == &other) return *this;
    reset();
    value = std::exchange(other.value, nullptr);
    return *this;
  }
  ~FtlHandle() { reset(); }

  void reset() noexcept {
    arx_pistoris_ftl_free(value);
    value = nullptr;
  }

  [[nodiscard]] ArxFtlHandle get() const noexcept { return value; }
};

struct TeaHandle {
  ArxTeaHandle value = nullptr;

  TeaHandle() = default;
  explicit TeaHandle(ArxTeaHandle handle) : value(handle) {}
  TeaHandle(const TeaHandle&) = delete;
  TeaHandle& operator=(const TeaHandle&) = delete;
  TeaHandle(TeaHandle&& other) noexcept : value(std::exchange(other.value, nullptr)) {}
  TeaHandle& operator=(TeaHandle&& other) noexcept {
    if (this == &other) return *this;
    reset();
    value = std::exchange(other.value, nullptr);
    return *this;
  }
  ~TeaHandle() { reset(); }

  void reset() noexcept {
    arx_pistoris_tea_free(value);
    value = nullptr;
  }

  [[nodiscard]] ArxTeaHandle get() const noexcept { return value; }
};

struct TeaArray {
  ArxTeaHandle* value = nullptr;
  std::size_t count = 0;

  TeaArray() = default;
  TeaArray(ArxTeaHandle* handles, std::size_t handle_count) : value(handles), count(handle_count) {}
  TeaArray(const TeaArray&) = delete;
  TeaArray& operator=(const TeaArray&) = delete;
  TeaArray(TeaArray&& other) noexcept
      : value(std::exchange(other.value, nullptr)), count(std::exchange(other.count, 0)) {}
  TeaArray& operator=(TeaArray&& other) noexcept {
    if (this == &other) return *this;
    reset();
    value = std::exchange(other.value, nullptr);
    count = std::exchange(other.count, 0);
    return *this;
  }
  ~TeaArray() { reset(); }

  void reset() noexcept {
    arx_pistoris_free_tea_array(value, count);
    value = nullptr;
    count = 0;
  }

  [[nodiscard]] ArxTeaHandle* get() const noexcept { return value; }
  [[nodiscard]] std::size_t size() const noexcept { return count; }
};

struct FtsHandle {
  ArxFts* value = nullptr;

  FtsHandle() = default;
  explicit FtsHandle(ArxFts* handle) : value(handle) {}
  FtsHandle(const FtsHandle&) = delete;
  FtsHandle& operator=(const FtsHandle&) = delete;
  FtsHandle(FtsHandle&& other) noexcept : value(std::exchange(other.value, nullptr)) {}
  FtsHandle& operator=(FtsHandle&& other) noexcept {
    if (this == &other) return *this;
    reset();
    value = std::exchange(other.value, nullptr);
    return *this;
  }
  ~FtsHandle() { reset(); }

  void reset() noexcept {
    arx_pistoris_fts_destroy(value);
    value = nullptr;
  }

  [[nodiscard]] ArxFts* get() const noexcept { return value; }
};

struct LlfHandle {
  ArxLlf* value = nullptr;

  LlfHandle() = default;
  explicit LlfHandle(ArxLlf* handle) : value(handle) {}
  LlfHandle(const LlfHandle&) = delete;
  LlfHandle& operator=(const LlfHandle&) = delete;
  LlfHandle(LlfHandle&& other) noexcept : value(std::exchange(other.value, nullptr)) {}
  LlfHandle& operator=(LlfHandle&& other) noexcept {
    if (this == &other) return *this;
    reset();
    value = std::exchange(other.value, nullptr);
    return *this;
  }
  ~LlfHandle() { reset(); }

  void reset() noexcept {
    arx_pistoris_llf_destroy(value);
    value = nullptr;
  }

  [[nodiscard]] ArxLlf* get() const noexcept { return value; }
};

struct DlfHandle {
  ArxDlf* value = nullptr;

  DlfHandle() = default;
  explicit DlfHandle(ArxDlf* handle) : value(handle) {}
  DlfHandle(const DlfHandle&) = delete;
  DlfHandle& operator=(const DlfHandle&) = delete;
  DlfHandle(DlfHandle&& other) noexcept : value(std::exchange(other.value, nullptr)) {}
  DlfHandle& operator=(DlfHandle&& other) noexcept {
    if (this == &other) return *this;
    reset();
    value = std::exchange(other.value, nullptr);
    return *this;
  }
  ~DlfHandle() { reset(); }

  void reset() noexcept {
    arx_pistoris_dlf_destroy(value);
    value = nullptr;
  }

  [[nodiscard]] ArxDlf* get() const noexcept { return value; }
};

struct DlfBundle {
  DlfHandle dlf;
  LlfHandle embedded_lighting;

  DlfBundle() = default;
  DlfBundle(ArxDlf* dlf_handle, ArxLlf* embedded_lighting_handle)
      : dlf(dlf_handle), embedded_lighting(embedded_lighting_handle) {}
};

struct LevelHandle {
  ArxLevel* value = nullptr;

  LevelHandle() = default;
  explicit LevelHandle(ArxLevel* handle) : value(handle) {}
  LevelHandle(const LevelHandle&) = delete;
  LevelHandle& operator=(const LevelHandle&) = delete;
  LevelHandle(LevelHandle&& other) noexcept : value(std::exchange(other.value, nullptr)) {}
  LevelHandle& operator=(LevelHandle&& other) noexcept {
    if (this == &other) return *this;
    reset();
    value = std::exchange(other.value, nullptr);
    return *this;
  }
  ~LevelHandle() { reset(); }

  void reset() noexcept {
    arx_pistoris_level_destroy(value);
    value = nullptr;
  }

  [[nodiscard]] ArxLevel* get() const noexcept { return value; }
};

inline std::vector<std::uint8_t> readBytes(const char* path) {
  std::ifstream file(path, std::ios::binary);
  if (!file) return {};
  return {std::istreambuf_iterator<char>(file), {}};
}

inline FtsHandle parseFtsFile(const char* path) {
  const std::vector<std::uint8_t> bytes = readBytes(path);
  if (bytes.empty()) return {};
  ArxFts* fts = nullptr;
  if (arx_pistoris_fts_parse(bytes.data(), bytes.size(), &fts) != ARX_OK) return {};
  return FtsHandle(fts);
}

inline LlfHandle parseLlfFile(const char* path) {
  const std::vector<std::uint8_t> bytes = readBytes(path);
  if (bytes.empty()) return {};
  ArxLlf* llf = nullptr;
  if (arx_pistoris_llf_parse(bytes.data(), bytes.size(), &llf) != ARX_OK) return {};
  return LlfHandle(llf);
}

inline DlfBundle parseDlfFile(const char* path) {
  const std::vector<std::uint8_t> bytes = readBytes(path);
  if (bytes.empty()) return {};
  ArxDlf* dlf = nullptr;
  ArxLlf* embedded_lighting = nullptr;
  const ArxReturnCode read_rc = arx_pistoris_dlf_parse(bytes.data(), bytes.size(), &dlf, &embedded_lighting);
  if (read_rc != ARX_OK) return {};
  return {dlf, embedded_lighting};
}

inline const ArxFts* level40Fts() {
  static const FtsHandle fixture = parseFtsFile("data/fixtures/level/fts/native/level40.fts");
  return fixture.get();
}

inline const ArxLlf* level40Llf() {
  static const LlfHandle fixture = parseLlfFile("data/fixtures/level/llf/native/level40.llf");
  return fixture.get();
}

inline const ArxDlf* level40Dlf() {
  static const DlfBundle fixture = parseDlfFile("data/fixtures/level/dlf/native/level40.dlf");
  return fixture.dlf.get();
}

inline void exerciseLevelFromNative(const ArxFts* fts, const ArxLlf* llf, const ArxDlf* dlf) {
  if (!fts) return;
  ArxLevel* raw_level = nullptr;
  const ArxReturnCode rc = arx_pistoris_level_from_native(fts, llf, dlf, &raw_level);
  if (rc != ARX_OK) return;
  LevelHandle level(raw_level);
  if (!level.get()) std::abort();
  if (arx_pistoris_level_validate(level.get()) != ARX_OK) std::abort();
}

inline void pushLe32(std::vector<std::uint8_t>& buffer, std::uint32_t value) {
  buffer.push_back(static_cast<std::uint8_t>(value & 0xffu));
  buffer.push_back(static_cast<std::uint8_t>((value >> 8) & 0xffu));
  buffer.push_back(static_cast<std::uint8_t>((value >> 16) & 0xffu));
  buffer.push_back(static_cast<std::uint8_t>((value >> 24) & 0xffu));
}

inline std::vector<std::uint8_t> buildGlbFromFuzzInput(const std::uint8_t* data, std::size_t size) {
  constexpr std::uint32_t kGlbMagic = 0x46546C67u;       // 'glTF'
  constexpr std::uint32_t kChunkTypeJson = 0x4E4F534Au;  // 'JSON'
  constexpr std::uint32_t kChunkTypeBin = 0x004E4942u;   // 'BIN\0'

  if (size < sizeof(std::uint32_t)) return {};

  std::uint32_t bin_len_request = 0;
  std::memcpy(&bin_len_request, data, sizeof(bin_len_request));
  data += sizeof(bin_len_request);
  size -= sizeof(bin_len_request);

  const std::size_t bin_len = (size > 0) ? (bin_len_request % size) : 0;
  const std::size_t json_len = size - bin_len;

  const std::uint8_t* json_data = data;
  const std::uint8_t* bin_data = data + json_len;

  const std::size_t json_pad = (4 - (json_len % 4)) % 4;
  const std::size_t bin_pad = (4 - (bin_len % 4)) % 4;

  std::uint32_t total_len = 12U + 8U + static_cast<std::uint32_t>(json_len + json_pad);
  if (bin_len > 0) total_len += 8U + static_cast<std::uint32_t>(bin_len + bin_pad);

  std::vector<std::uint8_t> glb;
  glb.reserve(total_len);
  pushLe32(glb, kGlbMagic);
  pushLe32(glb, 2U);
  pushLe32(glb, total_len);

  pushLe32(glb, static_cast<std::uint32_t>(json_len + json_pad));
  pushLe32(glb, kChunkTypeJson);
  glb.insert(glb.end(), json_data, json_data + json_len);
  for (std::size_t i = 0; i < json_pad; ++i) glb.push_back(' ');

  if (bin_len > 0) {
    pushLe32(glb, static_cast<std::uint32_t>(bin_len + bin_pad));
    pushLe32(glb, kChunkTypeBin);
    glb.insert(glb.end(), bin_data, bin_data + bin_len);
    for (std::size_t i = 0; i < bin_pad; ++i) glb.push_back(0);
  }

  return glb;
}

}  // namespace arx_fuzz
