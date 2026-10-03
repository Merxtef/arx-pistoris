// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#pragma once

#include "arx_pistoris/base/status.h"

#include <cassert>
#include <exception>
#include <optional>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <variant>

namespace pistoris {

template <class T, class Location>
class Result;

template <class Location>
class Error {
 public:
  [[nodiscard]] ArxReturnCode code() const noexcept { return code_; }
  [[nodiscard]] const std::optional<Location>& location() const noexcept { return location_; }
  [[nodiscard]] std::string_view detail() const noexcept { return detail_; }

 private:
  explicit Error(ArxReturnCode code) noexcept : code_(code) {}

  Error(ArxReturnCode code, std::optional<Location> location, std::string detail)
      : code_(code), location_(std::move(location)), detail_(std::move(detail)) {}

  ArxReturnCode code_;
  std::optional<Location> location_;
  std::string detail_;

  template <class T, class OtherLocation>
  friend class Result;
};

template <class T, class Location>
class [[nodiscard]] Result {
 public:
  using Value = T;
  using LocationType = Location;
  using ErrorType = Error<Location>;

  Result(T value) : storage_(std::move(value)) {}
  Result(const Result&) = default;
  // NOLINTNEXTLINE(bugprone-exception-escape): MSVC debug STL misreports some noexcept container moves
  Result(Result&&) noexcept(std::is_nothrow_move_constructible_v<std::variant<T, ErrorType>>) = default;
  Result& operator=(const Result& other) {
    if (this == &other) return *this;
    const AssignmentInvariantGuard guard(*this);
    storage_ = other.storage_;
    return *this;
  }
  // NOLINTNEXTLINE(bugprone-exception-escape): the specification follows the instantiated storage type
  Result& operator=(Result&& other) noexcept(std::is_nothrow_move_assignable_v<std::variant<T, ErrorType>>) {
    if (this == &other) return *this;
    const AssignmentInvariantGuard guard(*this);
    storage_ = std::move(other.storage_);
    return *this;
  }

  [[nodiscard]] static Result success(T value) { return Result(std::move(value)); }

  [[nodiscard]] static Result failure(ArxReturnCode code, std::optional<Location> location, std::string detail = {}) {
    assert(code != ARX_OK);
    if (code == ARX_OK) code = ARX_INTERNAL_ERROR;
    return Result(ErrorType{code, std::move(location), std::move(detail)});
  }

  template <class U>
  [[nodiscard]] Result<U, Location> propagate() && {
    assert(!hasValue());
    ErrorType* stored_error = std::get_if<1>(&storage_);
    assert(stored_error != nullptr);
    if (!stored_error) return Result<U, Location>::failure(ARX_INTERNAL_ERROR, std::nullopt);
    return Result<U, Location>(std::move(*stored_error));
  }

  [[nodiscard]] explicit operator bool() const noexcept { return storage_.index() == 0; }
  [[nodiscard]] bool hasValue() const noexcept { return storage_.index() == 0; }
  [[nodiscard]] ArxReturnCode code() const noexcept {
    const ErrorType* stored_error = error();
    if (stored_error) return stored_error->code();
    return hasValue() ? ARX_OK : ARX_INTERNAL_ERROR;
  }

  [[nodiscard]] T* get() noexcept { return std::get_if<0>(&storage_); }
  [[nodiscard]] const T* get() const noexcept { return std::get_if<0>(&storage_); }
  [[nodiscard]] const ErrorType* error() const noexcept { return std::get_if<1>(&storage_); }

  [[nodiscard]] T& operator*() & noexcept {
    assert(hasValue());
    return *get();
  }
  [[nodiscard]] const T& operator*() const& noexcept {
    assert(hasValue());
    return *get();
  }
  [[nodiscard]] T&& operator*() && noexcept {
    assert(hasValue());
    return std::move(*get());
  }
  [[nodiscard]] T* operator->() noexcept {
    assert(hasValue());
    return get();
  }
  [[nodiscard]] const T* operator->() const noexcept {
    assert(hasValue());
    return get();
  }

