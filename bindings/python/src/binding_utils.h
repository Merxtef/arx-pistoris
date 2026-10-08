// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#pragma once

#include "arx_pistoris/pistoris.hpp"
#include "arx_pistoris/resource_io/location.hpp"

#include "paths/entity_class.h"
#include "utils/identifier.h"
#include "utils/resource_path.h"

#include <algorithm>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <exception>
#include <filesystem>
#include <initializer_list>
#include <limits>
#include <nanobind/nanobind.h>
#include <optional>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <variant>
#include <vector>

namespace pistoris::python {

namespace nb = nanobind;

inline std::size_t checkedBufferProduct(std::size_t left, std::size_t right);

// A borrowed view of a native-endian scalar buffer. Bulk operations
// accept PEP 3118 providers (including ndarray, array.array, and memoryview)
// without converting through Python scalar objects or allocating a staging
// vector. Callers keep this object alive for the full duration of the native
// call.
class ScalarBufferView {
 public:
  ScalarBufferView(nb::handle source, char expected_format, std::size_t expected_item_size, bool writable = false)
      : expected_item_size_(expected_item_size) {
    if (PyObject_GetBuffer(source.ptr(), &view_, PyBUF_FORMAT | PyBUF_STRIDES | (writable ? PyBUF_WRITABLE : 0)) < 0) {
      throw nb::python_error();
    }
    acquired_ = true;
    try {
      if (view_.itemsize != static_cast<Py_ssize_t>(expected_item_size) || !formatMatches(expected_format)) {
        throw nb::type_error("buffer has an incompatible dtype or byte order");
      }
      if (view_.ndim < 1 || view_.shape == nullptr) throw nb::value_error("buffer must have at least one dimension");
      if (!PyBuffer_IsContiguous(&view_, 'C')) throw nb::value_error("buffer must be C-contiguous");
      if (view_.suboffsets != nullptr) {
        for (int dimension = 0; dimension < view_.ndim; ++dimension) {
          if (view_.suboffsets[dimension] >= 0) throw nb::value_error("indirect buffers are not supported");
        }
      }
      if (view_.len < 0 || (view_.len != 0 && view_.buf == nullptr) ||
          view_.len % static_cast<Py_ssize_t>(expected_item_size) != 0) {
        throw nb::value_error("buffer has an invalid byte length");
      }
      size_ = static_cast<std::size_t>(view_.len) / expected_item_size;
      std::size_t shape_size = 1;
      for (int dimension = 0; dimension < view_.ndim; ++dimension) {
        if (view_.shape[dimension] < 0) throw nb::value_error("buffer has an invalid shape");
        shape_size = checkedBufferProduct(shape_size, static_cast<std::size_t>(view_.shape[dimension]));
      }
      if (shape_size != size_) throw nb::value_error("buffer shape does not match its byte length");
    } catch (...) {
      PyBuffer_Release(&view_);
      acquired_ = false;
      throw;
    }
  }

  ScalarBufferView(const ScalarBufferView&) = delete;
  ScalarBufferView& operator=(const ScalarBufferView&) = delete;
  ScalarBufferView(ScalarBufferView&&) = delete;
  ScalarBufferView& operator=(ScalarBufferView&&) = delete;

  ~ScalarBufferView() {
    if (acquired_) PyBuffer_Release(&view_);
  }

  [[nodiscard]] const void* data() const noexcept { return view_.buf; }
  [[nodiscard]] std::size_t size() const noexcept { return size_; }
  [[nodiscard]] int ndim() const noexcept { return view_.ndim; }
  template <class T>
  [[nodiscard]] std::span<const T> asSpan() const {
    if (sizeof(T) != expected_item_size_) throw nb::type_error("buffer item size does not match requested span type");
    if (size_ == 0) return {};
    if (view_.buf != nullptr && reinterpret_cast<std::uintptr_t>(view_.buf) % alignof(T) != 0) {
      throw nb::value_error("buffer data is not aligned for its dtype");
    }
    return {static_cast<const T*>(view_.buf), size_};
  }
  [[nodiscard]] Py_ssize_t shape(int dimension) const {
    if (dimension < 0 || dimension >= view_.ndim) throw nb::value_error("buffer has the wrong number of dimensions");
    return view_.shape[dimension];
  }

  template <class T>
  [[nodiscard]] std::span<T> asWritableSpan() const {
    if (view_.readonly) throw nb::value_error("output buffer must be writable");
    const auto values = asSpan<T>();
    return {const_cast<T*>(values.data()), values.size()};
  }

