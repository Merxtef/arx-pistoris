// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#include "arx_pistoris/base/status.h"
#include "arx_pistoris/base/string_view.h"
#include "arx_pistoris/paths.h"
#include "arx_pistoris/paths.hpp"
#include "arx_pistoris/paths/types.h"

#include "api/c/internal.h"
#include "api/c/paths/internal.h"

#include <cstddef>
#include <string>

using namespace pistoris::c_paths;

// NOLINTBEGIN(readability-identifier-naming)

size_t arx_pistoris_path_model_selector_type_count(void) noexcept {
  return pistoris::paths::modelSelectorTypes().size();
}

ArxReturnCode arx_pistoris_path_model_selector_type(size_t index, ArxStringView* out_type) noexcept {
  return typeAt(pistoris::paths::modelSelectorTypes(), index, out_type);
}

ArxReturnCode arx_pistoris_path_model_ftl(ArxModelPathView model, char* out, size_t capacity,
                                          size_t* out_size) noexcept {
  if (!valid(model)) return ARX_INVALID_DATA_POINTER;
  return buildPath(out, capacity, out_size, [&](std::string& result) {
    return pistoris::paths::modelFtl(modelView(model), result);
  });
}

ArxReturnCode arx_pistoris_path_model_from_ftl(ArxStringView path, ArxModelPathView* out_model) noexcept {
  if (!pistoris::c_api::valid(path) || !out_model) return ARX_INVALID_DATA_POINTER;
  *out_model = {};
  return pistoris::c_api::guard([&]() -> ArxReturnCode {
    pistoris::paths::ModelPathView result;
    if (!pistoris::paths::modelFromFtl(pistoris::c_api::stringView(path), result)) return ARX_INVALID_IDENTIFIER;
    *out_model = cView(result);
    return ARX_OK;
  });
}

ArxReturnCode arx_pistoris_path_entity_class_from_ftl(ArxStringView path, char* out, size_t capacity,
                                                      size_t* out_size) noexcept {
  if (!pistoris::c_api::valid(path)) return ARX_INVALID_DATA_POINTER;
  return buildPath(out, capacity, out_size, [&](std::string& result) {
    return pistoris::paths::entityClassFromFtl(pistoris::c_api::stringView(path), result);
  });
}

ArxReturnCode arx_pistoris_path_ftl_from_entity_class(ArxStringView path, char* out, size_t capacity,
                                                      size_t* out_size) noexcept {
  if (!pistoris::c_api::valid(path)) return ARX_INVALID_DATA_POINTER;
  return buildPath(out, capacity, out_size, [&](std::string& result) {
    return pistoris::paths::ftlFromEntityClass(pistoris::c_api::stringView(path), result);
  });
}

ArxReturnCode arx_pistoris_path_entity_class_kind(ArxStringView path, ArxEntityClassKind* out_kind) noexcept {
  if (!pistoris::c_api::valid(path) || !out_kind) return ARX_INVALID_DATA_POINTER;
  *out_kind = ARX_ENTITY_CLASS_KIND_UNKNOWN;
  return pistoris::c_api::guard([&]() -> ArxReturnCode {
    ArxEntityClassKind result = ARX_ENTITY_CLASS_KIND_UNKNOWN;
    if (!pistoris::paths::entityClassKind(pistoris::c_api::stringView(path), result)) return ARX_INVALID_IDENTIFIER;
    *out_kind = result;
    return ARX_OK;
  });
}

ArxReturnCode arx_pistoris_path_item_icon_from_entity_class(ArxStringView path, char* out, size_t capacity,
                                                            size_t* out_size) noexcept {
  if (!pistoris::c_api::valid(path)) return ARX_INVALID_DATA_POINTER;
  return buildPath(out, capacity, out_size, [&](std::string& result) {
    return pistoris::paths::itemIconFromEntityClass(pistoris::c_api::stringView(path), result);
  });
}

ArxReturnCode arx_pistoris_path_entity_class_from_model(ArxModelPathView model, char* out, size_t capacity,
                                                        size_t* out_size) noexcept {
  if (!valid(model)) return ARX_INVALID_DATA_POINTER;
  return buildPath(out, capacity, out_size, [&](std::string& result) {
    return pistoris::paths::entityClassFromModel(modelView(model), result);
  });
}

ArxReturnCode arx_pistoris_path_base_entity_class_from_model(ArxModelPathView model, char* out, size_t capacity,
                                                             size_t* out_size) noexcept {
  if (!valid(model)) return ARX_INVALID_DATA_POINTER;
  return buildPath(out, capacity, out_size, [&](std::string& result) {
    return pistoris::paths::baseEntityClassFromModel(modelView(model), result);
  });
}

ArxReturnCode arx_pistoris_path_model_from_entity_class(ArxStringView path, ArxModelPathView* out_model) noexcept {
  if (!pistoris::c_api::valid(path) || !out_model) return ARX_INVALID_DATA_POINTER;
  *out_model = {};
  return pistoris::c_api::guard([&]() -> ArxReturnCode {
    pistoris::paths::ModelPathView result;
    if (!pistoris::paths::modelFromEntityClass(pistoris::c_api::stringView(path), result))
      return ARX_INVALID_IDENTIFIER;
    *out_model = cView(result);
    return ARX_OK;
  });
}

ArxReturnCode arx_pistoris_path_model_selector(ArxModelPathView model, char* out, size_t capacity,
                                               size_t* out_size) noexcept {
  if (!valid(model)) return ARX_INVALID_DATA_POINTER;
  return buildPath(out, capacity, out_size, [&](std::string& result) {
    return pistoris::paths::modelSelector(modelView(model), result);
  });
}

ArxReturnCode arx_pistoris_path_model_from_selector(ArxStringView selector, ArxModelPathView* out_model) noexcept {
  if (!pistoris::c_api::valid(selector) || !out_model) return ARX_INVALID_DATA_POINTER;
  *out_model = {};
  return pistoris::c_api::guard([&]() -> ArxReturnCode {
    pistoris::paths::ModelPathView result;
    if (!pistoris::paths::modelFromSelector(pistoris::c_api::stringView(selector), result))
      return ARX_INVALID_IDENTIFIER;
    *out_model = cView(result);
    return ARX_OK;
  });
}

ArxReturnCode arx_pistoris_path_model_search_location(ArxStringView type,
                                                      ArxResourceSearchLocation* out_location) noexcept {
  if (!pistoris::c_api::valid(type) || !out_location) return ARX_INVALID_DATA_POINTER;
  *out_location = {};
  pistoris::paths::ResourceSearchLocation result;
  if (!pistoris::paths::modelSearchLocation(pistoris::c_api::stringView(type), result)) return ARX_INVALID_IDENTIFIER;
  *out_location = cView(result);
  return ARX_OK;
}

// NOLINTEND(readability-identifier-naming)
