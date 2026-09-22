// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#include "names.h"

#include "arx_pistoris/base/math.h"
#include "arx_pistoris/sound.hpp"

#include "external/glb/utils/names.h"
#include "external/glb/utils/tokens.h"
#include "modules/cinematic.h"
#include "utils/name_tokens.h"

#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace pistoris::glb_cinematic {
namespace {

constexpr std::string_view kRoot = "arx_cinematic";
constexpr std::string_view kIllustration = "arx_illustration";

bool consumeValue(std::string_view token, std::string_view prefix, std::string_view& out) noexcept {
  if (!token.starts_with(prefix) || token.size() == prefix.size()) return false;
  out = token.substr(prefix.size());
  return true;
}

bool parseColor(std::string_view token, std::string_view prefix, CinematicColor& out) noexcept {
  std::string_view value;
  if (!consumeValue(token, prefix, value)) return false;
  ArxColor3 normalized{};
  if (!glb::parseColor3Token(value, normalized) || normalized.r < 0.0f || normalized.r > 1.0f || normalized.g < 0.0f ||
      normalized.g > 1.0f || normalized.b < 0.0f || normalized.b > 1.0f)
    return false;
  out = {static_cast<std::uint8_t>(std::lround(normalized.r * 255.0f)),
         static_cast<std::uint8_t>(std::lround(normalized.g * 255.0f)),
         static_cast<std::uint8_t>(std::lround(normalized.b * 255.0f))};
  return true;
}

std::string colorToken(std::string_view prefix, CinematicColor color) {
  return std::string(prefix) + glb::formatColor3Token({color.r / 255.0f, color.g / 255.0f, color.b / 255.0f});
}

}  // namespace

std::string rootName(const CinematicData& cinematic) {
  std::vector<std::string> owned;
  owned.reserve(2);
  std::vector<std::string_view> tokens{kRoot};
  if (cinematic.fps != 25.0f) {
    owned.push_back("FPS_" + glb::formatFloatToken(cinematic.fps));
    tokens.push_back(owned.back());
  }
  if (cinematic.keyframes.empty() || cinematic.end_frame != cinematic.keyframes.back().frame) {
    owned.push_back("END_" + std::to_string(cinematic.end_frame));
    tokens.push_back(owned.back());
  }
  tokens.push_back("cinematic");
  return joinDoubleUnderscore(tokens);
}

bool parseRootName(std::string_view name, RootName& out, glb::ParsedLabel* label) {
  return glb::parseRecoverableLabel(
      name, out, label, {{}, {"FPS_", "END_"}}, [](std::span<const std::string_view> tokens, RootName& parsed) {
        if (tokens.empty() || tokens.front() != kRoot) return false;
        RootName result;
        bool seen_fps = false;
        bool seen_end = false;
        for (std::size_t index = 1; index < tokens.size(); ++index) {
          std::string_view value;
          if (consumeValue(tokens[index], "FPS_", value) && !seen_fps) {
            float fps = 0.0f;
            if (!glb::parseFloatToken(value, fps)) return false;
            result.fps = fps;
            seen_fps = true;
          } else if (consumeValue(tokens[index], "END_", value) && !seen_end) {
            const std::optional<std::int32_t> end = glb::parseSignedToken(value);
            if (!end.has_value()) return false;
            result.end_frame = end;
            seen_end = true;
          } else {
            return false;
          }
        }
        parsed = result;
        return true;
      });
}

bool rootNameCandidate(std::string_view name) noexcept {
  return name == kRoot || (name.starts_with(kRoot) && name.substr(kRoot.size()).starts_with("__"));
}

std::string illustrationName(std::uint32_t ordinal, const CinematicIllustration& illustration) {
  std::string result = std::string(kIllustration) + "__" + std::to_string(ordinal);
  if (illustration.subdivision_scale != 1) result += "__SUBDIVISION_" + std::to_string(illustration.subdivision_scale);
  return result + "__illustration_" + std::to_string(ordinal);
}

bool parseIllustrationName(std::string_view name, IllustrationName& out, glb::ParsedLabel* label) {
  return glb::parseRecoverableLabel(
      name, out, label, {{}, {"SUBDIVISION_"}}, [](std::span<const std::string_view> tokens, IllustrationName& parsed) {
        if (tokens.size() < 2 || tokens.size() > 3 || tokens.front() != kIllustration) return false;
        const std::optional<std::uint32_t> ordinal = glb::parseUnsignedToken(tokens[1]);
        if (!ordinal) return false;
        IllustrationName result{.ordinal = *ordinal};
        if (tokens.size() == 3) {
          std::string_view value;
          if (!consumeValue(tokens[2], "SUBDIVISION_", value)) return false;
          const std::optional<std::int32_t> scale = glb::parseSignedToken(value);
          if (!scale) return false;
          result.subdivision_scale = *scale;
        }
        parsed = result;
        return true;
      });
}

bool illustrationNameCandidate(std::string_view name) noexcept {
  return name == kIllustration ||
         (name.starts_with(kIllustration) && name.substr(kIllustration.size()).starts_with("__"));
}

