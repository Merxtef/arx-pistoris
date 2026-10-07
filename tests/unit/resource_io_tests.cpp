// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#include "doctest/doctest.h"

#include "arx_pistoris/base/status.h"
#include "arx_pistoris/cinematic/types.h"
#include "arx_pistoris/glb/location.hpp"
#include "arx_pistoris/native.hpp"
#include "arx_pistoris/paths.hpp"
#include "arx_pistoris/paths/types.h"
#include "arx_pistoris/resource_io.hpp"
#include "arx_pistoris/runtime.hpp"
#include "arx_pistoris/runtime/types.h"
#include "arx_pistoris/sound.h"
#include "arx_pistoris/sound.hpp"
#include "arx_pistoris/texture.h"

#include "support/fixture_catalog.h"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <ios>
#include <iosfwd>
#include <iterator>
#include <limits>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>
#include <variant>
#include <vector>

namespace {

class TemporaryDirectory {
 public:
  TemporaryDirectory() {
    static std::atomic<unsigned> sequence = 0;
    const auto timestamp = std::chrono::steady_clock::now().time_since_epoch().count();
    path_ = std::filesystem::temp_directory_path() /
            ("arx-pistoris-resource-io-" + std::to_string(timestamp) + "-" + std::to_string(sequence++));
    std::error_code error;
    REQUIRE(std::filesystem::create_directories(path_, error));
    REQUIRE_FALSE(error);
  }

  TemporaryDirectory(const TemporaryDirectory&) = delete;
  TemporaryDirectory& operator=(const TemporaryDirectory&) = delete;

  ~TemporaryDirectory() noexcept {
    try {
      std::error_code error;
      std::filesystem::remove_all(path_, error);
    } catch (...) {  // NOLINT(bugprone-empty-catch): destructors cannot report test cleanup failures
      // Temporary-directory cleanup cannot be reported from a destructor.
    }
  }

  [[nodiscard]] const std::filesystem::path& path() const noexcept { return path_; }

