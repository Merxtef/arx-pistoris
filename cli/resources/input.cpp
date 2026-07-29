// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#include "resources/input.h"

#include "arx_pistoris/paths/types.h"

#include "console/diagnostics.h"
#include "formats/classification.h"
#include "formats/format.h"
#include "io/path_location.h"
#include "io/service.h"
#include "resources/selector.h"

#include <cstddef>
#include <cstdint>
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

bool appendClassified(std::string path, PathLocation location, std::vector<std::uint8_t> buffer,
                      std::size_t positional_index, ArxResourceKind resource_kind, std::vector<ClassifiedPath>& out) {
  ClassifiedPath input{.path = std::move(path),
                       .location = std::move(location),
                       .buffer = std::move(buffer),
                       .positional_index = positional_index,
                       .resource_kind = resource_kind};
  input.facts = classifyInput(input.buffer, input.path.c_str());
  if (!validateClassification(input)) return false;
  out.push_back(std::move(input));
  return true;
}

PathLocation resourceLocation(std::string path) {
  return {.path = std::move(path), .address = PathAddress::kMountRelative};
}

bool readRequiredResource(IoService& io, std::string_view path, std::vector<std::uint8_t>& out) {
  ResourceReadResult result = io.readResource(path, out);
  switch (result) {
    case ResourceReadResult::kSuccess:
      return true;
    case ResourceReadResult::kNotFound:
      diagnostic(DiagnosticCode::kResourceNotFound,
                 "Mounted resource not found: %.*s",
                 static_cast<int>(path.size()),
                 path.data());
      return false;
    case ResourceReadResult::kInvalidPath:
      diagnostic(DiagnosticCode::kResourceSelectorInvalid,
                 "Invalid mounted resource path: %.*s",
                 static_cast<int>(path.size()),
                 path.data());
      return false;
    case ResourceReadResult::kReadFailed:
      diagnostic(DiagnosticCode::kResourceReadFailed,
                 "Mounted resource cannot be read: %.*s",
                 static_cast<int>(path.size()),
                 path.data());
      return false;
  }
  return false;
}

bool loadRawInput(const char* argument, std::size_t positional_index, IoService& io, std::vector<ClassifiedPath>& out) {
  PathLocation location;
  std::string error;
  if (!io.resolvePathLocation(argument, location, error)) {
    diagnostic(DiagnosticCode::kResourceReadFailed, "Invalid input path '%s': %s", argument, error.c_str());
    return false;
  }
  std::vector<std::uint8_t> buffer;
  ResourceReadResult result = io.readPath(location, buffer);
  if (result != ResourceReadResult::kSuccess) {
    diagnostic(result == ResourceReadResult::kNotFound ? DiagnosticCode::kResourceNotFound
                                                       : DiagnosticCode::kResourceReadFailed,
               result == ResourceReadResult::kNotFound ? "Input file not found: %s" : "Input file cannot be read: %s",
               argument);
    return false;
  }
  std::string path = location.path;
  return appendClassified(
      std::move(path), std::move(location), std::move(buffer), positional_index, ARX_RESOURCE_KIND_NONE, out);
}

bool loadSelectedResource(const ResourceSelector& selector, std::size_t positional_index, IoService& io,
                          std::vector<ClassifiedPath>& out) {
  std::vector<std::uint8_t> buffer;
  if (!readRequiredResource(io, selector.logical_path, buffer)) return false;
  return appendClassified(selector.logical_path,
                          resourceLocation(selector.logical_path),
                          std::move(buffer),
                          positional_index,
                          selector.kind,
                          out);
}

}  // namespace

bool appendClassifiedInput(std::string path, PathLocation location, std::vector<std::uint8_t> buffer,
                           std::size_t positional_index, ArxResourceKind resource_kind,
                           std::vector<ClassifiedPath>& out) {
  return appendClassified(
      std::move(path), std::move(location), std::move(buffer), positional_index, resource_kind, out);
}

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