  void requireShape(std::initializer_list<Py_ssize_t> expected) const {
    if (view_.ndim != static_cast<int>(expected.size())) throw nb::value_error("buffer has the wrong shape");
    int dimension = 0;
    for (const Py_ssize_t extent : expected) {
      if (view_.shape[dimension++] != extent) throw nb::value_error("buffer has the wrong shape");
    }
  }

  void requireFlatOrShape(std::size_t flat_size, std::initializer_list<Py_ssize_t> expected) const {
    if (flat_size > static_cast<std::size_t>(std::numeric_limits<Py_ssize_t>::max()))
      throw nb::value_error("expected buffer length is too large");
    if (view_.ndim == 1 && view_.shape[0] == static_cast<Py_ssize_t>(flat_size)) return;
    requireShape(expected);
  }

  [[nodiscard]] std::size_t tupleCount(std::size_t width) const {
    if (width == 0) throw nb::value_error("tuple width must be positive");
    if (ndim() == 1) {
      if (size_ % width != 0) throw nb::value_error("buffer length does not contain complete tuples");
      return size_ / width;
    }
    if (ndim() != 2 || shape(1) != static_cast<Py_ssize_t>(width)) throw nb::value_error("buffer has the wrong shape");
    return static_cast<std::size_t>(shape(0));
  }

 private:
  [[nodiscard]] bool formatMatches(char expected) const noexcept {
    if (view_.format == nullptr || view_.format[0] == '\0') return false;
    const char* format = view_.format;
    bool format_is_little_endian = std::endian::native == std::endian::little;
    if (*format == '@' || *format == '=') {
      ++format;
    } else if (*format == '<' || *format == '>') {
      format_is_little_endian = *format == '<';
      ++format;
    } else if (*format == '!') {
      format_is_little_endian = false;
      ++format;
    }
    if (format_is_little_endian != (std::endian::native == std::endian::little)) return false;
    if (format[1] != '\0') return false;
    if (format[0] == expected) return true;
    if (expected == 'I' && format[0] == 'L' && sizeof(unsigned long) == sizeof(std::uint32_t)) return true;
    // PEP 3118 aliases for native-width scalar formats vary between buffer
    // providers (notably NumPy's uint64 uses 'L' on some platforms).
    if (expected == 'Q' && sizeof(unsigned long) == sizeof(std::uint64_t)) return format[0] == 'L';
    if (expected == 'L' && sizeof(unsigned long long) == sizeof(std::uint64_t)) return format[0] == 'Q';
    return false;
  }

  Py_buffer view_{};
  bool acquired_ = false;
  std::size_t expected_item_size_ = 0;
  std::size_t size_ = 0;
};

inline std::size_t checkedBufferProduct(std::size_t left, std::size_t right) {
  if (right != 0 && left > std::numeric_limits<std::size_t>::max() / right)
    throw nb::value_error("buffer dimensions are too large");
  return left * right;
}

inline void requireCopyDestinations(std::initializer_list<nb::handle> destinations) {
  if (std::none_of(destinations.begin(), destinations.end(), [](nb::handle value) { return !value.is_none(); }))
    throw nb::value_error("copy requires at least one destination buffer");
}

template <class T>
class ScalarBufferOutput {
 public:
  ScalarBufferOutput(nb::handle destination, std::size_t size, std::initializer_list<Py_ssize_t> shape) {
    if (destination.is_none()) return;
    view_.emplace(destination,
                  std::is_same_v<T, float>             ? 'f'
                  : sizeof(T) == sizeof(std::uint64_t) ? 'Q'
                                                       : 'I',
                  sizeof(T),
                  true);
    view_->requireFlatOrShape(size, shape);
  }

  [[nodiscard]] std::optional<std::span<T>> span() const {
    return view_ ? std::optional{view_->template asWritableSpan<T>()} : std::nullopt;
  }

  [[nodiscard]] std::span<T> requiredSpan() const {
    if (!view_) throw nb::value_error("copy requires a destination buffer");
    return view_->template asWritableSpan<T>();
  }

