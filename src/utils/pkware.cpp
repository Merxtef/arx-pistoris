// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#include "pkware.h"

#include "arx_pistoris/base/status.h"

#include <algorithm>
#include <cstdint>
#include <cstring>
#include <limits>
#include <new>
#include <optional>
#include <span>
#include <utility>
#include <vector>

extern "C" {
#include "blast/blast.h"
}
#undef local

#include "pklib/pklib.h"

namespace pistoris::pkware {
namespace {

struct CompressContext {
  std::span<const std::uint8_t> input;
  std::size_t input_offset = 0;
  std::vector<std::uint8_t> output;
  bool failed = false;
  bool bad_alloc = false;
};

unsigned int readCompressedSource(char* buffer, unsigned int* size, void* parameter) noexcept {
  auto& context = *static_cast<CompressContext*>(parameter);
  const std::size_t requested = static_cast<std::size_t>(*size);
  const std::size_t remaining = context.input.size() - context.input_offset;
  const std::size_t count = std::min(requested, remaining);
  if (count != 0) {
    std::memcpy(buffer, context.input.data() + context.input_offset, count);
    context.input_offset += count;
  }
  return static_cast<unsigned int>(count);
}

void writeCompressedData(char* buffer, unsigned int* size, void* parameter) noexcept {
  auto& context = *static_cast<CompressContext*>(parameter);
  if (context.failed) return;
  try {
    const auto* first = reinterpret_cast<const std::uint8_t*>(buffer);
    context.output.insert(context.output.end(), first, first + *size);
  } catch (const std::bad_alloc&) {
    context.failed = true;
    context.bad_alloc = true;
  } catch (...) {
    context.failed = true;
  }
}

struct DecompressContext {
  std::span<const std::uint8_t> input;
  std::size_t input_offset = 0;
  std::vector<std::uint8_t> output;
  std::size_t max_output = 0;
  std::optional<std::size_t> expected_size;
  bool output_limit_exceeded = false;
  bool expected_size_exceeded = false;
  bool failed = false;
  bool bad_alloc = false;
};

unsigned int readDclSource(void* parameter, unsigned char** buffer) noexcept {
  auto& context = *static_cast<DecompressContext*>(parameter);
  const std::size_t remaining = context.input.size() - context.input_offset;
  const std::size_t count = std::min(remaining, static_cast<std::size_t>(std::numeric_limits<unsigned int>::max()));
  *buffer = const_cast<unsigned char*>(context.input.data() + context.input_offset);
  context.input_offset += count;
  return static_cast<unsigned int>(count);
}

int writeDecompressedData(void* parameter, unsigned char* buffer, unsigned int length) noexcept {
  auto& context = *static_cast<DecompressContext*>(parameter);
  const std::size_t size = static_cast<std::size_t>(length);
  if (size > context.max_output - context.output.size()) {
    context.output_limit_exceeded = true;
    return 1;
  }
  const auto& expected_size = context.expected_size;
  if (expected_size && size > *expected_size - context.output.size()) {
    context.expected_size_exceeded = true;
    return 1;
  }
  try {
    context.output.insert(context.output.end(), buffer, buffer + size);
  } catch (const std::bad_alloc&) {
    context.failed = true;
    context.bad_alloc = true;
    return 1;
  } catch (...) {
    context.failed = true;
    return 1;
  }
  return 0;
}

}  // namespace

bool looksLikeDcl(std::span<const std::uint8_t> data) noexcept {
  return data.size() >= 2 && data[0] <= 1 && data[1] >= 4 && data[1] <= 6;
}

ArxReturnCode compress(std::span<const std::uint8_t> input, std::vector<std::uint8_t>& out) {
  CompressContext context;
  context.input = input;
  try {
    context.output.reserve(input.size());
  } catch (const std::bad_alloc&) {
    return ARX_BAD_ALLOC;
  }

  TCmpStruct work{};
  unsigned int type = CMP_BINARY;
  unsigned int dictionary_size = CMP_IMPLODE_DICT_SIZE3;
  const unsigned int result = implode(
      readCompressedSource, writeCompressedData, reinterpret_cast<char*>(&work), &context, &type, &dictionary_size);
  if (context.bad_alloc) return ARX_BAD_ALLOC;
  if (context.failed || result != CMP_NO_ERROR) return ARX_COMPRESSION_FAILED;
  out = std::move(context.output);
  return ARX_OK;
}

ArxReturnCode decompress(std::span<const std::uint8_t> input, std::vector<std::uint8_t>& out, std::size_t max_output,
                         std::optional<std::size_t> expected_size) {
  if (!looksLikeDcl(input)) return ARX_DECOMPRESSION_FAILED;
  if (expected_size && *expected_size > max_output) return ARX_DECOMPRESSION_LIMIT_EXCEEDED;

  DecompressContext context;
  context.input = input;
  context.max_output = max_output;
  context.expected_size = expected_size;
  if (expected_size) {
    try {
      context.output.reserve(*expected_size);
    } catch (const std::bad_alloc&) {
      return ARX_BAD_ALLOC;
    }
  }

  const int result = blast(readDclSource, &context, writeDecompressedData, &context, nullptr, nullptr);
  if (context.bad_alloc) return ARX_BAD_ALLOC;
  if (context.output_limit_exceeded) return ARX_DECOMPRESSION_LIMIT_EXCEEDED;
  if (context.failed || context.expected_size_exceeded || result != 0 ||
      (expected_size && context.output.size() != *expected_size)) {
    return ARX_DECOMPRESSION_FAILED;
  }
  out = std::move(context.output);
  return ARX_OK;
}

}  // namespace pistoris::pkware
