// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#include "native/storage.h"

#include "arx_pistoris/base/status.h"
#include "arx_pistoris/native/dlf.hpp"
#include "arx_pistoris/native/ftl.hpp"
#include "arx_pistoris/native/fts.hpp"
#include "arx_pistoris/native/llf.hpp"
#include "arx_pistoris/native/location.hpp"

#include "api/result_failure.h"
#include "native/dlf.h"
#include "native/ftl.h"
#include "native/fts.h"
#include "native/llf.h"
#include "utils/cursor.h"
#include "utils/pkware.h"

#include <cstdint>
#include <cstring>
#include <limits>
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

FtlBinaryResult<ftl::Data> parseFtl(std::span<const std::uint8_t> raw, NativeBinaryRegion region) {
  ReadCursor cursor(raw.data(), raw.size());
  return loadFtl(cursor, region);
}

FtsBinaryResult<fts::Data> parseFts(std::span<const std::uint8_t> raw, NativeBinaryRegion region) {
  ReadCursor cursor(raw.data(), raw.size());
  return loadFts(cursor, region);
}

LlfBinaryResult<llf::Data> parseLlf(std::span<const std::uint8_t> raw, NativeBinaryRegion region) {
  ReadCursor cursor(raw.data(), raw.size());
  return loadLlf(cursor, region);
}

DlfBinaryResult<DlfLoad> parseDlf(std::span<const std::uint8_t> raw, NativeBinaryRegion region,
                                  bool read_embedded_lighting) {
  ReadCursor cursor(raw.data(), raw.size());
  return loadDlf(cursor, read_embedded_lighting, region);
}

