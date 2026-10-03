// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#pragma once

#include "arx_pistoris/base/location.hpp"
#include "arx_pistoris/base/result.hpp"
#include "arx_pistoris/base/status.h"

#include "api/result_failure.h"
#include "utils/log.h"

#include <cstddef>
#include <exception>
#include <new>
#include <optional>
#include <source_location>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>

namespace pistoris::api_detail {

template <class Element>
[[nodiscard]] ResourceLocation<Element> resourceLocation(Element element, std::size_t index = kNoElementIndex,
                                                         std::size_t subindex = kNoElementIndex) noexcept {
  ResourceLocation<Element> result;
  result.element = element;
  result.index = index;
  result.subindex = subindex;
  return result;
}

template <class Element>
[[nodiscard]] ResourceLocation<Element> resourceLocation(std::string_view resource_path, Element element,
                                                         std::size_t index = kNoElementIndex,
                                                         std::size_t subindex = kNoElementIndex,
                                                         std::size_t input_index = kNoInputIndex,
                                                         std::string_view label = {}) noexcept {
  ResourceLocation<Element> result;
  result.input_index = input_index;
  result.element = element;
  result.index = index;
  result.subindex = subindex;
  try {
    result.resource_path = resource_path;
  } catch (...) {
    // Location text is best-effort metadata and must not make a noexcept API fail.
    result.resource_path.clear();
  }
  try {
    result.label = label;
  } catch (...) {
    result.label.clear();
  }
  return result;
}

[[nodiscard]] inline CinematicLocation resourceLocation(CinematicElement element, std::size_t index = kNoElementIndex,
                                                        std::size_t subindex = kNoElementIndex) noexcept {
  CinematicLocation result;
  result.element = element;
  result.index = index;
  result.subindex = subindex;
  return result;
}

[[nodiscard]] inline CinematicLocation resourceLocation(std::string_view resource_path, CinematicElement element,
                                                        std::size_t index = kNoElementIndex,
                                                        std::size_t subindex = kNoElementIndex,
                                                        std::size_t input_index = kNoInputIndex,
                                                        std::string_view label = {}) noexcept {
  CinematicLocation result;
  result.input_index = input_index;
  result.element = element;
  result.index = index;
  result.subindex = subindex;
  try {
    result.resource_path = resource_path;
  } catch (...) {
    result.resource_path.clear();
  }
  try {
    result.label = label;
  } catch (...) {
    result.label.clear();
  }
  return result;
}

[[nodiscard]] inline CinematicLocation cinematicSoundLocation(std::string_view resource_path,
                                                              SoundHandle sound) noexcept {
  CinematicLocation result = resourceLocation(resource_path, CinematicElement::kSound);
  result.sound_handle = sound;
  return result;
}

[[nodiscard]] inline CinematicLocation cinematicSoundEncodingLocation(std::string_view resource_path, SoundHandle sound,
                                                                      LanguageId language) noexcept {
  CinematicLocation result = resourceLocation(resource_path, CinematicElement::kSoundEncoding);
  result.sound_handle = sound;
  result.language_id = language;
  return result;
}

[[nodiscard]] inline CinematicLocation cinematicLanguageLocation(std::string_view resource_path,
                                                                 LanguageId language) noexcept {
  CinematicLocation result = resourceLocation(resource_path, CinematicElement::kLanguage);
  result.language_id = language;
  return result;
}

template <class T, class Element>
// NOLINTNEXTLINE(bugprone-exception-escape): MSVC debug STL misreports some noexcept container moves
[[nodiscard]] Result<T, ResourceLocation<Element>> withResourceIdentity(Result<T, ResourceLocation<Element>>&& result,
                                                                        std::string_view resource_path,
                                                                        std::size_t input_index = kNoInputIndex,
                                                                        std::string_view label = {}) noexcept {
  using EnrichedResult = Result<T, ResourceLocation<Element>>;
  try {
    if (result) return std::move(result);
    const Error<ResourceLocation<Element>>* error = result.error();
    if (error == nullptr) return std::move(result);
    if (!error->location()) {
      ResourceLocation<Element> location =
          resourceLocation(resource_path, Element::kResource, kNoElementIndex, kNoElementIndex, input_index, label);
      return EnrichedResult::failure(result.code(), std::move(location), std::string(error->detail()));
    }
    const ResourceLocation<Element>& source = *error->location();
    const bool has_resource_path = !source.resource_path.empty() || resource_path.empty();
    const bool has_input_index = source.input_index != kNoInputIndex || input_index == kNoInputIndex;
    const bool has_label = !source.label.empty() || label.empty();
    if (has_resource_path && has_input_index && has_label) return std::move(result);
    ResourceLocation<Element> location = source;
    if (location.resource_path.empty()) location.resource_path = resource_path;
    if (location.input_index == kNoInputIndex) location.input_index = input_index;
    if (location.label.empty()) location.label = label;
    return EnrichedResult::failure(result.code(), std::move(location), std::string(error->detail()));
  } catch (...) {
    return std::move(result);
  }
}

template <class T>
[[nodiscard]] CinematicResult<T> withResourceIdentity(CinematicResult<T>&& result, std::string_view resource_path,
                                                      std::size_t input_index = kNoInputIndex,
                                                      std::string_view label = {}) noexcept {
  try {
    if (result) return std::move(result);
    const Error<CinematicLocation>* error = result.error();
    if (error == nullptr) return std::move(result);
    if (!error->location()) {
      CinematicLocation location = resourceLocation(
          resource_path, CinematicElement::kResource, kNoElementIndex, kNoElementIndex, input_index, label);
      return CinematicResult<T>::failure(result.code(), std::move(location), std::string(error->detail()));
    }
    const CinematicLocation& source = *error->location();
    const bool has_resource_path = !source.resource_path.empty() || resource_path.empty();
    const bool has_input_index = source.input_index != kNoInputIndex || input_index == kNoInputIndex;
    const bool has_label = !source.label.empty() || label.empty();
    if (has_resource_path && has_input_index && has_label) return std::move(result);
    CinematicLocation location = source;
    if (location.resource_path.empty()) location.resource_path = resource_path;
    if (location.input_index == kNoInputIndex) location.input_index = input_index;
    if (location.label.empty()) location.label = label;
    return CinematicResult<T>::failure(result.code(), std::move(location), std::string(error->detail()));
  } catch (...) {
    return std::move(result);
  }
}

template <class T, class TargetLocation, class U, class SourceLocation, class Mapper>
[[nodiscard]] Result<T, TargetLocation> remapFailure(
    Result<U, SourceLocation>&& failure, Mapper&& mapper, std::string_view detail = {},
    std::source_location where = std::source_location::current()) noexcept {
  const auto source_failure = [&](ArxReturnCode code) {
    auto result = Result<T, TargetLocation>::failure(code, std::nullopt);
    if (const auto* error = result.error())
      logLazy(ARX_LOG_DEBUG,
              [&] { return result_failure_detail::sourceFailureMessage("Failure remapping", *error, where); });
    return result;
  };
  if (failure) return source_failure(ARX_INTERNAL_ERROR);
  const auto* error = failure.error();
  if (!error) return source_failure(ARX_INTERNAL_ERROR);
  try {
    std::optional<TargetLocation> location;
    if (error->location()) location.emplace(std::forward<Mapper>(mapper)(*error->location()));
    const std::string remapped_detail = detail.empty() ? std::string(error->detail()) : std::string(detail);
    return Result<T, TargetLocation>::failure(failure.code(), std::move(location), remapped_detail);
  } catch (const std::bad_alloc&) {
    return source_failure(ARX_BAD_ALLOC);
  } catch (...) {
    return source_failure(ARX_INTERNAL_ERROR);
  }
}

template <class Result, class Fn>
Result resourceValidationBoundary(std::string_view resource_path, Fn&& fn,
                                  const typename Result::LocationType& location,
                                  std::source_location where = std::source_location::current()) noexcept {
  const auto failure = [&](ArxReturnCode code, const typename Result::LocationType* source_location) noexcept {
    try {
      std::optional<typename Result::LocationType> stored_location;
      if (source_location != nullptr) {
        stored_location = *source_location;
        if (stored_location->resource_path.empty()) stored_location->resource_path = resource_path;
      }
      Result result = Result::failure(code, std::move(stored_location));
      const auto* error = result.error();
      if (error)
        logLazy(ARX_LOG_DEBUG,
                [&] { return result_failure_detail::sourceFailureMessage("Resource validation", *error, where); });
      return result;
    } catch (const std::bad_alloc&) {
      result_failure_detail::logFallback("Resource validation", ARX_BAD_ALLOC);
      return Result::failure(ARX_BAD_ALLOC, std::nullopt);
    } catch (...) {
      result_failure_detail::logFallback("Resource validation", ARX_INTERNAL_ERROR);
      return Result::failure(ARX_INTERNAL_ERROR, std::nullopt);
    }
  };
  try {
    const ArxReturnCode code = std::forward<Fn>(fn)();
    if (code == ARX_OK) return Result::success();
    return failure(code, &location);
  } catch (const std::bad_alloc&) {
    return failure(ARX_BAD_ALLOC, &location);
  } catch (...) {
    return failure(ARX_INTERNAL_ERROR, &location);
  }
}

template <class Fn>
ArxReturnCode silentStatusBoundary(Fn&& fn) noexcept {
  try {
    return std::forward<Fn>(fn)();
  } catch (const std::bad_alloc&) {
    return ARX_BAD_ALLOC;
  } catch (...) {
    return ARX_INTERNAL_ERROR;
  }
}

template <class Result, class Fn>
Result validationBoundary(Fn&& fn, const typename Result::LocationType& location,
                          std::source_location where = std::source_location::current()) noexcept {
  const auto failure = [&](ArxReturnCode code, std::optional<typename Result::LocationType> stored_location) {
    Result result = Result::failure(code, std::move(stored_location));
    const auto* error = result.error();
    if (error)
      logLazy(ARX_LOG_DEBUG,
              [&] { return result_failure_detail::sourceFailureMessage("Native validation", *error, where); });
    return result;
  };
  try {
    const ArxReturnCode code = std::forward<Fn>(fn)();
    if (code != ARX_OK) return failure(code, location);
    return Result::success();
  } catch (const std::bad_alloc&) {
    return failure(ARX_BAD_ALLOC, std::nullopt);
  } catch (...) {
    return failure(ARX_INTERNAL_ERROR, std::nullopt);
  }
}

template <class Fn>
ArxReturnCode statusBoundary(Fn&& fn, std::source_location where = std::source_location::current()) noexcept {
  try {
    return std::forward<Fn>(fn)();
  } catch (const std::bad_alloc&) {
    log(ARX_LOG_DEBUG,
        "C API operation ran out of memory at {}:{} in {}",
        where.file_name(),
        where.line(),
        where.function_name());
    return ARX_BAD_ALLOC;
  } catch (const std::exception& exception) {
    log(ARX_LOG_DEBUG,
        "C API operation caught an unexpected exception at {}:{} in {}: {}",
        where.file_name(),
        where.line(),
        where.function_name(),
        exception.what());
    return ARX_INTERNAL_ERROR;
  } catch (...) {
    log(ARX_LOG_DEBUG,
        "C API operation caught an unexpected non-standard exception at {}:{} in {}",
        where.file_name(),
        where.line(),
        where.function_name());
    return ARX_INTERNAL_ERROR;
  }
}

template <class Fn, class Failure>
auto resultBoundary(Fn&& fn, Failure&& failure) noexcept -> std::invoke_result_t<Fn> {
  try {
    return std::forward<Fn>(fn)();
  } catch (const std::bad_alloc&) {
    return std::forward<Failure>(failure)(ARX_BAD_ALLOC, std::string_view{});
  } catch (const std::exception& exception) {
    return std::forward<Failure>(failure)(ARX_INTERNAL_ERROR, exception.what());
  } catch (...) {
    return std::forward<Failure>(failure)(ARX_INTERNAL_ERROR, "unexpected non-standard exception");
  }
}

template <class Fn>
auto levelBoundary(Fn&& fn, std::string_view operation = "Level operation",
                   std::source_location where = std::source_location::current()) noexcept -> std::invoke_result_t<Fn> {
  using Result = std::invoke_result_t<Fn>;
  using Value = typename Result::Value;
  return resultBoundary(std::forward<Fn>(fn), [&](ArxReturnCode code, std::string_view detail) {
    return levelFailure<Value>(code, std::nullopt, detail, operation, where);
  });
}

template <class Fn>
auto levelBoundary(std::string_view resource_path, Fn&& fn, std::string_view operation = "Level operation",
                   std::source_location where = std::source_location::current()) noexcept -> std::invoke_result_t<Fn> {
  using Result = std::invoke_result_t<Fn>;
  using Value = typename Result::Value;
  return withResourceIdentity(
      resultBoundary(std::forward<Fn>(fn),
                     [&](ArxReturnCode code, std::string_view detail) {
                       return levelFailure<Value>(
                           code, resourceLocation(resource_path, LevelElement::kResource), detail, operation, where);
                     }),
      resource_path);
}

template <class Fn>
auto modelBoundary(Fn&& fn, std::string_view operation = "Model operation",
                   std::source_location where = std::source_location::current()) noexcept -> std::invoke_result_t<Fn> {
  using Result = std::invoke_result_t<Fn>;
  using Value = typename Result::Value;
  return resultBoundary(std::forward<Fn>(fn), [&](ArxReturnCode code, std::string_view detail) {
    return modelFailure<Value>(code, std::nullopt, detail, operation, where);
  });
}

template <class Fn>
auto modelBoundary(std::string_view resource_path, Fn&& fn, std::string_view operation = "Model operation",
                   std::source_location where = std::source_location::current()) noexcept -> std::invoke_result_t<Fn> {
  using Result = std::invoke_result_t<Fn>;
  using Value = typename Result::Value;
  return withResourceIdentity(
      resultBoundary(std::forward<Fn>(fn),
                     [&](ArxReturnCode code, std::string_view detail) {
                       return modelFailure<Value>(
                           code, resourceLocation(resource_path, ModelElement::kResource), detail, operation, where);
                     }),
      resource_path);
}

template <class Fn>
auto animationBoundary(Fn&& fn, std::string_view operation = "Animation operation",
                       std::source_location where = std::source_location::current()) noexcept
    -> std::invoke_result_t<Fn> {
  using Result = std::invoke_result_t<Fn>;
  using Value = typename Result::Value;
  return resultBoundary(std::forward<Fn>(fn), [&](ArxReturnCode code, std::string_view detail) {
    return animationFailure<Value>(code, std::nullopt, detail, operation, where);
  });
}

template <class Fn>
auto animationBoundary(std::string_view resource_path, Fn&& fn, std::string_view operation = "Animation operation",
                       std::source_location where = std::source_location::current()) noexcept
    -> std::invoke_result_t<Fn> {
  using Result = std::invoke_result_t<Fn>;
  using Value = typename Result::Value;
  return withResourceIdentity(
      resultBoundary(
          std::forward<Fn>(fn),
          [&](ArxReturnCode code, std::string_view detail) {
            return animationFailure<Value>(
                code, resourceLocation(resource_path, AnimationElement::kResource), detail, operation, where);
          }),
      resource_path);
}

template <class Fn>
auto ambianceBoundary(Fn&& fn, std::string_view operation = "Ambiance operation",
                      std::source_location where = std::source_location::current()) noexcept
    -> std::invoke_result_t<Fn> {
  using Result = std::invoke_result_t<Fn>;
  using Value = typename Result::Value;
  return resultBoundary(std::forward<Fn>(fn), [&](ArxReturnCode code, std::string_view detail) {
    return ambianceFailure<Value>(code, std::nullopt, detail, operation, where);
  });
}

template <class Fn>
auto ambianceBoundary(std::string_view resource_path, Fn&& fn, std::string_view operation = "Ambiance operation",
                      std::source_location where = std::source_location::current()) noexcept
    -> std::invoke_result_t<Fn> {
  using Result = std::invoke_result_t<Fn>;
  using Value = typename Result::Value;
  return withResourceIdentity(
      resultBoundary(std::forward<Fn>(fn),
                     [&](ArxReturnCode code, std::string_view detail) {
                       return ambianceFailure<Value>(
                           code, resourceLocation(resource_path, AmbianceElement::kResource), detail, operation, where);
                     }),
      resource_path);
}

template <class Fn>
auto cinematicBoundary(Fn&& fn, std::string_view operation = "Cinematic operation",
                       std::source_location where = std::source_location::current()) noexcept
    -> std::invoke_result_t<Fn> {
  using Result = std::invoke_result_t<Fn>;
  using Value = typename Result::Value;
  return resultBoundary(std::forward<Fn>(fn), [&](ArxReturnCode code, std::string_view detail) {
    return cinematicFailure<Value>(code, std::nullopt, detail, operation, where);
  });
}

template <class Fn>
auto cinematicBoundary(std::string_view resource_path, Fn&& fn, std::string_view operation = "Cinematic operation",
                       std::source_location where = std::source_location::current()) noexcept
    -> std::invoke_result_t<Fn> {
  using Result = std::invoke_result_t<Fn>;
  using Value = typename Result::Value;
  return withResourceIdentity(
      resultBoundary(
          std::forward<Fn>(fn),
          [&](ArxReturnCode code, std::string_view detail) {
            return cinematicFailure<Value>(
                code, resourceLocation(resource_path, CinematicElement::kResource), detail, operation, where);
          }),
      resource_path);
}

template <class Fn>
auto glbBoundary(Fn&& fn, std::string_view operation = "GLB import",
                 std::source_location where = std::source_location::current()) noexcept -> std::invoke_result_t<Fn> {
  using Result = std::invoke_result_t<Fn>;
  using Value = typename Result::Value;
  return resultBoundary(std::forward<Fn>(fn), [&](ArxReturnCode code, std::string_view detail) {
    return glbFailure<Value>(code, std::nullopt, detail, operation, where);
  });
}

template <class Fn>
auto objBoundary(Fn&& fn, std::string_view operation = "OBJ -> Model conversion",
                 std::source_location where = std::source_location::current()) noexcept -> std::invoke_result_t<Fn> {
  using Result = std::invoke_result_t<Fn>;
  using Value = typename Result::Value;
  return resultBoundary(std::forward<Fn>(fn), [&](ArxReturnCode code, std::string_view detail) {
    return objFailure<Value>(code, std::nullopt, detail, operation, where);
  });
}

template <class Fn>
auto ambBoundary(Fn&& fn, std::string_view operation = "AMB -> Ambiance conversion",
                 std::source_location where = std::source_location::current()) noexcept -> std::invoke_result_t<Fn> {
  using Result = std::invoke_result_t<Fn>;
  using Value = typename Result::Value;
  return resultBoundary(std::forward<Fn>(fn), [&](ArxReturnCode code, std::string_view detail) {
    return ambFailure<Value>(code, std::nullopt, detail, operation, where);
  });
}

template <class Fn>
auto cinBoundary(Fn&& fn, std::string_view operation = "CIN -> Cinematic conversion",
                 std::source_location where = std::source_location::current()) noexcept -> std::invoke_result_t<Fn> {
  using Result = std::invoke_result_t<Fn>;
  using Value = typename Result::Value;
  return resultBoundary(std::forward<Fn>(fn), [&](ArxReturnCode code, std::string_view detail) {
    return cinFailure<Value>(code, std::nullopt, detail, operation, where);
  });
}

template <class Fn>
auto ftlBoundary(Fn&& fn, std::string_view operation = "FTL -> Model conversion",
                 std::source_location where = std::source_location::current()) noexcept -> std::invoke_result_t<Fn> {
  using Result = std::invoke_result_t<Fn>;
  using Value = typename Result::Value;
  return resultBoundary(std::forward<Fn>(fn), [&](ArxReturnCode code, std::string_view detail) {
    return ftlFailure<Value>(code, std::nullopt, detail, operation, where);
  });
}

template <class Fn>
auto teaBoundary(Fn&& fn, std::string_view operation = "TEA -> Animation conversion",
                 std::source_location where = std::source_location::current()) noexcept -> std::invoke_result_t<Fn> {
  using Result = std::invoke_result_t<Fn>;
  using Value = typename Result::Value;
  return resultBoundary(std::forward<Fn>(fn), [&](ArxReturnCode code, std::string_view detail) {
    return teaFailure<Value>(code, std::nullopt, detail, operation, where);
  });
}

template <class Fn>
auto levelNativeBoundary(Fn&& fn, std::string_view operation = "FTS + LLF + DLF -> Level conversion",
                         std::source_location where = std::source_location::current()) noexcept
    -> std::invoke_result_t<Fn> {
  using Result = std::invoke_result_t<Fn>;
  using Value = typename Result::Value;
  return resultBoundary(std::forward<Fn>(fn), [&](ArxReturnCode code, std::string_view detail) {
    return levelNativeFailure<Value>(code, std::nullopt, detail, operation, where);
  });
}

template <class Fn>
auto levelGlbExportBoundary(const LevelGlbExportLocation& location, Fn&& fn,
                            std::string_view operation = "Level -> GLB conversion",
                            std::source_location where = std::source_location::current()) noexcept
    -> std::invoke_result_t<Fn> {
  using Result = std::invoke_result_t<Fn>;
  using Value = typename Result::Value;
  return resultBoundary(std::forward<Fn>(fn), [&](ArxReturnCode code, std::string_view detail) {
    return levelGlbExportFailure<Value>(code, location, detail, operation, where);
  });
}

template <class Fn>
auto modelGlbExportBoundary(const ModelGlbExportLocation& location, Fn&& fn,
                            std::string_view operation = "Model -> GLB conversion",
                            std::source_location where = std::source_location::current()) noexcept
    -> std::invoke_result_t<Fn> {
  using Result = std::invoke_result_t<Fn>;
  using Value = typename Result::Value;
  return resultBoundary(std::forward<Fn>(fn), [&](ArxReturnCode code, std::string_view detail) {
    return modelGlbExportFailure<Value>(code, location, detail, operation, where);
  });
}

template <class Fn>
auto ambianceGlbExportBoundary(const AmbianceGlbExportLocation& location, Fn&& fn,
                               std::string_view operation = "Ambiance -> GLB conversion",
                               std::source_location where = std::source_location::current()) noexcept
    -> std::invoke_result_t<Fn> {
  using Result = std::invoke_result_t<Fn>;
  using Value = typename Result::Value;
  return resultBoundary(std::forward<Fn>(fn), [&](ArxReturnCode code, std::string_view detail) {
    return ambianceGlbExportFailure<Value>(code, location, detail, operation, where);
  });
}

template <class Result, class Fn, class Failure>
Result statusResultBoundary(Fn&& fn, Failure&& failure) noexcept {
  return resultBoundary(
      [&]() -> Result {
        const ArxReturnCode code = std::forward<Fn>(fn)();
        if (code != ARX_OK) return failure(code, std::string_view{});
        return Result::success();
      },
      std::forward<Failure>(failure));
}

template <class T, class Fn>
LevelResult<T> levelStatusBoundary(Fn&& fn, const LevelLocation& location,
                                   std::string_view operation = "Level operation",
                                   std::source_location where = std::source_location::current()) noexcept {
  return statusResultBoundary<LevelResult<T>>(std::forward<Fn>(fn), [&](ArxReturnCode code, std::string_view detail) {
    return levelFailure<T>(code, location, detail, operation, where);
  });
}

template <class T, class Fn>
LevelResult<T> levelStatusBoundary(std::string_view resource_path, Fn&& fn, const LevelLocation& source_location,
                                   std::string_view operation = "Level operation",
                                   std::source_location where = std::source_location::current()) noexcept {
  return levelBoundary(
      resource_path,
      [&]() -> LevelResult<T> {
        LevelLocation location = source_location;
        if (location.resource_path.empty()) location.resource_path = resource_path;
        return levelStatusBoundary<T>(std::forward<Fn>(fn), location, operation, where);
      },
      operation,
      where);
}

template <class T>
LevelResult<T> levelStatus(ArxReturnCode code, const LevelLocation& location,
                           std::string_view operation = "Level operation",
                           std::source_location where = std::source_location::current()) noexcept {
  if (code != ARX_OK) return levelFailure<T>(code, location, {}, operation, where);
  return LevelResult<T>::success();
}

template <class T, class Fn>
ModelResult<T> modelStatusBoundary(Fn&& fn, const ModelLocation& location,
                                   std::string_view operation = "Model operation",
                                   std::source_location where = std::source_location::current()) noexcept {
  return statusResultBoundary<ModelResult<T>>(std::forward<Fn>(fn), [&](ArxReturnCode code, std::string_view detail) {
    return modelFailure<T>(code, location, detail, operation, where);
  });
}

template <class T, class Fn>
ModelResult<T> modelStatusBoundary(std::string_view resource_path, Fn&& fn, const ModelLocation& source_location,
                                   std::string_view operation = "Model operation",
                                   std::source_location where = std::source_location::current()) noexcept {
  return modelBoundary(
      resource_path,
      [&]() -> ModelResult<T> {
        ModelLocation location = source_location;
        if (location.resource_path.empty()) location.resource_path = resource_path;
        return modelStatusBoundary<T>(std::forward<Fn>(fn), location, operation, where);
      },
      operation,
      where);
}

template <class T>
ModelResult<T> modelStatus(ArxReturnCode code, const ModelLocation& location,
                           std::string_view operation = "Model operation",
                           std::source_location where = std::source_location::current()) noexcept {
  if (code != ARX_OK) return modelFailure<T>(code, location, {}, operation, where);
  return ModelResult<T>::success();
}

template <class T, class Fn>
AnimationResult<T> animationStatusBoundary(Fn&& fn, const AnimationLocation& location,
                                           std::string_view operation = "Animation operation",
                                           std::source_location where = std::source_location::current()) noexcept {
  return statusResultBoundary<AnimationResult<T>>(
      std::forward<Fn>(fn), [&](ArxReturnCode code, std::string_view detail) {
        return animationFailure<T>(code, location, detail, operation, where);
      });
}

template <class T, class Fn>
AnimationResult<T> animationStatusBoundary(std::string_view resource_path, Fn&& fn,
                                           const AnimationLocation& source_location,
                                           std::string_view operation = "Animation operation",
                                           std::source_location where = std::source_location::current()) noexcept {
  return animationBoundary(
      resource_path,
      [&]() -> AnimationResult<T> {
        AnimationLocation location = source_location;
        if (location.resource_path.empty()) location.resource_path = resource_path;
        return animationStatusBoundary<T>(std::forward<Fn>(fn), location, operation, where);
      },
      operation,
      where);
}

template <class T, class Fn>
AmbianceResult<T> ambianceStatusBoundary(Fn&& fn, const AmbianceLocation& location,
                                         std::string_view operation = "Ambiance operation",
                                         std::source_location where = std::source_location::current()) noexcept {
  return statusResultBoundary<AmbianceResult<T>>(std::forward<Fn>(fn),
                                                 [&](ArxReturnCode code, std::string_view detail) {
                                                   return ambianceFailure<T>(code, location, detail, operation, where);
                                                 });
}

template <class T, class Fn>
AmbianceResult<T> ambianceStatusBoundary(std::string_view resource_path, Fn&& fn,
                                         const AmbianceLocation& source_location,
                                         std::string_view operation = "Ambiance operation",
                                         std::source_location where = std::source_location::current()) noexcept {
  return ambianceBoundary(
      resource_path,
      [&]() -> AmbianceResult<T> {
        AmbianceLocation location = source_location;
        if (location.resource_path.empty()) location.resource_path = resource_path;
        return ambianceStatusBoundary<T>(std::forward<Fn>(fn), location, operation, where);
      },
      operation,
      where);
}

template <class T, class Fn>
CinematicResult<T> cinematicStatusBoundary(Fn&& fn, const CinematicLocation& location,
                                           std::string_view operation = "Cinematic operation",
                                           std::source_location where = std::source_location::current()) noexcept {
  return statusResultBoundary<CinematicResult<T>>(
      std::forward<Fn>(fn), [&](ArxReturnCode code, std::string_view detail) {
        return cinematicFailure<T>(code, location, detail, operation, where);
      });
}

template <class T, class Fn>
CinematicResult<T> cinematicStatusBoundary(std::string_view resource_path, Fn&& fn,
                                           const CinematicLocation& source_location,
                                           std::string_view operation = "Cinematic operation",
                                           std::source_location where = std::source_location::current()) noexcept {
  return cinematicBoundary(
      resource_path,
      [&]() -> CinematicResult<T> {
        CinematicLocation location = source_location;
        if (location.resource_path.empty()) location.resource_path = resource_path;
        return cinematicStatusBoundary<T>(std::forward<Fn>(fn), location, operation, where);
      },
      operation,
      where);
}

template <class T>
CinematicResult<T> cinematicStatus(ArxReturnCode code, const CinematicLocation& location,
                                   std::string_view operation = "Cinematic operation",
                                   std::source_location where = std::source_location::current()) noexcept {
  if (code != ARX_OK) return cinematicFailure<T>(code, location, {}, operation, where);
  return CinematicResult<T>::success();
}

}  // namespace pistoris::api_detail
