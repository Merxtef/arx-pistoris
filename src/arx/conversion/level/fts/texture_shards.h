// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#pragma once

#include "arx_pistoris/indices.h"
#include "arx_pistoris/native/fts.hpp"

#include <cstddef>
#include <cstdint>
#include <unordered_map>
#include <vector>

namespace pistoris::arx_level_conversion::fts_bake {

class TextureShardAllocator {
 public:
  TextureShardAllocator(std::size_t room_count, std::size_t texture_count,
                        std::uint32_t max_corners = static_cast<std::uint32_t>(kFtsMaxRoomTextureVertices));

  std::size_t assign(std::size_t room, TextureIndex texture, std::uint32_t corners);

 private:
  using RemainingByTexture = std::unordered_map<TextureIndex, std::vector<std::uint32_t>>;

  std::vector<RemainingByTexture> remaining_by_room_;
  std::vector<std::size_t> shard_counts_;
  std::uint32_t max_corners_;
};

}  // namespace pistoris::arx_level_conversion::fts_bake
