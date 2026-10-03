// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#include "arx_pistoris/base/status.h"
#include "arx_pistoris/level.hpp"
#include "arx_pistoris/level/location.hpp"
#include "arx_pistoris/native/location.hpp"
#include "arx_pistoris/native/text.hpp"

#include "api/result_failure.h"
#include "api/status_boundary.h"
#include "level/data.h"
#include "level/native/api.h"
#include "utils/native_text.h"

#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace pistoris {

LevelNativeResult<Level> Level::importNative(const fts::Data& fts, const llf::Data* llf, const dlf::Data* dlf,
                                             std::vector<std::string>* texture_source_paths,
                                             NativeTextMode text_mode) noexcept {
  return api_detail::levelNativeBoundary([&]() -> LevelNativeResult<Level> {
    if (!native_text::validMode(text_mode))
      return api_detail::levelNativeFailure<Level>(
          ARX_INVALID_OPTIONS, LevelNativeLocation{FtsLocation{.element = FtsElement::kHeader, .field = {}}});
    Level result;
    std::vector<std::string> source_paths;
    LevelNativeLocation failure_location = FtsLocation{.element = FtsElement::kHeader, .field = {}};
    ArxReturnCode rc = level_native::buildLevel({fts, llf, dlf},
                                                static_cast<LevelModules&>(*result.data_),
                                                &result.data_->validation,
                                                texture_source_paths ? &source_paths : nullptr,
                                                text_mode,
                                                &failure_location);
    if (rc != ARX_OK) return api_detail::levelNativeFailure<Level>(rc, failure_location);
    if (texture_source_paths) *texture_source_paths = std::move(source_paths);
    return result;
  });
}

LevelResult<NativeLevelBundle> Level::bakeNativeBundle(const NativeBakeOptions& options) const noexcept {
  constexpr std::string_view kOperation = "Level -> FTS + LLF + DLF conversion";
  if (!data_)
    return api_detail::levelFailure<NativeLevelBundle>(
        ARX_INVALID_STATE, api_detail::resourceLocation(resourcePath(), LevelElement::kResource), {}, kOperation);
  return api_detail::levelBoundary(
      resourcePath(),
      [&]() -> LevelResult<NativeLevelBundle> {
        LevelResult<void> validation = validate();
        if (!validation) return api_detail::levelFailure<NativeLevelBundle>(std::move(validation), kOperation);
        NativeLevelBundle out;
        const ArxReturnCode rc =
            level_native::bakeValidatedNativeLevelBundle(static_cast<const LevelModules&>(*data_), options, out);
        if (rc != ARX_OK)
          return api_detail::levelFailure<NativeLevelBundle>(
              rc, api_detail::resourceLocation(resourcePath(), LevelElement::kResource), {}, kOperation);
        return out;
      },
      kOperation);
}

LevelResult<dlf::Data> Level::bakeDlf(const DlfBakeOptions& options) const noexcept {
  constexpr std::string_view kOperation = "Level -> DLF conversion";
  if (!data_)
    return api_detail::levelFailure<dlf::Data>(
        ARX_INVALID_STATE, api_detail::resourceLocation(resourcePath(), LevelElement::kResource), {}, kOperation);
  return api_detail::levelBoundary(
      resourcePath(),
      [&]() -> LevelResult<dlf::Data> {
        LevelResult<void> player_spawn = validatePlayerSpawn();
        if (!player_spawn) return api_detail::levelFailure<dlf::Data>(std::move(player_spawn), kOperation);
        LevelResult<void> entities = validateEntities();
        if (!entities) return api_detail::levelFailure<dlf::Data>(std::move(entities), kOperation);
        LevelResult<void> fogs = validateFogs();
        if (!fogs) return api_detail::levelFailure<dlf::Data>(std::move(fogs), kOperation);
        LevelResult<void> zones = validateZones();
        if (!zones) return api_detail::levelFailure<dlf::Data>(std::move(zones), kOperation);
        LevelResult<void> paths = validatePaths();
        if (!paths) return api_detail::levelFailure<dlf::Data>(std::move(paths), kOperation);
        dlf::Data out;
        const ArxReturnCode rc = level_native::bakeValidatedNativeDlf(data_->scene, options, out);
        if (rc != ARX_OK)
          return api_detail::levelFailure<dlf::Data>(
              rc, api_detail::resourceLocation(resourcePath(), LevelElement::kResource), {}, kOperation);
        return out;
      },
      kOperation);
}

}  // namespace pistoris
