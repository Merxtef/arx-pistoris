// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#pragma once

#include "doctest/doctest.h"

#include "arx_pistoris/base/status.h"
#if defined(ARX_PISTORIS_CPP_API)
#include "arx_pistoris/runtime.hpp"
#else
#include "arx_pistoris/runtime.h"
#endif

#include <cstdint>
#include <filesystem>
#include <format>
#include <fstream>
#include <ios>
#include <iterator>
#include <string>
#include <string_view>
#include <vector>

namespace test_support {

inline bool checkCorpusCondition(const std::filesystem::path& path, std::string_view stage, bool condition,
                                 std::string_view detail) {
  if (condition) return true;
  CHECK_MESSAGE(false, std::format("{}: {} failed: {}", path.generic_string(), stage, detail));
  return false;
}

inline bool checkCorpusStatus(const std::filesystem::path& path, std::string_view stage, ArxReturnCode status) {
  if (status == ARX_OK) return true;
#if defined(ARX_PISTORIS_CPP_API)
  const char* message = pistoris::errorString(status);
#else
  const char* message = arx_pistoris_strerror(status);
#endif
  CHECK_MESSAGE(false, std::format("{}: {} failed: {} ({})", path.generic_string(), stage, message, status));
  return false;
}

inline bool readCorpusBytes(const std::filesystem::path& path, std::vector<std::uint8_t>& out) {
  std::ifstream file(path, std::ios::binary);
  if (!checkCorpusCondition(path, "read file", file.is_open(), "could not open file")) return false;
  out.assign(std::istreambuf_iterator<char>(file), {});
  return checkCorpusCondition(path, "read file", !file.bad(), "I/O error");
}

}  // namespace test_support
