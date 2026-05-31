// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#pragma once

#include <bit>
#include <cstdint>
#include <cstring>
#include <span>
#include <type_traits>
#include <utility>
#include <vector>

static_assert(sizeof(float) == 4, "expected IEEE 754 single-precision");
static_assert(sizeof(double) == 8, "expected IEEE 754 double-precision");
static_assert(std::endian::native == std::endian::little, "only little-endian hosts supported");

namespace pistoris {

enum class CursorErrorKind : std::uint8_t {
  kOk            = 0,
  kUnexpectedEof = 1,
  kBadAlloc      = 2,
};

struct CursorError {
  std::size_t offset   = 0;
  std::size_t needed   = 0;
  CursorErrorKind kind = CursorErrorKind::kOk;
};

struct ReadCursor {
  ReadCursor(const std::uint8_t* data, std::size_t size) : buf_(data, size), off_(0), ok_(true) {}

  explicit operator bool() const noexcept { return ok_; }
  CursorError error() const noexcept { return err_; }

  std::size_t remaining() const noexcept { return buf_.size() - off_; }

  template <class T>
  ReadCursor& read(T& out) noexcept {
    static_assert(std::is_trivially_copyable_v<T>);
    if (!ok_) return *this;
    if (sizeof(T) > remaining()) return fail(sizeof(T));
    std::memcpy(&out, buf_.data() + off_, sizeof(T));
    off_ += sizeof(T);
    return *this;
  }

  template <class T>
  ReadCursor& readArray(std::vector<T>& out) noexcept {
    static_assert(std::is_trivially_copyable_v<T>);
    if (!ok_) return *this;
    if (out.empty()) return *this;
    if (out.size() > remaining() / sizeof(T))
      return fail((out.size() > SIZE_MAX / sizeof(T)) ? SIZE_MAX : sizeof(T) * out.size());

    std::memcpy(out.data(), buf_.data() + off_, sizeof(T) * out.size());
    off_ += sizeof(T) * out.size();
    return *this;
  }

  ReadCursor& skip(std::size_t n) noexcept {
    if (!ok_) return *this;
    if (n > remaining()) return fail(n);
    off_ += n;
    return *this;
  }

 private:
  ReadCursor& fail(std::size_t needed) noexcept {
    if (ok_) {
      ok_  = false;
      err_ = {off_, needed, CursorErrorKind::kUnexpectedEof};
    }
    return *this;
  }

  std::span<const std::uint8_t> buf_;
  std::size_t off_ = 0;
  bool ok_         = true;
  CursorError err_{};
};

struct WriteCursor {
  WriteCursor() = default;

  explicit operator bool() const noexcept { return ok_; }
  CursorError error() const noexcept { return err_; }

  std::size_t size() const noexcept { return buf_.size(); }

  std::vector<std::uint8_t> take() noexcept { return std::move(buf_); }

  template <class T>
  WriteCursor& write(const T& val) noexcept {
    static_assert(std::is_trivially_copyable_v<T>);
    if (!ok_) return *this;
    const auto* src = reinterpret_cast<const std::uint8_t*>(&val);
    try {
      buf_.insert(buf_.end(), src, src + sizeof(T));
    } catch (const std::bad_alloc&) {
      fail(sizeof(T));
    }
    return *this;
  }

  template <class T>
  WriteCursor& writeArray(const std::vector<T>& vals) noexcept {
    static_assert(std::is_trivially_copyable_v<T>);
    if (!ok_ || vals.empty()) return *this;
    const auto* src = reinterpret_cast<const std::uint8_t*>(vals.data());
    try {
      buf_.insert(buf_.end(), src, src + sizeof(T) * vals.size());
    } catch (const std::bad_alloc&) {
      fail(sizeof(T) * vals.size());
    }
    return *this;
  }

  template <class T>
  WriteCursor& writeN(const T* data, std::size_t count) noexcept {
    static_assert(std::is_trivially_copyable_v<T>);
    if (!ok_ || count == 0) return *this;
    const auto* src = reinterpret_cast<const std::uint8_t*>(data);
    try {
      buf_.insert(buf_.end(), src, src + sizeof(T) * count);
    } catch (const std::bad_alloc&) {
      fail(sizeof(T) * count);
    }
    return *this;
  }

  WriteCursor& pad(std::size_t n) noexcept {
    if (!ok_ || n == 0) return *this;
    try {
      buf_.insert(buf_.end(), n, 0);
    } catch (const std::bad_alloc&) {
      fail(n);
    }
    return *this;
  }

 private:
  WriteCursor& fail(std::size_t needed) noexcept {
    if (ok_) {
      ok_  = false;
      err_ = {buf_.size(), needed, CursorErrorKind::kBadAlloc};
    }
    return *this;
  }

  std::vector<std::uint8_t> buf_;
  bool ok_ = true;
  CursorError err_{};
};

}  // namespace pistoris
