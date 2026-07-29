// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#include "doctest/doctest.h"

#include "formats/classification.h"
#include "formats/format.h"

#include <cstdint>
#include <cstring>
#include <string_view>
#include <vector>

namespace {

std::vector<std::uint8_t> dlfHeader(std::int32_t num_scenes = 1) {
  constexpr std::size_t kHeaderSize = 8520;
  constexpr std::size_t kNumScenesOffset = 304;
  std::vector<std::uint8_t> bytes(kHeaderSize);
  const float version = 0.0f;
  std::memcpy(bytes.data(), &version, sizeof(version));
  std::memcpy(bytes.data() + sizeof(version), "DANAE_FILE", sizeof("DANAE_FILE"));
  std::memcpy(bytes.data() + kNumScenesOffset, &num_scenes, sizeof(num_scenes));
  return bytes;
}

std::vector<std::uint8_t> jsonBytes(std::string_view text) { return {text.begin(), text.end()}; }

}  // namespace

TEST_SUITE("CLI classification") {
  TEST_CASE("DLF identity classification does not validate the payload") {
    for (std::int32_t num_scenes : {0, 1, 2}) {
      const cli::FileFacts facts = cli::classifyInput(dlfHeader(num_scenes), "level.dlf");
      CHECK(facts.format == cli::Format::kDlf);
      CHECK(facts.kind == cli::PayloadKind::kDlf);
    }
  }

  TEST_CASE("Stable native identity overrides a misleading extension") {
    const cli::FileFacts facts = cli::classifyInput(dlfHeader(1), "scene.fts");
    CHECK(facts.format == cli::Format::kDlf);
    CHECK(facts.kind == cli::PayloadKind::kDlf);
  }

  TEST_CASE("Extension selects native formats without stable stored identity") {
    const std::vector<std::uint8_t> bytes = {0xff};
    CHECK(cli::classifyInput(bytes, "model.ftl").kind == cli::PayloadKind::kFtl);
    CHECK(cli::classifyInput(bytes, "level.fts").kind == cli::PayloadKind::kFts);
    CHECK(cli::classifyInput(bytes, "lights.llf").kind == cli::PayloadKind::kLlf);
  }

  TEST_CASE("TEA detection requires its complete stored identity") {
    constexpr std::string_view kIdentity = "Theo Animation File";
    std::vector<std::uint8_t> bytes(kIdentity.begin(), kIdentity.end());
    CHECK(cli::classifyInput(bytes, "animation.bin").kind == cli::PayloadKind::kUnknown);
    bytes.push_back(0);
    CHECK(cli::classifyInput(bytes, "animation.bin").kind == cli::PayloadKind::kTea);
  }

  TEST_CASE("Arx-convert schemas classify native JSON carriers") {
    const cli::FileFacts ftl = cli::classifyInput(
        jsonBytes(R"({"$schema":"https://arx-tools.github.io/schemas/ftl.schema.json"})"), "model.json");
    const cli::FileFacts tea = cli::classifyInput(
        jsonBytes(R"({"$schema":"https://arx-tools.github.io/schemas/tea.schema.json"})"), "animation.json");
    const cli::FileFacts fts = cli::classifyInput(
        jsonBytes(R"({"$schema":"https://arx-tools.github.io/schemas/fts.schema.json"})"), "level1.fts.json");
    const cli::FileFacts dlf = cli::classifyInput(
        jsonBytes(R"({"$schema":"https://arx-tools.github.io/schemas/dlf.schema.json"})"), "level1.dlf.json");
    const cli::FileFacts llf = cli::classifyInput(
        jsonBytes(R"({"$schema":"https://arx-tools.github.io/schemas/llf.schema.json"})"), "level1.llf.json");
    CHECK(ftl.kind == cli::PayloadKind::kFtl);
    CHECK(tea.kind == cli::PayloadKind::kTea);
    CHECK(fts.kind == cli::PayloadKind::kFts);
    CHECK(dlf.kind == cli::PayloadKind::kDlf);
    CHECK(llf.kind == cli::PayloadKind::kLlf);
  }

  TEST_CASE("Structural FTS JSON detection takes precedence over FTL vertex fields") {
    const cli::FileFacts facts = cli::classifyInput(
        jsonBytes(R"({"polygons":[],"roomDistances":[],"textureContainers":[],"vertices":[]})"), "level1.fts.json");
    CHECK(facts.kind == cli::PayloadKind::kFts);
  }
}
