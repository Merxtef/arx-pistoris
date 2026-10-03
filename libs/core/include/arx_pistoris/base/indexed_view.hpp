// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#pragma once

#include <cassert>
#include <compare>
#include <cstddef>
#include <iterator>
#include <ranges>

namespace pistoris {

class Level;
class Model;
class Animation;
class Ambiance;
class Cinematic;

template <class Value, class Tag>
class IndexedView : public std::ranges::view_interface<IndexedView<Value, Tag>> {
 public:
  class Iterator {
   public:
    using iterator_concept = std::random_access_iterator_tag;   // NOLINT(readability-identifier-naming)
    using iterator_category = std::random_access_iterator_tag;  // NOLINT(readability-identifier-naming)
    using value_type = Value;                                   // NOLINT(readability-identifier-naming)
    using difference_type = std::ptrdiff_t;                     // NOLINT(readability-identifier-naming)
    using reference = Value;                                    // NOLINT(readability-identifier-naming)

    Iterator() noexcept = default;

    [[nodiscard]] Value operator*() const noexcept {
      assert(getter_);
      return getter_(owner_, qualifier_, index_);
    }
    [[nodiscard]] Value operator[](difference_type offset) const noexcept { return *(*this + offset); }

    Iterator& operator++() noexcept {
      ++index_;
      return *this;
    }
    Iterator operator++(int) noexcept {
      Iterator previous = *this;
      ++*this;
      return previous;
    }
    Iterator& operator--() noexcept {
      --index_;
      return *this;
    }
    Iterator operator--(int) noexcept {
      Iterator previous = *this;
      --*this;
      return previous;
    }
    Iterator& operator+=(difference_type offset) noexcept {
      index_ = static_cast<std::size_t>(static_cast<difference_type>(index_) + offset);
      return *this;
    }
    Iterator& operator-=(difference_type offset) noexcept { return *this += -offset; }

    friend Iterator operator+(Iterator iterator, difference_type offset) noexcept { return iterator += offset; }
    friend Iterator operator+(difference_type offset, Iterator iterator) noexcept { return iterator += offset; }
    friend Iterator operator-(Iterator iterator, difference_type offset) noexcept { return iterator -= offset; }
    friend difference_type operator-(const Iterator& left, const Iterator& right) noexcept {
      return static_cast<difference_type>(left.index_) - static_cast<difference_type>(right.index_);
    }

    friend bool operator==(const Iterator&, const Iterator&) noexcept = default;
    friend std::strong_ordering operator<=>(const Iterator& left, const Iterator& right) noexcept {
      return left.index_ <=> right.index_;
    }

   private:
    friend class IndexedView;

    using Getter = Value (*)(const void*, std::size_t, std::size_t) noexcept;

    Iterator(const void* owner, std::size_t qualifier, std::size_t index, Getter getter) noexcept
        : owner_(owner), qualifier_(qualifier), index_(index), getter_(getter) {}

    const void* owner_ = nullptr;
    std::size_t qualifier_ = 0;
    std::size_t index_ = 0;
    Getter getter_ = nullptr;
  };

  IndexedView() noexcept = default;

  [[nodiscard]] Iterator begin() const noexcept { return {owner_, qualifier_, 0, getter_}; }
  [[nodiscard]] Iterator end() const noexcept { return {owner_, qualifier_, size_, getter_}; }
  [[nodiscard]] std::size_t size() const noexcept { return size_; }
  [[nodiscard]] Value operator[](std::size_t index) const noexcept {
    assert(index < size_);
    return getter_(owner_, qualifier_, index);
  }

 private:
  friend class Level;
  friend class Model;
  friend class Animation;
  friend class Ambiance;
  friend class Cinematic;

  using Getter = Value (*)(const void*, std::size_t, std::size_t) noexcept;

  IndexedView(const void* owner, std::size_t qualifier, std::size_t size, Getter getter) noexcept
      : owner_(owner), qualifier_(qualifier), size_(size), getter_(getter) {}

  const void* owner_ = nullptr;
  std::size_t qualifier_ = 0;
  std::size_t size_ = 0;
  Getter getter_ = nullptr;
};

}  // namespace pistoris

template <class Value, class Tag>
// NOLINTNEXTLINE(readability-identifier-naming)
inline constexpr bool std::ranges::enable_borrowed_range<pistoris::IndexedView<Value, Tag>> = true;
