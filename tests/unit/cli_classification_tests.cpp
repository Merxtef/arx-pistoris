// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#include "doctest/doctest.h"

#include "arx_pistoris/native/amb.hpp"
#include "arx_pistoris/native/cin.hpp"

#include "formats/classification.h"
#include "formats/format.h"
#include "io/path_location.h"
#include "resources/layout.h"

#include <cstdint>
#include <cstring>
#include <ostream>  // IWYU pragma: keep
#include <string_view>
#include <utility>
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
  TEST_CASE("Primary resource layouts distinguish game carriers from loose formats") {
    for (const cli::Format format :
         {cli::Format::kFtl, cli::Format::kTea, cli::Format::kDlf, cli::Format::kAmb, cli::Format::kCin}) {
      CHECK(cli::primaryResourceLayout(format, cli::PathAddress::kMountRelative) == cli::ResourceLayout::kGame);
      CHECK(cli::primaryResourceLayout(format, cli::PathAddress::kAbsolute) == cli::ResourceLayout::kLoose);
    }
    for (const cli::Format format :
         {cli::Format::kFts, cli::Format::kLlf, cli::Format::kJson, cli::Format::kObj, cli::Format::kGlb}) {
      CHECK(cli::primaryResourceLayout(format, cli::PathAddress::kMountRelative) == cli::ResourceLayout::kLoose);
      CHECK(cli::primaryResourceLayout(format, cli::PathAddress::kAbsolute) == cli::ResourceLayout::kLoose);
    }
  }

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

  TEST_CASE("AMB magic provides stable native identity") {
    std::vector<std::uint8_t> bytes(sizeof(pistoris::kAmbMagic));
    std::memcpy(bytes.data(), &pistoris::kAmbMagic, sizeof(pistoris::kAmbMagic));
    const cli::FileFacts facts = cli::classifyInput(bytes, "ambiance.bin");
    CHECK(facts.format == cli::Format::kAmb);
    CHECK(facts.kind == cli::PayloadKind::kAmb);
  }

  TEST_CASE("CIN magic provides stable native identity") {
    std::vector<std::uint8_t> bytes(pistoris::kCinMagic.begin(), pistoris::kCinMagic.end());
    const cli::FileFacts facts = cli::classifyInput(bytes, "cinematic.bin");
    CHECK(facts.format == cli::Format::kCin);
    CHECK(facts.kind == cli::PayloadKind::kCin);
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
    const cli::FileFacts amb = cli::classifyInput(
        jsonBytes(R"({"$schema":"https://arx-tools.github.io/schemas/amb.schema.json"})"), "ambiance.json");
    CHECK(ftl.kind == cli::PayloadKind::kFtl);
    CHECK(tea.kind == cli::PayloadKind::kTea);
    CHECK(fts.kind == cli::PayloadKind::kFts);
    CHECK(dlf.kind == cli::PayloadKind::kDlf);
    CHECK(llf.kind == cli::PayloadKind::kLlf);
    CHECK(amb.kind == cli::PayloadKind::kAmb);
  }

  TEST_CASE("Compound JSON suffixes identify payloads without rewriting path case") {
    constexpr std::pair<std::string_view, cli::PayloadKind> kCases[] = {
        {"model.FTL.JSON", cli::PayloadKind::kFtl},
        {"animation.tea.json", cli::PayloadKind::kTea},
        {"level.fts.json", cli::PayloadKind::kFts},
        {"scene.dlf.json", cli::PayloadKind::kDlf},
        {"lights.llf.json", cli::PayloadKind::kLlf},
        {"ambiance.amb.json", cli::PayloadKind::kAmb},
    };
    for (const auto& [path, expected] : kCases) {
      CAPTURE(path);
      CHECK(cli::classifyInput(jsonBytes("{"), path).kind == expected);
    }
  }

  TEST_CASE("Compound JSON suffix takes precedence over conflicting document hints") {
    const cli::FileFacts facts = cli::classifyInput(
        jsonBytes(R"({"$schema":"https://arx-tools.github.io/schemas/tea.schema.json","keyframes":[]})"),
        "model.ftl.json");
    CHECK(facts.format == cli::Format::kJson);
    CHECK(facts.kind == cli::PayloadKind::kFtl);
  }

  TEST_CASE("Generic JSON classification observes only root tokens") {
    CHECK(cli::classifyInput(jsonBytes(R"({"note":"vertices textureContainers tracks","nested":{"tracks":[]}})"),
                             "unknown.json")
              .kind == cli::PayloadKind::kUnknown);
    CHECK(
        cli::classifyInput(jsonBytes(R"({"nested":{"$schema":"https://arx-tools.github.io/schemas/amb.schema.json"}})"),
                           "unknown.json")
            .kind == cli::PayloadKind::kUnknown);
    CHECK(cli::classifyInput(jsonBytes(R"({"tracks":[]})"), "ambiance.json").kind == cli::PayloadKind::kAmb);
    CHECK(cli::classifyInput(jsonBytes(R"({"header":{"totalNumberOfFrames":0},"keyframes":[]})"), "animation.json")
              .kind == cli::PayloadKind::kTea);
    CHECK(cli::classifyInput(jsonBytes(R"({"interactiveObjects":[],"fogs":[],"zones":[]})"), "scene.json").kind ==
          cli::PayloadKind::kDlf);
    CHECK(cli::classifyInput(jsonBytes(R"({"header":{"numberOfPolygonsInFTS":0},"lights":[],"colors":[]})"),
                             "lights.json")
              .kind == cli::PayloadKind::kLlf);
    CHECK(cli::classifyInput(jsonBytes(R"({"vertices":[],"textureContainers":[]})"), "model.json").kind ==
          cli::PayloadKind::kFtl);
  }

  TEST_CASE("Malformed generic JSON remains unclassified") {
    CHECK(cli::classifyInput(jsonBytes(R"({"tracks":[])"), "ambiance.json").kind == cli::PayloadKind::kUnknown);
  }

  TEST_CASE("Structural AMB JSON detection supports documents without a schema") {
    const cli::FileFacts facts = cli::classifyInput(
        jsonBytes(R"({"tracks":[{"keys":[{"delayMin":0,"delayMax":0,"volume":{},"pitch":{},"pan":{}}]}]})"),
        "ambiance.json");
    CHECK(facts.kind == cli::PayloadKind::kAmb);
  }

  TEST_CASE("Structural FTS JSON detection takes precedence over FTL vertex fields") {
    const cli::FileFacts facts = cli::classifyInput(
        jsonBytes(R"({"polygons":[],"roomDistances":[],"textureContainers":[],"vertices":[]})"), "level1.fts.json");
    CHECK(facts.kind == cli::PayloadKind::kFts);
  }
}
