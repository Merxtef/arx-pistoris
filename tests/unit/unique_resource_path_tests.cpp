// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#include "doctest/doctest.h"

#include "utils/portable_filename.h"
#include "utils/resource_path.h"

#include <string>
#include <string_view>
#include <unordered_set>
#include <vector>

TEST_SUITE("Resource path uniqueness") {
  TEST_CASE("Portable resource components use the structural path policy without canonical case") {
    using namespace pistoris;

    CHECK(isPortableResourcePathComponent("My texture_(stone)&[wet]__01.png"));
    CHECK_FALSE(isPortableResourcePathComponent("my/texture.png"));
    CHECK_FALSE(isPortableResourcePathComponent(R"(my\texture.png)"));
    CHECK_FALSE(isPortableResourcePathComponent("my#texture.png"));
    CHECK_FALSE(isPortableResourcePathComponent("NUL.png"));
    CHECK_FALSE(isPortableResourcePathComponent("texture.png."));
    CHECK_FALSE(isPortableResourcePathComponent(".."));
  }

  TEST_CASE("Normalization, validation, and identity use the same path rules") {
    using namespace pistoris;

    const ResourcePathNormalization normalized = normalizeResourcePath(R"(SFX\Steps\Hit?.MP3)");
    REQUIRE(normalized.error == ResourcePathError::kNone);
    CHECK(normalized.value == "sfx/steps/hit-.mp3");
    CHECK(hasResourcePathRepair(normalized.repair, ResourcePathRepair::kCase));
    CHECK(hasResourcePathRepair(normalized.repair, ResourcePathRepair::kSeparators));
    CHECK(hasResourcePathRepair(normalized.repair, ResourcePathRepair::kCharacters));
    CHECK(isResourcePath(normalized.value));
    CHECK_FALSE(isResourcePath(R"(SFX\Steps\Hit?.MP3)"));

    const ResourcePathNormalization fragment = normalizeResourcePath("sfx/steps/hit#hard.wav");
    REQUIRE(fragment.error == ResourcePathError::kNone);
    CHECK(fragment.value == "sfx/steps/hit-hard.wav");
    CHECK(hasResourcePathRepair(fragment.repair, ResourcePathRepair::kCharacters));
    CHECK_FALSE(isResourcePath("sfx/steps/hit#hard.wav"));
    CHECK(isResourcePath(fragment.value));

    std::unordered_set<std::string, ResourcePathIdentityHash, ResourcePathIdentityEqual> paths;
    paths.emplace("sfx/steps/hit-.mp3");
    CHECK(paths.contains(std::string_view(R"(SFX\Steps\Hit-.MP3)")));
    CHECK(resourcePathIdentityKey(R"(SFX\Steps\Hit-.MP3)") == "sfx/steps/hit-.mp3");
  }

  TEST_CASE("Resource directories accept a root and trailing separators") {
    using namespace pistoris;

    ResourcePathNormalization directory = normalizeResourceDirectory({});
    REQUIRE(directory.error == ResourcePathError::kNone);
    CHECK(directory.value.empty());
    CHECK(directory.repair == ResourcePathRepair::kNone);

    directory = normalizeResourceDirectory(R"(SFX\Ambiance\)");
    REQUIRE(directory.error == ResourcePathError::kNone);
    CHECK(directory.value == "sfx/ambiance");
    CHECK(hasResourcePathRepair(directory.repair, ResourcePathRepair::kCase));
    CHECK(hasResourcePathRepair(directory.repair, ResourcePathRepair::kSeparators));
    CHECK_FALSE(hasStructuralResourcePathRepair(directory.repair));

    directory = normalizeResourceDirectory("custom?/audio/");
    REQUIRE(directory.error == ResourcePathError::kNone);
    CHECK(directory.value == "custom-/audio");
    CHECK(hasStructuralResourcePathRepair(directory.repair));
  }

  TEST_CASE("Resource directories reject non-relative and ambiguous structure") {
    using namespace pistoris;

    for (std::string_view directory :
         {"/absolute", R"(\absolute)", "C:/absolute", "../outside", "a/./b", "a//b", R"(a\\b)"})
      CHECK(normalizeResourceDirectory(directory).error == ResourcePathError::kBadPath);
  }

  TEST_CASE("Paths are made portable before collisions are resolved") {
    using namespace pistoris;

    std::vector<std::string> paths = {R"(SFX\Steps\Hit?.MP3)", "sfx/steps/hit*.mp3", "sfx/steps/hit-_1.mp3"};
    std::vector<ResourcePathRepair> repairs(paths.size());
    ResourcePathUniquifier uniquifier;
    uniquifier.reserve(paths.size());
    for (std::string& path : paths) uniquifier.add(path);

    ResourcePathRepairSummary summary;
    REQUIRE(uniquifier.apply(&summary, repairs) == ResourcePathError::kNone);
    CHECK(paths == std::vector<std::string>{"sfx/steps/hit-.mp3", "sfx/steps/hit-_2.mp3", "sfx/steps/hit-_1.mp3"});
    CHECK(summary.changed == 2);
    CHECK(summary.normalized == 2);
    CHECK(summary.deduplicated == 1);
    CHECK(hasResourcePathRepair(repairs[0], ResourcePathRepair::kCase));
    CHECK(hasResourcePathRepair(repairs[0], ResourcePathRepair::kSeparators));
    CHECK(hasResourcePathRepair(repairs[0], ResourcePathRepair::kCharacters));
    CHECK(hasResourcePathRepair(repairs[1], ResourcePathRepair::kDuplicate));
  }

  TEST_CASE("Suffixes precede the final extension and skip occupied paths") {
    using namespace pistoris;

    std::string path = "sfx/step.wav";
    ResourcePathUniquifier uniquifier;
    uniquifier.reserve(1, 2);
    REQUIRE(uniquifier.occupy("sfx/step.wav") == ResourcePathError::kNone);
    REQUIRE(uniquifier.occupy("sfx/step_1.wav") == ResourcePathError::kNone);
    uniquifier.add(path);

    REQUIRE(uniquifier.apply() == ResourcePathError::kNone);
    CHECK(path == "sfx/step_2.wav");
    REQUIRE(uniquifier.apply() == ResourcePathError::kNone);
    CHECK(path == "sfx/step_2.wav");
  }

  TEST_CASE("Invalid roots fail without changing any supplied path") {
    using namespace pistoris;

    std::vector<std::string> paths = {"valid/step.wav", "../outside.wav"};
    ResourcePathUniquifier uniquifier;
    for (std::string& path : paths) uniquifier.add(path);

    CHECK(uniquifier.apply() == ResourcePathError::kBadPath);
    CHECK(paths == std::vector<std::string>{"valid/step.wav", "../outside.wav"});
  }

  TEST_CASE("Collision suffixes keep components within the portable limit") {
    using namespace pistoris;

    const std::string path = "a." + std::string(253, 'x');
    std::vector<std::string> paths = {path, path};
    ResourcePathUniquifier uniquifier;
    for (std::string& value : paths) uniquifier.add(value);

    REQUIRE(uniquifier.apply() == ResourcePathError::kNone);
    CHECK(paths[0].size() == kPortableNameMax);
    CHECK(paths[1].size() == kPortableNameMax);
    CHECK(paths[0] != paths[1]);
  }

  TEST_CASE("Length repair preserves canonical component boundaries") {
    using namespace pistoris;

    std::string trailing(kPortableNameMax + 1U, 'a');
    trailing[kPortableNameMax - 1U] = ' ';
    const ResourcePathNormalization normalized_trailing = normalizeResourcePath(trailing);
    REQUIRE(normalized_trailing.error == ResourcePathError::kNone);
    CHECK(normalized_trailing.value.size() == kPortableNameMax);
    CHECK(normalized_trailing.value.back() == '-');
    CHECK(hasResourcePathRepair(normalized_trailing.repair, ResourcePathRepair::kLength));
    CHECK(hasResourcePathRepair(normalized_trailing.repair, ResourcePathRepair::kTrailing));
    CHECK(isResourcePath(normalized_trailing.value));

    const std::string reserved = "con." + std::string(kPortableNameMax - 4U, 'x');
    const ResourcePathNormalization normalized_reserved = normalizeResourcePath(reserved);
    REQUIRE(normalized_reserved.error == ResourcePathError::kNone);
    CHECK(normalized_reserved.value.size() == kPortableNameMax);
    CHECK(normalized_reserved.value.starts_with("con-."));
    CHECK(hasResourcePathRepair(normalized_reserved.repair, ResourcePathRepair::kReserved));
    CHECK(isResourcePath(normalized_reserved.value));
  }
}
