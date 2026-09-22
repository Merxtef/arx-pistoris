// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#include "resources/model_input.h"

#include "arx_pistoris/base/status.h"
#include "arx_pistoris/model.hpp"
#include "arx_pistoris/native.hpp"
#include "arx_pistoris/native/text.hpp"
#include "arx_pistoris/paths.hpp"
#include "arx_pistoris/runtime.hpp"

#include "base/bytes.h"
#include "console/diagnostics.h"
#include "formats/classification.h"
#include "formats/format.h"

#include <span>
#include <string_view>
#include <utility>
#include <vector>

namespace cli {
namespace {

bool conversionFailure(DiagnosticCode code, std::string_view description, const ClassifiedPath& input,
                       ArxReturnCode rc) {
  diagnostic(code,
             "%.*s input failed (%s): %s (code %d)",
             static_cast<int>(description.size()),
             description.data(),
             input.path.c_str(),
             pistoris::errorString(rc),
             static_cast<int>(rc));
  return false;
}

bool applyResourcePath(const ClassifiedPath& input, std::string_view description, DiagnosticCode failure_code,
                       pistoris::Model& model) {
  pistoris::paths::ModelPathView parsed;
  if (!pistoris::paths::modelFromFtl(input.path, parsed)) return true;
  const ArxReturnCode rc = model.setResourcePath(input.path);
  return rc == ARX_OK || conversionFailure(failure_code, description, input, rc);
}

}  // namespace

bool isModelInput(FileFacts facts) noexcept {
  switch (facts.format) {
    case Format::kFtl:
    case Format::kObj:
    case Format::kGlb:
      return facts.kind != PayloadKind::kTea;
    case Format::kJson:
      return facts.kind == PayloadKind::kFtl || facts.kind == PayloadKind::kUnknown;
    default:
      return false;
  }
}

bool convertModelInput(const ClassifiedPath& input, std::span<const ModelMaterialLibraryInput> material_libraries,
                       const ModelInputConversionOptions& options, DiagnosticCode failure_code,
                       std::string_view description, ConvertedModelInput& out) {
  ConvertedModelInput converted;
  ArxReturnCode rc = ARX_OK;
  switch (input.facts.format) {
    case Format::kFtl: {
      pistoris::Ftl native;
      rc = pistoris::readFtl(input.buffer, native);
      if (rc == ARX_OK)
        rc = pistoris::Model::importNative(
            converted.model, native, &converted.texture_source_paths, options.native_text_mode);
      break;
    }
    case Format::kJson: {
      pistoris::Ftl native;
      rc = pistoris::fromJson(byteStringView(input.buffer), native, pistoris::NativeTextMode::kUtf8);
      if (rc == ARX_OK)
        rc = pistoris::Model::importNative(
            converted.model, native, &converted.texture_source_paths, pistoris::NativeTextMode::kUtf8);
      break;
    }
    case Format::kObj: {
      std::vector<pistoris::ObjMaterialLibraryView> libraries;
      libraries.reserve(material_libraries.size());
      for (const ModelMaterialLibraryInput& library : material_libraries)
        libraries.push_back({library.path, byteStringView(library.data)});
      rc = pistoris::Model::importObj(
          converted.model, byteStringView(input.buffer), libraries, &converted.texture_source_paths);
      break;
    }
    case Format::kGlb: {
      rc = pistoris::Model::importGlb(converted.model,
                                      converted.animations,
                                      input.buffer,
                                      options.glb,
                                      nullptr,
                                      &converted.texture_source_paths,
                                      &converted.sound_sources);
      break;
    }
    default:
      diagnostic(
          failure_code, "Unsupported %.*s input format", static_cast<int>(description.size()), description.data());
      return false;
  }

  if (rc != ARX_OK) return conversionFailure(failure_code, description, input, rc);
  if (!applyResourcePath(input, description, failure_code, converted.model)) return false;
  out.model.swap(converted.model);
  out.animations = std::move(converted.animations);
  out.sound_sources = std::move(converted.sound_sources);
  out.texture_source_paths = std::move(converted.texture_source_paths);
  return true;
}

}  // namespace cli
