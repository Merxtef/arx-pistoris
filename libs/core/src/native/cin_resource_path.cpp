// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#include "native/cin_resource_path.h"

#include "arx_pistoris/native/cin.hpp"

#include "native/resource_path.h"

#include <cstddef>
#include <string>
#include <string_view>
#include <utility>

namespace pistoris {
namespace {

char lowerAscii(char value) noexcept {
  return value >= 'A' && value <= 'Z' ? static_cast<char>(value - 'A' + 'a') : value;
}

void lowerAscii(std::string& value) noexcept {
  for (char& character : value) character = lowerAscii(character);
}

}  // namespace

bool decodeCinIllustrationPath(std::string_view stored_path, std::string& out) {
  std::string path(stored_path);
  lowerAscii(path);
  constexpr std::string_view kRootMarker = "arx\\";
  const std::size_t marker = path.find(kRootMarker);
  if (marker != std::string::npos) path.erase(0, marker + kRootMarker.size());
  return normalizeNativeResourceStem(path, out);
}

bool encodeCinIllustrationPath(std::string_view path, std::string& out) {
  std::string encoded;
  if (!encodeNativeResourceStem(path, encoded)) return false;
  out = "arx\\";
  out += encoded;
  return true;
}

bool decodeCinSoundPath(std::string_view stored_path, cin::Sound& out) {
  std::string path(stored_path);
  lowerAscii(path);

  const std::size_t sfx = path.find("\\sfx\\");
  if (sfx != std::string::npos) path.erase(0, sfx + 5U);

  bool speech = false;
  const std::size_t speech_marker = path.find("speech\\");
  if (speech_marker != std::string::npos) {
    const std::size_t language_end = path.find('\\', speech_marker + 7U);
    if (language_end != std::string::npos) {
      path.erase(0, language_end + 1U);
      speech = true;
    }
  }

  std::string canonical;
  if (!normalizeNativeResourceStem(path, canonical)) return false;
  out.path = std::move(canonical);
  out.speech = speech;
  return true;
}

bool encodeCinSoundPath(const cin::Sound& sound, std::string& out) {
  std::string encoded;
  if (!encodeNativeResourceStem(sound.path, encoded)) return false;
  out = sound.speech ? "speech\\\\" : "";
  out += encoded;
  return true;
}

}  // namespace pistoris
