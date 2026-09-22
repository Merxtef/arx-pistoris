// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#pragma once

#include "arx_pistoris/base/status.h"

#include "cgltf/cgltf.h"

#include <cstdint>
#include <span>
#include <string_view>

namespace pistoris::glb {

class Asset {
 public:
  Asset() = default;
  ~Asset();

  Asset(const Asset&) = delete;
  Asset& operator=(const Asset&) = delete;
  Asset(Asset&& other) noexcept;
  Asset& operator=(Asset&& other) noexcept;

  [[nodiscard]] const cgltf_data* data() const { return data_; }
  [[nodiscard]] cgltf_data* data() { return data_; }

 private:
  friend ArxReturnCode parse(std::span<const std::uint8_t>, Asset&);
  cgltf_data* data_ = nullptr;
};

ArxReturnCode parse(std::span<const std::uint8_t> glb, Asset& out);
ArxReturnCode validateRequiredExtensions(const cgltf_data& data, std::span<const std::string_view> supported) noexcept;

}  // namespace pistoris::glb
