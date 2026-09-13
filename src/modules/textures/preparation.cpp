// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#include "arx_pistoris/base/indices.h"
#include "arx_pistoris/runtime/types.h"

#include "modules/textures.h"
#include "utils/encoded_image.h"
#include "utils/encoded_image_internal.h"
#include "utils/log.h"

#include <algorithm>
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
         options.fallback_format == image::Format::kPng;
}

Error imageError(image::Error error) noexcept {
  if (error == image::Error::kOutOfMemory) return Error::kOutOfMemory;
  return error == image::Error::kNone ? Error::kNone : Error::kBadImage;
}

constexpr std::size_t kNoRequest = std::numeric_limits<std::size_t>::max();

enum class PreparationKind : std::uint8_t {
  kBorrow,
  kPng,
  kPowerOfTwo,
};

struct ImagePlan {
  TextureIndex texture = kNoTexture;
  image::Info source;
  image::Info png_info;
  image::Info pot_info;
  bool convert_png = false;
  bool convert_pot = false;
  bool pot_rescaled = false;
  std::size_t last_png = kNoRequest;
  std::size_t last_pot = kNoRequest;
  std::vector<std::uint8_t> png;
  std::vector<std::uint8_t> pot;
};

PreparationKind preparationKind(const ImagePreparationOptions& options, const image::Info& source) noexcept {
  const bool accepted = (options.accepted_formats & image::formatFlag(source.format)) != 0;
  const bool valid_dimensions =
      !options.require_power_of_two || (std::has_single_bit(source.width) && std::has_single_bit(source.height));
  if (!valid_dimensions) return PreparationKind::kPowerOfTwo;
  return accepted ? PreparationKind::kBorrow : PreparationKind::kPng;
}

}  // namespace

Error prepareImages(const TexturesData& textures, std::span<const ImagePreparationRequest> requests,
                    std::vector<PreparedImage>& out) {
  try {
    std::vector<PreparedImage> prepared(requests.size());
    std::vector<std::size_t> plan_by_texture(textures.textures.size(), kNoRequest);
    std::vector<PreparationKind> kinds(requests.size(), PreparationKind::kBorrow);
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

      std::size_t& plan_index = plan_by_texture[request.texture];
      if (plan_index == kNoRequest) {
        ImagePlan plan;
        plan.texture = request.texture;
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
        case PreparationKind::kPowerOfTwo:
          plan.convert_pot = true;
          plan.last_pot = index;
          break;
      }
    }

    for (ImagePlan& plan : plans) {
      const Texture& texture = textures.textures[plan.texture];
      image::ImageVariants variants;
      const Error error = imageError(image::prepareVariants(
          texture.encoded_image, plan.convert_png, plan.convert_pot, image::BmpColorKey::kAntialiased, variants));
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

    for (std::size_t index = 0; index < requests.size(); ++index) {
      const ImagePreparationRequest& request = requests[index];
      ImagePlan& plan = plans[plan_by_texture[request.texture]];
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
