// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#include "resources/model_input.h"

#include "arx_pistoris/model.hpp"
#include "arx_pistoris/model/obj.hpp"
#include "arx_pistoris/native.hpp"
#include "arx_pistoris/native/text.hpp"
#include "arx_pistoris/paths.hpp"

#include "base/bytes.h"
#include "console/diagnostics.h"
#include "formats/classification.h"
#include "formats/format.h"
#include "routes/conversion_failure.h"

#include <span>
#include <string_view>
#include <utility>
#include <vector>

namespace cli {
namespace {

template <class Result>
bool conversionFailure(DiagnosticCode code, std::string_view description, const ClassifiedPath& input,
                       const Result& result) {
  return conversionInputFailure(code, description, input.path, result);
}

bool applyResourcePath(const ClassifiedPath& input, std::string_view description, DiagnosticCode failure_code,
                       pistoris::Model& model) {
  pistoris::paths::ModelPathView parsed;
  if (!pistoris::paths::modelFromFtl(input.path, parsed)) return true;
  const auto result = model.setResourcePath(input.path);
  return result || conversionFailure(failure_code, description, input, result);
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
  switch (input.facts.format) {
    case Format::kFtl: {
      auto native = pistoris::readFtl(input.buffer);
      if (!native) return conversionFailure(failure_code, description, input, native);
      auto imported = pistoris::Model::importNative(*native, &converted.texture_source_paths, options.native_text_mode);
      if (!imported) return conversionFailure(failure_code, description, input, imported);
      converted.model = std::move(*imported);
      break;
    }
    case Format::kJson: {
      auto native = pistoris::fromFtlJson(byteStringView(input.buffer), pistoris::NativeTextMode::kUtf8);
      if (!native) return conversionFailure(failure_code, description, input, native);
      auto imported =
          pistoris::Model::importNative(*native, &converted.texture_source_paths, pistoris::NativeTextMode::kUtf8);
      if (!imported) return conversionFailure(failure_code, description, input, imported);
      converted.model = std::move(*imported);
      break;
    }
    case Format::kObj: {
      std::vector<pistoris::ObjMaterialLibraryView> libraries;
      libraries.reserve(material_libraries.size());
      for (const ModelMaterialLibraryInput& library : material_libraries)
        libraries.push_back({library.path, byteStringView(library.data)});
      auto imported =
          pistoris::Model::importObj(byteStringView(input.buffer), libraries, &converted.texture_source_paths);
      if (!imported) return conversionFailure(failure_code, description, input, imported);
      converted.model = std::move(*imported);
      break;
    }
    case Format::kGlb: {
      auto imported = pistoris::Model::importGlbWithAnimations(
          input.buffer, options.glb, nullptr, &converted.texture_source_paths, &converted.sound_sources);
      if (!imported) return conversionFailure(failure_code, description, input, imported);
      converted.model = std::move(imported->model);
      converted.animations = std::move(imported->animations);
      break;
    }
    default:
      diagnostic(
          failure_code, "Unsupported %.*s input format", static_cast<int>(description.size()), description.data());
      return false;
  }

  if (!applyResourcePath(input, description, failure_code, converted.model)) return false;
  out.model.swap(converted.model);
  out.animations = std::move(converted.animations);
  out.sound_sources = std::move(converted.sound_sources);
  out.texture_source_paths = std::move(converted.texture_source_paths);
  return true;
}

}  // namespace cli
