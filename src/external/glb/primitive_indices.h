// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#pragma once

#include "accessor.h"

#include <cstddef>
#include <cstdint>
#include <span>

namespace pistoris::glb {

class PrimitiveIndices {
 public:
  std::size_t size() const noexcept { return count_; }
  bool empty() const noexcept { return count_ == 0; }
  std::uint32_t operator[](std::size_t index) const noexcept;

 private:
  friend ArxReturnCode readPrimitiveIndices(AccessorCache&, const cgltf_primitive&, std::size_t, bool, bool,
                                            PrimitiveIndices&);

  std::span<const std::uint32_t> explicit_;
  std::size_t count_ = 0;
  bool mirrored_ = false;
};

ArxReturnCode readPrimitiveIndices(AccessorCache& accessors, const cgltf_primitive& primitive,
                                   std::size_t position_count, bool require_indices, bool mirrored,
                                   PrimitiveIndices& out);

}  // namespace pistoris::glb
