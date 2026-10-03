// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#pragma once

#include <bit>
#include <cassert>
#include <cstdint>
#include <cstring>
#include <limits>
#include <new>
#include <span>
#include <stdexcept>
#include <string_view>
#include <type_traits>
#include <utility>
#include <vector>

static_assert(sizeof(float) == 4, "expected IEEE 754 single-precision");
static_assert(sizeof(double) == 8, "expected IEEE 754 double-precision");
static_assert(std::endian::native == std::endian::little, "only little-endian hosts supported");

namespace pistoris {

enum class CursorErrorKind : std::uint8_t {
  kOk = 0,
  kUnexpectedEof = 1,
  kBadAlloc = 2,
};

struct CursorError {
  std::size_t offset = 0;
  std::size_t needed = 0;
  CursorErrorKind kind = CursorErrorKind::kOk;
  std::uint32_t element = 0;
  std::size_t index = std::numeric_limits<std::size_t>::max();
  std::size_t subindex = std::numeric_limits<std::size_t>::max();
  bool secondary = false;
  std::string_view field;
};

struct CursorLocation {
  std::size_t offset = 0;
  std::uint32_t element = 0;
  std::size_t index = std::numeric_limits<std::size_t>::max();
  std::size_t subindex = std::numeric_limits<std::size_t>::max();
  bool secondary = false;
  std::string_view field;
};

struct ReadCursor {
  ReadCursor(const std::uint8_t* data, std::size_t size) : buf_(data, size), off_(0), ok_(true) {}

  explicit operator bool() const noexcept { return ok_; }
  CursorError error() const noexcept { return err_; }
  CursorLocation location() const noexcept { return location_; }
  std::size_t offset() const noexcept { return off_; }

  std::size_t remaining() const noexcept { return buf_.size() - off_; }

  template <class Element>
  ReadCursor& locate(Element element, std::string_view field,
                     std::size_t index = std::numeric_limits<std::size_t>::max(),
                     std::size_t subindex = std::numeric_limits<std::size_t>::max(), bool secondary = false) noexcept {
    static_assert(std::is_enum_v<Element>);
    if (ok_) {
      location_ = {
          off_,
          static_cast<std::uint32_t>(element),
          index,
          subindex,
          secondary,
          field,
      };
    }
    return *this;
  }

  template <class Element>
  [[nodiscard]] CursorLocation mark(Element element, std::string_view field,
                                    std::size_t index = std::numeric_limits<std::size_t>::max(),
                                    std::size_t subindex = std::numeric_limits<std::size_t>::max(),
                                    bool secondary = false) noexcept {
    locate(element, field, index, subindex, secondary);
    return location_;
  }

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
      ok_ = false;
      err_ = {
          off_,
          needed,
          CursorErrorKind::kUnexpectedEof,
          location_.element,
          location_.index,
          location_.subindex,
          location_.secondary,
          location_.field,
      };
    }
    return *this;
  }

  std::span<const std::uint8_t> buf_;
  std::size_t off_ = 0;
  bool ok_ = true;
  CursorError err_{};
  CursorLocation location_{};
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
    const auto* src = reinterpret_cast<const std::uint8_t*>(&val);
    return append(src, sizeof(T));
  }

  template <class T>
  WriteCursor& writeArray(const std::vector<T>& vals) noexcept {
    static_assert(std::is_trivially_copyable_v<T>);
    return writeN(vals.data(), vals.size());
  }

  template <class T>
  WriteCursor& writeN(const T* data, std::size_t count) noexcept {
    static_assert(std::is_trivially_copyable_v<T>);
    if (!ok_ || count == 0) return *this;
    assert(data != nullptr);
    if (count > std::numeric_limits<std::size_t>::max() / sizeof(T))
      return fail(std::numeric_limits<std::size_t>::max());
    const auto* src = reinterpret_cast<const std::uint8_t*>(data);
    return append(src, sizeof(T) * count);
  }

  WriteCursor& pad(std::size_t n) noexcept {
    if (!ok_ || n == 0) return *this;
    if (n > buf_.max_size() - buf_.size()) return fail(n);
    try {
      buf_.insert(buf_.end(), n, 0);
    } catch (const std::bad_alloc&) {
      fail(n);
    } catch (const std::length_error&) {
      fail(n);
    } catch (...) {
      fail(n);
    }
    return *this;
  }

 private:
  WriteCursor& append(const std::uint8_t* data, std::size_t count) noexcept {
    if (!ok_ || count == 0) return *this;
    if (count > buf_.max_size() - buf_.size()) return fail(count);
    try {
      buf_.insert(buf_.end(), data, data + count);
    } catch (const std::bad_alloc&) {
      fail(count);
    } catch (const std::length_error&) {
      fail(count);
    } catch (...) {
      fail(count);
    }
    return *this;
  }

  WriteCursor& fail(std::size_t needed) noexcept {
    if (ok_) {
      ok_ = false;
      err_ = {};
      err_.offset = buf_.size();
      err_.needed = needed;
      err_.kind = CursorErrorKind::kBadAlloc;
    }
    return *this;
  }

  std::vector<std::uint8_t> buf_;
  bool ok_ = true;
  CursorError err_{};
};

}  // namespace pistoris