 private:
  std::filesystem::path path_;
};

void writeFile(const std::filesystem::path& path, std::span<const std::uint8_t> bytes) {
  std::error_code error;
  std::filesystem::create_directories(path.parent_path(), error);
  REQUIRE_FALSE(error);
  std::ofstream output(path, std::ios::binary);
  REQUIRE(output.good());
  if (!bytes.empty())
    output.write(reinterpret_cast<const char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
  REQUIRE(output.good());
}

void writeFile(const std::filesystem::path& path, std::string_view text) {
  writeFile(path, {reinterpret_cast<const std::uint8_t*>(text.data()), text.size()});
}

std::vector<std::uint8_t> readFile(const std::filesystem::path& path) {
  std::ifstream input(path, std::ios::binary | std::ios::ate);
  REQUIRE(input.good());
  const std::streampos end = input.tellg();
  REQUIRE(end >= 0);
  const auto size = static_cast<std::uintmax_t>(static_cast<std::streamoff>(end));
  REQUIRE(size <= std::numeric_limits<std::size_t>::max());
  std::vector<std::uint8_t> result(static_cast<std::size_t>(size));
  input.seekg(0);
  if (!result.empty()) input.read(reinterpret_cast<char*>(result.data()), static_cast<std::streamsize>(result.size()));
  REQUIRE(input.good());
  return result;
}

std::filesystem::path canonicalExistingPath(const std::filesystem::path& path) {
  std::error_code error;
  std::filesystem::path result = std::filesystem::canonical(path, error);
  REQUIRE_FALSE(error);
  return result;
}

struct LogCapture {
  std::vector<std::string> debug;
  std::vector<std::string> warnings;
};

void captureLog(ArxLogLevel level, const char* message, void* userdata) {
  if (!message) return;
  auto& capture = *static_cast<LogCapture*>(userdata);
  if (level == ARX_LOG_DEBUG) capture.debug.emplace_back(message);
  if (level == ARX_LOG_WARN) capture.warnings.emplace_back(message);
}

struct LogReset {
  ~LogReset() { pistoris::setLogCallback(nullptr, nullptr); }
};

void checkDisengagedMounts(pistoris::resource_io::ResourceMounts& source) {
  CHECK(source.readMounts().empty());
  CHECK_FALSE(source.writeMount());
  CHECK(source.availableMounts() == 0);
  CHECK_FALSE(source.highestPriorityMountId(pistoris::resource_io::kAllResourceMounts));
  CHECK(source.read("missing").code() == ARX_INVALID_STATE);
  CHECK(source.resolveWritePath("output.bin").code() == ARX_INVALID_STATE);
  CHECK(source.enumerate("", 1).code() == ARX_INVALID_STATE);
  CHECK(source.listDirectory().code() == ARX_INVALID_STATE);

  const pistoris::resource_io::ResourceMounts copied(source);  // NOLINT(performance-unnecessary-copy-initialization)
  CHECK(copied.readMounts().empty());
  CHECK(copied.read("missing").code() == ARX_INVALID_STATE);

  pistoris::resource_io::ResourceMounts assigned;
  assigned = source;
  CHECK(assigned.readMounts().empty());
  CHECK(assigned.read("missing").code() == ARX_INVALID_STATE);
}

const pistoris::resource_io::ResourceDirectoryEntry* findEntry(
    std::span<const pistoris::resource_io::ResourceDirectoryEntry> entries, std::string_view name) {
  const auto found = std::ranges::find(entries, name, &pistoris::resource_io::ResourceDirectoryEntry::name);
  return found == entries.end() ? nullptr : &*found;
}

}  // namespace

TEST_SUITE("Resource IO") {
  TEST_CASE("Resource IO owns negative status codes and delegates core descriptions") {
    static_assert(ARX_RESOURCE_IO_INVALID_PATH == -1000);
    static_assert(ARX_RESOURCE_IO_INVALID_METADATA < 0);
    static_assert(ARX_RESOURCE_IO_INVALID_METADATA >= -1000);

    CHECK(std::string_view(arx_pistoris_resource_io_strerror(ARX_RESOURCE_IO_NOT_FOUND)) ==
          "Resource I/O: resource not found");
    CHECK(std::string_view(arx_pistoris_resource_io_strerror(ARX_RESOURCE_IO_AMBIGUOUS_PATH)) ==
          "Resource I/O: ambiguous path");
    CHECK(std::string_view(arx_pistoris_resource_io_strerror(ARX_RESOURCE_IO_INVALID_METADATA)) ==
          "Resource I/O: invalid resource metadata");
    CHECK(std::string_view(pistoris::resource_io::errorString(ARX_BAD_ALLOC)) ==
          std::string_view(pistoris::errorString(ARX_BAD_ALLOC)));
    CHECK(std::string_view(pistoris::errorString(ARX_RESOURCE_IO_NOT_FOUND)) == "unknown error code");
  }

  TEST_CASE("Moved-from mounts have a stable disengaged state") {
    pistoris::resource_io::ResourceMounts source;
    pistoris::resource_io::ResourceMounts destination(std::move(source));
    CHECK(destination.readMounts().empty());
    checkDisengagedMounts(source);  // NOLINT(bugprone-use-after-move): moved-from contract under test
  }

  TEST_CASE("Resource failures log their source location once at debug level") {
    TemporaryDirectory temporary;
    auto opened =
        pistoris::resource_io::ResourceMounts::open({.read_mounts = {temporary.path()}, .write_mount = std::nullopt});
    REQUIRE(opened);

    LogCapture capture;
    LogReset reset;
    pistoris::setLogCallback(captureLog, &capture);
    auto missing = opened->read("folder/missing.ftl");
    REQUIRE_FALSE(missing);
    REQUIRE(missing.error());
    REQUIRE(missing.error()->location());
    CHECK(missing.error()->location()->resource_path == "folder/missing.ftl");
    CHECK(missing.error()->location()->native_path.empty());
    CHECK(missing.error()->location()->mount_mask == pistoris::resource_io::kAllResourceMounts);
    CHECK(std::ranges::count_if(capture.debug, [](const std::string& message) {
            return message.find("Resource I/O source failure during resource reading") != std::string::npos &&
                   message.find("folder/missing.ftl") != std::string::npos;
          }) == 1);
  }

  TEST_CASE("Resource operations reject unsupported flags") {
    TemporaryDirectory temporary;
    writeFile(temporary.path() / "value.bin", "value");
    auto opened = pistoris::resource_io::ResourceMounts::open(
        {.read_mounts = {temporary.path()}, .write_mount = temporary.path()});
    REQUIRE(opened);

    constexpr pistoris::resource_io::ResourceIoFlags kUnsupportedFlags =
        pistoris::resource_io::kResourceIoFlagsAll | (1U << 31);
    CHECK(opened->read("value.bin", {.flags = kUnsupportedFlags}).code() == ARX_INVALID_OPTIONS);
    CHECK(opened->resolve("value.bin", {.flags = kUnsupportedFlags}).code() == ARX_INVALID_OPTIONS);
    CHECK(opened->resolveWritePath("value.bin", kUnsupportedFlags).code() == ARX_INVALID_OPTIONS);
    CHECK(opened->write("value.bin", {}, kUnsupportedFlags).code() == ARX_INVALID_OPTIONS);
    CHECK(opened->enumerate("", 0, {.flags = kUnsupportedFlags}).code() == ARX_INVALID_OPTIONS);
    CHECK(opened->listDirectory("", {.flags = kUnsupportedFlags}).code() == ARX_INVALID_OPTIONS);
  }

  TEST_CASE("Resource classification reports unknown data without treating it as a failure") {
    const std::vector<std::uint8_t> data = {'n', 'o', 't', ' ', 'a', ' ', 'f', 'o', 'r', 'm', 'a', 't'};
    auto classified = pistoris::resource_io::classifyResource(data, "resource.unknown");
    REQUIRE(classified);
    CHECK(classified->format == pistoris::resource_io::ResourceFormat::kUnknown);
    CHECK(classified->payload == pistoris::resource_io::ResourcePayload::kUnknown);
  }

  TEST_CASE("Case-colliding native entries fail by default and recover deterministically when requested") {
    TemporaryDirectory temporary;
    const auto directory = temporary.path() / "shared";
    writeFile(directory / "VALUE.FTL", "upper");
    writeFile(directory / "value.ftl", "lower");
    writeFile(directory / "other.ftl", "other");

    std::error_code error;
    const auto entry_count = static_cast<std::size_t>(
        std::distance(std::filesystem::directory_iterator(directory, error), std::filesystem::directory_iterator()));
    REQUIRE_FALSE(error);
    if (entry_count < 3) return;

    auto opened =
        pistoris::resource_io::ResourceMounts::open({.read_mounts = {temporary.path()}, .write_mount = std::nullopt});
    REQUIRE(opened);

    CHECK(opened->read("shared/other.ftl"));
    CHECK(opened->listDirectory("shared").code() == ARX_RESOURCE_IO_AMBIGUOUS_PATH);

    auto ambiguous = opened->read("shared/value.ftl");
    REQUIRE_FALSE(ambiguous);
    CHECK(ambiguous.code() == ARX_RESOURCE_IO_AMBIGUOUS_PATH);
    REQUIRE(ambiguous.error());
    REQUIRE(ambiguous.error()->location());
    CHECK(ambiguous.error()->location()->resource_path == "shared/value.ftl");
    CHECK(ambiguous.error()->location()->native_path == directory);

    LogCapture capture;
    LogReset reset;
    pistoris::setLogCallback(captureLog, &capture);
    auto recovered =
        opened->read("shared/value.ftl", {.flags = pistoris::resource_io::kResourceIoRecoverCaseCollisions});
    REQUIRE(recovered);
    CHECK(std::string_view(reinterpret_cast<const char*>(recovered->data.data()), recovered->data.size()) == "upper");
    CHECK(std::ranges::count_if(capture.warnings, [](const std::string& message) {
            return message.find("case-colliding entries") != std::string::npos &&
                   message.find("deterministic lexical order") != std::string::npos;
          }) == 1);
  }

  TEST_CASE("Mount masks preserve priority and duplicates do not consume bits") {
    TemporaryDirectory temporary;
    const auto first = temporary.path() / "first";
    const auto second = temporary.path() / "second";
    writeFile(first / "folder" / "value.bin", "first");
    writeFile(second / "folder" / "value.bin", "second");

    pistoris::resource_io::MountValidationReport report;
    auto opened = pistoris::resource_io::ResourceMounts::open(
        {.read_mounts = {first, first / ".", second}, .write_mount = std::nullopt}, &report);
    REQUIRE(opened);
    REQUIRE(opened->readMounts().size() == 2);
    CHECK(opened->readMounts()[0].id == 1);
    CHECK(opened->readMounts()[1].id == 2);
    CHECK(opened->availableMounts() == 3);
    CHECK(opened->highestPriorityMountId(3) == 1);
    REQUIRE(report.messages.size() == 1);
    CHECK(report.messages[0].kind == pistoris::resource_io::MountValidationKind::kDuplicateReadMount);

    auto preferred = opened->read("FOLDER/value.bin");
    REQUIRE(preferred);
    CHECK(std::string(preferred->data.begin(), preferred->data.end()) == "first");
    CHECK(preferred->mount_id == 1);

    auto lower = opened->read("folder/value.bin", {.mount_mask = 2});
    REQUIRE(lower);
    CHECK(std::string(lower->data.begin(), lower->data.end()) == "second");
    CHECK(lower->mount_id == 2);

    auto disabled = opened->read("folder/value.bin", {.mount_mask = 0});
    CHECK_FALSE(disabled);
    CHECK(disabled.code() == ARX_RESOURCE_IO_NOT_FOUND);
  }

  TEST_CASE("Mount configuration extends without renumbering existing roots") {
    TemporaryDirectory temporary;
    const auto first = temporary.path() / "first";
    const auto second = temporary.path() / "second";
    REQUIRE(std::filesystem::create_directory(first));
    REQUIRE(std::filesystem::create_directory(second));

    pistoris::resource_io::ResourceMounts mounts;
    pistoris::resource_io::MountValidationReport report;
    REQUIRE(mounts.addReadMount(first, &report));
    REQUIRE(report.messages.empty());
    REQUIRE(mounts.readMounts().size() == 1);
    CHECK(mounts.readMounts()[0].id == 1);

    REQUIRE(mounts.addReadMount(first, &report));
    CHECK(report.messages.empty());
    REQUIRE(mounts.readMounts().size() == 1);
    CHECK(mounts.readMounts()[0].id == 1);

    REQUIRE(mounts.addReadMount(second, &report));
    REQUIRE(mounts.readMounts().size() == 2);
    CHECK(mounts.readMounts()[0].id == 1);
    CHECK(mounts.readMounts()[1].id == 2);

    REQUIRE(mounts.addReadMount(temporary.path() / "missing", &report));
    REQUIRE(report.messages.size() == 1);
    CHECK(report.messages[0].kind == pistoris::resource_io::MountValidationKind::kMissingReadMount);
    CHECK(mounts.readMounts().size() == 2);
  }

  TEST_CASE("Default Resources expose their internally owned mounts") {
    TemporaryDirectory temporary;
    pistoris::resource_io::Resources resources;
    CHECK(resources.mounts().readMounts().empty());
    REQUIRE(resources.mounts().addReadMount(temporary.path()));
    REQUIRE(resources.mounts().readMounts().size() == 1);
    CHECK(resources.mounts().readMounts()[0].id == 1);
  }

  TEST_CASE("Libertatis mount composition is idempotent and keeps write selection independent") {
    pistoris::resource_io::ResourceMounts mounts;
    auto root = pistoris::resource_io::libertatisResourceRoot();
    if (!root) {
      CHECK_FALSE(mounts.addLibertatisMounts());
      CHECK_FALSE(mounts.setLibertatisWriteMount());
      return;
    }

    REQUIRE(mounts.addLibertatisMounts());
    const std::vector<pistoris::resource_io::ResourceMount> first(mounts.readMounts().begin(),
                                                                  mounts.readMounts().end());
    REQUIRE(mounts.addLibertatisMounts());
    REQUIRE(mounts.readMounts().size() == first.size());
    for (std::size_t index = 0; index < first.size(); ++index) {
      CHECK(mounts.readMounts()[index].id == first[index].id);
      CHECK(mounts.readMounts()[index].path == first[index].path);
    }

    CHECK_FALSE(mounts.writeMount());
    REQUIRE(mounts.setLibertatisWriteMount());
    CHECK(mounts.writeMount().has_value());
    REQUIRE(mounts.readMounts().size() == first.size());
  }

  TEST_CASE("At most 64 unique read mounts can be opened") {
    TemporaryDirectory temporary;
    pistoris::resource_io::ResourceMountOptions options;
    for (std::size_t index = 0; index < pistoris::resource_io::kMaximumReadMounts + 1; ++index) {
      const auto path = temporary.path() / std::to_string(index);
      REQUIRE(std::filesystem::create_directory(path));
      options.read_mounts.push_back(path);
    }
    auto opened = pistoris::resource_io::ResourceMounts::open(options);
    CHECK_FALSE(opened);
    CHECK(opened.code() == ARX_RESOURCE_IO_TOO_MANY_MOUNTS);

    options.read_mounts.pop_back();
    opened = pistoris::resource_io::ResourceMounts::open(options);
    REQUIRE(opened);
    const auto rejected =
        opened->addReadMount(temporary.path() / std::to_string(pistoris::resource_io::kMaximumReadMounts));
    CHECK_FALSE(rejected);
    CHECK(rejected.code() == ARX_RESOURCE_IO_TOO_MANY_MOUNTS);
    CHECK(opened->readMounts().size() == pistoris::resource_io::kMaximumReadMounts);
  }

#ifdef _WIN32
  TEST_CASE("Windows mounts reject native reserved syntax") {
    TemporaryDirectory root;
    pistoris::resource_io::ResourceMountOptions options;
    options.read_mounts = {root.path() / "invalid*mount"};

    const auto opened = pistoris::resource_io::ResourceMounts::open(options);
    REQUIRE_FALSE(opened);
    CHECK(opened.code() == ARX_RESOURCE_IO_INVALID_MOUNT);
  }
#endif

  TEST_CASE("Directory views are live and aggregate only equivalent providers") {
    TemporaryDirectory temporary;
    const auto first = temporary.path() / "first";
    const auto second = temporary.path() / "second";
    writeFile(first / "shared" / "same.ftl", "first");
    writeFile(second / "shared" / "same.ftl", "second");
    writeFile(first / "shared" / "conflict", "file");
    REQUIRE(std::filesystem::create_directories(second / "shared" / "conflict"));
    writeFile(first / "shared" / "masked" / "inside.ftl", "inside");
    writeFile(second / "shared" / "masked", "lower file");

    auto opened =
        pistoris::resource_io::ResourceMounts::open({.read_mounts = {first, second}, .write_mount = std::nullopt});
    REQUIRE(opened);
    auto before = opened->listDirectory("shared");
    REQUIRE(before);
    const auto* same = findEntry(*before, "same.ftl");
    REQUIRE(same);
    CHECK(same->kind == pistoris::resource_io::ResourceDirectoryEntry::Kind::kFtl);
    CHECK(same->mount_mask == 3);
    const auto* conflict = findEntry(*before, "conflict");
    REQUIRE(conflict);
    CHECK(conflict->kind == pistoris::resource_io::ResourceDirectoryEntry::Kind::kUnknownFile);
    CHECK(conflict->mount_mask == 1);
    const auto* masked = findEntry(*before, "masked");
    REQUIRE(masked);
    CHECK(masked->kind == pistoris::resource_io::ResourceDirectoryEntry::Kind::kDirectory);
    CHECK(masked->mount_mask == 1);

    auto masked_file = opened->read("shared/masked");
    CHECK_FALSE(masked_file);
    CHECK(masked_file.code() == ARX_RESOURCE_IO_NOT_FOUND);
    auto blocked_directory = opened->listDirectory("shared/conflict");
    REQUIRE(blocked_directory);
    CHECK(blocked_directory->empty());

    auto recursive = opened->enumerate("shared", 2);
    REQUIRE(recursive);
    CHECK(std::ranges::any_of(*recursive, [](const auto& file) {
      return file.logical_path == "shared/masked/inside.ftl" && file.mount_mask == 1;
    }));
    CHECK_FALSE(std::ranges::any_of(*recursive, [](const auto& file) { return file.logical_path == "shared/masked"; }));

    writeFile(second / "shared" / "later.ogg", "later");
    auto after = opened->listDirectory("shared");
    REQUIRE(after);
    const auto* later = findEntry(*after, "later.ogg");
    REQUIRE(later);
    CHECK(later->kind == pistoris::resource_io::ResourceDirectoryEntry::Kind::kOgg);
    CHECK(later->mount_mask == 2);
  }

  TEST_CASE("Catalog scans are live immutable snapshots") {
    TemporaryDirectory temporary;
    const auto first = temporary.path() / "first";
    const auto second = temporary.path() / "second";
    const auto human = std::filesystem::path("game/graph/obj3d/interactive/npc/human/human.ftl");
    writeFile(first / human, "first");
    writeFile(second / human, "second");

    auto opened =
        pistoris::resource_io::ResourceMounts::open({.read_mounts = {first, second}, .write_mount = std::nullopt});
    REQUIRE(opened);
    pistoris::resource_io::Resources resources(std::move(*opened));
    auto initial = resources.scanCatalog();
    REQUIRE(initial);
    REQUIRE(initial->entries().size() == 1);
    CHECK(initial->entries()[0].selector == "model:npc:human");
    CHECK(initial->entries()[0].mount_mask == 3);

    writeFile(first / "game/graph/obj3d/interactive/npc/guard/guard.ftl", "guard");
    auto rescanned = resources.scanCatalog();
    REQUIRE(rescanned);
    CHECK(initial->entries().size() == 1);
    REQUIRE(rescanned->entries().size() == 2);
    CHECK(std::ranges::any_of(rescanned->entries(), [](const auto& entry) {
      return entry.selector == "model:npc:guard" && entry.mount_mask == 1;
    }));
  }

  TEST_CASE("Complete resource loaders hydrate native fixture sidecars") {
    auto opened = pistoris::resource_io::ResourceMounts::open(
        {.read_mounts = {test_support::kFixtureMount}, .write_mount = std::nullopt});
    REQUIRE(opened);
    pistoris::resource_io::Resources resources(std::move(*opened));

    pistoris::paths::ResourceSelector selector;
    REQUIRE(pistoris::paths::parseResourceSelector("model:weapons:sword_00", selector));
    auto model = resources.loadModel(selector);
    REQUIRE(model);
    REQUIRE(model->model.textureCount() > 0);
    CHECK(std::ranges::all_of(model->model.textures(),
                              [](const ArxTextureView& texture) { return texture.encoded_image.size > 0; }));
    CHECK(model->model.inventoryIcon().encoded_image.size > 0);

    REQUIRE(pistoris::paths::parseResourceSelector("anim:npc:human_male_gathering", selector));
    auto animation = resources.loadAnimation(selector);
    REQUIRE(animation);
    REQUIRE(animation->soundCount() == 2);
    CHECK(std::ranges::all_of(animation->sounds(),
                              [](const ArxSoundView& sound) { return sound.encoded_audio.size > 0; }));

    REQUIRE(pistoris::paths::parseResourceSelector("ambiance:explore", selector));
    auto ambiance = resources.loadAmbiance(selector);
    REQUIRE(ambiance);
    REQUIRE(ambiance->soundCount() > 0);
    CHECK(std::ranges::all_of(ambiance->sounds(),
                              [](const ArxSoundView& sound) { return sound.encoded_audio.size > 0; }));

    REQUIRE(pistoris::paths::parseResourceSelector("cinematic:numbers", selector));
    auto cinematic = resources.loadCinematic(selector);
    REQUIRE(cinematic);
    REQUIRE(cinematic->illustrationCount() == 3);
    CHECK(std::ranges::all_of(cinematic->textures(),
                              [](const ArxTextureView& texture) { return texture.encoded_image.size > 0; }));
    CHECK(cinematic->languageCount() == 3);
    REQUIRE(cinematic->soundEncodings().size() == 28);
    CHECK(std::ranges::all_of(cinematic->soundEncodings(), [](const ArxCinematicSoundEncodingView& encoding) {
      return encoding.encoded_audio.size > 0;
    }));

    REQUIRE(pistoris::paths::parseResourceSelector("level:9", selector));
    auto level = resources.loadLevel(selector);
    REQUIRE(level);
    CHECK(level->minimap().encoded_image.size > 0);
    CHECK(level->loadingScreen().size > 0);
  }

  TEST_CASE("Native bundles preserve carriers and coalesce discovered resource files") {
    auto opened = pistoris::resource_io::ResourceMounts::open(
        {.read_mounts = {test_support::kFixtureMount}, .write_mount = std::nullopt});
    REQUIRE(opened);
    pistoris::resource_io::Resources resources(std::move(*opened));
    pistoris::paths::ResourceSelector selector;

    REQUIRE(pistoris::paths::parseResourceSelector("model:weapons:sword_00", selector));
    auto model_document = resources.readDocument(selector);
    REQUIRE(model_document);
    CHECK(model_document->mount_mask == pistoris::resource_io::kAllResourceMounts);
    CHECK(model_document->mount_id != 0);

    REQUIRE(pistoris::paths::parseResourceSelector("anim:npc:human_male_gathering", selector));
    auto animation_document = resources.readDocument(selector);
    REQUIRE(animation_document);
    std::vector<pistoris::resource_io::ResourceDocument> animations;
    animations.push_back(*animation_document);
    animations.push_back(std::move(*animation_document));
    auto model = resources.loadModelNativeBundle(*model_document, animations);
    REQUIRE(model);
    CHECK(model->model().source().selector_kind == ARX_RESOURCE_KIND_MODEL);
    CHECK(model->animations().size() == 2);
    CHECK_FALSE(model->resources().references().empty());
    CHECK(model->resources().files().size() < model->resources().references().size());
    CHECK(std::ranges::all_of(model->resources().files(),
                              [](const auto& file) { return !file.nativePath().empty() && !file.data().empty(); }));

    REQUIRE(pistoris::paths::parseResourceSelector("level:9", selector));
    auto level_document = resources.readDocument(selector);
    REQUIRE(level_document);
    auto level = resources.loadLevelNativeBundle(*level_document);
    REQUIRE(level);
    REQUIRE(level->geometry());
    REQUIRE(level->lighting());
    REQUIRE(level->scene());
    CHECK(std::ranges::any_of(level->resources().references(), [](const auto& reference) {
      return reference.role() == pistoris::resource_io::NativeResourceRole::kMinimap;
    }));

    REQUIRE(pistoris::paths::parseResourceSelector("ambiance:explore", selector));
    auto ambiance_document = resources.readDocument(selector);
    REQUIRE(ambiance_document);
    auto ambiance = resources.loadAmbianceNativeBundle(*ambiance_document);
    REQUIRE(ambiance);
    CHECK(std::ranges::any_of(ambiance->resources().references(), [](const auto& reference) {
      return reference.role() == pistoris::resource_io::NativeResourceRole::kSound && reference.status() == ARX_OK;
    }));

    REQUIRE(pistoris::paths::parseResourceSelector("cinematic:numbers", selector));
    auto cinematic_document = resources.readDocument(selector);
    REQUIRE(cinematic_document);
    auto cinematic = resources.loadCinematicNativeBundle(*cinematic_document);
    REQUIRE(cinematic);
    CHECK(std::ranges::any_of(cinematic->resources().references(), [](const auto& reference) {
      return reference.role() == pistoris::resource_io::NativeResourceRole::kSound && !reference.language().empty() &&
             reference.status() == ARX_OK;
    }));
  }

  TEST_CASE("Native bundle resource caches keep lookup flags isolated") {
    const auto& catalog = test_support::fixtureCatalog();
    const auto animation_fixture =
        std::ranges::find(catalog.animations, "human_male_gathering", &test_support::AnimationFixture::name);
    REQUIRE(animation_fixture != catalog.animations.end());
    auto native_animation = pistoris::readTea(readFile(animation_fixture->tea));
    REQUIRE(native_animation);
    std::vector<pistoris::SoundSourceReference> sound_sources;
    REQUIRE(pistoris::Animation::importNative(*native_animation, &sound_sources));
    REQUIRE_FALSE(sound_sources.empty());

    const std::string authored_sound_path = sound_sources.front().path;
    std::string sound_path = authored_sound_path;
    std::ranges::replace(sound_path, '\\', '/');
    if (!sound_path.starts_with("sfx/")) sound_path.insert(0, "sfx/");
    std::filesystem::path source_path = std::filesystem::path(test_support::kFixtureMount) / sound_path;
    if (!std::filesystem::is_regular_file(source_path)) {
      for (const std::string_view extension : {".wav", ".mp3", ".ogg"}) {
        std::string candidate = sound_path;
        if (candidate.ends_with('.')) candidate.pop_back();
        candidate += extension;
        source_path = std::filesystem::path(test_support::kFixtureMount) / candidate;
        if (std::filesystem::is_regular_file(source_path)) {
          sound_path = std::move(candidate);
          break;
        }
      }
    }
    REQUIRE(std::filesystem::is_regular_file(source_path));

    TemporaryDirectory temporary;
    const auto collision_root = temporary.path() / "collisions";
    const std::filesystem::path exact_path = collision_root / sound_path;
    std::string colliding_name = exact_path.filename().string();
    for (char& character : colliding_name) {
      if (character >= 'a' && character <= 'z') character = static_cast<char>(character - ('a' - 'A'));
    }
    const std::filesystem::path colliding_path = exact_path.parent_path() / colliding_name;
    writeFile(exact_path, readFile(source_path));
    writeFile(colliding_path, readFile(source_path));

    std::error_code error;
    const auto entry_count = static_cast<std::size_t>(std::distance(
        std::filesystem::directory_iterator(exact_path.parent_path(), error), std::filesystem::directory_iterator()));
    REQUIRE_FALSE(error);
    if (entry_count < 2) return;

    auto opened = pistoris::resource_io::ResourceMounts::open(
        {.read_mounts = {collision_root, test_support::kFixtureMount}, .write_mount = std::nullopt});
    REQUIRE(opened);
    pistoris::resource_io::Resources resources(std::move(*opened));
    pistoris::paths::ResourceSelector selector;
    REQUIRE(pistoris::paths::parseResourceSelector("model:weapons:sword_00", selector));
    auto model_document = resources.readDocument(selector);
    REQUIRE(model_document);
    REQUIRE(pistoris::paths::parseResourceSelector("anim:npc:human_male_gathering", selector));
    auto animation_document = resources.readDocument(selector);
    REQUIRE(animation_document);

    std::vector<pistoris::resource_io::ResourceDocument> animations(2, *animation_document);
    animations[0].flags = pistoris::resource_io::kResourceIoFlagNone;
    animations[1].flags = pistoris::resource_io::kResourceIoRecoverCaseCollisions;
    auto bundle = resources.loadModelNativeBundle(
        *model_document,
        animations,
        {.native_text_mode = pistoris::NativeTextMode::kAuto, .suppress_related_resource_errors = true});
    REQUIRE(bundle);
    CHECK(std::ranges::any_of(bundle->resources().references(), [&](const auto& reference) {
      return reference.ownerKind() == ARX_RESOURCE_KIND_ANIMATION && reference.ownerIndex() == 0 &&
             reference.authoredPath() == authored_sound_path && reference.status() == ARX_RESOURCE_IO_AMBIGUOUS_PATH;
    }));
    CHECK(std::ranges::any_of(bundle->resources().references(), [&](const auto& reference) {
      return reference.ownerKind() == ARX_RESOURCE_KIND_ANIMATION && reference.ownerIndex() == 1 &&
             reference.authoredPath() == authored_sound_path && reference.status() == ARX_OK;
    }));
  }

  TEST_CASE("Retained documents convert without rereading their primary file") {
    const auto& catalog = test_support::fixtureCatalog();
    const auto fixture = std::ranges::find(catalog.models, "human_male", &test_support::ModelFixture::name);
    REQUIRE(fixture != catalog.models.end());
    TemporaryDirectory temporary;
    const auto source = temporary.path() / "model.glb";
    writeFile(source, readFile(fixture->glb.path));

    pistoris::resource_io::Resources resources;
    auto document = resources.readDocumentFile(source);
    REQUIRE(document);
    REQUIRE(std::filesystem::remove(source));
    auto loaded = resources.loadModel(*document);
    REQUIRE(loaded);
    CHECK(loaded->model.vertexCount() > 0);
  }

  TEST_CASE("Resource conversion failures retain typed format locations") {
    TemporaryDirectory temporary;
    const auto glb = temporary.path() / "broken.glb";
    writeFile(glb, std::string_view("glTF"));

    pistoris::resource_io::Resources resources;
    auto broken_glb = resources.loadModelFile(glb);
    REQUIRE_FALSE(broken_glb);
    REQUIRE(broken_glb.error());
    REQUIRE(broken_glb.error()->location());
    REQUIRE(broken_glb.error()->location()->content_location);
    CHECK(std::holds_alternative<pistoris::GlbLocation>(*broken_glb.error()->location()->content_location));
    CHECK(broken_glb.error()->location()->native_path == glb);

    const auto ftl = temporary.path() / "broken.ftl";
    writeFile(ftl, std::string_view("short"));
    auto broken_ftl = resources.loadModelFile(ftl);
    REQUIRE_FALSE(broken_ftl);
    REQUIRE(broken_ftl.error());
    REQUIRE(broken_ftl.error()->location());
    REQUIRE(broken_ftl.error()->location()->content_location);
    CHECK(std::holds_alternative<pistoris::FtlBinaryLocation>(*broken_ftl.error()->location()->content_location));
    CHECK(broken_ftl.error()->location()->native_path == ftl);
  }

  TEST_CASE("JSON documents use loose dependency layout regardless of discovery path") {
    TemporaryDirectory temporary;
    writeFile(temporary.path() / "model.ftl.json", std::string_view("{}"));
    writeFile(temporary.path() / "level.dlf.json", std::string_view("{}"));
    writeFile(temporary.path() / "level.fts.json", std::string_view("{}"));

    auto opened =
        pistoris::resource_io::ResourceMounts::open({.read_mounts = {temporary.path()}, .write_mount = std::nullopt});
    REQUIRE(opened);
    pistoris::resource_io::Resources resources(std::move(*opened));

    auto model = resources.readDocument("model.ftl.json");
    auto scene = resources.readDocument("level.dlf.json");
    auto geometry = resources.readDocument("level.fts.json");
    REQUIRE(model);
    REQUIRE(scene);
    REQUIRE(geometry);
    CHECK(model->layout == pistoris::resource_io::ResourceLayout::kLoose);
    CHECK(scene->layout == pistoris::resource_io::ResourceLayout::kLoose);
    CHECK(geometry->layout == pistoris::resource_io::ResourceLayout::kLoose);

    auto loose_model = resources.readDocumentFile(temporary.path() / "model.ftl.json");
    REQUIRE(loose_model);
    CHECK(loose_model->layout == pistoris::resource_io::ResourceLayout::kLoose);
  }

  TEST_CASE("JSON native bundles report the encoding used for their carrier fields") {
    const auto& fixture = test_support::fixtureCatalog().models.front();
    auto native = pistoris::readFtl(readFile(fixture.ftl));
    REQUIRE(native);
    auto json = pistoris::toFtlJson(*native, false);
    REQUIRE(json);
    TemporaryDirectory temporary;
    const auto source = temporary.path() / "model.ftl.json";
    writeFile(source, *json);

    pistoris::resource_io::Resources resources;
    auto document = resources.readDocumentFile(source);
    REQUIRE(document);
    auto bundle =
        resources.loadModelNativeBundle(*document, {}, {.native_text_mode = pistoris::NativeTextMode::kLatin1});
    REQUIRE(bundle);
    CHECK(bundle->model().textMode() == pistoris::NativeTextMode::kLatin1);
    CHECK(resources.loadModel(*document, {.glb = {}, .native_text_mode = pistoris::NativeTextMode::kLatin1}));
  }

  TEST_CASE("Resource loaders accept semantic paths, extensionless logical paths, and loose files") {
    const auto& catalog = test_support::fixtureCatalog();
    const auto model_fixture = std::ranges::find(catalog.models, "sword_00", &test_support::ModelFixture::name);
    REQUIRE(model_fixture != catalog.models.end());
    auto opened = pistoris::resource_io::ResourceMounts::open(
        {.read_mounts = {test_support::kFixtureMount}, .write_mount = std::nullopt});
    REQUIRE(opened);
    pistoris::resource_io::Resources resources(std::move(*opened));

    const std::string model_path = model_fixture->ftl.lexically_relative(test_support::kFixtureMount).generic_string();
    pistoris::paths::ModelPathView semantic;
    REQUIRE(pistoris::paths::modelFromFtl(model_path, semantic));
    CHECK(resources.loadModel(semantic));

    std::string extensionless = model_path;
    REQUIRE(extensionless.ends_with(".ftl"));
    extensionless.resize(extensionless.size() - 4U);
    CHECK(resources.loadModel(extensionless));
    CHECK(resources.loadModelFile(model_fixture->ftl));

    const test_support::LevelFixture& level_fixture = catalog.levels.front();
    CHECK(resources.loadLevelFile(level_fixture.fts, level_fixture.llf, level_fixture.dlf));

    const test_support::CinematicFixture& cinematic_fixture = catalog.cinematics.front();
    auto cinematic = resources.loadCinematicFile(cinematic_fixture.glb);
    REQUIRE(cinematic);
    CHECK(cinematic->languageCount() == cinematic_fixture.audio.languages.size());
    CHECK(cinematic->soundEncodingCount() == cinematic_fixture.audio.encodings);

    const auto animated_model = std::ranges::find(catalog.models, "human_male", &test_support::ModelFixture::name);
    REQUIRE(animated_model != catalog.models.end());
    auto glb_model = resources.loadModelFile(animated_model->glb.path);
    REQUIRE(glb_model);
    CHECK_FALSE(glb_model->animations.empty());
  }

  TEST_CASE("Resource writers support mounted and loose destinations") {
    const auto& catalog = test_support::fixtureCatalog();
    const auto model_fixture = std::ranges::find(catalog.models, "sword_00", &test_support::ModelFixture::name);
    REQUIRE(model_fixture != catalog.models.end());
    TemporaryDirectory temporary;
    auto opened = pistoris::resource_io::ResourceMounts::open(
        {.read_mounts = {test_support::kFixtureMount}, .write_mount = temporary.path() / "mounted"});
    REQUIRE(opened);
    pistoris::resource_io::Resources resources(std::move(*opened));
    pistoris::paths::ResourceSelector selector;
    REQUIRE(pistoris::paths::parseResourceSelector(model_fixture->selector, selector));
    auto model = resources.loadModel(selector);
    REQUIRE(model);

    const auto collision_upper = temporary.path() / "mounted/CaseCollision";
    const auto collision_lower = temporary.path() / "mounted/casecollision";
    std::error_code collision_error;
    REQUIRE(std::filesystem::create_directories(collision_upper, collision_error));
    REQUIRE_FALSE(collision_error);
    std::filesystem::create_directories(collision_lower, collision_error);
    REQUIRE_FALSE(collision_error);
    const bool distinct_collision_paths =
        !std::filesystem::equivalent(collision_upper, collision_lower, collision_error);
    REQUIRE_FALSE(collision_error);

    pistoris::resource_io::ModelWriteOptions recovered_case_options;
    recovered_case_options.resource.outputs = pistoris::resource_io::kResourceOutputPrimary;
    recovered_case_options.resource.io_flags = pistoris::resource_io::kResourceIoRecoverCaseCollisions;
    auto recovered_case_outputs =
        resources.prepareModelOutputs(model->model, "CASECOLLISION/recovered.glb", recovered_case_options);
    REQUIRE(recovered_case_outputs);
    REQUIRE(recovered_case_outputs->size() == 1);
    CHECK((*recovered_case_outputs)[0].io_flags == pistoris::resource_io::kResourceIoRecoverCaseCollisions);

    if (distinct_collision_paths) {
      auto strict_outputs = *recovered_case_outputs;
      strict_outputs[0].io_flags = pistoris::resource_io::kResourceIoFlagNone;
      auto strict_plan = resources.prepareWrite(std::move(strict_outputs));
      REQUIRE_FALSE(strict_plan);
      CHECK(strict_plan.code() == ARX_RESOURCE_IO_AMBIGUOUS_PATH);
    }

    auto recovered_case_plan = resources.prepareWrite(std::move(*recovered_case_outputs));
    REQUIRE(recovered_case_plan);
    REQUIRE(recovered_case_plan->entries().size() == 1);
    CHECK(recovered_case_plan->entries()[0].nativePath() == canonicalExistingPath(collision_upper) / "recovered.glb");
    REQUIRE(recovered_case_plan->execute());

    REQUIRE(resources.writeModel(*model, "exports/sword"));
    CHECK(std::filesystem::is_regular_file(temporary.path() / "mounted/exports/sword.ftl"));

    REQUIRE(resources.writeModel(*model, "exports/sword.obj"));
    CHECK(std::filesystem::is_regular_file(temporary.path() / "mounted/exports/sword.mtl"));
    REQUIRE(resources.mounts().addReadMount(temporary.path() / "mounted"));
    CHECK(resources.loadModel("exports/sword.obj"));

    REQUIRE(resources.writeModel(*model, "editing/sword.ftl.json"));
    auto json_model = resources.loadModel("editing/sword.ftl.json");
    REQUIRE(json_model);
    CHECK(std::ranges::all_of(json_model->model.textures(),
                              [](const ArxTextureView& texture) { return texture.encoded_image.size > 0; }));

    auto unsupported = resources.writeModel(*model, "exports/sword.unsupported");
    REQUIRE_FALSE(unsupported);
    REQUIRE(unsupported.error());
    REQUIRE(unsupported.error()->location());
    CHECK(unsupported.error()->location()->operation == pistoris::resource_io::ResourceIoOperation::kWrite);

    const auto loose = temporary.path() / "loose/sword.glb";
    REQUIRE(resources.writeModelFile(*model, loose));
    CHECK(std::filesystem::is_regular_file(loose));
    CHECK(std::filesystem::is_regular_file(temporary.path() / "loose/sword[icon].png"));
    auto reloaded = resources.loadModelFile(loose);
    REQUIRE(reloaded);
    CHECK(reloaded->model.inventoryIcon().encoded_image.size > 0);

    const auto primary_only = temporary.path() / "primary/sword.glb";
    pistoris::resource_io::ModelWriteOptions model_primary_options;
    model_primary_options.resource.outputs = pistoris::resource_io::kResourceOutputPrimary;
    auto prepared = resources.prepareModelFileOutputs(model->model, primary_only, model_primary_options);
    REQUIRE(prepared);
    REQUIRE(prepared->size() == 1);
    CHECK((*prepared)[0].primary);
    CHECK((*prepared)[0].native_path == primary_only);
    auto plan = resources.prepareWrite(std::move(*prepared));
    REQUIRE(plan);
    REQUIRE(plan->execute());
    CHECK(std::filesystem::is_regular_file(primary_only));
    CHECK_FALSE(std::filesystem::exists(temporary.path() / "primary/sword[icon].png"));

    REQUIRE(pistoris::paths::parseResourceSelector("level:9", selector));
    auto level = resources.loadLevel(selector);
    REQUIRE(level);
    pistoris::resource_io::LevelWriteOptions level_primary_options;
    level_primary_options.resource.outputs = pistoris::resource_io::kResourceOutputPrimary;
    REQUIRE(resources.writeLevel(*level, "external/level9.glb", level_primary_options));
    REQUIRE(resources.mounts().addReadMount(temporary.path() / "mounted"));
    CHECK(resources.loadLevel("external/level9.glb"));
    REQUIRE(resources.writeLevel(*level, "external/level9.fts", level_primary_options));
    CHECK(resources.loadLevel("external/level9.fts"));
  }

  TEST_CASE("Loose native resource writers round trip their sidecars") {
    const auto& catalog = test_support::fixtureCatalog();
    TemporaryDirectory temporary;
    auto opened = pistoris::resource_io::ResourceMounts::open(
        {.read_mounts = {test_support::kFixtureMount}, .write_mount = std::nullopt});
    REQUIRE(opened);
    pistoris::resource_io::Resources resources(std::move(*opened));
    pistoris::paths::ResourceSelector selector;

    REQUIRE(pistoris::paths::parseResourceSelector(catalog.animations.front().selector, selector));
    auto animation = resources.loadAnimation(selector);
    REQUIRE(animation);
    const auto tea = temporary.path() / "animation/animation.tea";
    REQUIRE(resources.writeAnimationFile(*animation, tea));
    auto loaded_animation = resources.loadAnimationFile(tea);
    REQUIRE(loaded_animation);
    CHECK(loaded_animation->soundCount() == animation->soundCount());
    for (std::size_t index = 0; index < animation->soundCount(); ++index) {
      const ArxEncodedAudioView expected = animation->sounds()[index].encoded_audio;
      const ArxEncodedAudioView actual = loaded_animation->sounds()[index].encoded_audio;
      CHECK(std::ranges::equal(std::span(expected.data, expected.size), std::span(actual.data, actual.size)));
    }

    REQUIRE(pistoris::paths::parseResourceSelector(catalog.ambiances.front().selector, selector));
    auto ambiance = resources.loadAmbiance(selector);
    REQUIRE(ambiance);
    const auto amb = temporary.path() / "ambiance/ambiance.amb";
    REQUIRE(resources.writeAmbianceFile(*ambiance, amb));
    auto loaded_ambiance = resources.loadAmbianceFile(amb);
    REQUIRE(loaded_ambiance);
    CHECK(loaded_ambiance->soundCount() == ambiance->soundCount());
    for (std::size_t index = 0; index < ambiance->soundCount(); ++index) {
      const ArxEncodedAudioView expected = ambiance->sounds()[index].encoded_audio;
      const ArxEncodedAudioView actual = loaded_ambiance->sounds()[index].encoded_audio;
      CHECK(std::ranges::equal(std::span(expected.data, expected.size), std::span(actual.data, actual.size)));
    }

    REQUIRE(pistoris::paths::parseResourceSelector(catalog.cinematics.front().selector, selector));
    auto cinematic = resources.loadCinematic(selector);
    REQUIRE(cinematic);
    const auto cin = temporary.path() / "cinematic/cinematic.cin";
    REQUIRE(resources.writeCinematicFile(*cinematic, cin));
    auto loaded_cinematic = resources.loadCinematicFile(cin);
    REQUIRE(loaded_cinematic);
    REQUIRE(loaded_cinematic->soundEncodingCount() == cinematic->soundEncodingCount());
    for (std::size_t index = 0; index < cinematic->soundEncodingCount(); ++index) {
      const ArxEncodedAudioView expected = cinematic->soundEncodings()[index].encoded_audio;
      const ArxEncodedAudioView actual = loaded_cinematic->soundEncodings()[index].encoded_audio;
      CHECK(std::ranges::equal(std::span(expected.data, expected.size), std::span(actual.data, actual.size)));
    }

    REQUIRE(pistoris::paths::parseResourceSelector(catalog.levels.front().selector, selector));
    auto level = resources.loadLevel(selector);
    REQUIRE(level);
    const auto fts = temporary.path() / "level/level.fts";
    const auto llf = temporary.path() / "level/level.llf";
    const auto dlf = temporary.path() / "level/level.dlf";
    REQUIRE(resources.writeLevelFile(*level, fts, llf, dlf));
    auto loaded_level = resources.loadLevelFile(fts, llf, dlf);
    REQUIRE(loaded_level);
    CHECK(loaded_level->minimap().encoded_image.size > 0);
    CHECK(loaded_level->loadingScreen().size > 0);

    const auto derived_fts = temporary.path() / "level-derived/level.fts";
    REQUIRE(resources.writeLevelFile(*level, derived_fts));
    CHECK(std::filesystem::is_regular_file(temporary.path() / "level-derived/level.llf"));
    CHECK(std::filesystem::is_regular_file(temporary.path() / "level-derived/level.dlf"));
    CHECK(resources.loadLevelFile(derived_fts));
  }

  TEST_CASE("Loose Level discovery accepts independently encoded companions") {
    const auto& catalog = test_support::fixtureCatalog();
    TemporaryDirectory temporary;
    auto opened = pistoris::resource_io::ResourceMounts::open(
        {.read_mounts = {test_support::kFixtureMount}, .write_mount = temporary.path() / "mounted"});
    REQUIRE(opened);
    pistoris::resource_io::Resources resources(std::move(*opened));
    pistoris::paths::ResourceSelector selector;
    REQUIRE(pistoris::paths::parseResourceSelector(catalog.levels.front().selector, selector));
    auto level = resources.loadLevel(selector);
    REQUIRE(level);

    const auto primary = temporary.path() / "mixed/level.fts.json";
    const auto llf = temporary.path() / "mixed/level.llf";
    const auto dlf = temporary.path() / "mixed/level.dlf.json";
    REQUIRE(resources.writeLevelFile(*level, primary, llf, dlf, selector.level));

    auto discovered = resources.loadLevelFile(primary);
    REQUIRE(discovered);
    CHECK(discovered->lightCount() == level->lightCount());
    CHECK(discovered->entityCount() == level->entityCount());

    const auto duplicate_llf = temporary.path() / "mixed/level.llf.json";
    pistoris::resource_io::LevelWriteOptions companion_only;
    companion_only.resource.outputs = pistoris::resource_io::kResourceOutputCompanions;
    REQUIRE(resources.writeLevelFile(*level,
                                     primary,
                                     duplicate_llf,
                                     std::nullopt,
                                     selector.level,
                                     companion_only,
                                     {.existing_file_policy = pistoris::resource_io::ExistingFilePolicy::kOverwrite}));
    auto ambiguous = resources.loadLevelFile(primary);
    REQUIRE_FALSE(ambiguous);
    CHECK(ambiguous.code() == ARX_RESOURCE_IO_AMBIGUOUS_PATH);
    CHECK(resources.loadLevelFile(primary, llf, dlf));
  }

  TEST_CASE("Resource outputs become plans that coalesce destinations and reject conflicts before writing") {
    TemporaryDirectory temporary;
    pistoris::resource_io::Resources resources;
    const auto identical_path = temporary.path() / "identical.bin";
    pistoris::resource_io::ResourceOutput first;
    first.address = pistoris::resource_io::ResourceOutputAddress::kNative;
    first.native_path = identical_path;
    first.data = {1, 2, 3};
    pistoris::resource_io::ResourceOutput second = first;
    auto identical = resources.prepareWrite({std::move(first), std::move(second)});
    REQUIRE(identical);
    REQUIRE(identical->execute());
    const std::vector<std::uint8_t> expected = {1, 2, 3};
    CHECK(readFile(identical_path) == expected);

    const auto conflict_path = temporary.path() / "conflict.bin";
    pistoris::resource_io::ResourceOutput left;
    left.address = pistoris::resource_io::ResourceOutputAddress::kNative;
    left.native_path = conflict_path;
    left.data = {1};
    pistoris::resource_io::ResourceOutput right = left;
    right.data = {2};
    auto conflict = resources.prepareWrite({std::move(left), std::move(right)});
    REQUIRE(conflict);
    auto written = conflict->execute();
    REQUIRE_FALSE(written);
    CHECK(written.code() == ARX_RESOURCE_IO_DECISION_REQUIRED);
    CHECK_FALSE(std::filesystem::exists(conflict_path));

    const auto mounted_root = temporary.path() / "mounted";
    auto opened = pistoris::resource_io::ResourceMounts::open({.read_mounts = {}, .write_mount = mounted_root});
    REQUIRE(opened);
    pistoris::resource_io::Resources mounted_resources(std::move(*opened));
    pistoris::resource_io::ResourceOutput logical;
    logical.address = pistoris::resource_io::ResourceOutputAddress::kLogical;
    logical.resource_path = "alias.bin";
    logical.data = {1};
    pistoris::resource_io::ResourceOutput native;
    native.address = pistoris::resource_io::ResourceOutputAddress::kNative;
    native.native_path = mounted_root / "alias.bin";
    native.data = {2};
    auto alias = mounted_resources.prepareWrite({std::move(logical), std::move(native)});
    REQUIRE(alias);
    auto alias_written = alias->execute();
    REQUIRE_FALSE(alias_written);
    CHECK(alias_written.code() == ARX_RESOURCE_IO_DECISION_REQUIRED);
    CHECK_FALSE(std::filesystem::exists(mounted_root / "alias.bin"));

    pistoris::resource_io::ResourceOutput invalid_flags_output;
    invalid_flags_output.address = pistoris::resource_io::ResourceOutputAddress::kNative;
    invalid_flags_output.native_path = temporary.path() / "invalid-flags.bin";
    invalid_flags_output.io_flags = 1U << 31U;
    auto invalid_flags = resources.prepareWrite({std::move(invalid_flags_output)});
    REQUIRE_FALSE(invalid_flags);
    CHECK(invalid_flags.code() == ARX_INVALID_OPTIONS);

    pistoris::resource_io::ResourceOutput invalid_address_output;
    invalid_address_output.address = static_cast<pistoris::resource_io::ResourceOutputAddress>(255);
    invalid_address_output.native_path = temporary.path() / "invalid-address.bin";
    auto invalid_address = resources.prepareWrite({std::move(invalid_address_output)});
    REQUIRE_FALSE(invalid_address);
    CHECK(invalid_address.code() == ARX_INVALID_OPTIONS);

    const auto invalid_policy_path = temporary.path() / "invalid-policy.bin";
    writeFile(invalid_policy_path, "old");
    pistoris::resource_io::ResourceOutput invalid_policy_output;
    invalid_policy_output.address = pistoris::resource_io::ResourceOutputAddress::kNative;
    invalid_policy_output.native_path = invalid_policy_path;
    invalid_policy_output.data = {'n', 'e', 'w'};
    auto invalid_policy = resources.prepareWrite({std::move(invalid_policy_output)});
    REQUIRE(invalid_policy);
    invalid_policy->setDefaultExistingFilePolicy(static_cast<pistoris::resource_io::ExistingFilePolicy>(255));
    auto invalid_policy_result = invalid_policy->preflight();
    REQUIRE_FALSE(invalid_policy_result);
    CHECK(invalid_policy_result.code() == ARX_INVALID_OPTIONS);
  }

  TEST_CASE("Direct resource writers require an explicit policy for differing existing files") {
    const auto& fixture = test_support::fixtureCatalog().models.front();
    pistoris::resource_io::Resources resources;
    auto loaded = resources.loadModelFile(fixture.glb.path);
    REQUIRE(loaded);

    TemporaryDirectory temporary;
    const auto target = temporary.path() / "model.glb";
    pistoris::resource_io::ModelWriteOptions primary_only;
    primary_only.resource.outputs = pistoris::resource_io::kResourceOutputPrimary;

    auto unsupported_outputs = primary_only;
    unsupported_outputs.resource.outputs |= 1U << 31U;
    auto invalid_outputs = resources.prepareModelFileOutputs(loaded->model, target, unsupported_outputs);
    REQUIRE_FALSE(invalid_outputs);
    CHECK(invalid_outputs.code() == ARX_INVALID_OPTIONS);

    const pistoris::resource_io::ResourceWriteOptions unsupported_policy{
        .existing_file_policy = static_cast<pistoris::resource_io::ExistingFilePolicy>(255)};
    auto invalid_writer_policy = resources.writeModelFile(loaded->model, target, primary_only, unsupported_policy);
    REQUIRE_FALSE(invalid_writer_policy);
    CHECK(invalid_writer_policy.code() == ARX_INVALID_OPTIONS);

    auto unsupported_io_flags = primary_only;
    unsupported_io_flags.resource.io_flags = 1U << 31U;
    auto invalid_writer_flags = resources.prepareModelFileOutputs(loaded->model, target, unsupported_io_flags);
    REQUIRE_FALSE(invalid_writer_flags);
    CHECK(invalid_writer_flags.code() == ARX_INVALID_OPTIONS);

    auto invalid_glb_scale = primary_only;
    invalid_glb_scale.glb = pistoris::Model::GlbExportOptions{};
    invalid_glb_scale.glb->arx_units_per_glb_unit = 0.0f;
    auto invalid_conversion = resources.prepareModelFileOutputs(loaded->model, target, invalid_glb_scale);
    REQUIRE_FALSE(invalid_conversion);
    CHECK(invalid_conversion.code() == ARX_INVALID_OPTIONS);

    auto native_options_for_glb = primary_only;
    native_options_for_glb.native_text_mode = pistoris::NativeTextMode::kAuto;
    auto invalid_native_options = resources.prepareModelFileOutputs(loaded->model, target, native_options_for_glb);
    REQUIRE_FALSE(invalid_native_options);
    CHECK(invalid_native_options.code() == ARX_INVALID_OPTIONS);

    auto glb_options_for_native = primary_only;
    glb_options_for_native.glb = pistoris::Model::GlbExportOptions{};
    auto invalid_glb_options =
        resources.prepareModelFileOutputs(loaded->model, temporary.path() / "model.ftl", glb_options_for_native);
    REQUIRE_FALSE(invalid_glb_options);
    CHECK(invalid_glb_options.code() == ARX_INVALID_OPTIONS);

    auto initial_write = resources.writeModelFile(loaded->model, target, primary_only);
    REQUIRE(initial_write);
    REQUIRE(initial_write->entries().size() == 1);
    CHECK(initial_write->entries()[0].nativePath() == canonicalExistingPath(temporary.path()) / "model.glb");
    CHECK(initial_write->entries()[0].status() == pistoris::resource_io::ResourceWriteStatus::kWritten);
    writeFile(target, "existing");
    const std::vector<std::uint8_t> existing = {'e', 'x', 'i', 's', 't', 'i', 'n', 'g'};

    auto undecided = resources.writeModelFile(loaded->model, target, primary_only);
    REQUIRE_FALSE(undecided);
    CHECK(undecided.code() == ARX_RESOURCE_IO_DECISION_REQUIRED);
    CHECK(readFile(target) == existing);

    REQUIRE(resources.writeModelFile(loaded->model,
                                     target,
                                     primary_only,
                                     {.existing_file_policy = pistoris::resource_io::ExistingFilePolicy::kPreserve}));
    CHECK(readFile(target) == existing);

    REQUIRE(resources.writeModelFile(loaded->model,
                                     target,
                                     primary_only,
                                     {.existing_file_policy = pistoris::resource_io::ExistingFilePolicy::kOverwrite}));
    CHECK(readFile(target) != existing);
  }

  TEST_CASE("Write plans expose producer and existing-file decisions without losing completion state") {
    TemporaryDirectory temporary;
    pistoris::resource_io::Resources resources;

    const auto coalesced_path = temporary.path() / "coalesced.bin";
    pistoris::resource_io::ResourceOutput first;
    first.address = pistoris::resource_io::ResourceOutputAddress::kNative;
    first.native_path = coalesced_path;
    first.data = {1, 2, 3};
    pistoris::resource_io::ResourceOutput second = first;
    auto coalesced = resources.prepareWrite({std::move(first), std::move(second)});
    REQUIRE(coalesced);
    REQUIRE(coalesced->entries().size() == 1);
    REQUIRE(coalesced->entries()[0].candidates().size() == 2);
    REQUIRE(coalesced->execute());
    CHECK(coalesced->entries()[0].status() == pistoris::resource_io::ResourceWriteStatus::kWritten);
    CHECK(std::ranges::all_of(coalesced->entries()[0].candidates(),
                              [](const auto& candidate) { return candidate.written; }));

    const auto existing_path = temporary.path() / "existing.bin";
    writeFile(existing_path, "old");
    pistoris::resource_io::ResourceOutput replacement;
    replacement.address = pistoris::resource_io::ResourceOutputAddress::kNative;
    replacement.native_path = existing_path;
    replacement.data = {'n', 'e', 'w'};
    auto existing = resources.prepareWrite({std::move(replacement)});
    REQUIRE(existing);
    CHECK(existing->defaultExistingFilePolicy() == pistoris::resource_io::ExistingFilePolicy::kError);
    CHECK_FALSE(existing->entries()[0].existingFilePolicy());
    REQUIRE(existing->preflight());
    REQUIRE(existing->entries().size() == 1);
    CHECK(existing->entries()[0].status() == pistoris::resource_io::ResourceWriteStatus::kNeedsExistingFilePolicy);
    auto undecided = existing->execute();
    REQUIRE_FALSE(undecided);
    CHECK(undecided.code() == ARX_RESOURCE_IO_DECISION_REQUIRED);
    const std::vector<std::uint8_t> old_data = {'o', 'l', 'd'};
    const std::vector<std::uint8_t> new_data = {'n', 'e', 'w'};
    CHECK(readFile(existing_path) == old_data);

    existing->entries()[0].setExistingFilePolicy(pistoris::resource_io::ExistingFilePolicy::kPreserve);
    REQUIRE(existing->entries()[0].existingFilePolicy());
    REQUIRE(existing->execute());
    CHECK(existing->entries()[0].status() == pistoris::resource_io::ResourceWriteStatus::kPreserved);
    CHECK(readFile(existing_path) == old_data);

    existing->entries()[0].clearExistingFilePolicy();
    CHECK_FALSE(existing->entries()[0].existingFilePolicy());
    REQUIRE(existing->preflight());
    CHECK(existing->entries()[0].status() == pistoris::resource_io::ResourceWriteStatus::kNeedsExistingFilePolicy);

    existing->entries()[0].setExistingFilePolicy(pistoris::resource_io::ExistingFilePolicy::kOverwrite);
    REQUIRE(existing->execute());
    CHECK(existing->entries()[0].status() == pistoris::resource_io::ResourceWriteStatus::kWritten);
    CHECK(readFile(existing_path) == new_data);
  }

  TEST_CASE("Native dependencies honor masks while mounted external sidecars stay beside their primary") {
    const auto& catalog = test_support::fixtureCatalog();
    const auto animation_fixture =
        std::ranges::find(catalog.animations, "human_male_gathering", &test_support::AnimationFixture::name);
    REQUIRE(animation_fixture != catalog.animations.end());
    auto native_animation = pistoris::readTea(readFile(animation_fixture->tea));
    REQUIRE(native_animation);
    std::vector<pistoris::SoundSourceReference> sound_sources;
    auto imported_animation = pistoris::Animation::importNative(*native_animation, &sound_sources);
    REQUIRE(imported_animation);
    REQUIRE_FALSE(sound_sources.empty());

    TemporaryDirectory temporary;
    const auto first = temporary.path() / "first";
    const auto second = temporary.path() / "second";
    const auto animation_path = animation_fixture->tea.lexically_relative(test_support::kFixtureMount);
    writeFile(first / animation_path, readFile(animation_fixture->tea));
    for (const auto& source : sound_sources) {
      std::string path = source.path;
      std::ranges::replace(path, '\\', '/');
      if (!path.starts_with("sfx/")) path.insert(0, "sfx/");
      std::filesystem::path source_path = std::filesystem::path(test_support::kFixtureMount) / path;
      if (!std::filesystem::is_regular_file(source_path)) {
        for (const std::string_view extension : {".wav", ".mp3", ".ogg"}) {
          std::string candidate = path;
          if (candidate.ends_with('.')) candidate.pop_back();
          candidate += extension;
          source_path = std::filesystem::path(test_support::kFixtureMount) / candidate;
          if (std::filesystem::is_regular_file(source_path)) {
            path = std::move(candidate);
            break;
          }
        }
      }
      writeFile(second / path, readFile(source_path));
    }

    const auto obj_directory = std::filesystem::path("external/custom_dagger");
    const auto obj_fixture = std::filesystem::path("data/fixtures/model/obj/custom_dagger");
    writeFile(first / obj_directory / "custom_dagger.obj", readFile(obj_fixture / "custom_dagger.obj"));
    writeFile(first / obj_directory / "custom_dagger.mtl", readFile(obj_fixture / "custom_dagger.mtl"));
    writeFile(second / obj_directory / "custom_dagger_texture.png",
              readFile(obj_fixture / "custom_dagger_texture.png"));

    auto opened =
        pistoris::resource_io::ResourceMounts::open({.read_mounts = {first, second}, .write_mount = std::nullopt});
    REQUIRE(opened);
    pistoris::resource_io::Resources resources(std::move(*opened));

    auto masked = resources.loadAnimation(animation_path.generic_string(), {}, {.mount_mask = 1});
    REQUIRE(masked);
    CHECK(
        std::ranges::all_of(masked->sounds(), [](const ArxSoundView& sound) { return sound.encoded_audio.size == 0; }));
    auto complete = resources.loadAnimation(animation_path.generic_string(), {}, {.mount_mask = 3});
    REQUIRE(complete);
    CHECK(std::ranges::all_of(complete->sounds(),
                              [](const ArxSoundView& sound) { return sound.encoded_audio.size > 0; }));

    auto external = resources.loadModel("external/custom_dagger/custom_dagger.obj");
    REQUIRE(external);
    REQUIRE(external->model.textureCount() > 0);
    CHECK(std::ranges::all_of(external->model.textures(),
                              [](const ArxTextureView& texture) { return texture.encoded_image.size == 0; }));
    writeFile(first / obj_directory / "custom_dagger_texture.png", readFile(obj_fixture / "custom_dagger_texture.png"));
    external = resources.loadModel("external/custom_dagger/custom_dagger.obj");
    REQUIRE(external);
    CHECK(std::ranges::all_of(external->model.textures(),
                              [](const ArxTextureView& texture) { return texture.encoded_image.size > 0; }));

    const auto absolute_directory = temporary.path() / "absolute-obj";
    const auto absolute_mtl = absolute_directory / "materials/custom_dagger.mtl";
    const auto absolute_texture = absolute_directory / "textures/custom_dagger_texture.png";
    std::vector<std::uint8_t> obj_bytes = readFile(obj_fixture / "custom_dagger.obj");
    std::string obj_text(reinterpret_cast<const char*>(obj_bytes.data()), obj_bytes.size());
    const std::string authored_mtl = "mtllib custom_dagger.mtl";
    const std::size_t mtl_reference = obj_text.find(authored_mtl);
    REQUIRE(mtl_reference != std::string::npos);
    obj_text.replace(mtl_reference, authored_mtl.size(), "mtllib " + absolute_mtl.generic_string());
    std::vector<std::uint8_t> mtl_bytes = readFile(obj_fixture / "custom_dagger.mtl");
    std::string mtl_text(reinterpret_cast<const char*>(mtl_bytes.data()), mtl_bytes.size());
    const std::string authored_texture = "custom_dagger_texture.png";
    const std::size_t texture_reference = mtl_text.find(authored_texture);
    REQUIRE(texture_reference != std::string::npos);
    mtl_text.replace(texture_reference, authored_texture.size(), absolute_texture.generic_string());
    const auto absolute_obj = absolute_directory / "custom_dagger.obj";
    writeFile(absolute_obj, obj_text);
    writeFile(absolute_mtl, mtl_text);
    writeFile(absolute_texture, readFile(obj_fixture / "custom_dagger_texture.png"));
    auto absolute = resources.loadModelFile(absolute_obj);
    const std::string absolute_error =
        absolute ? std::string{} : pistoris::resource_io::describeError(*absolute.error());
    INFO(absolute_error);
    REQUIRE(absolute);
    CHECK(std::ranges::all_of(absolute->model.textures(),
                              [](const ArxTextureView& texture) { return texture.encoded_image.size > 0; }));

    const auto authored_case_directory = temporary.path() / "authored-case";
    writeFile(authored_case_directory / "custom_dagger.obj", readFile(obj_fixture / "custom_dagger.obj"));
    std::vector<std::uint8_t> authored_case_mtl_bytes = readFile(obj_fixture / "custom_dagger.mtl");
    std::string authored_case_mtl(reinterpret_cast<const char*>(authored_case_mtl_bytes.data()),
                                  authored_case_mtl_bytes.size());
    const std::size_t authored_case_reference = authored_case_mtl.find(authored_texture);
    REQUIRE(authored_case_reference != std::string::npos);
    authored_case_mtl.replace(authored_case_reference, authored_texture.size(), "Custom_Dagger_Texture.PNG");
    writeFile(authored_case_directory / "custom_dagger.mtl", authored_case_mtl);
    writeFile(authored_case_directory / "custom_dagger_texture.png",
              readFile(obj_fixture / "custom_dagger_texture.png"));
    if (!std::filesystem::is_regular_file(authored_case_directory / "Custom_Dagger_Texture.PNG")) {
      auto case_mismatch = resources.loadModelFile(authored_case_directory / "custom_dagger.obj");
      REQUIRE(case_mismatch);
      CHECK(std::ranges::all_of(case_mismatch->model.textures(),
                                [](const ArxTextureView& texture) { return texture.encoded_image.size == 0; }));

      writeFile(authored_case_directory / "Custom_Dagger_Texture.PNG",
                readFile(obj_fixture / "custom_dagger_texture.png"));
      auto exact_case = resources.loadModelFile(authored_case_directory / "custom_dagger.obj");
      REQUIRE(exact_case);
      CHECK(std::ranges::all_of(exact_case->model.textures(),
                                [](const ArxTextureView& texture) { return texture.encoded_image.size > 0; }));
    }
  }

  TEST_CASE("FTS JSON carries loose Level identity and explicit output identity is authoritative") {
    const test_support::LevelFixture& fixture = test_support::fixtureCatalog().levels.front();
    auto native = pistoris::readFts(readFile(fixture.fts));
    REQUIRE(native);
    auto json = pistoris::toFtsJson(*native, 9, true);
    REQUIRE(json);

    TemporaryDirectory temporary;
    const auto source = temporary.path() / "source.fts.json";
    writeFile(source, *json);
    pistoris::resource_io::Resources resources;
    auto level = resources.loadLevelFile(source);
    REQUIRE(level);
    CHECK(level->resourcePath() == pistoris::paths::levelDlf(9));

    auto with_scene = resources.loadLevelFile(source, std::nullopt, fixture.dlf);
    REQUIRE(with_scene);
    CHECK(with_scene->resourcePath() == pistoris::paths::levelDlf(9));

    const auto inferred = temporary.path() / "inferred.fts.json";
    REQUIRE(resources.writeLevelFile(*level, inferred));
    const std::vector<std::uint8_t> inferred_bytes = readFile(inferred);
    auto inferred_json = pistoris::fromFtsJson(
        std::string_view(reinterpret_cast<const char*>(inferred_bytes.data()), inferred_bytes.size()));
    REQUIRE(inferred_json);
    CHECK(inferred_json->level == 9);

    auto loose = resources.loadLevelFile(fixture.fts);
    REQUIRE(loose);
    CHECK(loose->resourcePath().empty());
    const auto missing = resources.writeLevelFile(*loose, temporary.path() / "missing.fts.json");
    CHECK_FALSE(missing);
    CHECK(missing.code() == ARX_INVALID_OPTIONS);
    const auto explicit_path = temporary.path() / "explicit.fts.json";
    REQUIRE(resources.writeLevelFile(*loose, explicit_path, std::nullopt, std::nullopt, 7));
    const std::vector<std::uint8_t> explicit_bytes = readFile(explicit_path);
    auto explicit_json = pistoris::fromFtsJson(
        std::string_view(reinterpret_cast<const char*>(explicit_bytes.data()), explicit_bytes.size()));
    REQUIRE(explicit_json);
    CHECK(explicit_json->level == 7);

    const auto scene_primary = temporary.path() / "scene.dlf.json";
    writeFile(scene_primary, std::string_view("{}"));
    auto loaded_scene_primary = resources.loadLevelFile(scene_primary);
    CHECK_FALSE(loaded_scene_primary);
    CHECK(loaded_scene_primary.code() == ARX_INVALID_OPTIONS);
    auto written_scene_primary = resources.writeLevelFile(*level, scene_primary, std::nullopt, std::nullopt, 9);
    CHECK_FALSE(written_scene_primary);
    CHECK(written_scene_primary.code() == ARX_INVALID_OPTIONS);
  }

  TEST_CASE("Missing media sidecars stay empty and malformed sidecars fail the complete load") {
    const auto& catalog = test_support::fixtureCatalog();
    const auto model_fixture = std::ranges::find(catalog.models, "sword_00", &test_support::ModelFixture::name);
    REQUIRE(model_fixture != catalog.models.end());
    const auto animation_fixture =
        std::ranges::find(catalog.animations, "human_male_gathering", &test_support::AnimationFixture::name);
    REQUIRE(animation_fixture != catalog.animations.end());

    TemporaryDirectory temporary;
    writeFile(temporary.path() / model_fixture->ftl.lexically_relative(test_support::kFixtureMount),
              readFile(model_fixture->ftl));
    writeFile(temporary.path() / animation_fixture->tea.lexically_relative(test_support::kFixtureMount),
              readFile(animation_fixture->tea));

    auto opened =
        pistoris::resource_io::ResourceMounts::open({.read_mounts = {temporary.path()}, .write_mount = std::nullopt});
    REQUIRE(opened);
    pistoris::resource_io::Resources resources(std::move(*opened));

    pistoris::paths::ResourceSelector selector;
    REQUIRE(pistoris::paths::parseResourceSelector(model_fixture->selector, selector));
    auto model = resources.loadModel(selector);
    REQUIRE(model);
    REQUIRE(model->model.textureCount() > 0);
    CHECK(std::ranges::all_of(model->model.textures(),
                              [](const ArxTextureView& texture) { return texture.encoded_image.size == 0; }));

    auto native_model = pistoris::readFtl(readFile(model_fixture->ftl));
    REQUIRE(native_model);
    std::vector<std::string> texture_sources;
    auto imported_model = pistoris::Model::importNative(*native_model, &texture_sources);
    REQUIRE(imported_model);
    REQUIRE_FALSE(texture_sources.empty());
    const std::string texture_source = texture_sources.front();
    std::string texture_path = texture_source;
    std::ranges::replace(texture_path, '\\', '/');
    writeFile(temporary.path() / texture_path, "not an image");
    model = resources.loadModel(selector);
    REQUIRE_FALSE(model);
    CHECK(model.code() == ARX_IMAGE_BAD_DATA);
    REQUIRE(model.error());
    REQUIRE(model.error()->location());
    CHECK(model.error()->location()->resource_path == texture_source);
    CHECK(model.error()->location()->native_path == canonicalExistingPath(temporary.path()) / texture_path);

    REQUIRE(pistoris::paths::parseResourceSelector(animation_fixture->selector, selector));
    auto animation = resources.loadAnimation(selector);
    REQUIRE(animation);
    REQUIRE(animation->soundCount() > 0);
    CHECK(std::ranges::all_of(animation->sounds(),
                              [](const ArxSoundView& sound) { return sound.encoded_audio.size == 0; }));

    auto native_animation = pistoris::readTea(readFile(animation_fixture->tea));
    REQUIRE(native_animation);
    std::vector<pistoris::SoundSourceReference> sound_sources;
    auto imported_animation = pistoris::Animation::importNative(*native_animation, &sound_sources);
    REQUIRE(imported_animation);
    REQUIRE_FALSE(sound_sources.empty());
    std::string sound_path = sound_sources.front().path;
    std::ranges::replace(sound_path, '\\', '/');
    if (!sound_path.starts_with("sfx/")) sound_path.insert(0, "sfx/");
    writeFile(temporary.path() / sound_path, "not audio");
    animation = resources.loadAnimation(selector);
    REQUIRE_FALSE(animation);
    CHECK(animation.code() == ARX_AUDIO_BAD_DATA);
    REQUIRE(animation.error());
    REQUIRE(animation.error()->location());
    CHECK(animation.error()->location()->resource_path == sound_sources.front().path);
    CHECK(animation.error()->location()->native_path == canonicalExistingPath(temporary.path()) / sound_path);
  }

  TEST_CASE("Level loading uses external lighting before embedded lighting and retains DLF identity") {
    const test_support::LevelFixture& fixture = test_support::fixtureCatalog().levels.front();
    pistoris::paths::ResourceSelector selector;
    REQUIRE(pistoris::paths::parseResourceSelector(fixture.selector, selector));
    REQUIRE(selector.kind == ARX_RESOURCE_KIND_LEVEL);

    const std::vector<std::uint8_t> source_dlf = readFile(fixture.dlf);
    const std::vector<std::uint8_t> source_llf = readFile(fixture.llf);
    auto dlf = pistoris::readDlf(source_dlf);
    REQUIRE(dlf);
    auto llf = pistoris::readLlf(source_llf);
    REQUIRE(llf);
    llf->lights.push_back({});
    auto embedded = pistoris::writeDlf(dlf->dlf, {.embedded_lighting = &*llf, .signer = "resource-io-test"});
    REQUIRE(embedded);

    TemporaryDirectory temporary;
    const std::string fts_path = pistoris::paths::levelFts(selector.level);
    const std::string llf_path = pistoris::paths::levelLlf(selector.level);
    const std::string dlf_path = pistoris::paths::levelDlf(selector.level);
    writeFile(temporary.path() / fts_path, readFile(fixture.fts));
    writeFile(temporary.path() / dlf_path, *embedded);
    writeFile(temporary.path() / pistoris::paths::minimapOffsetsFile(), "malformed");

    auto opened =
        pistoris::resource_io::ResourceMounts::open({.read_mounts = {temporary.path()}, .write_mount = std::nullopt});
    REQUIRE(opened);
    pistoris::resource_io::Resources resources(std::move(*opened));

    auto embedded_level = resources.loadLevel(selector.level);
    REQUIRE(embedded_level);
    CHECK(embedded_level->resourcePath() == dlf_path);
    CHECK(embedded_level->lightCount() == llf->lights.size());

    pistoris::Llf external = *llf;
    external.lights.clear();
    auto external_bytes = pistoris::writeLlf(external);
    REQUIRE(external_bytes);
    writeFile(temporary.path() / llf_path, *external_bytes);

    auto external_level = resources.loadLevel(selector.level);
    REQUIRE(external_level);
    CHECK(external_level->resourcePath() == dlf_path);
    CHECK(external_level->lightCount() == 0);

    const auto loose = temporary.path() / "loose";
    writeFile(loose / "level.fts", readFile(fixture.fts));
    writeFile(loose / "level.dlf", *embedded);
    writeFile(loose / "level.llf", *external_bytes);
    auto geometry_document = resources.readDocumentFile(loose / "level.fts");
    auto scene_document = resources.readDocumentFile(loose / "level.dlf");
    REQUIRE(geometry_document);
    REQUIRE(scene_document);
    auto loose_level = resources.loadLevel(*geometry_document, nullptr, &*scene_document);
    REQUIRE(loose_level);
    CHECK(loose_level->lightCount() == 0);
    CHECK(loose_level->resourcePath() == dlf_path);
  }

  TEST_CASE("Mounted level minimaps apply level projection overrides without offset metadata") {
    const test_support::LevelFixture& fixture = test_support::fixtureCatalog().levels.front();
    pistoris::paths::ResourceSelector fixture_selector;
    REQUIRE(pistoris::paths::parseResourceSelector(fixture.selector, fixture_selector));
    REQUIRE(fixture_selector.kind == ARX_RESOURCE_KIND_LEVEL);

    constexpr std::uint32_t kLevel = 15;
    TemporaryDirectory temporary;
    writeFile(temporary.path() / pistoris::paths::levelFts(kLevel), readFile(fixture.fts));
    writeFile(temporary.path() / pistoris::paths::levelLlf(kLevel), readFile(fixture.llf));
    auto dlf = pistoris::readDlf(readFile(fixture.dlf));
    REQUIRE(dlf);
    const std::string scene_path = "graph/levels/level" + std::to_string(kLevel);
    REQUIRE(scene_path.size() < std::size(dlf->dlf.scene_path));
    std::ranges::fill(dlf->dlf.scene_path, '\0');
    std::ranges::copy(scene_path, std::begin(dlf->dlf.scene_path));
    auto dlf_bytes = pistoris::writeDlf(dlf->dlf, {.signer = "resource-io-test"});
    REQUIRE(dlf_bytes);
    writeFile(temporary.path() / pistoris::paths::levelDlf(kLevel), *dlf_bytes);
    const auto fixture_minimap = std::filesystem::path(test_support::kFixtureMount) /
                                 (pistoris::paths::levelMinimap(fixture_selector.level) + ".png");
    writeFile(temporary.path() / (pistoris::paths::levelMinimap(kLevel) + ".png"), readFile(fixture_minimap));

    auto opened =
        pistoris::resource_io::ResourceMounts::open({.read_mounts = {temporary.path()}, .write_mount = std::nullopt});
    REQUIRE(opened);
    pistoris::resource_io::Resources resources(std::move(*opened));
    auto level = resources.loadLevel(kLevel);
    const std::string level_error =
        level.error() ? pistoris::resource_io::describeError(*level.error()) : std::string{};
    INFO(level_error);
    REQUIRE(level);

    auto rendered = level->renderMinimap();
    REQUIRE(rendered);
    CHECK(rendered->projection_offset.x == doctest::Approx(2015.0f));
    CHECK(rendered->projection_offset.y == doctest::Approx(-217.0f));
  }

  TEST_CASE("Write mount is independent from read mounts") {
    TemporaryDirectory temporary;
    const auto write = temporary.path() / "prospective";
    pistoris::resource_io::MountValidationReport report;
    auto opened = pistoris::resource_io::ResourceMounts::open({.read_mounts = {}, .write_mount = write}, &report);
    REQUIRE(opened);
    CHECK(opened->readMounts().empty());
    REQUIRE(opened->writeMount());
    CHECK(*opened->writeMount() == canonicalExistingPath(temporary.path()) / "prospective");
    REQUIRE(report.messages.size() == 1);
    CHECK(report.messages[0].kind == pistoris::resource_io::MountValidationKind::kProspectiveWriteMount);

    const std::vector<std::uint8_t> bytes = {1, 2, 3};
    REQUIRE(opened->write("folder/file.bin", bytes));
    CHECK(std::filesystem::is_regular_file(write / "folder" / "file.bin"));
    auto unavailable = opened->read("folder/file.bin");
    CHECK_FALSE(unavailable);
    CHECK(unavailable.code() == ARX_RESOURCE_IO_NOT_FOUND);
  }

  TEST_CASE("Write mounts can be replaced and cleared without rebuilding reads") {
    TemporaryDirectory temporary;
    pistoris::resource_io::ResourceMounts mounts;
    REQUIRE(mounts.addReadMount(temporary.path()));

    pistoris::resource_io::MountValidationReport report;
    const auto prospective = temporary.path() / "prospective";
    REQUIRE(mounts.setWriteMount(prospective, &report));
    REQUIRE(mounts.writeMount());
    CHECK(*mounts.writeMount() == canonicalExistingPath(temporary.path()) / "prospective");
    REQUIRE(report.messages.size() == 1);
    CHECK(report.messages[0].kind == pistoris::resource_io::MountValidationKind::kProspectiveWriteMount);
    REQUIRE(mounts.readMounts().size() == 1);
    CHECK(mounts.readMounts()[0].id == 1);

    REQUIRE(mounts.setWriteMount(std::nullopt, &report));
    CHECK_FALSE(mounts.writeMount());
    CHECK(report.messages.empty());
    REQUIRE(mounts.readMounts().size() == 1);
    CHECK(mounts.readMounts()[0].id == 1);
  }

  TEST_CASE("Write paths preserve the native spelling of existing directories") {
    TemporaryDirectory temporary;
    const auto write = temporary.path() / "write";
    REQUIRE(std::filesystem::create_directories(write / "MixedCase"));

    auto opened = pistoris::resource_io::ResourceMounts::open({.read_mounts = {}, .write_mount = write});
    REQUIRE(opened);
    auto resolved = opened->resolveWritePath("mixedcase/output.bin");
    REQUIRE(resolved);
    CHECK(*resolved == canonicalExistingPath(write / "MixedCase") / "output.bin");

    const std::vector<std::uint8_t> bytes = {1, 2, 3};
    REQUIRE(opened->write("mixedcase/output.bin", bytes));
    CHECK(std::filesystem::is_regular_file(write / "MixedCase" / "output.bin"));
  }

  TEST_CASE("Symbolic links do not escape mount roots") {
    TemporaryDirectory temporary;
    const auto read = temporary.path() / "read";
    const auto write = temporary.path() / "write";
    const auto external = temporary.path() / "external";
    REQUIRE(std::filesystem::create_directories(read));
    REQUIRE(std::filesystem::create_directories(write));
    writeFile(external / "outside.ftl", "outside");

    std::error_code error;
    std::filesystem::create_directory_symlink(external, read / "linked", error);
    if (error) return;
    std::filesystem::create_directory_symlink(external, write / "linked", error);
    if (error) return;

    auto opened = pistoris::resource_io::ResourceMounts::open({.read_mounts = {read}, .write_mount = write});
    REQUIRE(opened);
    auto unavailable = opened->read("linked/outside.ftl");
    CHECK_FALSE(unavailable);
    CHECK(unavailable.code() == ARX_RESOURCE_IO_NOT_FOUND);
    auto unsafe_write = opened->resolveWritePath("linked/output.ftl");
    CHECK_FALSE(unsafe_write);
    CHECK(unsafe_write.code() == ARX_RESOURCE_IO_INVALID_PATH);
  }
}
