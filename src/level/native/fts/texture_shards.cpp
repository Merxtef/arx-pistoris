// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#include "level/native/fts/texture_shards.h"

#include "arx_pistoris/base/indices.h"

#include <cassert>
#include <cstddef>
#include <cstdint>
#include <vector>

namespace pistoris::level_native::fts_bake {

TextureShardAllocator::TextureShardAllocator(std::size_t room_count, std::size_t texture_count,
                                             std::uint32_t max_corners)
    : remaining_by_room_(room_count), shard_counts_(texture_count, 1U), max_corners_(max_corners) {
  assert(max_corners != 0);
}

std::size_t TextureShardAllocator::assign(std::size_t room, TextureIndex texture, std::uint32_t corners) {
  assert(room < remaining_by_room_.size());
  assert(static_cast<std::size_t>(texture) < shard_counts_.size());
  assert(corners != 0 && corners <= max_corners_);

  std::vector<std::uint32_t>& remaining = remaining_by_room_[room][texture];
  const std::size_t shard_count = shard_counts_[static_cast<std::size_t>(texture)];
  for (std::size_t shard = 0; shard < shard_count; ++shard) {
    const std::uint32_t available = shard < remaining.size() ? remaining[shard] : max_corners_;
    if (corners > available) continue;
    if (remaining.size() <= shard) remaining.resize(shard + 1U, max_corners_);
    remaining[shard] -= corners;
    return shard;
  }

  const std::size_t shard = shard_counts_[static_cast<std::size_t>(texture)]++;
  remaining.resize(shard + 1U, max_corners_);
  remaining[shard] -= corners;
  return shard;
}

}  // namespace pistoris::level_native::fts_bake
