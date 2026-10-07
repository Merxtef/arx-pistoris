// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#include "resources/input.h"

#include "arx_pistoris/paths.hpp"
#include "arx_pistoris/paths/types.h"
#include "arx_pistoris/resource_io/document.hpp"
#include "arx_pistoris/resource_io/location.hpp"
#include "arx_pistoris/resource_io/status.h"

#include "console/diagnostics.h"
#include "formats/classification.h"
#include "formats/format.h"
#include "io/native_path.h"
#include "io/path_location.h"
#include "io/service.h"
#include "resources/layout.h"
#include "resources/selector.h"

#include <cstddef>
#include <filesystem>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace cli {
namespace {

bool validateClassification(const ClassifiedPath& input) {
  Format extension_format = formatFromPath(input.path.c_str());
  const FileFacts& facts = input.facts;
  switch (extension_format) {
    case Format::kFtl:
    case Format::kTea:
    case Format::kAmb:
    case Format::kCin:
    case Format::kGlb:
    case Format::kFts:
    case Format::kLlf:
    case Format::kDlf:
      if (facts.kind == PayloadKind::kUnknown) {
        diagnostic(DiagnosticCode::kClassificationFailed,
                   "%s classification failed: %s",
                   formatName(extension_format),
                   input.path.c_str());
        return false;
      }
      break;
    default:
      break;
  }
  return true;
}

bool appendClassified(pistoris::resource_io::ResourceDocument document, std::string path, PathLocation location,
                      std::size_t positional_index, ArxResourceKind resource_kind,
                      std::optional<ResourceLayout> explicit_layout, std::vector<ClassifiedPath>& out) {
  ClassifiedPath input{.document = std::move(document),
                       .path = std::move(path),
                       .location = std::move(location),
                       .positional_index = positional_index,
                       .resource_kind = resource_kind};
  input.facts = classifyInput(input.document.data, input.path);
  input.layout = explicit_layout.value_or(primaryResourceLayout(input.facts.format, input.location.address));
  if (!validateClassification(input)) return false;
  out.push_back(std::move(input));
  return true;
}

bool appendDocument(pistoris::resource_io::ResourceDocument document, std::size_t positional_index,
                    std::vector<ClassifiedPath>& out) {
  const ResourceLayout layout =
      document.layout == pistoris::resource_io::ResourceLayout::kGame ? ResourceLayout::kGame : ResourceLayout::kLoose;
  const PathAddress address = document.address == pistoris::resource_io::ResourceAddress::kLogical
                                  ? PathAddress::kMountRelative
                                  : PathAddress::kAbsolute;
  std::string path = document.logical_path.empty() ? document.requested_path : document.logical_path;
  PathLocation location{.path = std::move(path), .address = address};
  const std::string display_path = location.path;
  const ArxResourceKind selector_kind = document.selector_kind;
  return appendClassified(
      std::move(document), display_path, std::move(location), positional_index, selector_kind, layout, out);
}

template <class Result>
bool documentReadFailure(std::string_view display_path, const Result& result) {
  if (result.code() == ARX_RESOURCE_IO_NOT_FOUND) {
    diagnostic(DiagnosticCode::kResourceNotFound,
               "Input not found: %.*s",
               static_cast<int>(display_path.size()),
               display_path.data());
    return false;
  }
  if (result.code() == ARX_RESOURCE_IO_INVALID_PATH) {
    diagnostic(DiagnosticCode::kResourcePathInvalid,
               "Input path is invalid: %.*s",
               static_cast<int>(display_path.size()),
               display_path.data());
    return false;
  }
  const auto* error = result.error();
  const std::string detail = error ? pistoris::resource_io::describeError(*error)
                                   : std::string(pistoris::resource_io::errorString(result.code()));
  diagnostic(DiagnosticCode::kResourceReadFailed,
             "Cannot read input '%.*s': %s",
             static_cast<int>(display_path.size()),
             display_path.data(),
             detail.c_str());
  return false;
}

bool loadRawInput(const char* argument, std::size_t positional_index, IoService& io, std::vector<ClassifiedPath>& out) {
  PathLocation location;
  std::string error;
  if (!io.resolvePathLocation(argument, location, error)) {
    diagnostic(DiagnosticCode::kIoPathInvalid, "Invalid input path '%s': %s", argument, error.c_str());
    return false;
  }
  if (location.address == PathAddress::kMountRelative) {
    auto document = io.resources().readDocument(location.path);
    if (!document) return documentReadFailure(argument, document);
    return appendDocument(std::move(*document), positional_index, out);
  }

  std::filesystem::path native;
  if (!io_detail::pathFromUtf8(location.path, native, error)) {
    diagnostic(DiagnosticCode::kIoPathInvalid, "Invalid input path '%s': %s", argument, error.c_str());
    return false;
  }
  auto document = io.resources().readDocumentFile(native);
  if (!document) return documentReadFailure(argument, document);
  return appendDocument(std::move(*document), positional_index, out);
}

bool loadSelectedResource(const ResourceSelector& selector, std::size_t positional_index, IoService& io,
                          std::vector<ClassifiedPath>& out) {
  pistoris::paths::ResourceSelector parsed{.kind = selector.kind,
                                           .model_type = selector.model_type,
                                           .animation_type = selector.animation_type,
                                           .name = selector.name,
                                           .tweak = selector.tweak,
                                           .level = selector.level};
  auto document = io.resources().readDocument(parsed);
  if (!document) return documentReadFailure(selector.logical_path, document);
  return appendDocument(std::move(*document), positional_index, out);
}

}  // namespace

bool loadClassifiedInputs(std::span<const char* const> arguments, IoService& io, std::vector<ClassifiedPath>& out) {
  out.clear();
  for (std::size_t index = 0; index < arguments.size(); ++index) {
    ResourceSelector selector;
    std::string error;
    SelectorParseStatus status = parseResourceSelector(arguments[index], selector, error);
    if (status == SelectorParseStatus::kInvalid) {
      diagnostic(DiagnosticCode::kResourceSelectorInvalid,
                 "Invalid input resource selector '%s': %s",
                 arguments[index],
                 error.c_str());
      return false;
    }
    if (status == SelectorParseStatus::kNotSelector) {
      if (!loadRawInput(arguments[index], index, io, out)) return false;
      continue;
    }

    if (!loadSelectedResource(selector, index, io, out)) return false;
  }
  return true;
}

}  // namespace cli
