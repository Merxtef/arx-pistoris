// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#include "arx_pistoris/base/image.h"
#include "arx_pistoris/base/indices.h"
#include "arx_pistoris/base/status.h"
#include "arx_pistoris/cinematic.hpp"
#include "arx_pistoris/cinematic/bake.hpp"
#include "arx_pistoris/cinematic/sound.hpp"
#include "arx_pistoris/native/cin.hpp"
#include "arx_pistoris/native/text.hpp"
#include "arx_pistoris/runtime/types.h"
#include "arx_pistoris/sound.hpp"
#include "arx_pistoris/texture.hpp"

#include "api/status_boundary.h"
#include "cinematic/data.h"
#include "cinematic/internal.h"
#include "modules/cinematic.h"
#include "modules/sounds.h"
#include "modules/textures.h"
#include "native/cin.h"
#include "utils/cinematic_constraints.h"
#include "utils/encoded_image.h"
#include "utils/log.h"
#include "utils/native_text.h"
#include "utils/prepared_bytes.h"
#include "utils/resource_path.h"

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

namespace pistoris {
namespace {

constexpr float kCameraZScale = 0.8f;

Texture importedIllustration(std::string_view source) { return textures::fromImagePath(source); }

struct ImportedSoundPath {
  SoundKind kind = SoundKind::kEffect;
  std::string path;
};

bool importedSoundPath(std::string_view source, bool speech, ImportedSoundPath& out) {
  ResourcePathNormalization normalized = normalizeResourcePath(source);
  if (normalized.error != ResourcePathError::kNone || !sounds::validPath(normalized.value)) return false;
  out = {speech ? SoundKind::kSpeech : SoundKind::kEffect, std::move(normalized.value)};
  return true;
}

CinematicColor unpackColor(std::uint32_t color) noexcept {
  return {
      static_cast<std::uint8_t>((color >> 16U) & 0xffU),
      static_cast<std::uint8_t>((color >> 8U) & 0xffU),
      static_cast<std::uint8_t>(color & 0xffU),
  };
}

std::uint32_t packColor(CinematicColor color) noexcept {
  return 0xff000000U | (static_cast<std::uint32_t>(color.r) << 16U) | (static_cast<std::uint32_t>(color.g) << 8U) |
         static_cast<std::uint32_t>(color.b);
}

CinematicBaseEffect importedBaseEffect(std::uint32_t effects) noexcept {
  switch (effects & 0xffU) {
    case 1:
      return CinematicBaseEffect::kFadeIn;
    case 2:
      return CinematicBaseEffect::kFadeOut;
    case 3:
      return CinematicBaseEffect::kBlur;
    default:
      return CinematicBaseEffect::kNone;
  }
}

CinematicPostEffect importedPostEffect(std::uint32_t effects) noexcept {
  const std::uint32_t selector = (effects >> 16U) & 0xffU;
  if (selector == 0) return CinematicPostEffect::kNone;
  return selector == 1 ? CinematicPostEffect::kFlash : CinematicPostEffect::kSuppressFlash;
}

CinematicInterpolation importedInterpolation(std::int16_t value) noexcept {
  if (value < 0) return CinematicInterpolation::kNone;
  return value == 0 ? CinematicInterpolation::kBezier : CinematicInterpolation::kLinear;
}

CinematicKeyframe importedKeyframe(const cin::Keyframe& source, SoundHandle sound) noexcept {
  CinematicKeyframe result;
  result.frame = source.frame;
  result.illustration = static_cast<CinematicIllustrationIndex>(source.bitmap);
  result.camera_position = source.camera_position;
  result.camera_position.z *= kCameraZScale;
  result.camera_roll = source.camera_roll;
  result.outgoing_speed = source.outgoing_speed;
  result.sound = sound;
  result.interpolation = importedInterpolation(source.interpolation);
  result.base_effect = importedBaseEffect(source.effects);
  result.post_effect = importedPostEffect(source.effects);
  result.crossfade = source.crossfade != 0;
  result.dream = ((source.effects >> 8U) & 0xffU) == 1U;
  result.light_active = ((source.effects >> 24U) & 0xffU) == 1U;
  if (result.base_effect == CinematicBaseEffect::kFadeIn || result.base_effect == CinematicBaseEffect::kFadeOut) {
    result.color = unpackColor(source.color);
    result.secondary_color = unpackColor(source.secondary_color);
  }
  if (result.post_effect == CinematicPostEffect::kFlash) {
    result.flash_color = unpackColor(source.flash_color);
    result.flash_decay = source.flash_decay;
  }
  if (result.light_active && source.light.intensity >= 0.0f) {
    result.light = {source.light.position,
                    source.light.fall_in,
                    source.light.fall_out,
                    source.light.color,
                    source.light.intensity,
                    source.light.random_intensity};
  }
  return result;
}

std::uint32_t nativeEffects(const CinematicKeyframe& source) noexcept {
  std::uint32_t effects = 0;
  switch (source.base_effect) {
    case CinematicBaseEffect::kNone:
      break;
    case CinematicBaseEffect::kFadeIn:
      effects = 1;
      break;
    case CinematicBaseEffect::kFadeOut:
      effects = 2;
      break;
    case CinematicBaseEffect::kBlur:
      effects = 3;
      break;
  }
  if (source.dream) effects |= 1U << 8U;
  if (source.post_effect == CinematicPostEffect::kFlash)
    effects |= 1U << 16U;
  else if (source.post_effect == CinematicPostEffect::kSuppressFlash)
    effects |= 2U << 16U;
  if (source.light_active) effects |= 1U << 24U;
  return effects;
}

cin::Keyframe nativeKeyframe(const CinematicKeyframe& source, std::int32_t sound) noexcept {
  cin::Keyframe result;
  result.frame = source.frame;
  result.bitmap = static_cast<std::int32_t>(source.illustration);
  result.effects = nativeEffects(source);
  result.interpolation = static_cast<std::int16_t>(source.interpolation);
  result.crossfade = source.crossfade ? 1 : 0;
  result.camera_position = source.camera_position;
  result.camera_position.z /= kCameraZScale;
  result.camera_roll = source.camera_roll;
  result.color = packColor(source.color);
  result.secondary_color = packColor(source.secondary_color);
  result.flash_color = packColor(source.flash_color);
  result.flash_decay = source.flash_decay;
  result.light = {source.light.position,
                  source.light.fall_in,
                  source.light.fall_out,
                  source.light.color,
                  source.light.intensity,
                  source.light.random_intensity};
  result.outgoing_speed = source.outgoing_speed;
  result.sound = sound;
  return result;
}

std::string nativeSoundFilePath(const SoundsData& sounds, const SoundEncoding& encoding) {
  SoundKind kind = SoundKind::kEffect;
  if (soundHandleKind(encoding.sound, kind) != ARX_OK) return {};
  std::string result;
  if (kind == SoundKind::kEffect) {
    result = "sfx/";
  } else {
    const auto language = sounds.languages.find(encoding.language);
    if (language == sounds.languages.end()) return {};
    result = "speech/" + language->second + '/';
  }
  result += sounds::path(sounds, encoding.sound);
  result += ".wav";
  ResourcePathNormalization normalized = normalizeResourcePath(result);
  return normalized.error == ResourcePathError::kNone ? std::move(normalized.value) : std::string{};
}

void copyPrepared(const PreparedBytes& prepared, std::vector<std::uint8_t>& out) {
  if (prepared.converted.empty())
    out.assign(prepared.borrowed.begin(), prepared.borrowed.end());
  else
    out = prepared.converted;
}

ArxReturnCode validateIllustrationGrids(const CinematicModules& modules) {
  std::vector<image::Info> dimensions(modules.cinematic.illustrations.size());
  std::size_t unknown = 0;
  for (std::size_t index = 0; index < dimensions.size(); ++index) {
    const CinematicIllustration& illustration = modules.cinematic.illustrations[index];
    const Texture& texture = modules.textures.textures[illustration.texture];
    image::Info& info = dimensions[index];
    if (texture.encoded_image.empty()) {
      info.width = 1;
      info.height = 1;
      ++unknown;
    } else if (image::inspectMetadata(texture.encoded_image, info) != image::Error::kNone) {
      return ARX_CINEMATIC_BAD_TEXTURE_IMAGE;
    }
    if (!cinematic_constraints::safeGrid(info.width, info.height, illustration.subdivision_scale, false)) {
      log(ARX_LOG_DEBUG, "Cinematic native bake: illustration {} exceeds renderer grid capacity", index);
      return ARX_CINEMATIC_BAD_ILLUSTRATION_SCALE;
    }
  }
  for (std::size_t index = 0; index < modules.cinematic.keyframes.size(); ++index) {
    const CinematicKeyframe& key = modules.cinematic.keyframes[index];
    const bool dream_crossfade = index != 0 && modules.cinematic.keyframes[index - 1U].dream &&
                                 modules.cinematic.keyframes[index - 1U].crossfade;
    const bool dream_draw = key.dream || dream_crossfade;
    if (!dream_draw) continue;
    const CinematicIllustration& illustration = modules.cinematic.illustrations[key.illustration];
    const image::Info& info = dimensions[key.illustration];
    if (!cinematic_constraints::safeGrid(info.width, info.height, illustration.subdivision_scale, true)) {
      log(ARX_LOG_DEBUG, "Cinematic native bake: illustration {} exceeds dream grid capacity", key.illustration);
      return ARX_CINEMATIC_BAD_ILLUSTRATION_SCALE;
    }
    if (!dream_crossfade) continue;
    const CinematicKeyframe& previous = modules.cinematic.keyframes[index - 1U];
    const CinematicIllustration& previous_illustration = modules.cinematic.illustrations[previous.illustration];
    if (modules.textures.textures[previous_illustration.texture].encoded_image.empty() ||
        modules.textures.textures[illustration.texture].encoded_image.empty())
      continue;
    const image::Info& previous_info = dimensions[previous.illustration];
    const cinematic_constraints::GridVertexDimensions previous_grid = cinematic_constraints::gridVertexDimensions(
        previous_info.width, previous_info.height, previous_illustration.subdivision_scale);
    const cinematic_constraints::GridVertexDimensions grid =
        cinematic_constraints::gridVertexDimensions(info.width, info.height, illustration.subdivision_scale);
    if (previous_grid == grid) continue;
    log(ARX_LOG_WARN,
        "Cinematic native bake: Dream crossfade from frame {} to {} uses different grids ({}x{} -> {}x{} vertices); "
        "distortion may be uneven",
        previous.frame,
        key.frame,
        previous_grid.columns,
        previous_grid.rows,
        grid.columns,
        grid.rows);
  }
  if (unknown != 0)
    log(ARX_LOG_WARN,
        "Cinematic native bake: renderer grid capacity could not be checked against {} missing illustration image(s)",
        unknown);
  return ARX_OK;
}

ArxReturnCode prepareIllustrationFiles(const CinematicModules& modules, ArxImageFormat output_format,
                                       std::vector<NativeTextureFile>& out) {
  std::vector<std::uint8_t> used(modules.textures.textures.size(), 0);
  for (const CinematicIllustration& illustration : modules.cinematic.illustrations) used[illustration.texture] = 1;

  std::vector<textures::ImagePreparationRequest> requests;
  requests.reserve(modules.textures.textures.size());
  for (std::size_t index = 0; index < used.size(); ++index) {
    if (used[index] == 0 || modules.textures.textures[index].encoded_image.empty()) continue;
    const image::Format fallback = output_format == ARX_IMAGE_FORMAT_BMP ? image::Format::kBmp : image::Format::kTga;
    const image::FormatFlags accepted =
        output_format == ARX_IMAGE_FORMAT_UNKNOWN
            ? image::formatFlag(image::Format::kBmp) | image::formatFlag(image::Format::kTga)
            : image::formatFlag(fallback);
    requests.push_back({static_cast<TextureIndex>(index),
                        {.accepted_formats = accepted,
                         .fallback_format = fallback,
                         .require_power_of_two = false,
                         .bmp_color_key = image::BmpColorKey::kNone}});
  }

  std::vector<textures::PreparedImage> prepared;
  ArxReturnCode rc = cinematic_detail::textureError(textures::prepareImages(modules.textures, requests, prepared));
  if (rc != ARX_OK) return rc;
  out.reserve(prepared.size());
  for (std::size_t index = 0; index < prepared.size(); ++index) {
    const TextureIndex texture = requests[index].texture;
    NativeTextureFile file;
    file.source_texture = texture;
    file.resource_path = modules.textures.textures[texture].path;
    file.resource_path += image::extension(prepared[index].info.format);
    copyPrepared(prepared[index].bytes, file.encoded_image);
    out.push_back(std::move(file));
  }
  return ARX_OK;
}

ArxReturnCode prepareSoundFiles(const CinematicModules& modules, const std::unordered_set<SoundHandle>& used,
                                std::vector<CinematicSoundFile>& out) {
  std::vector<sounds::AudioPreparationRequest> requests;
  std::vector<const SoundEncoding*> encodings;
  requests.reserve(modules.sounds.encodings.size());
  encodings.reserve(modules.sounds.encodings.size());
  for (const SoundEncoding& encoding : modules.sounds.encodings) {
    if (!used.contains(encoding.sound)) continue;
    requests.push_back({encoding.sound,
                        encoding.language,
                        {sounds::audioFormatFlag(sounds::AudioFormat::kWav),
                         sounds::AudioFormat::kWav,
                         sounds::ChannelMode::kPreserve,
                         true}});
    encodings.push_back(&encoding);
  }

  std::vector<sounds::PreparedAudio> prepared;
  ArxReturnCode rc = cinematic_detail::soundError(sounds::prepareAudio(modules.sounds, requests, prepared));
  if (rc != ARX_OK) return rc;
  out.reserve(prepared.size());
  for (std::size_t index = 0; index < prepared.size(); ++index) {
    const SoundEncoding& encoding = *encodings[index];
    CinematicSoundFile file;
    file.source_sound = encoding.sound;
    file.language = encoding.language;
    file.path = nativeSoundFilePath(modules.sounds, encoding);
    if (file.path.empty()) return ARX_INTERNAL_ERROR;
    copyPrepared(prepared[index].bytes, file.encoded_audio);
    out.push_back(std::move(file));
  }
  return ARX_OK;
}

}  // namespace

ArxReturnCode Cinematic::importNative(Cinematic& out, const cin::Data& native,
                                      std::vector<std::string>* illustration_source_paths,
                                      std::vector<CinematicSoundSourceReference>* sound_sources,
                                      NativeTextMode text_mode) noexcept {
  return api_detail::statusBoundary([&]() -> ArxReturnCode {
    if (!native_text::validMode(text_mode)) return ARX_INVALID_OPTIONS;
    ArxReturnCode rc = validateCin(&native);
    if (rc != ARX_OK) return rc;

    Cinematic result;
    result.data_->cinematic.end_frame = native.end_frame;
    result.data_->cinematic.fps = native.fps;

    std::vector<std::string> illustration_sources;
    std::unordered_map<std::string, std::string, ResourcePathIdentityHash, ResourcePathIdentityEqual>
        decoded_illustration_sources;
    if (illustration_source_paths) illustration_sources.reserve(native.bitmaps.size());
    std::unordered_map<std::string, TextureIndex, ResourcePathIdentityHash, ResourcePathIdentityEqual> textures_by_path;
    textures_by_path.reserve(native.bitmaps.size());
    result.data_->cinematic.illustrations.reserve(native.bitmaps.size());
    for (const cin::Bitmap& bitmap : native.bitmaps) {
      std::string decoded_path;
      if (!native_text::decode(bitmap.path, text_mode, decoded_path)) return ARX_CIN_BAD_BITMAP_PATH;
      const auto [source, new_source] = decoded_illustration_sources.try_emplace(decoded_path, bitmap.path);
      if (!new_source && !ResourcePathIdentityEqual{}(source->second, bitmap.path)) {
        log(ARX_LOG_ERROR, "CIN -> Cinematic: distinct native illustration paths decode to '{}'", decoded_path);
        return ARX_CINEMATIC_BAD_TEXTURE_PATH;
      }
      Texture texture = importedIllustration(decoded_path);
      auto existing = textures_by_path.find(std::string_view(texture.path));
      TextureIndex texture_index = kNoTexture;
      if (existing != textures_by_path.end()) {
        texture_index = existing->second;
        Texture& retained = result.data_->textures.textures[texture_index];
        if (retained.external_image_extension.empty())
          retained.external_image_extension = std::move(texture.external_image_extension);
      } else {
        texture_index = static_cast<TextureIndex>(result.data_->textures.textures.size());
        result.data_->textures.textures.push_back(std::move(texture));
        textures_by_path.emplace(result.data_->textures.textures.back().path, texture_index);
        if (illustration_source_paths) illustration_sources.push_back(std::move(decoded_path));
      }
      result.data_->cinematic.illustrations.push_back({texture_index, bitmap.subdivision_scale});
    }
    textures::PathRepairInfo texture_repairs;
    rc = cinematic_detail::textureError(textures::repairPaths(result.data_->textures.textures, &texture_repairs));
    if (rc != ARX_OK) return rc;
    for (const textures::PathRepairInfo::Repair& repair : texture_repairs.repairs)
      log(ARX_LOG_WARN,
          "CIN -> Cinematic: illustration path '{}' normalized to '{}'",
          repair.original,
          repair.repaired);

    std::vector<SoundHandle> native_sounds(native.sounds.size(), kNoSoundHandle);
    std::unordered_map<std::string, SoundHandle, ResourcePathIdentityHash, ResourcePathIdentityEqual> effects;
    std::unordered_map<std::string, SoundHandle, ResourcePathIdentityHash, ResourcePathIdentityEqual> speech;
    effects.reserve(native.sounds.size());
    speech.reserve(native.sounds.size());
    std::vector<CinematicSoundSourceReference> sources;
    std::unordered_map<std::string, std::string, ResourcePathIdentityHash, ResourcePathIdentityEqual>
        decoded_effect_sources;
    std::unordered_map<std::string, std::string, ResourcePathIdentityHash, ResourcePathIdentityEqual>
        decoded_speech_sources;
    if (sound_sources) sources.reserve(native.sounds.size());
    for (const cin::Keyframe& key : native.keyframes) {
      if (key.sound < 0 || native_sounds[key.sound] != kNoSoundHandle) continue;
      const cin::Sound& native_sound = native.sounds[key.sound];
      std::string decoded_path;
      if (!native_text::decode(native_sound.path, text_mode, decoded_path)) return ARX_CIN_BAD_SOUND_PATH;
      auto& decoded_sources = native_sound.speech ? decoded_speech_sources : decoded_effect_sources;
      const auto [source, new_source] = decoded_sources.try_emplace(decoded_path, native_sound.path);
      if (!new_source && !ResourcePathIdentityEqual{}(source->second, native_sound.path)) {
        log(ARX_LOG_ERROR, "CIN -> Cinematic: distinct native sound paths decode to '{}'", decoded_path);
        return ARX_CINEMATIC_BAD_SOUND_PATH;
      }
      ImportedSoundPath parsed;
      if (!importedSoundPath(decoded_path, native_sound.speech, parsed)) return ARX_CINEMATIC_BAD_SOUND_PATH;
      auto& identities = parsed.kind == SoundKind::kSpeech ? speech : effects;
      const auto existing = identities.find(parsed.path);
      if (existing != identities.end()) {
        native_sounds[key.sound] = existing->second;
        continue;
      }
      const std::string original = parsed.path;
      sounds::PathRepairInfo repairs;
      rc = cinematic_detail::soundError(
          sounds::repairPath(result.data_->sounds, parsed.kind, parsed.path, kNoSound, &repairs));
      if (rc != ARX_OK) return rc;
      const SoundHandle handle = sounds::addPath(result.data_->sounds, parsed.kind, std::move(parsed.path));
      identities.emplace(original, handle);
      native_sounds[key.sound] = handle;
      if (sound_sources) sources.push_back({handle, std::move(decoded_path)});
      for (const sounds::PathRepairInfo::Repair& repair : repairs.repairs)
        log(ARX_LOG_WARN, "CIN -> Cinematic: sound path '{}' normalized to '{}'", repair.original, repair.repaired);
    }

    result.data_->cinematic.keyframes.reserve(native.keyframes.size());
    std::size_t ignored_grille_keys = 0;
    for (const cin::Keyframe& key : native.keyframes) {
      const SoundHandle sound = key.sound < 0 ? kNoSoundHandle : native_sounds[key.sound];
      if (key.sound >= 0 && sound == kNoSoundHandle) return ARX_INTERNAL_ERROR;
      if (key.bitmap_position.x != 0.0f || key.bitmap_position.y != 0.0f || key.bitmap_position.z != 0.0f ||
          key.bitmap_roll != 0.0f)
        ++ignored_grille_keys;
      result.data_->cinematic.keyframes.push_back(importedKeyframe(key, sound));
    }
    if (ignored_grille_keys != 0)
      log(ARX_LOG_WARN,
          "CIN -> Cinematic: ignored nonzero illustration grid transforms on {} keyframe(s)",
          ignored_grille_keys);

    rc = result.validate();
    if (rc != ARX_OK) return rc;
    out.swap(result);
    if (illustration_source_paths) *illustration_source_paths = std::move(illustration_sources);
    if (sound_sources) *sound_sources = std::move(sources);
    log(ARX_LOG_INFO,
        "CIN -> Cinematic: {} keyframes, {} illustrations, {} textures, {} sounds",
        out.keyframeCount(),
        out.illustrationCount(),
        out.textureCount(),
        out.soundCount(SoundKind::kEffect) + out.soundCount(SoundKind::kSpeech));
    return ARX_OK;
  });
}

ArxReturnCode Cinematic::bakeNative(cin::Data& out) const noexcept {
  NativeCinematicBundle bundle;
  const ArxReturnCode rc =
      bakeNativeBundle({.include_illustration_files = false, .include_sound_files = false}, bundle);
  if (rc != ARX_OK) return rc;
  out = std::move(bundle.cin);
  return ARX_OK;
}

ArxReturnCode Cinematic::bakeNativeBundle(const NativeCinematicBakeOptions& options,
                                          NativeCinematicBundle& out) const noexcept {
  return api_detail::statusBoundary([&]() -> ArxReturnCode {
    if (!native_text::validMode(options.text_mode)) return ARX_INVALID_OPTIONS;
    if (options.illustration_format != ARX_IMAGE_FORMAT_UNKNOWN &&
        options.illustration_format != ARX_IMAGE_FORMAT_BMP && options.illustration_format != ARX_IMAGE_FORMAT_TGA)
      return ARX_INVALID_OPTIONS;
    ArxReturnCode rc = cinematic_detail::validateStructure(static_cast<const CinematicModules&>(*data_));
    if (rc != ARX_OK) return rc;
    rc = validateIllustrationGrids(static_cast<const CinematicModules&>(*data_));
    if (rc != ARX_OK) return rc;

    NativeCinematicBundle result;
    result.cin.end_frame = data_->cinematic.end_frame;
    result.cin.fps = data_->cinematic.fps;
    result.cin.bitmaps.reserve(data_->cinematic.illustrations.size());
    for (const CinematicIllustration& illustration : data_->cinematic.illustrations) {
      const Texture& texture = data_->textures.textures[illustration.texture];
      std::string path;
      if (!native_text::encode(texture.path, options.text_mode, path)) return ARX_CINEMATIC_BAD_TEXTURE_PATH;
      result.cin.bitmaps.push_back({illustration.subdivision_scale, std::move(path)});
    }

    std::unordered_map<SoundHandle, std::int32_t> sound_indices;
    sound_indices.reserve(data_->cinematic.keyframes.size());
    std::unordered_set<SoundHandle> used_sounds;
    used_sounds.reserve(data_->cinematic.keyframes.size());
    for (const CinematicKeyframe& key : data_->cinematic.keyframes) {
      if (key.sound == kNoSoundHandle || sound_indices.contains(key.sound)) continue;
      if (sound_indices.size() >= kCinMaxSounds) return ARX_CIN_BAD_SOUND_COUNT;
      const auto index = static_cast<std::int32_t>(sound_indices.size());
      sound_indices.emplace(key.sound, index);
      used_sounds.insert(key.sound);
      SoundKind kind = SoundKind::kEffect;
      if (soundHandleKind(key.sound, kind) != ARX_OK) return ARX_CINEMATIC_BAD_KEY_SOUND;
      std::string path;
      if (!native_text::encode(sounds::path(data_->sounds, key.sound), options.text_mode, path))
        return ARX_CINEMATIC_BAD_SOUND_PATH;
      if (path.empty()) return ARX_CINEMATIC_BAD_KEY_SOUND;
      result.cin.sounds.push_back({std::move(path), kind == SoundKind::kSpeech});
    }

    result.cin.keyframes.reserve(data_->cinematic.keyframes.size());
    for (const CinematicKeyframe& key : data_->cinematic.keyframes) {
      const std::int32_t sound = key.sound == kNoSoundHandle ? -1 : sound_indices.at(key.sound);
      result.cin.keyframes.push_back(nativeKeyframe(key, sound));
    }
    rc = validateCin(&result.cin);
    if (rc != ARX_OK) return rc;

    if (options.include_illustration_files) {
      rc = prepareIllustrationFiles(
          static_cast<const CinematicModules&>(*data_), options.illustration_format, result.illustration_files);
      if (rc != ARX_OK) return rc;
    }
    if (options.include_sound_files) {
      rc = prepareSoundFiles(static_cast<const CinematicModules&>(*data_), used_sounds, result.sound_files);
      if (rc != ARX_OK) return rc;
    }
    out = std::move(result);
    log(ARX_LOG_INFO,
        "Cinematic -> CIN: {} keyframes, {} illustrations, {} sounds",
        out.cin.keyframes.size(),
        out.cin.bitmaps.size(),
        out.cin.sounds.size());
    return ARX_OK;
  });
}

}  // namespace pistoris