std::string keyName(const CinematicKeyframe& key) {
  std::string result = "KEY_" + std::to_string(key.frame);
  if (key.interpolation == CinematicInterpolation::kNone)
    result += "__NONE";
  else if (key.interpolation == CinematicInterpolation::kBezier)
    result += "__BEZIER";
  if (key.outgoing_speed != 1.0f) result += "__SPEED_" + glb::formatFloatToken(key.outgoing_speed);
  switch (key.base_effect) {
    case CinematicBaseEffect::kNone:
      break;
    case CinematicBaseEffect::kFadeIn:
      result += "__FADE_IN";
      break;
    case CinematicBaseEffect::kFadeOut:
      result += "__FADE_OUT";
      break;
    case CinematicBaseEffect::kBlur:
      result += "__BLUR";
      break;
  }
  if (key.base_effect == CinematicBaseEffect::kFadeIn || key.base_effect == CinematicBaseEffect::kFadeOut) {
    result += "__" + colorToken("COLOR_", key.color);
    result += "__" + colorToken("SECONDARY_", key.secondary_color);
  }
  if (key.crossfade) result += "__CROSSFADE";
  if (key.dream) result += "__DREAM";
  return result + "__key_" + std::to_string(key.frame);
}

bool parseKeyName(std::string_view name, CinematicKeyframe& out, glb::ParsedLabel* label) {
  return glb::parseRecoverableLabel(
      name,
      out,
      label,
      {{"NONE", "BEZIER", "FADE_IN", "FADE_OUT", "BLUR", "CROSSFADE", "DREAM"}, {"SPEED_", "COLOR_", "SECONDARY_"}},
      [](std::span<const std::string_view> tokens, CinematicKeyframe& parsed) {
        if (tokens.empty() || !tokens.front().starts_with("KEY_")) return false;
        const std::optional<std::int32_t> frame = glb::parseSignedToken(tokens.front().substr(4));
        if (!frame) return false;
        CinematicKeyframe key;
        key.frame = *frame;
        bool seen_interpolation = false;
        bool seen_speed = false;
        bool seen_effect = false;
        bool seen_color = false;
        bool seen_secondary = false;
        bool seen_crossfade = false;
        bool seen_dream = false;
        for (std::size_t index = 1; index < tokens.size(); ++index) {
          std::string_view value;
          if (tokens[index] == "NONE" && !seen_interpolation) {
            key.interpolation = CinematicInterpolation::kNone;
            seen_interpolation = true;
          } else if (tokens[index] == "BEZIER" && !seen_interpolation) {
            key.interpolation = CinematicInterpolation::kBezier;
            seen_interpolation = true;
          } else if (consumeValue(tokens[index], "SPEED_", value) && !seen_speed) {
            if (!glb::parseFloatToken(value, key.outgoing_speed)) return false;
            seen_speed = true;
          } else if (tokens[index] == "FADE_IN" && !seen_effect) {
            key.base_effect = CinematicBaseEffect::kFadeIn;
            seen_effect = true;
          } else if (tokens[index] == "FADE_OUT" && !seen_effect) {
            key.base_effect = CinematicBaseEffect::kFadeOut;
            seen_effect = true;
          } else if (tokens[index] == "BLUR" && !seen_effect) {
            key.base_effect = CinematicBaseEffect::kBlur;
            seen_effect = true;
          } else if (tokens[index].starts_with("COLOR_") && !seen_color) {
            if (!parseColor(tokens[index], "COLOR_", key.color)) return false;
            seen_color = true;
          } else if (tokens[index].starts_with("SECONDARY_") && !seen_secondary) {
            if (!parseColor(tokens[index], "SECONDARY_", key.secondary_color)) return false;
            seen_secondary = true;
          } else if (tokens[index] == "CROSSFADE" && !seen_crossfade) {
            key.crossfade = true;
            seen_crossfade = true;
          } else if (tokens[index] == "DREAM" && !seen_dream) {
            key.dream = true;
            seen_dream = true;
          } else {
            return false;
          }
        }
        const bool fade =
            key.base_effect == CinematicBaseEffect::kFadeIn || key.base_effect == CinematicBaseEffect::kFadeOut;
        if (fade ? !seen_color || !seen_secondary : seen_color || seen_secondary) return false;
        parsed = key;
        return true;
      });
}

std::string flashName(const CinematicKeyframe& key) {
  std::string result = "FLASH";
  if (key.post_effect == CinematicPostEffect::kSuppressFlash) return result + "__HIDDEN__flash";
  result += "__" + colorToken("RGB_", key.flash_color);
  result += "__DECAY_" + glb::formatFloatToken(key.flash_decay);
  return result + "__flash";
}

