// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#include "doctest/doctest.h"

#include "arx_pistoris/base/status.h"

#include "utils/pkware.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

namespace {

std::vector<std::uint8_t> makePattern(std::size_t size) {
  std::vector<std::uint8_t> result(size);
  std::uint32_t state = 0x12345678U;
  for (std::uint8_t& byte : result) {
    state = state * 1664525U + 1013904223U;
    byte = static_cast<std::uint8_t>(state >> 24U);
  }
  return result;
}

void checkRoundtrip(const std::vector<std::uint8_t>& input) {
  std::vector<std::uint8_t> compressed;
  REQUIRE(pistoris::pkware::compress(input, compressed) == ARX_OK);
  REQUIRE(compressed.size() >= 2);
  CHECK(compressed[0] == 0);
  CHECK(compressed[1] == 6);

  std::vector<std::uint8_t> decoded;
  REQUIRE(pistoris::pkware::decompress(compressed, decoded, pistoris::pkware::kMaxDecodedBytes, input.size()) ==
          ARX_OK);
  CHECK(decoded == input);
}

}  // namespace

TEST_SUITE("pkware") {
  TEST_CASE("PkwareDclSignatureDetection") {
    CHECK_FALSE(pistoris::pkware::looksLikeDcl({}));
    CHECK_FALSE(pistoris::pkware::looksLikeDcl(std::array<std::uint8_t, 1>{0}));
    CHECK(pistoris::pkware::looksLikeDcl(std::array<std::uint8_t, 2>{0, 4}));
    CHECK(pistoris::pkware::looksLikeDcl(std::array<std::uint8_t, 2>{1, 6}));
    CHECK_FALSE(pistoris::pkware::looksLikeDcl(std::array<std::uint8_t, 2>{2, 6}));
    CHECK_FALSE(pistoris::pkware::looksLikeDcl(std::array<std::uint8_t, 2>{0, 7}));
  }

  TEST_CASE("PkwareCompressionRoundtrips") {
    checkRoundtrip({});
    checkRoundtrip({0x7f});
    checkRoundtrip({0x00, 0xff});
    checkRoundtrip(makePattern(8193));
    checkRoundtrip(std::vector<std::uint8_t>(16384, 0x41));
  }

  TEST_CASE("PkwareDecompressionRejectsMalformedAndTruncatedStreamsTransactionally") {
    const std::vector<std::uint8_t> unchanged = {1, 2, 3};
    std::vector<std::uint8_t> output = unchanged;
    CHECK(pistoris::pkware::decompress(std::array<std::uint8_t, 3>{2, 6, 0}, output) == ARX_DECOMPRESSION_FAILED);
    CHECK(output == unchanged);

    const std::vector<std::uint8_t> input = makePattern(1024);
    std::vector<std::uint8_t> compressed;
    REQUIRE(pistoris::pkware::compress(input, compressed) == ARX_OK);
    REQUIRE(compressed.size() > 2);
    compressed.pop_back();

    CHECK(pistoris::pkware::decompress(compressed, output) == ARX_DECOMPRESSION_FAILED);
    CHECK(output == unchanged);
  }

  TEST_CASE("PkwareDecompressionEnforcesOutputBoundsAndExpectedSize") {
    const std::vector<std::uint8_t> input(4096, 0x5a);
    std::vector<std::uint8_t> compressed;
    REQUIRE(pistoris::pkware::compress(input, compressed) == ARX_OK);

    const std::vector<std::uint8_t> unchanged = {4, 5, 6};
    std::vector<std::uint8_t> output = unchanged;
    CHECK(pistoris::pkware::decompress(compressed, output, input.size() - 1) == ARX_DECOMPRESSION_LIMIT_EXCEEDED);
    CHECK(output == unchanged);

    CHECK(pistoris::pkware::decompress(compressed, output, input.size(), input.size() - 1) == ARX_DECOMPRESSION_FAILED);
    CHECK(output == unchanged);

    CHECK(pistoris::pkware::decompress(compressed, output, input.size() - 1, input.size()) ==
          ARX_DECOMPRESSION_LIMIT_EXCEEDED);
    CHECK(output == unchanged);
  }
}
