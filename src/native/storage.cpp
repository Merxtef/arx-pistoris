// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#include "native/storage.h"

#include "arx_pistoris/base/status.h"
#include "arx_pistoris/native/dlf.hpp"
#include "arx_pistoris/native/ftl.hpp"
#include "arx_pistoris/native/fts.hpp"
#include "arx_pistoris/native/llf.hpp"

#include "native/dlf.h"
#include "native/ftl.h"
#include "native/fts.h"
#include "native/llf.h"
#include "utils/cursor.h"
#include "utils/pkware.h"

#include <cstdint>
#include <cstring>
#include <limits>
#include <optional>
#include <span>
#include <string_view>
#include <utility>
#include <vector>

namespace pistoris {
namespace {

constexpr std::size_t kDlfRawPrefixSize = 8520;
constexpr char kDlfIdentity[] = "DANAE_FILE";
constexpr char kLlfIdentity[] = "DANAE_LLH_FILE";

bool hasBytes(std::span<const std::uint8_t> data, std::size_t offset, const void* expected,
              std::size_t expected_size) noexcept {
  return offset <= data.size() && expected_size <= data.size() - offset &&
         std::memcmp(data.data() + offset, expected, expected_size) == 0;
}

bool isRawFtl(std::span<const std::uint8_t> stored) noexcept {
  return hasBytes(stored, 0, kFtlMagic, sizeof(kFtlMagic));
}

bool isRawLlf(std::span<const std::uint8_t> stored) noexcept {
  return hasBytes(stored, sizeof(float), kLlfIdentity, sizeof(kLlfIdentity));
}

bool hasRawDlfHeader(std::span<const std::uint8_t> stored) noexcept {
  if (stored.size() < kDlfRawPrefixSize || !hasBytes(stored, sizeof(float), kDlfIdentity, sizeof(kDlfIdentity)))
    return false;
  float version = 0.0f;
  std::memcpy(&version, stored.data(), sizeof(version));
  return version == kDlfVersion;
}

ArxReturnCode parseFtl(ftl::Data* data, std::span<const std::uint8_t> raw) {
  ReadCursor cursor(raw.data(), raw.size());
  return loadFtl(data, cursor);
}

ArxReturnCode parseFts(fts::Data* data, std::span<const std::uint8_t> raw) {
  ReadCursor cursor(raw.data(), raw.size());
  return loadFts(data, cursor);
}

ArxReturnCode parseLlf(llf::Data* data, std::span<const std::uint8_t> raw) {
  ReadCursor cursor(raw.data(), raw.size());
  return loadLlf(data, cursor);
}

ArxReturnCode parseDlf(dlf::Data* data, std::optional<llf::Data>* embedded_lighting,
                       std::span<const std::uint8_t> raw) {
  ReadCursor cursor(raw.data(), raw.size());
  return loadDlf(data, embedded_lighting, cursor);
}

template <class Data, class Save>
ArxReturnCode serializeRaw(const Data* data, std::vector<std::uint8_t>& raw, Save&& save) {
  WriteCursor cursor;
  ArxReturnCode rc = save(data, cursor);
  if (rc == ARX_OK) raw = cursor.take();
  return rc;
}

ArxReturnCode compressWhole(std::vector<std::uint8_t>& raw, std::vector<std::uint8_t>& stored, bool compress) {
  if (!compress) {
    stored = std::move(raw);
    return ARX_OK;
  }
  if (raw.size() > pkware::kMaxDecodedBytes) return ARX_COMPRESSION_INPUT_TOO_LARGE;
  return pkware::compress(raw, stored);
}

bool ftsPrefixSize(std::span<const std::uint8_t> stored, fts::Header& header, std::size_t& prefix_size) noexcept {
  if (stored.size() < sizeof(header)) return false;
  std::memcpy(&header, stored.data(), sizeof(header));
  if (header.version != kFtsVersion || header.count < 0 ||
      static_cast<std::size_t>(header.count) > kFtsMaxHeaderBlocks) {
    return false;
  }
  const std::size_t count = static_cast<std::size_t>(header.count);
  if (count > (std::numeric_limits<std::size_t>::max() - sizeof(header)) / sizeof(fts::UniqueHeader3)) return false;
  prefix_size = sizeof(header) + count * sizeof(fts::UniqueHeader3);
  return prefix_size <= stored.size();
}

std::vector<std::uint8_t> join(std::span<const std::uint8_t> prefix, std::span<const std::uint8_t> suffix) {
  std::vector<std::uint8_t> result;
  result.reserve(prefix.size() + suffix.size());
  result.insert(result.end(), prefix.begin(), prefix.end());
  result.insert(result.end(), suffix.begin(), suffix.end());
  return result;
}

}  // namespace

ArxReturnCode loadFtlStorage(ftl::Data* data, std::span<const std::uint8_t> stored) {
  if (!data) return ARX_INVALID_DATA_POINTER;
  if (isRawFtl(stored) || !pkware::looksLikeDcl(stored)) return parseFtl(data, stored);

  std::vector<std::uint8_t> raw;
  ArxReturnCode rc = pkware::decompress(stored, raw);
  if (rc != ARX_OK) return rc;
  return parseFtl(data, raw);
}

ArxReturnCode saveFtlStorage(const ftl::Data* data, std::vector<std::uint8_t>& stored, bool compress) {
  std::vector<std::uint8_t> raw;
  ArxReturnCode rc = serializeRaw(data, raw, saveFtl);
  if (rc != ARX_OK) return rc;
  return compressWhole(raw, stored, compress);
}

ArxReturnCode loadFtsStorage(fts::Data* data, std::span<const std::uint8_t> stored) {
  if (!data) return ARX_INVALID_DATA_POINTER;

  fts::Header header;
  std::size_t prefix_size = 0;
  if (!ftsPrefixSize(stored, header, prefix_size) || header.uncompressedsize <= 0) return parseFts(data, stored);

  const std::span<const std::uint8_t> payload = stored.subspan(prefix_size);
  ArxReturnCode compressed_rc = ARX_DECOMPRESSION_FAILED;
  if (pkware::looksLikeDcl(payload)) {
    std::vector<std::uint8_t> decoded;
    compressed_rc = pkware::decompress(
        payload, decoded, pkware::kMaxDecodedBytes, static_cast<std::size_t>(header.uncompressedsize));
    if (compressed_rc == ARX_BAD_ALLOC) return compressed_rc;
    if (compressed_rc == ARX_OK) {
      fts::Data candidate;
      ReadCursor prefix_cursor(stored.data(), prefix_size);
      ReadCursor payload_cursor(decoded.data(), decoded.size());
      compressed_rc = loadFts(&candidate, prefix_cursor, payload_cursor);
      if (compressed_rc == ARX_OK) {
        *data = std::move(candidate);
        return ARX_OK;
      }
    }
  }

  fts::Data raw_candidate;
  ArxReturnCode raw_rc = parseFts(&raw_candidate, stored);
  if (raw_rc == ARX_OK) {
    *data = std::move(raw_candidate);
    return ARX_OK;
  }
  return pkware::looksLikeDcl(payload) ? compressed_rc : raw_rc;
}

ArxReturnCode saveFtsStorage(const fts::Data* data, std::vector<std::uint8_t>& stored, bool compress) {
  std::vector<std::uint8_t> raw;
  ArxReturnCode rc = serializeRaw(data, raw, saveFts);
  if (rc != ARX_OK || !compress) {
    if (rc == ARX_OK) stored = std::move(raw);
    return rc;
  }

  fts::Header header;
  std::size_t prefix_size = 0;
  if (!ftsPrefixSize(raw, header, prefix_size)) return ARX_INTERNAL_ERROR;
  const std::size_t payload_size = raw.size() - prefix_size;
  if (payload_size > pkware::kMaxDecodedBytes) {
    return ARX_COMPRESSION_INPUT_TOO_LARGE;
  }

  std::vector<std::uint8_t> compressed;
  rc = pkware::compress(std::span<const std::uint8_t>(raw).subspan(prefix_size), compressed);
  if (rc != ARX_OK) return rc;

  std::vector<std::uint8_t> result = join(std::span<const std::uint8_t>(raw).first(prefix_size), compressed);
  header.uncompressedsize = static_cast<std::int32_t>(payload_size);
  std::memcpy(result.data(), &header, sizeof(header));
  stored = std::move(result);
  return ARX_OK;
}

ArxReturnCode loadLlfStorage(llf::Data* data, std::span<const std::uint8_t> stored) {
  if (!data) return ARX_INVALID_DATA_POINTER;
  if (isRawLlf(stored) || !pkware::looksLikeDcl(stored)) return parseLlf(data, stored);

  std::vector<std::uint8_t> raw;
  ArxReturnCode rc = pkware::decompress(stored, raw);
  if (rc != ARX_OK) return rc;
  return parseLlf(data, raw);
}

ArxReturnCode saveLlfStorage(const llf::Data* data, std::string_view signer, std::vector<std::uint8_t>& stored,
                             bool compress) {
  std::vector<std::uint8_t> raw;
  ArxReturnCode rc = serializeRaw(
      data, raw, [&](const llf::Data* source, WriteCursor& cursor) { return saveLlf(source, signer, cursor); });
  if (rc != ARX_OK) return rc;
  return compressWhole(raw, stored, compress);
}

ArxReturnCode loadDlfStorage(dlf::Data* data, std::optional<llf::Data>* embedded_lighting,
                             std::span<const std::uint8_t> stored) {
  if (!data) return ARX_INVALID_DATA_POINTER;
  if (!hasRawDlfHeader(stored)) return parseDlf(data, embedded_lighting, stored);

  const std::span<const std::uint8_t> suffix = stored.subspan(kDlfRawPrefixSize);
  if (!pkware::looksLikeDcl(suffix)) return parseDlf(data, embedded_lighting, stored);

  std::vector<std::uint8_t> decoded;
  ArxReturnCode compressed_rc = pkware::decompress(suffix, decoded);
  if (compressed_rc == ARX_BAD_ALLOC) return compressed_rc;
  if (compressed_rc == ARX_OK) {
    dlf::Data candidate;
    std::optional<llf::Data> candidate_lighting;
    ReadCursor prefix_cursor(stored.data(), kDlfRawPrefixSize);
    ReadCursor payload_cursor(decoded.data(), decoded.size());
    compressed_rc =
        loadDlf(&candidate, embedded_lighting ? &candidate_lighting : nullptr, prefix_cursor, payload_cursor);
    if (compressed_rc == ARX_OK) {
      *data = std::move(candidate);
      if (embedded_lighting) *embedded_lighting = std::move(candidate_lighting);
      return ARX_OK;
    }
  }

  dlf::Data raw_candidate;
  std::optional<llf::Data> raw_lighting;
  ArxReturnCode raw_rc = parseDlf(&raw_candidate, embedded_lighting ? &raw_lighting : nullptr, stored);
  if (raw_rc == ARX_OK) {
    *data = std::move(raw_candidate);
    if (embedded_lighting) *embedded_lighting = std::move(raw_lighting);
    return ARX_OK;
  }
  return compressed_rc;
}

ArxReturnCode saveDlfStorage(const dlf::Data* data, const llf::Data* embedded_lighting, std::string_view signer,
                             std::vector<std::uint8_t>& stored, bool compress) {
  std::vector<std::uint8_t> raw;
  ArxReturnCode rc = serializeRaw(data, raw, [&](const dlf::Data* source, WriteCursor& cursor) {
    return saveDlf(source, embedded_lighting, signer, cursor);
  });
  if (rc != ARX_OK || !compress) {
    if (rc == ARX_OK) stored = std::move(raw);
    return rc;
  }
  if (raw.size() < kDlfRawPrefixSize) return ARX_INTERNAL_ERROR;
  const std::size_t suffix_size = raw.size() - kDlfRawPrefixSize;
  if (suffix_size > pkware::kMaxDecodedBytes) return ARX_COMPRESSION_INPUT_TOO_LARGE;

  std::vector<std::uint8_t> compressed;
  rc = pkware::compress(std::span<const std::uint8_t>(raw).subspan(kDlfRawPrefixSize), compressed);
  if (rc != ARX_OK) return rc;
  stored = join(std::span<const std::uint8_t>(raw).first(kDlfRawPrefixSize), compressed);
  return ARX_OK;
}

}  // namespace pistoris
