// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#include "modules/geometry.h"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <limits>
#include <new>
#include <span>
#include <string_view>
#include <utility>
#include <vector>

#define STBI_NO_STDIO
#define STBI_ONLY_JPEG
#define STBI_ONLY_PNG
#define STBI_ONLY_BMP
#define STBI_ONLY_TGA
#define STBI_NO_HDR
#define STBI_NO_LINEAR
#define STBI_MAX_DIMENSIONS 16384
#include "stb/stb_image.h"

#define STBI_WRITE_NO_STDIO
#include "stb/stb_image_resize2.h"
#include "stb/stb_image_write.h"

namespace pistoris::geometry {
namespace {

constexpr std::uint64_t kMaxDecodedBytes = 256ULL * 1024ULL * 1024ULL;
constexpr std::array<std::uint8_t, 8> kPngSignature = {0x89, 'P', 'N', 'G', 0x0d, 0x0a, 0x1a, 0x0a};

std::uint16_t littleU16(std::span<const std::uint8_t> encoded, std::size_t offset) noexcept {
  return static_cast<std::uint16_t>(encoded[offset]) |
         static_cast<std::uint16_t>(static_cast<std::uint16_t>(encoded[offset + 1]) << 8U);
}

bool plausibleTga(std::span<const std::uint8_t> encoded) noexcept {
  if (encoded.size() < 18) return false;
  const std::uint8_t color_map_type = encoded[1];
  const std::uint8_t image_type = encoded[2];
  const bool color_mapped = image_type == 1 || image_type == 9;
  const bool true_color = image_type == 2 || image_type == 10;
  const bool grayscale = image_type == 3 || image_type == 11;
  if ((!color_mapped && !true_color && !grayscale) || color_map_type > 1) return false;
  if (color_mapped != (color_map_type == 1)) return false;
  if (littleU16(encoded, 12) == 0 || littleU16(encoded, 14) == 0) return false;

  const std::uint8_t pixel_depth = encoded[16];
  if (color_mapped) {
    const std::uint8_t entry_depth = encoded[7];
    if (littleU16(encoded, 5) == 0 ||
        (entry_depth != 15 && entry_depth != 16 && entry_depth != 24 && entry_depth != 32))
      return false;
    return pixel_depth == 8 || pixel_depth == 16;
  }
  if (true_color) return pixel_depth == 15 || pixel_depth == 16 || pixel_depth == 24 || pixel_depth == 32;
  return pixel_depth == 8 || pixel_depth == 16;
}

ImageError failureCode() noexcept {
  const char* reason = stbi_failure_reason();
  return reason != nullptr && std::strcmp(reason, "outofmem") == 0 ? ImageError::kOutOfMemory : ImageError::kMalformed;
}

ImageError encodedSize(std::span<const std::uint8_t> encoded, int& out) noexcept {
  if (encoded.empty() || encoded.size() > static_cast<std::size_t>(std::numeric_limits<int>::max()))
    return ImageError::kMalformed;
  out = static_cast<int>(encoded.size());
  return ImageError::kNone;
}

ImageError inspectDimensions(std::span<const std::uint8_t> encoded, ImageInfo& out) noexcept {
  int encoded_size = 0;
  ImageError error = encodedSize(encoded, encoded_size);
  if (error != ImageError::kNone) return error;

  int width = 0;
  int height = 0;
  int components = 0;
  if (stbi_info_from_memory(encoded.data(), encoded_size, &width, &height, &components) == 0) return failureCode();
  if (width <= 0 || height <= 0 || components <= 0 || components > 4 ||
      static_cast<std::uint64_t>(width) * static_cast<std::uint64_t>(height) * 4ULL > kMaxDecodedBytes) {
    return ImageError::kMalformed;
  }

  out.format = detectImageFormat(encoded);
  if (out.format == ImageFormat::kUnknown) return ImageError::kMalformed;
  out.width = static_cast<std::uint32_t>(width);
  out.height = static_cast<std::uint32_t>(height);
  out.components = static_cast<std::uint8_t>(components);
  return ImageError::kNone;
}

struct Decoded {
  stbi_uc* pixels = nullptr;
  ImageInfo info;

