// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#include "image.h"

#include "arx_pistoris/pistoris_types.h"

#include "modules/geometry.h"

#include <cctype>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <new>
#include <optional>
#include <span>
#include <string_view>
#include <utility>
#include <vector>

namespace pistoris::glb {
namespace {

bool asciiEqual(std::string_view first, std::string_view second) noexcept {
  if (first.size() != second.size()) return false;
  for (std::size_t i = 0; i < first.size(); ++i) {
    if (std::tolower(static_cast<unsigned char>(first[i])) != std::tolower(static_cast<unsigned char>(second[i])))
      return false;
  }
  return true;
}

std::optional<geometry::ImageFormat> mimeFormat(std::string_view mime) noexcept {
  if (asciiEqual(mime, "image/png")) return geometry::ImageFormat::kPng;
  if (asciiEqual(mime, "image/jpeg")) return geometry::ImageFormat::kJpeg;
  return std::nullopt;
}

int base64Value(char ch) noexcept {
  if (ch >= 'A' && ch <= 'Z') return ch - 'A';
  if (ch >= 'a' && ch <= 'z') return ch - 'a' + 26;
  if (ch >= '0' && ch <= '9') return ch - '0' + 52;
  if (ch == '+') return 62;
  if (ch == '/') return 63;
  return -1;
}

ArxReturnCode decodeBase64(std::string_view source, std::vector<std::uint8_t>& out) {
  if (source.empty() || source.size() % 4 != 0) return ARX_GLB_BAD_FORMAT;
  const std::size_t padding = source.ends_with("==") ? 2U : source.ends_with('=') ? 1U : 0U;
  const std::size_t size = source.size() / 4U * 3U - padding;
  if (size > static_cast<std::size_t>(std::numeric_limits<int>::max())) return ARX_GLB_BAD_FORMAT;

  try {
    out.resize(size);
  } catch (const std::bad_alloc&) {
    return ARX_BAD_ALLOC;
  }
  std::size_t output = 0;
  for (std::size_t i = 0; i < source.size(); i += 4) {
    int values[4]{};
    for (std::size_t j = 0; j < 4; ++j) {
      const bool padded = source[i + j] == '=';
      if (padded) {
        if (i + 4 != source.size() || j < 2 || j < 4 - padding) return ARX_GLB_BAD_FORMAT;
      } else {
        values[j] = base64Value(source[i + j]);
        if (values[j] < 0) return ARX_GLB_BAD_FORMAT;
      }
    }
    const std::uint32_t value =
        static_cast<std::uint32_t>(values[0] << 18 | values[1] << 12 | values[2] << 6 | values[3]);
    if (output < out.size()) out[output++] = static_cast<std::uint8_t>(value >> 16);
    if (output < out.size()) out[output++] = static_cast<std::uint8_t>(value >> 8);
    if (output < out.size()) out[output++] = static_cast<std::uint8_t>(value);
  }
  return ARX_OK;
}

int hexValue(char ch) noexcept {
  if (ch >= '0' && ch <= '9') return ch - '0';
  if (ch >= 'A' && ch <= 'F') return ch - 'A' + 10;
  if (ch >= 'a' && ch <= 'f') return ch - 'a' + 10;
  return -1;
}

ArxReturnCode decodePercentEncoded(std::string_view source, std::vector<std::uint8_t>& out) {
  if (source.size() > static_cast<std::size_t>(std::numeric_limits<int>::max())) return ARX_GLB_BAD_FORMAT;
  try {
    out.clear();
    out.reserve(source.size());
    for (std::size_t i = 0; i < source.size(); ++i) {
      if (source[i] != '%') {
        out.push_back(static_cast<std::uint8_t>(source[i]));
        continue;
      }
      if (i + 2 >= source.size()) return ARX_GLB_BAD_FORMAT;
      const int high = hexValue(source[i + 1]);
      const int low = hexValue(source[i + 2]);
      if (high < 0 || low < 0) return ARX_GLB_BAD_FORMAT;
      out.push_back(static_cast<std::uint8_t>(high * 16 + low));
      i += 2;
    }
  } catch (const std::bad_alloc&) {
    return ARX_BAD_ALLOC;
  }
  return out.empty() ? ARX_GLB_BAD_FORMAT : ARX_OK;
}

ArxReturnCode decodeDataUri(std::string_view uri, std::vector<std::uint8_t>& out, geometry::ImageFormat& expected) {
  const std::size_t comma = uri.find(',');
  if (comma == std::string_view::npos || comma < 5) return ARX_GLB_BAD_FORMAT;
  std::string_view metadata = uri.substr(5, comma - 5);
  const std::string_view payload = uri.substr(comma + 1);
  constexpr std::string_view kBase64 = ";base64";
  const bool base64 =
      metadata.size() >= kBase64.size() && asciiEqual(metadata.substr(metadata.size() - kBase64.size()), kBase64);
  if (base64) metadata.remove_suffix(kBase64.size());
  const std::optional<geometry::ImageFormat> format = mimeFormat(metadata);
  if (!format.has_value()) return ARX_GLB_BAD_FORMAT;
  expected = format.value();
  return base64 ? decodeBase64(payload, out) : decodePercentEncoded(payload, out);
}

}  // namespace

geometry::ImageError prepareImage(std::span<const std::uint8_t> source, PreparedImage& out) {
  geometry::ImageInfo info;
  const geometry::ImageError inspected = geometry::inspectImage(source, &info);
  if (inspected != geometry::ImageError::kNone) return inspected;

  PreparedImage prepared;
  if (info.format == geometry::ImageFormat::kPng || info.format == geometry::ImageFormat::kJpeg) {
    try {
      prepared.encoded.assign(source.begin(), source.end());
    } catch (const std::bad_alloc&) {
      return geometry::ImageError::kOutOfMemory;
    }
    prepared.info = info;
  } else {
    const geometry::ImageError transcoded = geometry::transcodeImageToPng(source, prepared.encoded, &prepared.info);
    if (transcoded != geometry::ImageError::kNone) return transcoded;
  }
  out = std::move(prepared);
  return geometry::ImageError::kNone;
}

std::string_view imageMimeType(geometry::ImageFormat format) noexcept {
  switch (format) {
    case geometry::ImageFormat::kJpeg:
      return "image/jpeg";
    case geometry::ImageFormat::kPng:
      return "image/png";
    case geometry::ImageFormat::kBmp:
    case geometry::ImageFormat::kTga:
    case geometry::ImageFormat::kUnknown:
      return {};
  }
  return {};
}

bool isDataUri(std::string_view uri) noexcept {
  constexpr std::string_view kPrefix = "data:";
  if (uri.size() < kPrefix.size()) return false;
  for (std::size_t i = 0; i < kPrefix.size(); ++i)
    if (std::tolower(static_cast<unsigned char>(uri[i])) != kPrefix[i]) return false;
  return true;
}

ArxReturnCode readEmbeddedImage(const cgltf_image& source, EmbeddedImageCache& cache,
                                const std::vector<std::uint8_t>*& out, geometry::ImageFormat& format) {
  auto existing = cache.find(&source);
  if (existing != cache.end()) {
    format = existing->second.format;
    out = &existing->second.encoded;
    return ARX_OK;
  }

  std::vector<std::uint8_t> encoded;
  std::optional<geometry::ImageFormat> expected;
  ArxReturnCode rc = ARX_OK;
  if (source.uri != nullptr && isDataUri(source.uri)) {
    geometry::ImageFormat data_format = geometry::ImageFormat::kPng;
    rc = decodeDataUri(source.uri, encoded, data_format);
    expected = data_format;
  } else if (source.buffer_view != nullptr && source.mime_type != nullptr) {
    expected = mimeFormat(source.mime_type);
    if (!expected.has_value()) return ARX_GLB_BAD_FORMAT;
    const std::uint8_t* bytes = cgltf_buffer_view_data(source.buffer_view);
    const std::size_t size = source.buffer_view->size;
    if (bytes == nullptr || size == 0 || size > static_cast<std::size_t>(std::numeric_limits<int>::max()))
      return ARX_GLB_BAD_FORMAT;
    try {
      encoded.assign(bytes, bytes + size);
    } catch (const std::bad_alloc&) {
      return ARX_BAD_ALLOC;
    }
  } else {
    return ARX_GLB_BAD_FORMAT;
  }
  if (rc != ARX_OK) return rc;

  geometry::ImageInfo info;
  const geometry::ImageError image_error = geometry::inspectImage(encoded, &info);
  if (image_error == geometry::ImageError::kOutOfMemory) return ARX_BAD_ALLOC;
  if (image_error != geometry::ImageError::kNone || info.format != *expected) return ARX_GLB_BAD_FORMAT;
  format = info.format;
  try {
    auto [entry, inserted] = cache.emplace(&source, EmbeddedImage{std::move(encoded), info.format});
    (void)inserted;
    out = &entry->second.encoded;
  } catch (const std::bad_alloc&) {
    return ARX_BAD_ALLOC;
  }
  return ARX_OK;
}

}  // namespace pistoris::glb
