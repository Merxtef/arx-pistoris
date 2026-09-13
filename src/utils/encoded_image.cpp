// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#include "utils/encoded_image.h"

#include "stb_config.h"
#include "utils/encoded_image_internal.h"

#include <algorithm>
#include <array>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <limits>
#include <new>
#include <optional>
#include <span>
#include <string_view>
#include <utility>
#include <vector>

namespace pistoris::image {
namespace {

constexpr std::uint64_t kMaxDecodedBytes = 256ULL * 1024ULL * 1024ULL;
constexpr std::array<std::uint8_t, 8> kPngSignature = {0x89, 'P', 'N', 'G', 0x0d, 0x0a, 0x1a, 0x0a};
static_assert(kMaxDimension == STBI_MAX_DIMENSIONS);
static_assert(static_cast<std::uint64_t>(kMaxDimension) * kMaxDimension * 4ULL <= kMaxDecodedBytes);

bool validDimensions(std::uint32_t width, std::uint32_t height) noexcept {
  return width != 0 && height != 0 && width <= kMaxDimension && height <= kMaxDimension;
}

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

Error failureCode() noexcept {
  const char* reason = stbi_failure_reason();
  return reason != nullptr && std::strcmp(reason, "outofmem") == 0 ? Error::kOutOfMemory : Error::kMalformed;
}

Error encodedSize(std::span<const std::uint8_t> encoded, int& out) noexcept {
  if (encoded.empty() || encoded.size() > static_cast<std::size_t>(std::numeric_limits<int>::max()))
    return Error::kMalformed;
  out = static_cast<int>(encoded.size());
  return Error::kNone;
}

Error inspectDimensions(std::span<const std::uint8_t> encoded, Info& out) noexcept {
  int encoded_size = 0;
  Error error = encodedSize(encoded, encoded_size);
  if (error != Error::kNone) return error;

  int width = 0;
  int height = 0;
  int components = 0;
  if (stbi_info_from_memory(encoded.data(), encoded_size, &width, &height, &components) == 0) return failureCode();
  if (width <= 0 || height <= 0 || components <= 0 || components > 4 ||
      !validDimensions(static_cast<std::uint32_t>(width), static_cast<std::uint32_t>(height))) {
    return Error::kMalformed;
  }

  out.format = detectFormat(encoded);
  if (out.format == Format::kUnknown) return Error::kMalformed;
  out.width = static_cast<std::uint32_t>(width);
  out.height = static_cast<std::uint32_t>(height);
  out.components = static_cast<std::uint8_t>(components);
  return Error::kNone;
}

struct Decoded {
  stbi_uc* pixels = nullptr;
  Info info;

