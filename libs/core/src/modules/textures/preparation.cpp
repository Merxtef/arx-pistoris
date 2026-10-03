// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#include "arx_pistoris/base/indices.h"
#include "arx_pistoris/runtime/types.h"

#include "modules/textures.h"
#include "utils/encoded_image.h"
#include "utils/encoded_image_internal.h"
#include "utils/log.h"

#include <algorithm>
#include <array>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <new>
#include <span>
#include <utility>
#include <vector>

namespace pistoris::textures {
namespace {

bool validOptions(const ImagePreparationOptions& options) noexcept {
  return options.accepted_formats != 0 && (options.accepted_formats & ~image::kFormatsAll) == 0 &&
         (options.fallback_format == image::Format::kPng || options.fallback_format == image::Format::kTga ||
          options.fallback_format == image::Format::kBmp) &&
         (!options.require_power_of_two || options.fallback_format == image::Format::kPng) &&
         (options.bmp_color_key == image::BmpColorKey::kNone || options.bmp_color_key == image::BmpColorKey::kBinary ||
          options.bmp_color_key == image::BmpColorKey::kAntialiased);
}

Error imageError(image::Error error) noexcept {
  if (error == image::Error::kOutOfMemory) return Error::kOutOfMemory;
  return error == image::Error::kNone ? Error::kNone : Error::kBadImage;
}

constexpr std::size_t kNoRequest = std::numeric_limits<std::size_t>::max();

enum class PreparationKind : std::uint8_t {
  kBorrow,
  kPng,
  kTga,
  kBmp,
  kPowerOfTwo,
};

struct ImagePlan {
  TextureIndex texture = kNoTexture;
  image::Info source;
  image::Info png_info;
  image::Info tga_info;
  image::Info bmp_info;
  image::Info pot_info;
  image::BmpColorKey bmp_color_key = image::BmpColorKey::kNone;
  bool convert_png = false;
  bool convert_tga = false;
  bool convert_bmp = false;
  bool convert_pot = false;
  bool pot_rescaled = false;
  std::size_t last_png = kNoRequest;
  std::size_t last_tga = kNoRequest;
  std::size_t last_bmp = kNoRequest;
  std::size_t last_pot = kNoRequest;
  std::vector<std::uint8_t> png;
  std::vector<std::uint8_t> tga;
  std::vector<std::uint8_t> bmp;
  std::vector<std::uint8_t> pot;
};

PreparationKind preparationKind(const ImagePreparationOptions& options, const image::Info& source) noexcept {
  const bool accepted = (options.accepted_formats & image::formatFlag(source.format)) != 0;
  const bool valid_dimensions =
      !options.require_power_of_two || (std::has_single_bit(source.width) && std::has_single_bit(source.height));
  if (!valid_dimensions) return PreparationKind::kPowerOfTwo;
  if (accepted) {
    // The game drops alpha when loading two-channel TGA
    if (source.format == image::Format::kTga && source.components == 2) return PreparationKind::kTga;
    return PreparationKind::kBorrow;
  }
  switch (options.fallback_format) {
    case image::Format::kTga:
      return PreparationKind::kTga;
    case image::Format::kBmp:
      return PreparationKind::kBmp;
    default:
      return PreparationKind::kPng;
  }
}

}  // namespace

Error prepareImages(const TexturesData& textures, std::span<const ImagePreparationRequest> requests,
                    std::vector<PreparedImage>& out) {
  try {
    std::vector<PreparedImage> prepared(requests.size());
    std::vector<std::array<std::size_t, 3>> plan_by_texture(textures.textures.size(),
                                                            {kNoRequest, kNoRequest, kNoRequest});
    std::vector<PreparationKind> kinds(requests.size(), PreparationKind::kBorrow);
    std::vector<std::size_t> request_plans(requests.size(), kNoRequest);
    std::vector<ImagePlan> plans;
    plans.reserve(std::min(textures.textures.size(), requests.size()));
    for (std::size_t index = 0; index < requests.size(); ++index) {
      const ImagePreparationRequest& request = requests[index];
      if (static_cast<std::size_t>(request.texture) >= textures.textures.size()) {
        log(ARX_LOG_DEBUG,
            "Image preparation: request {} references texture {} with texture count {}",
            index,
            request.texture,
            textures.textures.size());
        return Error::kBadIndex;
      }
      if (!validOptions(request.options)) {
        log(ARX_LOG_DEBUG,
            "Image preparation: request {} for texture {} has invalid options: formats {:#x}, fallback {}",
            index,
            request.texture,
            request.options.accepted_formats,
            static_cast<int>(request.options.fallback_format));
        return Error::kInvalidOptions;
      }
      const Texture& texture = textures.textures[request.texture];
      if (texture.encoded_image.empty()) {
        log(ARX_LOG_DEBUG,
            "Image preparation: request {} texture {} '{}' has no encoded image",
            index,
            request.texture,
            texture.path);
        return Error::kBadImage;
      }

      const auto color_key_index = static_cast<std::size_t>(request.options.bmp_color_key);
      std::size_t& plan_index = plan_by_texture[request.texture][color_key_index];
      if (plan_index == kNoRequest) {
        ImagePlan plan;
        plan.texture = request.texture;
        plan.bmp_color_key = request.options.bmp_color_key;
        const Error error = imageError(image::inspectMetadata(texture.encoded_image, plan.source));
        if (error != Error::kNone) {
          log(ARX_LOG_DEBUG,
              "Image preparation: texture {} '{}' metadata inspection failed: {} bytes, error {}",
              request.texture,
              texture.path,
              texture.encoded_image.size(),
              static_cast<int>(error));
          return error;
        }
        plan_index = plans.size();
        plans.push_back(std::move(plan));
      }
      request_plans[index] = plan_index;
      ImagePlan& plan = plans[plan_index];
      const PreparationKind kind = preparationKind(request.options, plan.source);
      kinds[index] = kind;
      switch (kind) {
        case PreparationKind::kBorrow:
          break;
        case PreparationKind::kPng:
          plan.convert_png = true;
          plan.last_png = index;
          break;
        case PreparationKind::kTga:
          plan.convert_tga = true;
          plan.last_tga = index;
          break;
        case PreparationKind::kBmp:
          plan.convert_bmp = true;
          plan.last_bmp = index;
          break;
        case PreparationKind::kPowerOfTwo:
          plan.convert_pot = true;
          plan.last_pot = index;
          break;
      }
    }

    for (ImagePlan& plan : plans) {
      const Texture& texture = textures.textures[plan.texture];
      if (plan.convert_png || plan.convert_pot) {
        image::ImageVariants variants;
        const Error error = imageError(image::prepareVariants(
            texture.encoded_image, plan.convert_png, plan.convert_pot, plan.bmp_color_key, variants));
        if (error != Error::kNone) {
          log(ARX_LOG_DEBUG,
              "Image preparation: texture {} '{}' conversion failed: PNG {}, power-of-two {}, error {}",
              plan.texture,
              texture.path,
              plan.convert_png,
              plan.convert_pot,
              static_cast<int>(error));
          return error;
        }
        plan.source = variants.source;
        plan.png_info = variants.png_info;
        plan.pot_info = variants.power_of_two_info;
        plan.png = std::move(variants.png);
        plan.pot = std::move(variants.power_of_two);
        plan.pot_rescaled = variants.power_of_two_rescaled;
      }
      if (plan.convert_tga) {
        const Error tga_error =
            imageError(image::transcodeToTga(texture.encoded_image, plan.tga, &plan.tga_info, plan.bmp_color_key));
        if (tga_error != Error::kNone) {
          log(ARX_LOG_DEBUG,
              "Image preparation: texture {} '{}' TGA conversion failed: error {}",
              plan.texture,
              texture.path,
              static_cast<int>(tga_error));
          return tga_error;
        }
      }
      if (plan.convert_bmp) {
        const Error bmp_error =
            imageError(image::transcodeToBmp(texture.encoded_image, plan.bmp, &plan.bmp_info, plan.bmp_color_key));
        if (bmp_error != Error::kNone) {
          log(ARX_LOG_DEBUG,
              "Image preparation: texture {} '{}' BMP conversion failed: error {}",
              plan.texture,
              texture.path,
              static_cast<int>(bmp_error));
          return bmp_error;
        }
      }
    }

    for (std::size_t index = 0; index < requests.size(); ++index) {
      const ImagePreparationRequest& request = requests[index];
      ImagePlan& plan = plans[request_plans[index]];
      PreparedImage& target = prepared[index];
      switch (kinds[index]) {
        case PreparationKind::kBorrow:
          target.bytes.borrowed = textures.textures[request.texture].encoded_image;
          target.info = plan.source;
          break;
        case PreparationKind::kPng:
          target.info = plan.png_info;
          if (index == plan.last_png)
            target.bytes.converted = std::move(plan.png);
          else
            target.bytes.converted = plan.png;
          break;
        case PreparationKind::kTga:
          target.info = plan.tga_info;
          if (index == plan.last_tga)
            target.bytes.converted = std::move(plan.tga);
          else
            target.bytes.converted = plan.tga;
          break;
        case PreparationKind::kBmp:
          target.info = plan.bmp_info;
          if (index == plan.last_bmp)
            target.bytes.converted = std::move(plan.bmp);
          else
            target.bytes.converted = plan.bmp;
          break;
        case PreparationKind::kPowerOfTwo:
          target.info = plan.pot_info;
          target.rescaled = plan.pot_rescaled;
          if (index == plan.last_pot)
            target.bytes.converted = std::move(plan.pot);
          else
            target.bytes.converted = plan.pot;
          break;
      }
    }
    out = std::move(prepared);
    return Error::kNone;
  } catch (const std::bad_alloc&) {
    return Error::kOutOfMemory;
  }
}

}  // namespace pistoris::textures
