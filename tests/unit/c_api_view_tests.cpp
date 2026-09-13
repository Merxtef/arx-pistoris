// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#include "doctest/doctest.h"

#include "arx_pistoris/base/audio.h"
#include "arx_pistoris/base/image.h"

#include "api/c/internal.h"

#include <cstdint>
#include <span>

TEST_CASE("C API byte views canonicalize empty spans") {
  const std::span<const std::uint8_t> empty;
  const ArxEncodedImageView image = pistoris::c_api::view(empty);
  const ArxEncodedAudioView audio = pistoris::c_api::audioView(empty);

  CHECK(image.data == nullptr);
  CHECK(image.size == 0);
  CHECK(audio.data == nullptr);
  CHECK(audio.size == 0);
}