  Decoded() = default;
  ~Decoded() { stbi_image_free(pixels); }
  Decoded(const Decoded&) = delete;
  Decoded& operator=(const Decoded&) = delete;
};

Error decode(std::span<const std::uint8_t> encoded, Decoded& out) noexcept {
  Info info;
  Error error = inspectDimensions(encoded, info);
  if (error != Error::kNone) return error;

  int width = 0;
  int height = 0;
  int components = 0;
  stbi_uc* pixels =
      stbi_load_from_memory(encoded.data(), static_cast<int>(encoded.size()), &width, &height, &components, 0);
  if (pixels == nullptr) return failureCode();
  if (width != static_cast<int>(info.width) || height != static_cast<int>(info.height) ||
      components != static_cast<int>(info.components)) {
    stbi_image_free(pixels);
    return Error::kMalformed;
  }
  out.pixels = pixels;
  out.info = info;
  return Error::kNone;
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

Error encodePng(const Info& info, std::span<const std::uint8_t> pixels, std::vector<std::uint8_t>& out) {
  const int width = static_cast<int>(info.width);
  const int height = static_cast<int>(info.height);
  const int components = static_cast<int>(info.components);
  if (width > std::numeric_limits<int>::max() / components) return Error::kMalformed;
  const std::size_t row_bytes = static_cast<std::size_t>(width) * static_cast<std::size_t>(components);
  if (pixels.size() != row_bytes * static_cast<std::size_t>(height)) return Error::kMalformed;

  PngWriteContext context;
  const int written =
      stbi_write_png_to_func(appendPng, &context, width, height, components, pixels.data(), width * components);
  if (context.bad_alloc) return Error::kOutOfMemory;
  if (written == 0 || context.failed || context.bytes.empty()) return Error::kMalformed;
  out = std::move(context.bytes);
  return Error::kNone;
}

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

bool sampleCovered(const std::uint8_t* alpha, std::size_t width, std::size_t height, int x, int y) noexcept {
  if (x < 0 || y < 0 || static_cast<std::size_t>(x) >= width || static_cast<std::size_t>(y) >= height) return false;
  return alpha[(static_cast<std::size_t>(y) * width + static_cast<std::size_t>(x)) * 4U] > 127U;
}

void copyColorToTransparentNeighbor(std::uint8_t* alpha, int x, int y, int offset_x, int offset_y, std::size_t width,
                                    std::size_t height) noexcept {
  const int neighbor_x = x + offset_x;
  const int neighbor_y = y + offset_y;
  if (neighbor_x < 0 || neighbor_y < 0 || static_cast<std::size_t>(neighbor_x) >= width ||
      static_cast<std::size_t>(neighbor_y) >= height)
    return;
  std::uint8_t* neighbor = alpha + offset_x * 4 + offset_y * static_cast<int>(width * 4U);
  if (*neighbor != 0) return;
  std::copy_n(alpha - 3, 3, neighbor - 3);
}

void antialiasColorKeyCorner(std::uint8_t* alpha, std::size_t width, std::size_t height, std::size_t x, std::size_t y,
                             bool filled) noexcept {
  constexpr std::size_t kChannels = 4;
  const std::size_t row = width * kChannels;
  int odd_x = 0;
  int odd_y = 0;
  for (std::size_t dy = 0; dy < 2; ++dy) {
    for (std::size_t dx = 0; dx < 2; ++dx) {
      const std::uint8_t value = alpha[kChannels * (x + dx) + row * (y + dy)];
      if ((value > 127U) == filled) {
        odd_x = static_cast<int>(dx);
        odd_y = static_cast<int>(dy);
      }
    }
  }
  x += static_cast<std::size_t>(odd_x);
  y += static_cast<std::size_t>(odd_y);
  std::uint8_t* pixel = alpha + kChannels * x + row * y;

  const int outside_x = odd_x != 0 ? 1 : -1;
  const int outside_y = odd_y != 0 ? 1 : -1;
  const bool adjacent_x =
      sampleCovered(alpha, width, height, static_cast<int>(x) + outside_x, static_cast<int>(y)) == filled;
  const bool adjacent_y =
      sampleCovered(alpha, width, height, static_cast<int>(x), static_cast<int>(y) + outside_y) == filled;
  if (!adjacent_x || !adjacent_y) return;

  const bool diagonal_x =
      sampleCovered(alpha, width, height, static_cast<int>(x) + outside_x, static_cast<int>(y) - outside_y) == filled;
  const bool diagonal_y =
      sampleCovered(alpha, width, height, static_cast<int>(x) - outside_x, static_cast<int>(y) + outside_y) == filled;
  if (!diagonal_x && !diagonal_y) return;

  if (diagonal_x && diagonal_y) {
    constexpr std::uint8_t kArea = static_cast<std::uint8_t>(0.5F * 0.5F / 2.0F * 255.0F);
    *pixel = filled ? static_cast<std::uint8_t>(255U - kArea) : kArea;
    if (!filled)
      copyColorToTransparentNeighbor(
          pixel, static_cast<int>(x), static_cast<int>(y), outside_x, outside_y, width, height);
    return;
  }

  unsigned length = 2;
  const int step_x = diagonal_x ? 0 : outside_x;
  const int step_y = diagonal_x ? outside_y : 0;
  for (int walk_x = static_cast<int>(x) + step_x * 2, walk_y = static_cast<int>(y) + step_y * 2;;
       walk_x += step_x, walk_y += step_y) {
    if (sampleCovered(alpha, width, height, walk_x, walk_y) != filled) {
      length /= 2;
      break;
    }
    if (sampleCovered(alpha, width, height, walk_x + step_x - outside_x, walk_y + step_y - outside_y) == filled) break;
    ++length;
  }

  unsigned area = static_cast<unsigned>(static_cast<float>(length) * 0.5F * 0.5F / 2.0F * 255.0F);
  for (unsigned index = 0; index < length; index += 2) {
    std::uint8_t current = static_cast<std::uint8_t>(std::min(area, 127U));
    if (index + 2 < length) {
      const unsigned remaining =
          static_cast<unsigned>(static_cast<float>(length - index - 2) * 0.5F * static_cast<float>(length - index - 2) /
                                static_cast<float>(length) * 0.5F / 2.0F * 255.0F);
      current = static_cast<std::uint8_t>(std::min(area - remaining, 127U));
      area = remaining;
    }
    *pixel = filled ? static_cast<std::uint8_t>(255U - current) : current;
    if (!filled)
      copyColorToTransparentNeighbor(
          pixel, static_cast<int>(x), static_cast<int>(y), outside_x, outside_y, width, height);
    pixel += step_x * static_cast<int>(kChannels) + step_y * static_cast<int>(row);
  }
}

void antialiasColorKey(std::span<std::uint8_t> rgba, std::size_t width, std::size_t height) noexcept {
  if (width <= 1 || height <= 1) return;
  constexpr std::size_t kChannels = 4;
  const std::size_t row = width * kChannels;
  std::uint8_t* alpha = rgba.data() + 3;
  for (std::size_t y = 0; y < height - 1; ++y) {
    for (std::size_t x = 0; x < width - 1; ++x) {
      std::uint8_t* pixel = alpha + kChannels * x + row * y;
      unsigned coverage = 0;
      for (std::size_t dy = 0; dy < 2; ++dy)
        for (std::size_t dx = 0; dx < 2; ++dx) coverage += pixel[kChannels * dx + row * dy] > 127U ? 1U : 0U;
      if (coverage == 3)
        antialiasColorKeyCorner(alpha, width, height, x, y, false);
      else if (coverage == 1)
        antialiasColorKeyCorner(alpha, width, height, x, y, true);
      else if (coverage == 2 && *pixel == pixel[kChannels + row]) {
        const bool filled = *pixel > 127U;
        constexpr std::uint8_t kArea = static_cast<std::uint8_t>(0.5F * 0.5F / 2.0F * 255.0F);
        *pixel = pixel[kChannels + row] = filled ? static_cast<std::uint8_t>(255U - kArea) : kArea;
        pixel[kChannels] = pixel[row] = filled ? kArea : static_cast<std::uint8_t>(255U - kArea);
        if (!filled) {
          copyColorToTransparentNeighbor(pixel, static_cast<int>(x), static_cast<int>(y), -1, -1, width, height);
          copyColorToTransparentNeighbor(
              pixel + kChannels + row, static_cast<int>(x + 1), static_cast<int>(y + 1), 1, 1, width, height);
        } else {
          copyColorToTransparentNeighbor(
              pixel + kChannels, static_cast<int>(x + 1), static_cast<int>(y), 1, -1, width, height);
          copyColorToTransparentNeighbor(
              pixel + row, static_cast<int>(x), static_cast<int>(y + 1), -1, 1, width, height);
        }
      }
    }
  }
}

Error applyBmpColorKey(const Decoded& decoded, BmpColorKey mode, std::vector<std::uint8_t>& out) {
  if (mode == BmpColorKey::kNone || decoded.info.format != Format::kBmp || decoded.info.components != 3)
    return Error::kNone;
  const std::size_t pixel_count = static_cast<std::size_t>(decoded.info.width) * decoded.info.height;
  bool has_black = false;
  for (std::size_t i = 0; i < pixel_count; ++i) {
    const stbi_uc* pixel = decoded.pixels + i * 3U;
    if (pixel[0] == 0 && pixel[1] == 0 && pixel[2] == 0) {
      has_black = true;
      break;
    }
  }
  if (!has_black) return Error::kNone;

  try {
    out.resize(pixel_count * 4U);
  } catch (const std::bad_alloc&) {
    return Error::kOutOfMemory;
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
  if (mode == BmpColorKey::kAntialiased) antialiasColorKey(out, decoded.info.width, decoded.info.height);
  return Error::kNone;
}

Error rgbaPixels(const Decoded& decoded, BmpColorKey bmp_color_key, std::vector<std::uint8_t>& out) {
  Error error = applyBmpColorKey(decoded, bmp_color_key, out);
  if (error != Error::kNone || !out.empty()) return error;

  const std::size_t pixel_count = static_cast<std::size_t>(decoded.info.width) * decoded.info.height;
  try {
    out.resize(pixel_count * 4U);
  } catch (const std::bad_alloc&) {
    return Error::kOutOfMemory;
  }
  for (std::size_t index = 0; index < pixel_count; ++index) {
    const std::uint8_t* source = decoded.pixels + index * decoded.info.components;
    std::uint8_t* target = out.data() + index * 4U;
    switch (decoded.info.components) {
      case 1:
        target[0] = source[0];
        target[1] = source[0];
        target[2] = source[0];
        target[3] = 0xff;
        break;
      case 2:
        target[0] = source[0];
        target[1] = source[0];
        target[2] = source[0];
        target[3] = source[1];
        break;
      case 3:
        std::copy_n(source, 3, target);
        target[3] = 0xff;
        break;
      case 4:
        std::copy_n(source, 4, target);
        break;
      default:
        return Error::kMalformed;
    }
  }
  return Error::kNone;
}

Error placeDecodedToPng(const Decoded& decoded, std::uint32_t canvas_width, std::uint32_t canvas_height,
                        Placement placement, const std::array<std::uint8_t, 4>& fill, std::vector<std::uint8_t>& out,
                        const std::optional<std::array<std::uint8_t, 4>>& border_color, BmpColorKey bmp_color_key) {
  if (!validDimensions(canvas_width, canvas_height) || !validDimensions(placement.width, placement.height) ||
      placement.x > std::numeric_limits<std::int64_t>::max() - placement.width ||
      placement.y > std::numeric_limits<std::int64_t>::max() - placement.height) {
    return Error::kMalformed;
  }

  std::vector<std::uint8_t> rgba;
  Error error = rgbaPixels(decoded, bmp_color_key, rgba);
  if (error != Error::kNone) return error;

  std::vector<std::uint8_t> canvas;
  try {
    canvas.resize(static_cast<std::size_t>(canvas_width) * canvas_height * 4U);
  } catch (const std::bad_alloc&) {
    return Error::kOutOfMemory;
  }
  for (std::size_t offset = 0; offset < canvas.size(); offset += 4U)
    std::copy(fill.begin(), fill.end(), canvas.begin() + static_cast<std::ptrdiff_t>(offset));

  const std::int64_t right = placement.x + placement.width;
  const std::int64_t bottom = placement.y + placement.height;
  const auto canvas_right = static_cast<std::int64_t>(canvas_width);
  const auto canvas_bottom = static_cast<std::int64_t>(canvas_height);
  const std::int64_t visible_left = std::clamp(placement.x, std::int64_t{0}, canvas_right);
  const std::int64_t visible_top = std::clamp(placement.y, std::int64_t{0}, canvas_bottom);
  const std::int64_t visible_right = std::clamp(right, std::int64_t{0}, canvas_right);
  const std::int64_t visible_bottom = std::clamp(bottom, std::int64_t{0}, canvas_bottom);
  if (visible_left < visible_right && visible_top < visible_bottom) {
    const bool unchanged = placement.width == decoded.info.width && placement.height == decoded.info.height;
    const bool fully_visible =
        visible_left == placement.x && visible_top == placement.y && visible_right == right && visible_bottom == bottom;
    if (unchanged) {
      const std::size_t copy_width = static_cast<std::size_t>(visible_right - visible_left) * 4U;
      const std::size_t source_x = static_cast<std::size_t>(visible_left - placement.x);
      const std::size_t source_y = static_cast<std::size_t>(visible_top - placement.y);
      for (std::size_t row = 0; row < static_cast<std::size_t>(visible_bottom - visible_top); ++row) {
        const std::uint8_t* source = rgba.data() + ((source_y + row) * decoded.info.width + source_x) * 4U;
        std::uint8_t* target = canvas.data() + ((static_cast<std::size_t>(visible_top) + row) * canvas_width +
                                                static_cast<std::size_t>(visible_left)) *
                                                   4U;
        std::copy_n(source, copy_width, target);
      }
    } else if (fully_visible) {
      std::uint8_t* target =
          canvas.data() +
          (static_cast<std::size_t>(placement.y) * canvas_width + static_cast<std::size_t>(placement.x)) * 4U;
      if (stbir_resize(rgba.data(),
                       static_cast<int>(decoded.info.width),
                       static_cast<int>(decoded.info.height),
                       0,
                       target,
                       static_cast<int>(placement.width),
                       static_cast<int>(placement.height),
                       static_cast<int>(canvas_width * 4U),
                       STBIR_RGBA,
                       STBIR_TYPE_UINT8_SRGB,
                       STBIR_EDGE_CLAMP,
                       STBIR_FILTER_DEFAULT) == nullptr) {
        return Error::kOutOfMemory;
      }
    } else {
      std::vector<std::uint8_t> resized;
      try {
        resized.resize(static_cast<std::size_t>(placement.width) * placement.height * 4U);
      } catch (const std::bad_alloc&) {
        return Error::kOutOfMemory;
      }
      if (stbir_resize(rgba.data(),
                       static_cast<int>(decoded.info.width),
                       static_cast<int>(decoded.info.height),
                       0,
                       resized.data(),
                       static_cast<int>(placement.width),
                       static_cast<int>(placement.height),
                       0,
                       STBIR_RGBA,
                       STBIR_TYPE_UINT8_SRGB,
                       STBIR_EDGE_CLAMP,
                       STBIR_FILTER_DEFAULT) == nullptr) {
        return Error::kOutOfMemory;
      }
      const std::size_t copy_width = static_cast<std::size_t>(visible_right - visible_left) * 4U;
      const std::size_t source_x = static_cast<std::size_t>(visible_left - placement.x);
      const std::size_t source_y = static_cast<std::size_t>(visible_top - placement.y);
      for (std::size_t row = 0; row < static_cast<std::size_t>(visible_bottom - visible_top); ++row) {
        const std::uint8_t* source = resized.data() + ((source_y + row) * placement.width + source_x) * 4U;
        std::uint8_t* target = canvas.data() + ((static_cast<std::size_t>(visible_top) + row) * canvas_width +
                                                static_cast<std::size_t>(visible_left)) *
                                                   4U;
        std::copy_n(source, copy_width, target);
      }
    }
  }

  if (border_color) {
    const auto paint = [&](std::uint32_t x, std::uint32_t y) {
      std::copy(border_color->begin(),
                border_color->end(),
                canvas.begin() + static_cast<std::ptrdiff_t>((static_cast<std::size_t>(y) * canvas_width + x) * 4U));
    };
    for (std::uint32_t x = 0; x < canvas_width; ++x) {
      paint(x, 0);
      paint(x, canvas_height - 1U);
    }
    for (std::uint32_t y = 1; y + 1U < canvas_height; ++y) {
      paint(0, y);
      paint(canvas_width - 1U, y);
    }
  }

  Info info{.format = Format::kPng, .width = canvas_width, .height = canvas_height, .components = 4};
  std::vector<std::uint8_t> prepared;
  error = encodePng(info, canvas, prepared);
  if (error != Error::kNone) return error;
  out = std::move(prepared);
  return Error::kNone;
}

}  // namespace

Format detectFormat(std::span<const std::uint8_t> encoded) noexcept {
  if (encoded.size() >= kPngSignature.size() &&
      std::equal(kPngSignature.begin(), kPngSignature.end(), encoded.begin())) {
    return Format::kPng;
  }
  if (encoded.size() >= 3 && encoded[0] == 0xff && encoded[1] == 0xd8 && encoded[2] == 0xff) return Format::kJpeg;
  if (encoded.size() >= 2 && encoded[0] == 'B' && encoded[1] == 'M') return Format::kBmp;
  return plausibleTga(encoded) ? Format::kTga : Format::kUnknown;
}

Error inspectMetadata(std::span<const std::uint8_t> encoded, Info& out) noexcept {
  return inspectDimensions(encoded, out);
}

Error inspect(std::span<const std::uint8_t> encoded, Info* out) noexcept {
  Decoded decoded;
  Error error = decode(encoded, decoded);
  if (error == Error::kNone && out != nullptr) *out = decoded.info;
  return error;
}

Error decodeResizedRgb(std::span<const std::uint8_t> encoded, std::uint32_t width, std::uint32_t height,
                       std::vector<std::uint8_t>& out) {
  if (!validDimensions(width, height)) return Error::kMalformed;
  Info info;
  Error error = inspectDimensions(encoded, info);
  if (error != Error::kNone) return error;

  int decoded_width = 0;
  int decoded_height = 0;
  int components = 0;
  stbi_uc* decoded = stbi_load_from_memory(
      encoded.data(), static_cast<int>(encoded.size()), &decoded_width, &decoded_height, &components, 3);
  if (decoded == nullptr) return failureCode();
  if (decoded_width != static_cast<int>(info.width) || decoded_height != static_cast<int>(info.height) ||
      components != static_cast<int>(info.components)) {
    stbi_image_free(decoded);
    return Error::kMalformed;
  }

  std::vector<std::uint8_t> prepared;
  try {
    prepared.resize(static_cast<std::size_t>(width) * height * 3U);
  } catch (const std::bad_alloc&) {
    stbi_image_free(decoded);
    return Error::kOutOfMemory;
  }
  if (width == info.width && height == info.height) {
    std::copy_n(decoded, prepared.size(), prepared.data());
  } else if (stbir_resize(decoded,
                          decoded_width,
                          decoded_height,
                          0,
                          prepared.data(),
                          static_cast<int>(width),
                          static_cast<int>(height),
                          0,
                          STBIR_RGB,
                          STBIR_TYPE_UINT8_SRGB,
                          STBIR_EDGE_CLAMP,
                          STBIR_FILTER_DEFAULT) == nullptr) {
    stbi_image_free(decoded);
    return Error::kOutOfMemory;
  }
  stbi_image_free(decoded);
  out = std::move(prepared);
  return Error::kNone;
}

Error encodeRgbPng(std::uint32_t width, std::uint32_t height, std::span<const std::uint8_t> rgb,
                   std::vector<std::uint8_t>& out) {
  if (!validDimensions(width, height)) return Error::kMalformed;
  return encodePng({.format = Format::kPng, .width = width, .height = height, .components = 3}, rgb, out);
}

Error prepareVariants(std::span<const std::uint8_t> encoded, bool make_png, bool make_power_of_two,
                      BmpColorKey bmp_color_key, ImageVariants& out) {
  Decoded decoded;
  Error error = decode(encoded, decoded);
  if (error != Error::kNone) return error;

  ImageVariants prepared;
  prepared.source = decoded.info;
  prepared.power_of_two_rescaled =
      make_power_of_two && (!std::has_single_bit(decoded.info.width) || !std::has_single_bit(decoded.info.height));

  std::vector<std::uint8_t> color_keyed;
  Info info = decoded.info;
  if (make_png || prepared.power_of_two_rescaled) {
    error = applyBmpColorKey(decoded, bmp_color_key, color_keyed);
    if (error != Error::kNone) return error;
    if (!color_keyed.empty()) info.components = 4;
  }

  const std::size_t pixel_bytes = static_cast<std::size_t>(info.width) * info.height * info.components;
  const std::span<const std::uint8_t> pixels = color_keyed.empty()
                                                   ? std::span<const std::uint8_t>(decoded.pixels, pixel_bytes)
                                                   : std::span<const std::uint8_t>(color_keyed);
  if (make_png) {
    error = encodePng(info, pixels, prepared.png);
    if (error != Error::kNone) return error;
    prepared.png_info = info;
    prepared.png_info.format = Format::kPng;
  }

  if (make_power_of_two && !prepared.power_of_two_rescaled) {
    try {
      prepared.power_of_two.assign(encoded.begin(), encoded.end());
    } catch (const std::bad_alloc&) {
      return Error::kOutOfMemory;
    }
    prepared.power_of_two_info = decoded.info;
  }

  if (prepared.power_of_two_rescaled) {
    const std::uint32_t target_width = nextPowerOfTwo(info.width);
    const std::uint32_t target_height = nextPowerOfTwo(info.height);
    if (!validDimensions(target_width, target_height)) return Error::kMalformed;

    std::vector<std::uint8_t> resized;
    try {
      resized.resize(static_cast<std::size_t>(target_width) * target_height * info.components);
    } catch (const std::bad_alloc&) {
      return Error::kOutOfMemory;
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
      return Error::kOutOfMemory;
    }
    info.width = target_width;
    info.height = target_height;
    error = encodePng(info, resized, prepared.power_of_two);
    if (error != Error::kNone) return error;
    prepared.power_of_two_info = info;
    prepared.power_of_two_info.format = Format::kPng;
  }

  out = std::move(prepared);
  return Error::kNone;
}

Error transcodeToPng(std::span<const std::uint8_t> encoded, std::vector<std::uint8_t>& out, Info* out_info,
                     BmpColorKey bmp_color_key) {
  ImageVariants prepared;
  const Error error = prepareVariants(encoded, true, false, bmp_color_key, prepared);
  if (error != Error::kNone) return error;
  out = std::move(prepared.png);
  if (out_info != nullptr) *out_info = prepared.png_info;
  return Error::kNone;
}

Error normalizeToPowerOfTwo(std::span<const std::uint8_t> encoded, std::vector<std::uint8_t>& out, Info* out_info,
                            bool* out_rescaled, BmpColorKey bmp_color_key) {
  ImageVariants prepared;
  const Error error = prepareVariants(encoded, false, true, bmp_color_key, prepared);
  if (error != Error::kNone) return error;
  out = std::move(prepared.power_of_two);
  if (out_info != nullptr) *out_info = prepared.power_of_two_info;
  if (out_rescaled != nullptr) *out_rescaled = prepared.power_of_two_rescaled;
  return Error::kNone;
}

Error fitToPng(std::span<const std::uint8_t> encoded, std::uint32_t width, std::uint32_t height, FitMode mode,
               std::vector<std::uint8_t>& out, BmpColorKey bmp_color_key) {
  if (!validDimensions(width, height)) return Error::kMalformed;

  switch (mode) {
    case FitMode::kCenter:
    case FitMode::kTopLeft:
    case FitMode::kTopRight:
    case FitMode::kBottomLeft:
    case FitMode::kBottomRight:
    case FitMode::kStretch:
      break;
    default:
      return Error::kMalformed;
  }

  Decoded decoded;
  Error error = decode(encoded, decoded);
  if (error != Error::kNone) return error;

  std::uint32_t fitted_width = width;
  std::uint32_t fitted_height = height;
  if (mode != FitMode::kStretch) {
    const std::uint64_t width_limited = static_cast<std::uint64_t>(width) * decoded.info.height;
    const std::uint64_t height_limited = static_cast<std::uint64_t>(height) * decoded.info.width;
    if (width_limited <= height_limited) {
      fitted_height = static_cast<std::uint32_t>(std::max<std::uint64_t>(1, width_limited / decoded.info.width));
    } else {
      fitted_width = static_cast<std::uint32_t>(std::max<std::uint64_t>(1, height_limited / decoded.info.height));
    }
  }

  const std::uint32_t remaining_x = width - fitted_width;
  const std::uint32_t remaining_y = height - fitted_height;
  std::uint32_t offset_x = 0;
  std::uint32_t offset_y = 0;
  switch (mode) {
    case FitMode::kCenter:
      offset_x = remaining_x / 2U;
      offset_y = remaining_y / 2U;
      break;
    case FitMode::kTopRight:
      offset_x = remaining_x;
      break;
    case FitMode::kBottomLeft:
      offset_y = remaining_y;
      break;
    case FitMode::kBottomRight:
      offset_x = remaining_x;
      offset_y = remaining_y;
      break;
    case FitMode::kTopLeft:
    case FitMode::kStretch:
      break;
  }
  return placeDecodedToPng(decoded,
                           width,
                           height,
                           {.x = offset_x, .y = offset_y, .width = fitted_width, .height = fitted_height},
                           {},
                           out,
                           std::nullopt,
                           bmp_color_key);
}

Error placeToPng(std::span<const std::uint8_t> encoded, std::uint32_t canvas_width, std::uint32_t canvas_height,
                 Placement placement, std::array<std::uint8_t, 4> fill, std::vector<std::uint8_t>& out,
                 std::optional<std::array<std::uint8_t, 4>> border_color, BmpColorKey bmp_color_key) {
  Decoded decoded;
  const Error error = decode(encoded, decoded);
  if (error != Error::kNone) return error;
  return placeDecodedToPng(decoded, canvas_width, canvas_height, placement, fill, out, border_color, bmp_color_key);
}

Error rotateQuarterTurnToPng(std::span<const std::uint8_t> encoded, QuarterTurn rotation,
                             std::vector<std::uint8_t>& out) {
  switch (rotation) {
    case QuarterTurn::kNone:
    case QuarterTurn::kClockwise90:
    case QuarterTurn::kClockwise180:
    case QuarterTurn::kClockwise270:
      break;
    default:
      return Error::kMalformed;
  }

  Decoded decoded;
  const Error error = decode(encoded, decoded);
  if (error != Error::kNone) return error;

  Info rotated_info = decoded.info;
  rotated_info.format = Format::kPng;
  if (rotation == QuarterTurn::kClockwise90 || rotation == QuarterTurn::kClockwise270)
    std::swap(rotated_info.width, rotated_info.height);

  const std::size_t components = decoded.info.components;
  std::vector<std::uint8_t> rotated;
  try {
    rotated.resize(static_cast<std::size_t>(rotated_info.width) * rotated_info.height * components);
  } catch (const std::bad_alloc&) {
    return Error::kOutOfMemory;
  }

  for (std::uint32_t source_y = 0; source_y < decoded.info.height; ++source_y) {
    for (std::uint32_t source_x = 0; source_x < decoded.info.width; ++source_x) {
      std::uint32_t target_x = source_x;
      std::uint32_t target_y = source_y;
      switch (rotation) {
        case QuarterTurn::kNone:
          break;
        case QuarterTurn::kClockwise90:
          target_x = decoded.info.height - 1U - source_y;
          target_y = source_x;
          break;
        case QuarterTurn::kClockwise180:
          target_x = decoded.info.width - 1U - source_x;
          target_y = decoded.info.height - 1U - source_y;
          break;
        case QuarterTurn::kClockwise270:
          target_x = source_y;
          target_y = decoded.info.width - 1U - source_x;
          break;
      }
      const std::size_t source_offset =
          (static_cast<std::size_t>(source_y) * decoded.info.width + source_x) * components;
      const std::size_t target_offset =
          (static_cast<std::size_t>(target_y) * rotated_info.width + target_x) * components;
      std::copy_n(decoded.pixels + source_offset, components, rotated.data() + target_offset);
    }
  }
  return encodePng(rotated_info, rotated, out);
}

bool hasAlpha(const Info& info) noexcept { return info.components == 2 || info.components == 4; }

std::string_view extension(Format format) noexcept {
  switch (format) {
    case Format::kJpeg:
      return ".jpg";
    case Format::kPng:
      return ".png";
    case Format::kBmp:
      return ".bmp";
    case Format::kTga:
      return ".tga";
    case Format::kUnknown:
      return {};
  }
  return {};
}

}  // namespace pistoris::image
