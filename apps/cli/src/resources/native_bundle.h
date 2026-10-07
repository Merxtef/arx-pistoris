// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#pragma once

#include "arx_pistoris/paths/types.h"
#include "arx_pistoris/resource_io/native_bundle.hpp"
#include "arx_pistoris/sound.hpp"
#include "arx_pistoris/texture.hpp"

#include "console/logging.h"
#include "io/service.h"

#include <cstddef>
#include <cstdint>
#include <limits>
#include <string_view>
#include <vector>

namespace cli {

inline const pistoris::resource_io::NativeResourceFile* resolvedNativeResource(
    const pistoris::resource_io::NativeResourceSet& resources,
    const pistoris::resource_io::NativeResourceReference& reference) noexcept {
  if (reference.status() != ARX_OK || reference.resource() >= resources.files().size()) return nullptr;
  return &resources.files()[reference.resource()];
}

inline void warnMissingNativeResource(IoService& io, std::string_view description,
                                      const pistoris::resource_io::NativeResourceReference& reference) {
  if (reference.status() != ARX_RESOURCE_IO_NOT_FOUND || reference.authoredPath().empty()) return;
  if (io.hasReadMounts()) {
    log(ARX_LOG_WARN,
        "%.*s not found: %.*s",
        static_cast<int>(description.size()),
        description.data(),
        static_cast<int>(reference.authoredPath().size()),
        reference.authoredPath().data());
  } else {
    log(ARX_LOG_WARN,
        "Referenced %.*s was not found because there are no readable mount folders: %.*s",
        static_cast<int>(description.size()),
        description.data(),
        static_cast<int>(reference.authoredPath().size()),
        reference.authoredPath().data());
  }
}

inline bool nativeTextureFiles(const pistoris::resource_io::NativeResourceSet& resources,
                               pistoris::resource_io::NativeResourceRole role, ArxResourceKind owner_kind,
                               std::size_t owner_index, IoService& io, std::string_view description,
                               std::vector<pistoris::NativeTextureFile>& out) {
  out.clear();
  for (const auto& reference : resources.references()) {
    if (reference.role() != role || reference.ownerKind() != owner_kind || reference.ownerIndex() != owner_index)
      continue;
    const auto* file = resolvedNativeResource(resources, reference);
    if (!file) {
      warnMissingNativeResource(io, description, reference);
      continue;
    }
    if (reference.element() > std::numeric_limits<pistoris::TextureIndex>::max()) return false;
    pistoris::NativeTextureFile texture;
    texture.source_texture = static_cast<pistoris::TextureIndex>(reference.element());
    texture.resource_path =
        file->logicalPath().empty() ? std::string(reference.authoredPath()) : std::string(file->logicalPath());
    texture.encoded_image.assign(file->data().begin(), file->data().end());
    out.push_back(std::move(texture));
  }
  return true;
}

inline bool nativeSoundFiles(const pistoris::resource_io::NativeResourceSet& resources, ArxResourceKind owner_kind,
                             std::size_t owner_index, IoService& io, std::string_view description,
                             std::vector<pistoris::SoundFile>& out) {
  out.clear();
  for (const auto& reference : resources.references()) {
    if (reference.role() != pistoris::resource_io::NativeResourceRole::kSound || reference.ownerKind() != owner_kind ||
        reference.ownerIndex() != owner_index)
      continue;
    const auto* file = resolvedNativeResource(resources, reference);
    if (!file) {
      warnMissingNativeResource(io, description, reference);
      continue;
    }
    pistoris::SoundIndex source_sound = pistoris::kNoSound;
    if (owner_kind == ARX_RESOURCE_KIND_CINEMATIC) {
      if (pistoris::soundHandleIndex(static_cast<pistoris::SoundHandle>(reference.element()), source_sound) != ARX_OK)
        return false;
    } else {
      if (reference.element() > std::numeric_limits<pistoris::SoundIndex>::max()) return false;
      source_sound = static_cast<pistoris::SoundIndex>(reference.element());
    }
    pistoris::SoundFile sound;
    sound.source_sound = source_sound;
    sound.path = file->logicalPath().empty() ? std::string(reference.authoredPath()) : std::string(file->logicalPath());
    sound.encoded_audio.assign(file->data().begin(), file->data().end());
    out.push_back(std::move(sound));
  }
  return true;
}

}  // namespace cli