 private:
  std::optional<ScalarBufferView> view_;
};

template <class Collection>
void registerCollectionProtocol(nb::class_<Collection>& binding, const char* protocol_name,
                                std::initializer_list<const char*> mixins,
                                std::initializer_list<const char*> overrides = {}) {
  const nb::object protocol = nb::module_::import_("collections.abc").attr(protocol_name);
  const nb::object attributes = binding.attr("__dict__");
  for (const char* name : mixins) {
    const bool defined = nb::cast<bool>(attributes.attr("__contains__")(name));
    if (!defined && nb::hasattr(protocol, name)) binding.attr(name) = protocol.attr(name);
  }
  for (const char* name : overrides) binding.attr(name) = protocol.attr(name);
  protocol.attr("register")(binding);
}

template <class Collection>
void registerSequence(nb::class_<Collection>& binding) {
  registerCollectionProtocol(binding, "Sequence", {});
}

template <class Collection>
void registerMutableSequence(nb::class_<Collection>& binding) {
  registerCollectionProtocol(binding,
                             "MutableSequence",
                             {"__iter__",
                              "__contains__",
                              "__reversed__",
                              "index",
                              "count",
                              "append",
                              "clear",
                              "reverse",
                              "extend",
                              "pop",
                              "remove",
                              "__iadd__"});
}

template <class Collection>
void registerMutableMapping(nb::class_<Collection>& binding) {
  binding.attr("__hash__") = nb::none();
  registerCollectionProtocol(binding,
                             "MutableMapping",
                             {"__contains__", "get", "pop", "popitem", "setdefault"},
                             {"keys", "items", "values", "update"});
}

template <class Collection>
void registerMappingValueEquality(nb::class_<Collection>& binding) {
  binding.attr("__eq__") = nb::module_::import_("collections.abc").attr("Mapping").attr("__eq__");
}

template <class Collection>
void registerMutableSet(nb::class_<Collection>& binding) {
  binding.def_static("_from_iterable", [](nb::iterable values) { return nb::set(std::move(values)); });
  registerCollectionProtocol(binding,
                             "MutableSet",
                             {"isdisjoint",
                              "__le__",
                              "__lt__",
                              "__gt__",
                              "__ge__",
                              "__eq__",
                              "__and__",
                              "__or__",
                              "__sub__",
                              "__xor__",
                              "__rand__",
                              "__ror__",
                              "__rsub__",
                              "__rxor__",
                              "pop",
                              "__ior__",
                              "__iand__",
                              "__ixor__",
                              "__isub__"});
}

template <class Collection>
void registerCollection(nb::class_<Collection>& binding) {
  registerCollectionProtocol(binding, "Collection", {});
}

inline std::string canonicalIdentifier(std::string_view value, const IdentifierPolicy& policy = {}) {
  return normalizeIdentifier(value, policy).value;
}

inline std::optional<std::string> canonicalOptionalIdentifier(const std::optional<std::string>& value,
                                                              const IdentifierPolicy& policy = {}) {
  return value ? std::optional<std::string>{canonicalIdentifier(std::string_view(*value), policy)} : std::nullopt;
}

inline std::string canonicalModelIdentifier(std::string_view value) {
  return canonicalIdentifier(value, {.letter_case = IdentifierCase::kLower, .max_length = 255});
}

inline std::optional<std::string> canonicalOptionalModelIdentifier(const std::optional<std::string>& value) {
  return value ? std::optional<std::string>{canonicalModelIdentifier(std::string_view(*value))} : std::nullopt;
}

inline std::string canonicalSelectionIdentifier(std::string_view value) {
  return canonicalIdentifier(value, {.letter_case = IdentifierCase::kLower, .max_length = 63});
}

inline std::string canonicalLowerIdentifier(std::string_view value) {
  return canonicalIdentifier(value, {.letter_case = IdentifierCase::kLower});
}

inline std::string canonicalResourcePath(std::string_view value) {
  if (value.empty()) return {};
  ResourcePathNormalization normalized = normalizeResourcePath(value);
  if (normalized.error != ResourcePathError::kNone) throw nb::value_error("invalid resource path");
  return std::move(normalized.value);
}

inline std::optional<std::string> canonicalOptionalResourcePath(const std::optional<std::string>& value) {
  return value ? std::optional<std::string>{canonicalResourcePath(std::string_view(*value))} : std::nullopt;
}

inline std::string canonicalEntityClassPath(std::string_view value) {
  if (value.empty()) return {};
  std::string normalized;
  std::string_view removed_extension;
  if (!normalizeEntityClassPath(value, normalized, removed_extension))
    throw nb::value_error("invalid entity class path");
  return normalized;
}

inline std::string canonicalZoneAmbiance(std::string_view value) {
  if (value.empty()) return {};
  std::string normalized;
  if (!paths::normalizeZoneAmbiance(value, normalized)) throw nb::value_error("invalid zone ambiance path");
  return normalized;
}

struct ErrorLocation {
  std::string domain;
  std::optional<std::int64_t> element;
  std::optional<std::string> element_name;
  std::optional<std::size_t> input_index;
  std::optional<std::size_t> index;
  std::optional<std::size_t> subindex;
  std::optional<std::size_t> byte_offset;
  std::optional<std::size_t> requested_bytes;
  std::optional<std::size_t> source_index;
  std::optional<std::size_t> line;
  std::optional<std::uint64_t> sound_handle;
  std::optional<std::uint32_t> language_id;
  std::optional<std::string> label;
  std::optional<std::string> property;
  std::optional<std::string> field;
  std::optional<std::string> resource_path;
  std::optional<std::filesystem::path> native_path;
  std::optional<std::string> source_path;
  std::optional<std::string> json_pointer;
  std::optional<std::string> binary_region;
  std::optional<resource_io::ResourceIoOperation> operation;
  std::optional<std::uint64_t> mount_mask;
};

inline std::optional<std::string> optionalString(std::string_view value) {
  return value.empty() ? std::nullopt : std::optional<std::string>{value};
}

class PistorisException final : public std::runtime_error {
 public:
  PistorisException(ArxReturnCode code, std::string_view message, std::string detail,
                    std::optional<ErrorLocation> location)
      : std::runtime_error(std::string(message)),
        code(code),
        detail(std::move(detail)),
        location(std::move(location)) {}

