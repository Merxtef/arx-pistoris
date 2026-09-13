// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#include "resources/level_image_io.h"

#include "arx_pistoris/base/image.h"
#include "arx_pistoris/base/math.hpp"
#include "arx_pistoris/base/status.h"
#include "arx_pistoris/level.hpp"
#include "arx_pistoris/level/images.hpp"
#include "arx_pistoris/paths.hpp"
#include "arx_pistoris/paths/types.h"
#include "arx_pistoris/runtime.hpp"
#include "arx_pistoris/runtime/types.h"

#include "base/resource_path.h"
#include "console/diagnostics.h"
#include "console/logging.h"
#include "formats/classification.h"
#include "formats/format.h"
#include "io/path_location.h"
#include "io/service.h"
#include "media/encoded.h"
#include "resources/layout.h"
#include "resources/resource_output.h"
#include "resources/selector.h"

#include <algorithm>
#include <array>
#include <charconv>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>
#include <vector>

namespace cli::level {
namespace {

constexpr std::uint32_t kMiniOffsetEntryCount = 29;

struct MinimapCandidate {
  PathLocation location;
  pistoris::ArxVector2 offset{};
  std::size_t extension_priority = 0;
  bool has_offset = false;
};

bool siblingStem(const PathLocation& primary, std::string_view suffix, IoService& io, PathLocation& out) {
  PathLocation parent;
  std::string error;
  const std::string stem = resourceStem(primary.path) + std::string(suffix);
  if (!io.parentPathLocation(primary, parent, error) || !io.appendPathLocation(parent, stem, out, error)) {
    diagnostic(DiagnosticCode::kIoPathInvalid, "Cannot resolve Level image sidecar path: %s", error.c_str());
    return false;
  }
  return true;
}

LevelImageLocation siblingInput(const PathLocation& primary, std::string_view suffix, IoService& io, bool& ok) {
  LevelImageLocation result;
  PathLocation stem;
  ok = siblingStem(primary, suffix, io, stem);
  if (!ok) return result;
  std::string error;
  if (!io.parentPathLocation(stem, result.base, error)) {
    diagnostic(DiagnosticCode::kIoPathInvalid, "Cannot resolve Level image sidecar folder: %s", error.c_str());
    ok = false;
    return result;
  }
  result.stem = resourceStem(stem.path);
  result.enabled = true;
  return result;
}

LevelImageLocation gameInput(std::string stem) { return {.enabled = true, .base = {}, .stem = std::move(stem)}; }

void applyMiniOffsetOverride(std::uint32_t level, pistoris::ArxVector2& offset) noexcept {
  switch (level) {
    case 0:
      offset = {0.0f, -0.5f};
      break;
    case 1:
      offset = {};
      break;
    case 14:
      offset = {130.0f, 0.0f};
      break;
    case 15:
      offset = {31.0f, -3.5f};
      break;
    default:
      break;
  }
}

bool parseFiniteFloat(std::string_view text, float& out) noexcept {
  if (text.empty()) return false;
  const char* begin = text.data();
  const char* end = begin + text.size();
  const auto [parsed, error] = std::from_chars(begin, end, out, std::chars_format::general);
  return error == std::errc{} && parsed == end && std::isfinite(out);
}

bool nextToken(std::string_view line, std::size_t& offset, std::string_view& out) noexcept {
  while (offset < line.size() && (line[offset] == ' ' || line[offset] == '\t' || line[offset] == '\r')) ++offset;
  const std::size_t begin = offset;
  while (offset < line.size() && line[offset] != ' ' && line[offset] != '\t' && line[offset] != '\r') ++offset;
  out = line.substr(begin, offset - begin);
  return !out.empty();
}

bool parseOffsetLine(std::string_view line, float& x, float& y) noexcept {
  std::size_t offset = 0;
  std::string_view label;
  std::string_view x_token;
  std::string_view y_token;
  if (!nextToken(line, offset, label) || !nextToken(line, offset, x_token) || !nextToken(line, offset, y_token))
    return false;
  return parseFiniteFloat(x_token, x) && parseFiniteFloat(y_token, y);
}

void loadMinimapProjectionOffset(IoService& io, std::uint32_t level, pistoris::ArxVector2& out) {
  pistoris::ArxVector2 mini_offset{};
  if (level < kMiniOffsetEntryCount) {
    std::vector<std::uint8_t> bytes;
    const ResourceReadResult read = io.readResource(pistoris::paths::minimapOffsetsFile(), bytes);
    if (read == ResourceReadResult::kSuccess) {
      std::string_view input(reinterpret_cast<const char*>(bytes.data()), bytes.size());
      float x = 0.0f;
      float y = 0.0f;
      for (std::uint32_t area = 0; area <= level; ++area) {
        const std::size_t newline = input.find('\n');
        const std::string_view line = input.substr(0, newline);
        if (!parseOffsetLine(line, x, y)) {
          log(ARX_LOG_WARN, "Level minimap offsets are malformed; missing values use zero");
          break;
        }
        if (area == level) {
          mini_offset = {x, y};
          break;
        }
        if (newline == std::string_view::npos) {
          input = {};
        } else {
          input.remove_prefix(newline + 1);
        }
      }
    } else if (read != ResourceReadResult::kNotFound) {
      log(ARX_LOG_WARN, "Level minimap offsets could not be read; using zero where no engine override exists");
    }
  }
  applyMiniOffsetOverride(level, mini_offset);
  if (pistoris::level_images::projectionOffsetFromMiniOffset(mini_offset, out) != ARX_OK) {
    out = {};
    log(ARX_LOG_WARN, "Level %u minimap offset cannot be represented; using zero", level);
  }
}

bool parseProjectionOffset(std::string_view suffix, pistoris::ArxVector2& out) noexcept {
  constexpr std::string_view kPrefix = "[offset_";
  if (!suffix.starts_with(kPrefix) || !suffix.ends_with(']')) return false;
  suffix.remove_prefix(kPrefix.size());
  suffix.remove_suffix(1);
  const std::size_t separator = suffix.find('_');
  if (separator == std::string_view::npos || suffix.find('_', separator + 1) != std::string_view::npos) return false;
  return parseFiniteFloat(suffix.substr(0, separator), out.x) && parseFiniteFloat(suffix.substr(separator + 1), out.y);
}

bool candidateFromPath(const PathLocation& location, std::string_view expected_stem, MinimapCandidate& out,
                       bool& malformed) {
  malformed = false;
  const std::string_view filename = resourceFilename(location.path);
  const std::string key = resourcePathKey(filename);
  std::size_t extension_priority = 0;
  std::string_view extension;
  const std::span<const std::string_view> extensions = media::imageLookupExtensions();
  for (; extension_priority < extensions.size(); ++extension_priority) {
    const std::string_view candidate = extensions[extension_priority];
    if (key.ends_with(candidate)) {
      extension = candidate;
      break;
    }
  }
  if (extension.empty()) return false;
  const std::string_view stem = filename.substr(0, filename.size() - extension.size());
  const std::string stem_key = resourcePathKey(stem);
  const std::string expected_key = resourcePathKey(expected_stem);
  if (stem_key == expected_key) {
    out = {.location = location, .extension_priority = extension_priority};
    return true;
  }
  if (!stem_key.starts_with(expected_key)) return false;
  const std::string_view suffix = stem.substr(expected_stem.size());
  pistoris::ArxVector2 offset{};
  if (!parseProjectionOffset(suffix, offset)) {
    malformed = resourcePathKey(suffix).starts_with("[offset_");
    return false;
  }
  out = {.location = location, .offset = offset, .extension_priority = extension_priority, .has_offset = true};
  return true;
}

void readLooseMinimap(IoService& io, const LevelImageLocation& location, LoadedMinimap& out) {
  out = {};
  if (!location.enabled) return;
  std::vector<PathLocation> paths;
  const ResourceEnumerationResult enumeration = io.enumerateFiles(location.base, paths);
  if (enumeration != ResourceEnumerationResult::kSuccess) {
    log(ARX_LOG_WARN, "Level minimap folder could not be inspected; minimap was skipped");
    return;
  }
  std::vector<MinimapCandidate> candidates;
  candidates.reserve(paths.size());
  for (const PathLocation& path : paths) {
    MinimapCandidate candidate;
    bool malformed = false;
    if (candidateFromPath(path, location.stem, candidate, malformed)) {
      candidates.push_back(std::move(candidate));
    } else if (malformed) {
      log(ARX_LOG_WARN, "Malformed Level minimap offset sidecar was ignored: %s", path.path.c_str());
    }
  }
  std::ranges::sort(candidates, [](const MinimapCandidate& lhs, const MinimapCandidate& rhs) {
    if (lhs.has_offset != rhs.has_offset) return !lhs.has_offset;
    if (lhs.extension_priority != rhs.extension_priority) return lhs.extension_priority < rhs.extension_priority;
    if (resourcePathLess(lhs.location.path, rhs.location.path)) return true;
    if (resourcePathLess(rhs.location.path, lhs.location.path)) return false;
    return lhs.location.path < rhs.location.path;
  });
  if (candidates.empty()) {
    log(ARX_LOG_INFO, "Level minimap was not found and was skipped");
    return;
  }
  for (const MinimapCandidate& candidate : candidates) {
    std::vector<std::uint8_t> encoded;
    const ResourceReadResult read = io.readPath(candidate.location, encoded);
    if (read != ResourceReadResult::kSuccess) {
      log(ARX_LOG_WARN,
          "Level minimap sidecar could not be read; trying next candidate: %s",
          candidate.location.path.c_str());
      continue;
    }
    if (media::prepareImage(std::move(encoded), out.image) != ARX_OK) {
      out.image = {};
      log(ARX_LOG_WARN,
          "Level minimap sidecar could not be decoded; trying next candidate: %s",
          candidate.location.path.c_str());
      continue;
    }
    out.projection_offset = candidate.offset;
    if (candidates.size() > 1)
      log(ARX_LOG_WARN, "Multiple Level minimap sidecars found; using %s", candidate.location.path.c_str());
    return;
  }
}

void readOptionalImage(IoService& io, const LevelImageLocation& location, std::string_view description,
                       media::PreparedImage& out) {
  out = {};
  if (!location.enabled) return;
  std::vector<std::uint8_t> encoded;
  const ResourceReadResult result =
      io.readImage(location.base, location.stem, ImageLookupMode::kGamePriority, encoded, nullptr, nullptr);
  if (result == ResourceReadResult::kSuccess) {
    if (media::prepareImage(std::move(encoded), out) == ARX_OK) return;
    out = {};
    log(ARX_LOG_WARN,
        "Level %.*s could not be decoded and was skipped",
        static_cast<int>(description.size()),
        description.data());
    return;
  }
  out = {};
  if (result == ResourceReadResult::kNotFound) {
    log(ARX_LOG_INFO,
        "Level %.*s was not found and was skipped",
        static_cast<int>(description.size()),
        description.data());
  } else {
    log(ARX_LOG_WARN,
        "Level %.*s could not be read and was skipped",
        static_cast<int>(description.size()),
        description.data());
  }
}

bool equalOffset(const pistoris::ArxVector2& first, const pistoris::ArxVector2& second) noexcept {
  return first.x == second.x && first.y == second.y;
}

void appendFloat(std::string& out, float value) {
  if (value == 0.0f) value = 0.0f;
  std::array<char, 32> buffer{};
  const auto [end, error] =
      std::to_chars(buffer.data(), buffer.data() + buffer.size(), value, std::chars_format::general);
  if (error == std::errc{}) out.append(buffer.data(), end);
}

PathLocation projectionOffsetMinimapStem(PathLocation stem, const pistoris::ArxVector2& offset) {
  if (offset.x == 0.0f && offset.y == 0.0f) return stem;
  stem.path += "[offset_";
  appendFloat(stem.path, offset.x);
  stem.path.push_back('_');
  appendFloat(stem.path, offset.y);
  stem.path.push_back(']');
  return stem;
}

bool addRawImage(ResourceOutputPlan& plan, PathLocation stem, const media::PreparedImage& image, std::size_t asset) {
  if (image.encoded.empty()) return false;
  const std::string_view extension = media::imageExtension(image.info.format);
  if (extension.empty()) return false;
  stem.path += extension;
  plan.add(ResourceFileKind::kImage, std::move(stem), image.encoded.data(), image.encoded.size(), asset);
  return true;
}

void addPng(ResourceOutputPlan& plan, PathLocation stem, const std::vector<std::uint8_t>& encoded, std::size_t asset) {
  if (encoded.empty()) return;
  stem.path += ".png";
  plan.add(ResourceFileKind::kImage, std::move(stem), encoded.data(), encoded.size(), asset);
}

bool renderFailure(std::string_view description, ArxReturnCode rc) {
  diagnostic(DiagnosticCode::kLevelOutputFailed,
             "%.*s failed: %s (code %d)",
             static_cast<int>(description.size()),
             description.data(),
             pistoris::errorString(rc),
             static_cast<int>(rc));
  return false;
}

}  // namespace

bool resolveLevelImageInput(const ClassifiedPath& input, IoService& io, LevelImageInput& out) {
  LevelImageInput resolved;
  if (input.facts.format == Format::kDlf && input.location.address == PathAddress::kMountRelative &&
      pistoris::paths::levelFromDlf(input.path, resolved.level)) {
    resolved.layout = ResourceLayout::kGame;
    resolved.minimap = gameInput(pistoris::paths::levelMinimap(resolved.level));
    resolved.loading_screen = gameInput(pistoris::paths::levelLoadingScreen(resolved.level));
    out = std::move(resolved);
    return true;
  }
  bool ok = true;
  if (input.facts.format != Format::kGlb) {
    resolved.minimap = siblingInput(input.location, "[map]", io, ok);
    if (!ok) return false;
  }
  resolved.loading_screen = siblingInput(input.location, "[loading]", io, ok);
  if (!ok) return false;
  out = std::move(resolved);
  return true;
}

bool resolveLevelImageOutput(const OutputTarget& output, IoService& io, LevelImageOutput& out) {
  LevelImageOutput resolved;
  bool game_level = false;
  if (output.format == Format::kDlf && output.address == PathAddress::kMountRelative) {
    if (output.selector.kind == ARX_RESOURCE_KIND_LEVEL) {
      resolved.level = output.selector.level;
      game_level = true;
    } else {
      game_level = pistoris::paths::levelFromDlf(output.path, resolved.level);
    }
  }
  if (game_level) {
    resolved.layout = ResourceLayout::kGame;
    resolved.minimap_enabled = true;
    resolved.loading_screen_enabled = true;
    resolved.minimap_stem = {.path = pistoris::paths::levelMinimap(resolved.level),
                             .address = PathAddress::kMountRelative};
    resolved.loading_screen_stem = {.path = pistoris::paths::levelLoadingScreen(resolved.level),
                                    .address = PathAddress::kMountRelative};
    loadMinimapProjectionOffset(io, resolved.level, resolved.projection_offset);
    out = std::move(resolved);
    return true;
  }
  resolved.minimap_enabled = output.format != Format::kGlb;
  resolved.loading_screen_enabled = true;
  if (resolved.minimap_enabled && !siblingStem(output, "[map]", io, resolved.minimap_stem)) return false;
  if (!siblingStem(output, "[loading]", io, resolved.loading_screen_stem)) return false;
  out = std::move(resolved);
  return true;
}

void loadLevelImages(IoService& io, const LevelImageInput& input, LoadedLevelImages& out) {
  LoadedLevelImages loaded;
  if (input.layout == ResourceLayout::kGame) {
    readOptionalImage(io, input.minimap, "minimap", loaded.minimap.image);
    if (!loaded.minimap.image.encoded.empty())
      loadMinimapProjectionOffset(io, input.level, loaded.minimap.projection_offset);
  } else {
    readLooseMinimap(io, input.minimap, loaded.minimap);
  }
  readOptionalImage(io, input.loading_screen, "loading screen", loaded.loading_screen);
  out = std::move(loaded);
}

void applyLevelImages(pistoris::Level& level, LoadedLevelImages& loaded) {
  if (!loaded.minimap.image.encoded.empty()) {
    const ArxReturnCode rc = level.setMinimapFromProjection(
        {loaded.minimap.image.encoded.data(), loaded.minimap.image.encoded.size()}, loaded.minimap.projection_offset);
    if (rc != ARX_OK)
      log(ARX_LOG_WARN,
          "Level minimap is invalid and was skipped: %s (code %d)",
          pistoris::errorString(rc),
          static_cast<int>(rc));
  }
  if (!loaded.loading_screen.encoded.empty()) {
    const ArxReturnCode rc =
        level.setLoadingScreen({loaded.loading_screen.encoded.data(), loaded.loading_screen.encoded.size()});
    if (rc != ARX_OK)
      log(ARX_LOG_WARN,
          "Level loading screen is invalid and was skipped: %s (code %d)",
          pistoris::errorString(rc),
          static_cast<int>(rc));
  }
  loaded = {};
}

bool addDirectLevelImageOutputs(ResourceOutputPlan& plan, const LevelImageInput& input, const LevelImageOutput& output,
                                const LoadedLevelImages& images, GeneratedLevelImages& generated, std::size_t asset) {
  if (output.minimap_enabled && !images.minimap.image.encoded.empty()) {
    bool added = false;
    if (output.layout == ResourceLayout::kLoose) {
      added = addRawImage(plan,
                          projectionOffsetMinimapStem(output.minimap_stem, images.minimap.projection_offset),
                          images.minimap.image,
                          asset);
    } else if (input.layout == ResourceLayout::kGame &&
               equalOffset(images.minimap.projection_offset, output.projection_offset)) {
      added = addRawImage(plan, output.minimap_stem, images.minimap.image, asset);
    } else {
      const ArxReturnCode rc =
          pistoris::level_images::reprojectGameMinimapPng(images.minimap.image.encoded,
                                                          {.source_projection_offset = images.minimap.projection_offset,
                                                           .target_projection_offset = output.projection_offset,
                                                           .border_color = output.minimap_border_color},
                                                          generated.minimap);
      if (rc != ARX_OK) return renderFailure("Level minimap reprojection", rc);
      addPng(plan, output.minimap_stem, generated.minimap, asset);
      added = !generated.minimap.empty();
    }
    if (output.layout == ResourceLayout::kGame && output.level > 31 && added)
      log(ARX_LOG_WARN, "Level %u minimap may not be displayed by the game", output.level);
  }
  if (output.loading_screen_enabled && !images.loading_screen.encoded.empty()) {
    const bool source_is_png = images.loading_screen.info.format == ARX_IMAGE_FORMAT_PNG;
    const bool same_game_layout = input.layout == ResourceLayout::kGame && output.layout == ResourceLayout::kGame &&
                                  ((input.level == 10) == (output.level == 10));
    if (same_game_layout) {
      addRawImage(plan, output.loading_screen_stem, images.loading_screen, asset);
    } else if (output.layout == ResourceLayout::kLoose && source_is_png) {
      addPng(plan, output.loading_screen_stem, images.loading_screen.encoded, asset);
    } else {
      const auto layout = output.layout == ResourceLayout::kLoose
                              ? pistoris::level_images::LoadingScreenLayout::kOriginal
                              : (output.level == 10 ? pistoris::level_images::LoadingScreenLayout::kFullscreen
                                                    : pistoris::level_images::LoadingScreenLayout::kNormal);
      const ArxReturnCode rc = pistoris::level_images::renderLoadingScreenPng(
          images.loading_screen.encoded, layout, generated.loading_screen);
      if (rc != ARX_OK) return renderFailure("Level loading screen rendering", rc);
      addPng(plan, output.loading_screen_stem, generated.loading_screen, asset);
    }
  }
  return true;
}

bool addIntermediateLevelImageOutputs(ResourceOutputPlan& plan, const LevelImageOutput& output,
                                      const pistoris::Level& level, GeneratedLevelImages& generated,
                                      std::size_t asset) {
  if (output.minimap_enabled && level.minimap().encoded_image.size != 0) {
    pistoris::ArxVector2 offset = output.projection_offset;
    ArxReturnCode rc = ARX_OK;
    if (output.layout == ResourceLayout::kLoose) {
      rc = level.renderCompactMinimapPng(offset, generated.minimap);
    } else {
      rc = level.renderGameMinimapPng({.projection_offset = offset, .border_color = output.minimap_border_color},
                                      generated.minimap);
    }
    if (rc != ARX_OK) return renderFailure("Level minimap rendering", rc);
    const PathLocation stem = output.layout == ResourceLayout::kLoose
                                  ? projectionOffsetMinimapStem(output.minimap_stem, offset)
                                  : output.minimap_stem;
    addPng(plan, stem, generated.minimap, asset);
    if (output.layout == ResourceLayout::kGame && output.level > 31 && !generated.minimap.empty())
      log(ARX_LOG_WARN, "Level %u minimap may not be displayed by the game", output.level);
  }
  if (output.loading_screen_enabled && level.loadingScreen().size != 0) {
    const ArxReturnCode rc =
        output.layout == ResourceLayout::kLoose
            ? level.transcodeLoadingScreenPng(generated.loading_screen)
            : (output.level == 10 ? level.renderFullscreenLoadingScreenPng(generated.loading_screen)
                                  : level.renderLoadingScreenPng(generated.loading_screen));
    if (rc != ARX_OK) return renderFailure("Level loading screen rendering", rc);
    addPng(plan, output.loading_screen_stem, generated.loading_screen, asset);
  }
  return true;
}

}  // namespace cli::level