template <class Location, class Element>
void storageFailure(Location* location, Element element, std::string_view field) {
  if (!location) return;
  location->element = element;
  location->region = NativeBinaryRegion::kStored;
  location->field = field;
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

bool ftsPrefixSize(std::span<const std::uint8_t> stored, fts_detail::StorageHeader& header,
                   std::size_t& prefix_size) noexcept {
  if (stored.size() < sizeof(header)) return false;
  std::memcpy(&header, stored.data(), sizeof(header));
  if (header.version != kFtsVersion || header.count < 0 ||
      static_cast<std::size_t>(header.count) > fts_detail::kMaxSourceChecks) {
    return false;
  }
  const std::size_t count = static_cast<std::size_t>(header.count);
  if (count > (std::numeric_limits<std::size_t>::max() - sizeof(header)) / fts_detail::kSourceCheckSize) return false;
  prefix_size = sizeof(header) + count * fts_detail::kSourceCheckSize;
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

FtlBinaryResult<ftl::Data> loadFtlStorage(std::span<const std::uint8_t> stored) {
  if (isRawFtl(stored) || !pkware::looksLikeDcl(stored)) return parseFtl(stored, NativeBinaryRegion::kStored);

  std::vector<std::uint8_t> raw;
  const ArxReturnCode rc = pkware::decompress(stored, raw);
  if (rc != ARX_OK) {
    FtlBinaryLocation location;
    storageFailure(&location, FtlElement::kHeader, "compressed_payload");
    return api_detail::ftlBinaryFailure<ftl::Data>(rc, std::move(location));
  }
  return parseFtl(raw, NativeBinaryRegion::kDecodedPayload);
}

ArxReturnCode saveFtlStorage(const ftl::Data* data, std::vector<std::uint8_t>& stored, bool compress) {
  std::vector<std::uint8_t> raw;
  ArxReturnCode rc = serializeRaw(data, raw, saveFtl);
  if (rc != ARX_OK) return rc;
  return compressWhole(raw, stored, compress);
}

FtsBinaryResult<fts::Data> loadFtsStorage(std::span<const std::uint8_t> stored) {
  fts_detail::StorageHeader header;
  std::size_t prefix_size = 0;
  if (!ftsPrefixSize(stored, header, prefix_size) || header.uncompressed_size <= 0)
    return parseFts(stored, NativeBinaryRegion::kStored);

  const std::span<const std::uint8_t> payload = stored.subspan(prefix_size);
  if (!pkware::looksLikeDcl(payload)) return parseFts(stored, NativeBinaryRegion::kStored);

  std::vector<std::uint8_t> decoded;
  const ArxReturnCode compressed_rc = pkware::decompress(
      payload, decoded, pkware::kMaxDecodedBytes, static_cast<std::size_t>(header.uncompressed_size));
  if (compressed_rc != ARX_OK) {
    if (compressed_rc == ARX_BAD_ALLOC) {
      FtsBinaryLocation location;
      storageFailure(&location, FtsElement::kHeader, "compressed_payload");
      return api_detail::ftsBinaryFailure<fts::Data>(compressed_rc, std::move(location));
    }
    auto raw_result = parseFts(stored, NativeBinaryRegion::kStored);
    if (raw_result) return raw_result;
    FtsBinaryLocation location;
    storageFailure(&location, FtsElement::kHeader, "compressed_payload");
    return api_detail::ftsBinaryFailure<fts::Data>(compressed_rc, std::move(location));
  }

  ReadCursor prefix_cursor(stored.data(), prefix_size);
  ReadCursor payload_cursor(decoded.data(), decoded.size());
  auto compressed_result =
      loadFts(prefix_cursor, payload_cursor, NativeBinaryRegion::kStored, NativeBinaryRegion::kDecodedPayload);
  if (compressed_result) return compressed_result;
  auto raw_result = parseFts(stored, NativeBinaryRegion::kStored);
  if (raw_result) return raw_result;
  return compressed_result;
}

ArxReturnCode saveFtsStorage(const fts::Data* data, std::vector<std::uint8_t>& stored, bool compress) {
  std::vector<std::uint8_t> raw;
  ArxReturnCode rc = serializeRaw(data, raw, saveFts);
  if (rc != ARX_OK || !compress) {
    if (rc == ARX_OK) stored = std::move(raw);
    return rc;
  }

  fts_detail::StorageHeader header;
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
  header.uncompressed_size = static_cast<std::int32_t>(payload_size);
  std::memcpy(result.data(), &header, sizeof(header));
  stored = std::move(result);
  return ARX_OK;
}

LlfBinaryResult<llf::Data> loadLlfStorage(std::span<const std::uint8_t> stored) {
  if (isRawLlf(stored) || !pkware::looksLikeDcl(stored)) return parseLlf(stored, NativeBinaryRegion::kStored);

  std::vector<std::uint8_t> raw;
  const ArxReturnCode rc = pkware::decompress(stored, raw);
  if (rc != ARX_OK) {
    LlfBinaryLocation location;
    storageFailure(&location, LlfElement::kHeader, "compressed_payload");
    return api_detail::llfBinaryFailure<llf::Data>(rc, std::move(location));
  }
  return parseLlf(raw, NativeBinaryRegion::kDecodedPayload);
}

ArxReturnCode saveLlfStorage(const llf::Data* data, std::string_view signer, std::vector<std::uint8_t>& stored,
                             bool compress) {
  std::vector<std::uint8_t> raw;
  ArxReturnCode rc = serializeRaw(
      data, raw, [&](const llf::Data* source, WriteCursor& cursor) { return saveLlf(source, signer, cursor); });
  if (rc != ARX_OK) return rc;
  return compressWhole(raw, stored, compress);
}

DlfBinaryResult<DlfLoad> loadDlfStorage(std::span<const std::uint8_t> stored, bool read_embedded_lighting) {
  if (!hasRawDlfHeader(stored)) return parseDlf(stored, NativeBinaryRegion::kStored, read_embedded_lighting);

  const std::span<const std::uint8_t> suffix = stored.subspan(kDlfRawPrefixSize);
  if (!pkware::looksLikeDcl(suffix)) return parseDlf(stored, NativeBinaryRegion::kStored, read_embedded_lighting);

  std::vector<std::uint8_t> decoded;
  const ArxReturnCode compressed_rc = pkware::decompress(suffix, decoded);
  if (compressed_rc != ARX_OK) {
    if (compressed_rc == ARX_BAD_ALLOC) {
      DlfBinaryLocation location;
      storageFailure(&location, DlfBinaryElement{DlfElement::kHeader}, "compressed_payload");
      return api_detail::dlfBinaryFailure<DlfLoad>(compressed_rc, std::move(location));
    }
    auto raw_result = parseDlf(stored, NativeBinaryRegion::kStored, read_embedded_lighting);
    if (raw_result) return raw_result;
    DlfBinaryLocation location;
    storageFailure(&location, DlfBinaryElement{DlfElement::kHeader}, "compressed_payload");
    return api_detail::dlfBinaryFailure<DlfLoad>(compressed_rc, std::move(location));
  }

  ReadCursor prefix_cursor(stored.data(), kDlfRawPrefixSize);
  ReadCursor payload_cursor(decoded.data(), decoded.size());
  auto compressed_result = loadDlf(prefix_cursor,
                                   payload_cursor,
                                   read_embedded_lighting,
                                   NativeBinaryRegion::kStored,
                                   NativeBinaryRegion::kDecodedPayload);
  if (compressed_result) return compressed_result;
  auto raw_result = parseDlf(stored, NativeBinaryRegion::kStored, read_embedded_lighting);
  if (raw_result) return raw_result;
  return compressed_result;
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