bool parseFlashName(std::string_view name, CinematicKeyframe& key, glb::ParsedLabel* label) {
  return glb::parseRecoverableLabel(name,
                                    key,
                                    label,
                                    {{"HIDDEN"}, {"RGB_", "DECAY_"}},
                                    [](std::span<const std::string_view> tokens, CinematicKeyframe& parsed) {
                                      if (tokens.empty() || tokens.front() != "FLASH") return false;
                                      if (tokens.size() == 2 && tokens[1] == "HIDDEN") {
                                        parsed.post_effect = CinematicPostEffect::kSuppressFlash;
                                        return true;
                                      }
                                      bool seen_color = false;
                                      bool seen_decay = false;
                                      for (std::size_t index = 1; index < tokens.size(); ++index) {
                                        std::string_view value;
                                        if (tokens[index].starts_with("RGB_") && !seen_color) {
                                          if (!parseColor(tokens[index], "RGB_", parsed.flash_color)) return false;
                                          seen_color = true;
                                        } else if (consumeValue(tokens[index], "DECAY_", value) && !seen_decay) {
                                          if (!glb::parseFloatToken(value, parsed.flash_decay)) return false;
                                          seen_decay = true;
                                        } else {
                                          return false;
                                        }
                                      }
                                      if (!seen_color || !seen_decay) return false;
                                      parsed.post_effect = CinematicPostEffect::kFlash;
                                      return true;
                                    });
}

std::string lightName(const CinematicLight& light) {
  if (light.intensity < 0.0f) return "LIGHT__OFF__light";
  return "LIGHT__FALL_IN_" + glb::formatFloatToken(light.fall_in) + "__FALL_OUT_" +
         glb::formatFloatToken(light.fall_out) + "__RGB_" +
         glb::formatColor3Token({light.color.r / 255.0f, light.color.g / 255.0f, light.color.b / 255.0f}) +
         "__INTENSITY_" + glb::formatFloatToken(light.intensity) + "__RANDOM_" +
         glb::formatFloatToken(light.random_intensity) + "__light";
}

bool parseLightName(std::string_view name, CinematicLight& out, glb::ParsedLabel* label) {
  return glb::parseRecoverableLabel(name,
                                    out,
                                    label,
                                    {{"OFF"}, {"FALL_IN_", "FALL_OUT_", "RGB_", "INTENSITY_", "RANDOM_"}},
                                    [](std::span<const std::string_view> tokens, CinematicLight& parsed) {
                                      if (tokens.empty() || tokens.front() != "LIGHT") return false;
                                      if (tokens.size() == 2 && tokens[1] == "OFF") {
                                        parsed = CinematicLight{};
                                        return true;
                                      }
                                      CinematicLight result;
                                      std::array<bool, 5> seen{};
                                      for (std::size_t index = 1; index < tokens.size(); ++index) {
                                        std::string_view value;
                                        if (consumeValue(tokens[index], "FALL_IN_", value) && !seen[0]) {
                                          if (!glb::parseFloatToken(value, result.fall_in)) return false;
                                          seen[0] = true;
                                        } else if (consumeValue(tokens[index], "FALL_OUT_", value) && !seen[1]) {
                                          if (!glb::parseFloatToken(value, result.fall_out)) return false;
                                          seen[1] = true;
                                        } else if (consumeValue(tokens[index], "RGB_", value) && !seen[2]) {
                                          if (!glb::parseColor3Token(value, result.color)) return false;
                                          seen[2] = true;
                                        } else if (consumeValue(tokens[index], "INTENSITY_", value) && !seen[3]) {
                                          if (!glb::parseFloatToken(value, result.intensity)) return false;
                                          seen[3] = true;
                                        } else if (consumeValue(tokens[index], "RANDOM_", value) && !seen[4]) {
                                          if (!glb::parseFloatToken(value, result.random_intensity)) return false;
                                          seen[4] = true;
                                        } else {
                                          return false;
                                        }
                                      }
                                      for (bool present : seen)
                                        if (!present) return false;
                                      if (result.intensity < 0.0f) return false;
                                      result.color.r *= 255.0f;
                                      result.color.g *= 255.0f;
                                      result.color.b *= 255.0f;
                                      parsed = result;
                                      return true;
                                    });
}

std::string soundName(SoundKind kind) {
  return kind == SoundKind::kSpeech ? "SOUND__SPEECH__sound" : "SOUND__EFFECT__sound";
}

bool parseSoundName(std::string_view name, SoundKind& out, glb::ParsedLabel* label) {
  return glb::parseRecoverableLabel(
      name, out, label, {{"EFFECT", "SPEECH"}, {}}, [](std::span<const std::string_view> tokens, SoundKind& parsed) {
        if (tokens.size() != 2 || tokens.front() != "SOUND") return false;
        if (tokens[1] == "EFFECT") {
          parsed = SoundKind::kEffect;
          return true;
        }
        if (tokens[1] == "SPEECH") {
          parsed = SoundKind::kSpeech;
          return true;
        }
        return false;
      });
}

std::string soundPathName(std::string_view path) { return "PATH_" + std::string(path) + "__path"; }

bool parseSoundPathName(std::string_view name, std::string& out) {
  const std::optional<glb::LabeledValue> labeled = glb::splitRequiredLabel(name);
  if (!labeled || !labeled->value.starts_with("PATH_") || labeled->value.size() == 5) return false;
  out = labeled->value.substr(5);
  return true;
}

}  // namespace pistoris::glb_cinematic