  Decoded() = default;
  ~Decoded() { stbi_image_free(pixels); }
  Decoded(const Decoded&) = delete;
  Decoded& operator=(const Decoded&) = delete;
};

ImageError decode(std::span<const std::uint8_t> encoded, Decoded& out) noexcept {
  ImageInfo info;
  ImageError error = inspectDimensions(encoded, info);
  if (error != ImageError::kNone) return error;

  int width = 0;
  int height = 0;
  int components = 0;
  stbi_uc* pixels =
      stbi_load_from_memory(encoded.data(), static_cast<int>(encoded.size()), &width, &height, &components, 0);
  if (pixels == nullptr) return failureCode();
  if (width != static_cast<int>(info.width) || height != static_cast<int>(info.height) ||
      components != static_cast<int>(info.components)) {
    stbi_image_free(pixels);
    return ImageError::kMalformed;
  }
  out.pixels = pixels;
  out.info = info;
  return ImageError::kNone;
}

struct PngWriteContext {
  std::vector<std::uint8_t> bytes;
  bool failed = false;
  bool bad_alloc = false;
};

void appendPng(void* context, void* data, int size) noexcept {
  auto& out = *static_cast<PngWriteContext*>(context);
  if (out.failed || size < 0) {
    out.failed = true;
    return;
  }
  try {
    const auto* first = static_cast<const std::uint8_t*>(data);
    out.bytes.insert(out.bytes.end(), first, first + size);
  } catch (const std::bad_alloc&) {
    out.failed = true;
    out.bad_alloc = true;
  } catch (...) {
    out.failed = true;
  }
}

ImageError encodePng(const ImageInfo& info, std::span<const std::uint8_t> pixels, std::vector<std::uint8_t>& out) {
  const int width = static_cast<int>(info.width);
  const int height = static_cast<int>(info.height);
  const int components = static_cast<int>(info.components);
  if (width > std::numeric_limits<int>::max() / components) return ImageError::kMalformed;
  const std::size_t row_bytes = static_cast<std::size_t>(width) * static_cast<std::size_t>(components);
  if (pixels.size() != row_bytes * static_cast<std::size_t>(height)) return ImageError::kMalformed;

  PngWriteContext context;
  const int written =
      stbi_write_png_to_func(appendPng, &context, width, height, components, pixels.data(), width * components);
  if (context.bad_alloc) return ImageError::kOutOfMemory;
  if (written == 0 || context.failed || context.bytes.empty()) return ImageError::kMalformed;
  out = std::move(context.bytes);
  return ImageError::kNone;
}

bool isPowerOfTwo(std::uint32_t value) noexcept { return value != 0 && (value & (value - 1U)) == 0; }

std::uint32_t nextPowerOfTwo(std::uint32_t value) noexcept {
  std::uint32_t result = 1;
  while (result < value) result <<= 1U;
  return result;
}

stbir_pixel_layout pixelLayout(std::uint8_t components) noexcept {
  switch (components) {
    case 1:
      return STBIR_1CHANNEL;
    case 2:
      return STBIR_RA;
    case 3:
      return STBIR_RGB;
    case 4:
      return STBIR_RGBA;
    default:
      return STBIR_1CHANNEL;
  }
}

bool sampleNonBlack(const Decoded& decoded, int x, int y, std::uint8_t* out) noexcept {
  if (x < 0 || y < 0 || x >= static_cast<int>(decoded.info.width) || y >= static_cast<int>(decoded.info.height))
    return false;
  const std::size_t offset = (static_cast<std::size_t>(y) * decoded.info.width + static_cast<std::size_t>(x)) * 3U;
  const stbi_uc* source = decoded.pixels + offset;
  if (source[0] == 0 && source[1] == 0 && source[2] == 0) return false;
  std::copy_n(source, 3, out);
  return true;
}

ImageError applyBmpColorKey(const Decoded& decoded, std::vector<std::uint8_t>& out) {
  if (decoded.info.format != ImageFormat::kBmp || decoded.info.components != 3) return ImageError::kNone;
  const std::size_t pixel_count = static_cast<std::size_t>(decoded.info.width) * decoded.info.height;
  bool has_black = false;
  for (std::size_t i = 0; i < pixel_count; ++i) {
    const stbi_uc* pixel = decoded.pixels + i * 3U;
    if (pixel[0] == 0 && pixel[1] == 0 && pixel[2] == 0) {
      has_black = true;
      break;
    }
  }
  if (!has_black) return ImageError::kNone;

  try {
    out.resize(pixel_count * 4U);
  } catch (const std::bad_alloc&) {
    return ImageError::kOutOfMemory;
  }
  constexpr std::array<std::pair<int, int>, 8> kNeighbors = {
      std::pair{0, -1}, {1, 0}, {0, 1}, {-1, 0}, {-1, -1}, {1, -1}, {1, 1}, {-1, 1}};
  for (int y = 0; y < static_cast<int>(decoded.info.height); ++y) {
    for (int x = 0; x < static_cast<int>(decoded.info.width); ++x) {
      const std::size_t index = static_cast<std::size_t>(y) * decoded.info.width + static_cast<std::size_t>(x);
      const stbi_uc* source = decoded.pixels + index * 3U;
      std::uint8_t* target = out.data() + index * 4U;
      const bool transparent = source[0] == 0 && source[1] == 0 && source[2] == 0;
      target[3] = transparent ? 0 : 0xff;
      if (!transparent) {
        std::copy_n(source, 3, target);
        continue;
      }
      bool sampled = false;
      for (const auto& [dx, dy] : kNeighbors) {
        if (sampleNonBlack(decoded, x + dx, y + dy, target)) {
          sampled = true;
          break;
        }
      }
      if (!sampled) std::fill_n(target, 3, 0);
    }
  }
  return ImageError::kNone;
}

}  // namespace

ImageFormat detectImageFormat(std::span<const std::uint8_t> encoded) noexcept {
  if (encoded.size() >= kPngSignature.size() &&
      std::equal(kPngSignature.begin(), kPngSignature.end(), encoded.begin())) {
    return ImageFormat::kPng;
  }
  if (encoded.size() >= 3 && encoded[0] == 0xff && encoded[1] == 0xd8 && encoded[2] == 0xff) return ImageFormat::kJpeg;
  if (encoded.size() >= 2 && encoded[0] == 'B' && encoded[1] == 'M') return ImageFormat::kBmp;
  return plausibleTga(encoded) ? ImageFormat::kTga : ImageFormat::kUnknown;
}

ImageError inspectImage(std::span<const std::uint8_t> encoded, ImageInfo* out) noexcept {
  Decoded decoded;
  ImageError error = decode(encoded, decoded);
  if (error == ImageError::kNone && out != nullptr) *out = decoded.info;
  return error;
}

ImageError transcodeImageToPng(std::span<const std::uint8_t> encoded, std::vector<std::uint8_t>& out,
                               ImageInfo* out_info) {
  Decoded decoded;
  ImageError error = decode(encoded, decoded);
  if (error != ImageError::kNone) return error;

  std::vector<std::uint8_t> prepared;
  std::vector<std::uint8_t> color_keyed;
  ImageInfo info = decoded.info;
  error = applyBmpColorKey(decoded, color_keyed);
  if (error != ImageError::kNone) return error;
  if (!color_keyed.empty()) info.components = 4;

  const std::size_t pixel_bytes = static_cast<std::size_t>(info.width) * info.height * info.components;
  std::span<const std::uint8_t> pixels = color_keyed.empty()
                                             ? std::span<const std::uint8_t>(decoded.pixels, pixel_bytes)
                                             : std::span<const std::uint8_t>(color_keyed);
  error = encodePng(info, pixels, prepared);
  if (error != ImageError::kNone) return error;

  info.format = ImageFormat::kPng;
  out = std::move(prepared);
  if (out_info != nullptr) *out_info = info;
  return ImageError::kNone;
}

ImageError normalizeImageToPowerOfTwo(std::span<const std::uint8_t> encoded, std::vector<std::uint8_t>& out,
                                      ImageInfo* out_info, bool* out_rescaled) {
  Decoded decoded;
  ImageError error = decode(encoded, decoded);
  if (error != ImageError::kNone) return error;

  const bool rescaled = !isPowerOfTwo(decoded.info.width) || !isPowerOfTwo(decoded.info.height);
  if (!rescaled) {
    std::vector<std::uint8_t> prepared;
    try {
      prepared.assign(encoded.begin(), encoded.end());
    } catch (const std::bad_alloc&) {
      return ImageError::kOutOfMemory;
    }
    out = std::move(prepared);
    if (out_info != nullptr) *out_info = decoded.info;
    if (out_rescaled != nullptr) *out_rescaled = false;
    return ImageError::kNone;
  }

  std::vector<std::uint8_t> color_keyed;
  ImageInfo info = decoded.info;
  error = applyBmpColorKey(decoded, color_keyed);
  if (error != ImageError::kNone) return error;
  if (!color_keyed.empty()) info.components = 4;

  const std::uint32_t target_width = nextPowerOfTwo(info.width);
  const std::uint32_t target_height = nextPowerOfTwo(info.height);
  if (static_cast<std::uint64_t>(target_width) * target_height * 4ULL > kMaxDecodedBytes) return ImageError::kMalformed;

  const std::size_t source_bytes = static_cast<std::size_t>(info.width) * info.height * info.components;
  const std::span<const std::uint8_t> pixels = color_keyed.empty()
                                                   ? std::span<const std::uint8_t>(decoded.pixels, source_bytes)
                                                   : std::span<const std::uint8_t>(color_keyed);
  std::vector<std::uint8_t> resized;
  try {
    resized.resize(static_cast<std::size_t>(target_width) * target_height * info.components);
  } catch (const std::bad_alloc&) {
    return ImageError::kOutOfMemory;
  }

  if (stbir_resize(pixels.data(),
                   static_cast<int>(info.width),
                   static_cast<int>(info.height),
                   0,
                   resized.data(),
                   static_cast<int>(target_width),
                   static_cast<int>(target_height),
                   0,
                   pixelLayout(info.components),
                   STBIR_TYPE_UINT8_SRGB,
                   STBIR_EDGE_WRAP,
                   STBIR_FILTER_DEFAULT) == nullptr) {
    return ImageError::kOutOfMemory;
  }

  info.width = target_width;
  info.height = target_height;
  std::vector<std::uint8_t> prepared;
  error = encodePng(info, resized, prepared);
  if (error != ImageError::kNone) return error;

  info.format = ImageFormat::kPng;
  out = std::move(prepared);
  if (out_info != nullptr) *out_info = info;
  if (out_rescaled != nullptr) *out_rescaled = true;
  return ImageError::kNone;
}

bool imageHasAlpha(const ImageInfo& info) noexcept { return info.components == 2 || info.components == 4; }

std::string_view imageExtension(ImageFormat format) noexcept {
  switch (format) {
    case ImageFormat::kJpeg:
      return ".jpg";
    case ImageFormat::kPng:
      return ".png";
    case ImageFormat::kBmp:
      return ".bmp";
    case ImageFormat::kTga:
      return ".tga";
    case ImageFormat::kUnknown:
      return {};
  }
  return {};
}

}  // namespace pistoris::geometry