  ArxReturnCode code;
  std::string detail;
  std::optional<ErrorLocation> location;
};

inline std::optional<std::size_t> optionalIndex(std::size_t value) {
  return value == kNoElementIndex ? std::nullopt : std::optional<std::size_t>{value};
}

template <class Index>
std::optional<Index> semanticIndex(Index value, Index absent) {
  return value == absent ? std::nullopt : std::optional<Index>{value};
}

inline std::optional<std::size_t> optionalInputIndex(std::size_t value) {
  return value == kNoInputIndex ? std::nullopt : std::optional<std::size_t>{value};
}

template <class Element>
constexpr std::int64_t elementValue(Element element) {
  return static_cast<std::int64_t>(element);
}

template <class Element>
constexpr std::string_view elementDomain() {
  if constexpr (std::is_same_v<Element, LevelElement>) return "level";
  if constexpr (std::is_same_v<Element, ModelElement>) return "model";
  if constexpr (std::is_same_v<Element, AnimationElement>) return "animation";
  if constexpr (std::is_same_v<Element, AmbianceElement>) return "ambiance";
  if constexpr (std::is_same_v<Element, AmbElement>) return "amb";
  if constexpr (std::is_same_v<Element, CinElement>) return "cin";
  if constexpr (std::is_same_v<Element, DlfElement>) return "dlf";
  if constexpr (std::is_same_v<Element, FtlElement>) return "ftl";
  if constexpr (std::is_same_v<Element, FtsElement>) return "fts";
  if constexpr (std::is_same_v<Element, LlfElement>) return "llf";
  if constexpr (std::is_same_v<Element, TeaElement>) return "tea";
  return "unknown";
}

template <class Element>
ErrorLocation normalizeLocation(const ResourceLocation<Element>& location) {
  ErrorLocation result;
  result.domain = elementDomain<Element>();
  result.element = elementValue(location.element);
  result.element_name = errorElementName(location.element);
  result.input_index = optionalInputIndex(location.input_index);
  result.index = optionalIndex(location.index);
  result.subindex = optionalIndex(location.subindex);
  result.label = optionalString(location.label);
  result.resource_path = optionalString(location.resource_path);
  return result;
}

inline ErrorLocation normalizeLocation(const CinematicLocation& location) {
  ErrorLocation result;
  result.domain = "cinematic";
  result.element = elementValue(location.element);
  result.element_name = errorElementName(location.element);
  result.input_index = optionalInputIndex(location.input_index);
  result.index = optionalIndex(location.index);
  result.subindex = optionalIndex(location.subindex);
  result.label = optionalString(location.label);
  result.resource_path = optionalString(location.resource_path);
  if (location.sound_handle != kNoSoundHandle) result.sound_handle = location.sound_handle;
  if (location.language_id != kInvalidLanguageId) result.language_id = location.language_id;
  return result;
}

inline ErrorLocation normalizeLocation(const GlbLocation& location) {
  ErrorLocation result;
  result.domain = "glb";
  result.element = elementValue(location.element);
  result.element_name = errorElementName(location.element);
  result.index = optionalIndex(location.index);
  result.subindex = optionalIndex(location.subindex);
  result.label = optionalString(location.label);
  result.property = optionalString(location.property);
  return result;
}

inline ErrorLocation normalizeLocation(const ObjLocation& location) {
  ErrorLocation result;
  result.domain = location.source == ObjSource::kObj ? "obj" : "mtl";
  result.element = elementValue(location.source);
  result.element_name = errorElementName(location.source);
  result.source_index = optionalInputIndex(location.source_index);
  if (location.line != 0) result.line = location.line;
  result.source_path = optionalString(location.source_path);
  return result;
}

inline ErrorLocation normalizeLocation(const JsonLocation& location) {
  ErrorLocation result;
  result.domain = "json";
  result.byte_offset = optionalIndex(location.byte_offset);
  result.json_pointer = optionalString(location.pointer);
  return result;
}

template <class Element>
ErrorLocation normalizeLocation(const NativeLocation<Element>& location) {
  ErrorLocation result;
  result.domain = elementDomain<Element>();
  result.element = elementValue(location.element);
  result.element_name = errorElementName(location.element);
  result.index = optionalIndex(location.index);
  result.subindex = optionalIndex(location.subindex);
  result.field = optionalString(location.field);
  return result;
}

struct NormalizedElement {
  std::string domain;
  std::int64_t value;
  std::string name;
};

template <class... Elements>
NormalizedElement nativeElement(const std::variant<Elements...>& element) {
  return std::visit(
      [](const auto value) {
        using Element = std::decay_t<decltype(value)>;
        return NormalizedElement{
            std::string(elementDomain<Element>()), elementValue(value), std::string(errorElementName(value))};
      },
      element);
}

template <class Element>
NormalizedElement nativeElement(Element element) {
  return {std::string(elementDomain<Element>()), elementValue(element), std::string(errorElementName(element))};
}

template <class Element>
ErrorLocation normalizeLocation(const NativeBinaryLocation<Element>& location) {
  auto normalized = nativeElement(location.element);
  ErrorLocation result;
  result.domain = std::move(normalized.domain);
  result.element = normalized.value;
  result.element_name = std::move(normalized.name);
  result.index = optionalIndex(location.index);
  result.subindex = optionalIndex(location.subindex);
  result.byte_offset = optionalIndex(location.byte_offset);
  if (location.requested_bytes != 0) result.requested_bytes = location.requested_bytes;
  result.field = optionalString(location.field);
  result.binary_region = location.region == NativeBinaryRegion::kStored ? "stored" : "decoded_payload";
  return result;
}

template <class... Locations>
ErrorLocation normalizeLocation(const std::variant<Locations...>& location) {
  return std::visit([](const auto& value) { return normalizeLocation(value); }, location);
}

inline ErrorLocation normalizeLocation(const resource_io::ResourceIoLocation& location) {
  ErrorLocation result = location.content_location ? normalizeLocation(*location.content_location) : ErrorLocation{};
  if (result.domain.empty()) result.domain = "resource_io";
  result.resource_path = optionalString(location.resource_path);
  if (!location.native_path.empty()) result.native_path = location.native_path;
  result.operation = location.operation;
  result.mount_mask = location.mount_mask;
  return result;
}

template <class Location>
[[noreturn]] void throwFailure(const Error<Location>& error) {
  std::optional<ErrorLocation> location;
  if (error.location()) location = normalizeLocation(*error.location());
  throw PistorisException(error.code(), describeError(error), std::string(error.detail()), std::move(location));
}

template <class T, class Location>
T unwrap(Result<T, Location>&& result) {
  if (!result) throwFailure(*result.error());
  return std::move(*result);
}

template <class Location>
void unwrap(Result<void, Location>&& result) {
  if (!result) throwFailure(*result.error());
}

class ReadableBuffer {
 public:
  explicit ReadableBuffer(nb::handle value) {
    if (PyObject_GetBuffer(value.ptr(), &view_, PyBUF_CONTIG_RO) != 0) throw nb::python_error();
    if (view_.itemsize != 1) {
      PyBuffer_Release(&view_);
      view_ = {};
      throw nb::type_error("expected a contiguous byte buffer");
    }
  }