 private:
  class AssignmentInvariantGuard {
   public:
    explicit AssignmentInvariantGuard(Result& result) noexcept
        : result_(result), exception_count_(std::uncaught_exceptions()) {}
    ~AssignmentInvariantGuard() {
      if (std::uncaught_exceptions() > exception_count_) result_.restoreInvariant();
    }

   private:
    Result& result_;
    int exception_count_;
  };

  Result(ErrorType error) noexcept(std::is_nothrow_move_constructible_v<ErrorType>)
      : storage_(std::in_place_index<1>, std::move(error)) {}

  void restoreInvariant() noexcept {
    if (!storage_.valueless_by_exception()) return;
    try {
      storage_.template emplace<1>(ErrorType{ARX_INTERNAL_ERROR});
    } catch (...) {
      std::terminate();
    }
  }

  std::variant<T, ErrorType> storage_;

  template <class U, class OtherLocation>
  friend class Result;
};

template <class Location>
class [[nodiscard]] Result<void, Location> {
 public:
  using Value = void;
  using LocationType = Location;
  using ErrorType = Error<Location>;

  Result() noexcept = default;
  Result(const Result&) = default;
  Result(Result&&) = default;
  Result& operator=(const Result& other) {
    if (this == &other) return *this;
    const AssignmentInvariantGuard guard(*this, other.error_.has_value());
    error_ = other.error_;
    return *this;
  }
  Result& operator=(Result&& other) noexcept(std::is_nothrow_move_assignable_v<std::optional<ErrorType>>) {
    if (this == &other) return *this;
    const AssignmentInvariantGuard guard(*this, other.error_.has_value());
    error_ = std::move(other.error_);
    return *this;
  }

  [[nodiscard]] static Result success() noexcept { return {}; }

  [[nodiscard]] static Result failure(ArxReturnCode code, std::optional<Location> location, std::string detail = {}) {
    assert(code != ARX_OK);
    if (code == ARX_OK) code = ARX_INTERNAL_ERROR;
    return Result(ErrorType{code, std::move(location), std::move(detail)});
  }

  template <class U>
  [[nodiscard]] Result<U, Location> propagate() && {
    assert(!hasValue());
    if (!error_) return Result<U, Location>::failure(ARX_INTERNAL_ERROR, std::nullopt);
    return Result<U, Location>(std::move(*error_));
  }

  [[nodiscard]] explicit operator bool() const noexcept { return !error_; }
  [[nodiscard]] bool hasValue() const noexcept { return !error_; }
  [[nodiscard]] ArxReturnCode code() const noexcept { return error_ ? error_->code() : ARX_OK; }

  [[nodiscard]] const ErrorType* error() const noexcept { return error_ ? &*error_ : nullptr; }

 private:
  class AssignmentInvariantGuard {
   public:
    AssignmentInvariantGuard(Result& result, bool assigning_error) noexcept
        : result_(result), assigning_error_(assigning_error), exception_count_(std::uncaught_exceptions()) {}
    ~AssignmentInvariantGuard() {
      if (std::uncaught_exceptions() > exception_count_) result_.restoreInvariant(assigning_error_);
    }

   private:
    Result& result_;
    bool assigning_error_;
    int exception_count_;
  };

  Result(ErrorType error) noexcept(std::is_nothrow_move_constructible_v<ErrorType>) : error_(std::move(error)) {}

  void restoreInvariant(bool assigning_error) noexcept {
    if (!assigning_error || error_) return;
    try {
      error_.emplace(ErrorType{ARX_INTERNAL_ERROR});
    } catch (...) {
      std::terminate();
    }
  }

  std::optional<ErrorType> error_;

  template <class U, class OtherLocation>
  friend class Result;
};

}  // namespace pistoris