  ReadableBuffer(const ReadableBuffer&) = delete;
  ReadableBuffer& operator=(const ReadableBuffer&) = delete;

  ReadableBuffer(ReadableBuffer&& other) noexcept : view_(other.view_) { other.view_ = {}; }
  ReadableBuffer& operator=(ReadableBuffer&&) = delete;

  ~ReadableBuffer() {
    if (view_.obj) PyBuffer_Release(&view_);
  }

  [[nodiscard]] const std::uint8_t* data() const noexcept { return static_cast<const std::uint8_t*>(view_.buf); }
  [[nodiscard]] std::size_t size() const noexcept { return static_cast<std::size_t>(view_.len); }
  [[nodiscard]] const std::uint8_t* begin() const noexcept { return data(); }
  [[nodiscard]] const std::uint8_t* end() const noexcept { return size() == 0 ? data() : data() + size(); }
  [[nodiscard]] operator std::span<const std::uint8_t>() const noexcept { return {data(), size()}; }

 private:
  Py_buffer view_{};
};

inline ReadableBuffer byteSpan(nb::handle value) { return ReadableBuffer(value); }

inline nb::bytes toBytes(std::span<const std::uint8_t> value) { return nb::bytes(value.data(), value.size()); }

inline nb::bytes toBytes(const std::vector<std::uint8_t>& value) { return toBytes(std::span{value}); }

inline nb::object toOptionalBytes(std::span<const std::uint8_t> value) {
  if (value.empty()) return nb::none();
  return toBytes(value);
}

inline nb::object toOptionalBytes(const std::vector<std::uint8_t>& value) { return toOptionalBytes(std::span{value}); }

inline nb::list materializeSequence(const nb::sequence& values) {
  nb::list result;
  const auto size = nb::len(values);
  for (std::size_t index = 0; index < size; ++index) result.append(values[index]);
  return result;
}

inline nb::list materializeIterable(const nb::iterable& values) {
  nb::list result;
  for (nb::handle value : values) result.append(value);
  return result;
}

template <class T, class Equal>
nb::object valueEquals(const T& left, nb::handle right, Equal&& equal) {
  if (!nb::isinstance<T>(right)) return nb::borrow<nb::object>(Py_NotImplemented);
  return nb::bool_(std::forward<Equal>(equal)(left, nb::cast<const T&>(right)));
}

template <class... Values>
Py_hash_t valueHash(Values&&... values) {
  const nb::tuple fields = nb::make_tuple(std::forward<Values>(values)...);
  const Py_hash_t hash = PyObject_Hash(fields.ptr());
  if (hash == -1) throw nb::python_error();
  return hash;
}

template <class T>
struct BorrowedPointers {
  nb::list owners;
  std::vector<const T*> values;
};

template <class T>
BorrowedPointers<T> borrowPointers(const nb::sequence& objects) {
  BorrowedPointers<T> result;
  result.owners = materializeSequence(objects);
  result.values.reserve(nb::len(result.owners));
  for (std::size_t index = 0; index < nb::len(result.owners); ++index) {
    result.values.push_back(&nb::cast<const T&>(result.owners[index]));
  }
  return result;
}

template <std::size_t Size>
nb::bytes fixedStringBytes(const char (&value)[Size]) {
  std::size_t length = 0;
  while (length < Size && value[length] != '\0') ++length;
  return nb::bytes(value, length);
}

template <std::size_t Size>
void setFixedStringBytes(char (&destination)[Size], const nb::bytes& value) {
  if (value.size() >= Size) throw nb::value_error("value does not fit in the native field");
  const std::string_view bytes(static_cast<const char*>(value.data()), value.size());
  if (bytes.find('\0') != std::string_view::npos) throw nb::value_error("native string contains an embedded null");
  std::fill(std::begin(destination), std::end(destination), '\0');
  std::memcpy(destination, bytes.data(), bytes.size());
}

inline nb::bytes stringBytes(const std::string& value) { return nb::bytes(value.data(), value.size()); }

inline void setStringBytes(std::string& destination, const nb::bytes& value) {
  destination.assign(static_cast<const char*>(value.data()), value.size());
}

inline void checkStatus(ArxReturnCode code) {
  if (code != ARX_OK) throw PistorisException(code, errorString(code), {}, std::nullopt);
}

template <class View>
nb::tuple snapshot(const View& view) {
  nb::list values;
  for (std::size_t index = 0; index < view.size(); ++index) values.append(view[index]);
  return nb::tuple(values);
}

}  // namespace pistoris::python
