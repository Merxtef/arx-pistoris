// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#pragma once

#include "binding_utils.h"
#include "resource_types.h"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <initializer_list>
#include <limits>
#include <map>
#include <memory>
#include <nanobind/stl/optional.h>
#include <nanobind/stl/pair.h>
#include <nanobind/stl/shared_ptr.h>
#include <optional>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <variant>
#include <vector>

namespace pistoris::python {

struct ElementToken {
  std::optional<std::size_t> index;
};

[[noreturn]] inline void throwInvalidReference();

class CollectionContext {
 public:
  explicit CollectionContext(std::size_t value) : value_(value) {}
  CollectionContext(std::size_t value, std::shared_ptr<ElementToken> token) : value_(value), token_(std::move(token)) {}

  [[nodiscard]] std::size_t value() const {
    if (!token_) return value_;
    if (!token_->index) throwInvalidReference();
    return *token_->index;
  }

  [[nodiscard]] bool sameIdentity(const CollectionContext& other) const noexcept {
    if (token_ || other.token_) return token_ == other.token_;
    return value_ == other.value_;
  }

  [[nodiscard]] Py_hash_t identityHash() const {
    return token_ ? valueHash(true, reinterpret_cast<std::uintptr_t>(token_.get())) : valueHash(false, value_);
  }

 private:
  std::size_t value_;
  std::shared_ptr<ElementToken> token_;
};

class CollectionTracker {
 public:
  [[nodiscard]] std::shared_ptr<ElementToken> track(std::size_t index) {
    if (tokens_.size() >= next_prune_size_) prune();
    auto token = std::make_shared<ElementToken>(ElementToken{index});
    tokens_.push_back(token);
    return token;
  }

  [[nodiscard]] std::shared_ptr<ElementToken> trackCanonical(std::size_t index) {
    prune();
    for (const std::weak_ptr<ElementToken>& weak : tokens_) {
      if (auto token = weak.lock(); token && token->index == index) return token;
    }
    auto token = std::make_shared<ElementToken>(ElementToken{index});
    tokens_.push_back(token);
    return token;
  }

  void insert(std::size_t index) {
    visit([index](ElementToken& token) {
      if (token.index && *token.index >= index) ++*token.index;
    });
  }

  void remove(std::size_t index) {
    visit([index](ElementToken& token) {
      if (!token.index) return;
      if (*token.index == index) {
        token.index.reset();
      } else if (*token.index > index) {
        --*token.index;
      }
    });
  }

  void removeMany(std::vector<std::size_t> indices) {
    if (indices.empty()) return;
    std::ranges::sort(indices);
    indices.erase(std::unique(indices.begin(), indices.end()), indices.end());
    visit([&indices](ElementToken& token) {
      if (!token.index) return;
      const auto removed = std::lower_bound(indices.begin(), indices.end(), *token.index);
      if (removed != indices.end() && *removed == *token.index) {
        token.index.reset();
      } else {
        *token.index -= static_cast<std::size_t>(removed - indices.begin());
      }
    });
  }

  void invalidateIndex(std::size_t index) {
    visit([index](ElementToken& token) {
      if (token.index == index) token.index.reset();
    });
  }

  void invalidate() {
    visit([](ElementToken& token) { token.index.reset(); });
  }

 private:
  template <class Operation>
  void visit(Operation operation) {
    auto next = tokens_.begin();
    for (auto current = tokens_.begin(); current != tokens_.end(); ++current) {
      if (auto token = current->lock()) {
        operation(*token);
        *next++ = *current;
      }
    }
    tokens_.erase(next, tokens_.end());
    const std::size_t doubled = tokens_.size() > std::numeric_limits<std::size_t>::max() / 2U
                                    ? std::numeric_limits<std::size_t>::max()
                                    : tokens_.size() * 2U;
    next_prune_size_ = std::max(kMinPruneSize, doubled);
  }

  void prune() {
    visit([](ElementToken&) {});
  }

  std::vector<std::weak_ptr<ElementToken>> tokens_;
  static constexpr std::size_t kMinPruneSize = 64;
  std::size_t next_prune_size_ = kMinPruneSize;
};

class NestedCollectionTrackers {
 public:
  [[nodiscard]] CollectionTracker& operator[](std::size_t parent) { return values_[parent]; }

  void invalidate(std::size_t parent) {
    const auto found = values_.find(parent);
    if (found != values_.end()) found->second.invalidate();
  }

  void removeElement(std::size_t parent, std::size_t index) {
    const auto found = values_.find(parent);
    if (found != values_.end()) found->second.remove(index);
  }

  void removeParent(std::size_t parent) {
    const auto removed = values_.find(parent);
    if (removed != values_.end()) {
      removed->second.invalidate();
      values_.erase(removed);
    }

    auto shifted = values_.upper_bound(parent);
    while (shifted != values_.end()) {
      auto node = values_.extract(shifted++);
      --node.key();
      values_.insert(std::move(node));
    }
  }

  void invalidateParent(std::size_t parent) {
    const auto found = values_.find(parent);
    if (found != values_.end()) {
      found->second.invalidate();
      values_.erase(found);
    }
  }

  void invalidate() {
    for (auto& [index, tracker] : values_) tracker.invalidate();
    values_.clear();
  }

 private:
  std::map<std::size_t, CollectionTracker> values_;
};

struct ModelTracking {
  CollectionTracker vertices;
  CollectionTracker faces;
  CollectionTracker textures;
  CollectionTracker bones;
  CollectionTracker action_points;
  CollectionTracker selections;
  CollectionTracker selection_leading_vertices;

  void invalidate() {
    vertices.invalidate();
    faces.invalidate();
    textures.invalidate();
    bones.invalidate();
    action_points.invalidate();
    selections.invalidate();
    selection_leading_vertices.invalidate();
  }
};

struct AnimationTracking {
  CollectionTracker keyframes;
  CollectionTracker sounds;
  CollectionTracker groups;
  NestedCollectionTrackers group_transforms;

  void invalidate() {
    keyframes.invalidate();
    sounds.invalidate();
    groups.invalidate();
    group_transforms.invalidate();
  }
};

struct AmbianceTracking {
  CollectionTracker tracks;
  CollectionTracker sounds;
  NestedCollectionTrackers keys;
  NestedCollectionTrackers spatial_automations;

  void invalidate() {
    tracks.invalidate();
    sounds.invalidate();
    keys.invalidate();
    spatial_automations.invalidate();
  }
};

struct CinematicTracking {
  CollectionTracker illustrations;
  CollectionTracker keyframes;
  CollectionTracker keyframe_lights;
  CollectionTracker textures;
  std::array<CollectionTracker, 2> sounds;
  CollectionTracker languages;

  void invalidate() {
    illustrations.invalidate();
    keyframes.invalidate();
    keyframe_lights.invalidate();
    textures.invalidate();
    for (auto& tracker : sounds) tracker.invalidate();
    languages.invalidate();
  }
};

struct LevelTracking {
  CollectionTracker vertices;
  CollectionTracker faces;
  CollectionTracker textures;
  CollectionTracker rooms;
  CollectionTracker portals;
  CollectionTracker anchors;
  CollectionTracker anchor_connections;
  CollectionTracker nav_vertices;
  CollectionTracker nav_triangles;
  CollectionTracker lights;
  CollectionTracker player_spawn;
  CollectionTracker entities;
  CollectionTracker fogs;
  CollectionTracker zones;
  CollectionTracker paths;
  NestedCollectionTrackers path_nodes;

  void invalidate() {
    vertices.invalidate();
    faces.invalidate();
    textures.invalidate();
    rooms.invalidate();
    portals.invalidate();
    anchors.invalidate();
    anchor_connections.invalidate();
    nav_vertices.invalidate();
    nav_triangles.invalidate();
    lights.invalidate();
    player_spawn.invalidate();
    entities.invalidate();
    fogs.invalidate();
    zones.invalidate();
    paths.invalidate();
    path_nodes.invalidate();
  }
};

template <class Resource, class Tracking>
// Live element references share ownership of this binding-only wrapper.
// NOLINTNEXTLINE(misc-multiple-inheritance)
class TrackedResource : public Resource, public std::enable_shared_from_this<TrackedResource<Resource, Tracking>> {
 public:
  TrackedResource() = default;
  explicit TrackedResource(const Resource& resource) : Resource(resource) {}
  explicit TrackedResource(Resource&& resource) : Resource(std::move(resource)) {}

  TrackedResource(const TrackedResource&) = delete;
  TrackedResource(TrackedResource&&) = delete;
  TrackedResource& operator=(const TrackedResource&) = delete;
  TrackedResource& operator=(TrackedResource&&) = delete;

  Tracking tracking;
};

using PythonModel = TrackedResource<Model, ModelTracking>;
using PythonAnimation = TrackedResource<Animation, AnimationTracking>;
using PythonAmbiance = TrackedResource<Ambiance, AmbianceTracking>;
using PythonCinematic = TrackedResource<Cinematic, CinematicTracking>;
using PythonLevel = TrackedResource<Level, LevelTracking>;

struct ModelImportOutput {
  std::shared_ptr<PythonModel> model;
  std::vector<std::shared_ptr<PythonAnimation>> animations;
  std::vector<std::string> texture_source_paths;
  std::vector<AnimationSoundSourceReference> sound_sources;
  std::optional<ArxAnimationConversionReport> animation_report;
};

struct ModelFaceAccess {
  using Owner = PythonModel;
  using Value = ModelFace;
  static std::size_t size(const Owner& owner, std::size_t);
  static CollectionTracker& tracker(Owner& owner, std::size_t);
  static Value get(const Owner& owner, std::size_t, std::size_t index);
  static void set(Owner& owner, std::size_t, std::size_t index, const Value& value);
  static void append(Owner& owner, std::size_t, const Value& value);
  static void remove(Owner& owner, std::size_t, std::size_t index);
};

struct ModelFaceCornerAccess {
  using ParentAccess = ModelFaceAccess;
  using Value = ModelCorner;
  static std::size_t size(const ModelFace&) { return 3; }
  static Value get(const ModelFace& face, std::size_t index) { return face.corners[index]; }
  static void set(ModelFace& face, std::size_t index, const Value& value) { face.corners[index] = value; }
};

struct LevelFaceAccess {
  using Owner = PythonLevel;
  using Value = LevelFace;
  static std::size_t size(const Owner& owner, std::size_t);
  static CollectionTracker& tracker(Owner& owner, std::size_t);
  static Value get(const Owner& owner, std::size_t, std::size_t index);
  static void set(Owner& owner, std::size_t, std::size_t index, const Value& value);
  static void append(Owner& owner, std::size_t, const Value& value);
  static void remove(Owner& owner, std::size_t, std::size_t index);
  static void validate(const Owner& owner, std::size_t);
};

struct LevelFaceCornerAccess {
  using ParentAccess = LevelFaceAccess;
  using Value = LevelCorner;
  static std::size_t size(const LevelFace&) { return 3; }
  static Value get(const LevelFace& face, std::size_t index) { return face.corners[index]; }
  static void set(LevelFace& face, std::size_t index, const Value& value) { face.corners[index] = value; }
};

template <class Resource, class Tracking>
std::shared_ptr<TrackedResource<Resource, Tracking>> tracked(Resource&& resource) {
  return std::make_shared<TrackedResource<Resource, Tracking>>(std::forward<Resource>(resource));
}

template <class Resource, class PythonResource>
BorrowedPointers<Resource> borrowResourcePointers(const nb::sequence& objects) {
  BorrowedPointers<Resource> result;
  result.owners = materializeSequence(objects);
  result.values.reserve(nb::len(result.owners));
  for (std::size_t index = 0; index < nb::len(result.owners); ++index) {
    result.values.push_back(&nb::cast<const PythonResource&>(result.owners[index]));
  }
  return result;
}

[[noreturn]] inline void throwInvalidReference() {
  PyErr_SetString(PyExc_ReferenceError, "referenced element no longer exists");
  throw nb::python_error();
}

template <class Key>
[[noreturn]] void throwMissingKey(const Key& key) {
  const nb::object value = nb::cast(key);
  const nb::tuple arguments = nb::make_tuple(value);
  PyErr_SetObject(PyExc_KeyError, arguments.ptr());
  throw nb::python_error();
}

inline std::size_t sequenceIndex(std::int64_t requested, std::size_t size) {
  const auto signed_size = static_cast<std::int64_t>(size);
  const auto index = requested < 0 ? requested + signed_size : requested;
  if (index < 0 || index >= signed_size) throw nb::index_error();
  return static_cast<std::size_t>(index);
}

inline std::string resourceRepr(std::string_view name, std::string_view resource_path,
                                std::initializer_list<std::pair<std::string_view, std::size_t>> counts) {
  std::string result = "<pistoris.";
  result += name;
  if (!resource_path.empty()) {
    result += " resource_path=";
    result += nb::repr(nb::cast(resource_path)).c_str();
  }
  for (const auto& [label, count] : counts) {
    result += " ";
    result += label;
    result += "=";
    result += std::to_string(count);
  }
  return result + ">";
}

template <class Index>
Index sequenceTargetIndex(std::int64_t requested, std::size_t size) {
  return static_cast<Index>(sequenceIndex(requested, size));
}

template <class Value>
void bindRecordMediaField(nb::class_<Value>& binding, const char* field, std::vector<std::uint8_t> Value::* member,
                          bool empty_is_none) {
  const std::string size_name = std::string("_pistoris_") + field + "_size";
  const std::string equals_name = std::string("_pistoris_") + field + "_equals";
  binding
      .def(size_name.c_str(),
           [member, empty_is_none](const Value& value) -> std::optional<std::size_t> {
             const auto& media = value.*member;
             if (empty_is_none && media.empty()) return std::nullopt;
             return media.size();
           })
      .def(equals_name.c_str(),
           [member](const Value& value, const Value& other) { return value.*member == other.*member; });
}

template <class Collection>
bool sequenceContains(const Collection& self, nb::handle value) {
  for (std::size_t index = 0; index < self.size(); ++index) {
    const auto element = nb::cast(self.at(static_cast<std::int64_t>(index)));
    const int equal = PyObject_RichCompareBool(element.ptr(), value.ptr(), Py_EQ);
    if (equal < 0) throw nb::python_error();
    if (equal != 0) return true;
  }
  return false;
}

template <class Collection>
std::size_t sequenceCount(const Collection& self, nb::handle value) {
  std::size_t count = 0;
  for (std::size_t index = 0; index < self.size(); ++index) {
    const auto element = nb::cast(self.at(static_cast<std::int64_t>(index)));
    const int equal = PyObject_RichCompareBool(element.ptr(), value.ptr(), Py_EQ);
    if (equal < 0) throw nb::python_error();
    if (equal != 0) ++count;
  }
  return count;
}

inline std::pair<std::size_t, std::size_t> sequenceBounds(std::int64_t start, std::int64_t stop, std::size_t size) {
  const auto signed_size = static_cast<std::int64_t>(size);
  if (start < 0) start = std::max<std::int64_t>(0, start + signed_size);
  if (stop < 0) stop = std::max<std::int64_t>(0, stop + signed_size);
  start = std::clamp(start, std::int64_t{0}, signed_size);
  stop = std::clamp(stop, std::int64_t{0}, signed_size);
  return {static_cast<std::size_t>(start), static_cast<std::size_t>(stop)};
}

template <class Collection>
std::size_t sequenceFind(const Collection& self, nb::handle value, std::int64_t start, std::int64_t stop) {
  const auto [first, last] = sequenceBounds(start, stop, self.size());
  for (std::size_t index = first; index < last; ++index) {
    const auto element = nb::cast(self.at(static_cast<std::int64_t>(index)));
    const int equal = PyObject_RichCompareBool(element.ptr(), value.ptr(), Py_EQ);
    if (equal < 0) throw nb::python_error();
    if (equal != 0) return index;
  }
  throw nb::value_error("value is not in sequence");
}

template <class Collection>
nb::object reversedSequence(const Collection& self) {
  nb::list values;
  for (std::size_t index = self.size(); index > 0; --index) {
    values.append(self.at(static_cast<std::int64_t>(index - 1)));
  }
  return values.attr("__iter__")();
}

template <class Collection>
void bindSequenceProtocol(nb::class_<Collection>& binding, const char* value_name) {
  const std::string reversed_signature = std::string("def __reversed__(self) -> Iterator[") + value_name + "]";
  binding.def("__contains__", &sequenceContains<Collection>, nb::arg("value"))
      .def("count", &sequenceCount<Collection>, nb::arg("value"))
      .def(
          "index",
          [](const Collection& self, nb::handle value, std::int64_t start, const std::optional<std::int64_t>& stop) {
            return sequenceFind(self, value, start, stop.value_or(std::numeric_limits<std::int64_t>::max()));
          },
          nb::arg("value"),
          nb::arg("start") = 0,
          nb::arg("stop") = nb::none())
      .def("__reversed__", &reversedSequence<Collection>, nb::sig(reversed_signature.c_str()));
  registerSequence(binding);
}

template <class Access, class = void>
struct DetachedParentType {
  using Type = typename Access::Value;
  static constexpr bool available = false;  // NOLINT(readability-identifier-naming)
};

template <class Access>
struct DetachedParentType<Access, std::void_t<typename Access::DetachedParent>> {
  using Type = typename Access::DetachedParent;
  static constexpr bool available = true;  // NOLINT(readability-identifier-naming)
};

template <class Access, class = void>
struct CollectionValueType {
  using Type = typename Access::Value;
  static constexpr bool custom = false;  // NOLINT(readability-identifier-naming)
};

template <class Access>
struct CollectionValueType<Access, std::void_t<typename Access::CollectionValue>> {
  using Type = typename Access::CollectionValue;
  static constexpr bool custom = true;  // NOLINT(readability-identifier-naming)
};

struct ElementDisplayLabel {
  std::string_view field;
  std::string value;
};

template <class Access>
class ElementRef {
 public:
  using Owner = typename Access::Owner;
  using Value = typename Access::Value;

  ElementRef(std::shared_ptr<Owner> owner, std::shared_ptr<ElementToken> token, CollectionContext context)
      : backing_(ResourceBacking{std::move(owner), std::move(context)}), token_(std::move(token)) {}
  ElementRef(const nb::object& owner, std::size_t index)
      : backing_(DetachedBacking{owner, detachedRevision(owner)}),
        token_(std::make_shared<ElementToken>(ElementToken{index})) {}

  [[nodiscard]] std::size_t index() const {
    validateBacking();
    if (!token_->index) throwInvalidReference();
    return *token_->index;
  }

  [[nodiscard]] Value copy() const {
    if (const auto* resource = std::get_if<ResourceBacking>(&backing_)) {
      return Access::get(*resource->owner, resource->context.value(), index());
    }
    if constexpr (DetachedParentType<Access>::available) return Access::get(detachedOwner(), index());
    throw nb::type_error("this collection does not support detached values");
  }

  [[nodiscard]] std::optional<ElementDisplayLabel> displayLabel() const {
    if (const auto* resource = std::get_if<ResourceBacking>(&backing_)) {
      if constexpr (requires { Access::displayLabel(*resource->owner, resource->context.value(), index()); }) {
        return Access::displayLabel(*resource->owner, resource->context.value(), index());
      }
    }
    return std::nullopt;
  }

  void set(const Value& value) {
    if (auto* resource = std::get_if<ResourceBacking>(&backing_)) {
      Access::set(*resource->owner, resource->context.value(), index(), value);
    } else if constexpr (DetachedParentType<Access>::available) {
      Access::set(detachedOwner(), index(), value);
    } else {
      throw nb::type_error("this collection does not support detached values");
    }
  }

  [[nodiscard]] bool resourceBacked() const noexcept { return std::holds_alternative<ResourceBacking>(backing_); }
  [[nodiscard]] Owner& owner() const { return *std::get<ResourceBacking>(backing_).owner; }
  [[nodiscard]] std::size_t context() const { return std::get<ResourceBacking>(backing_).context.value(); }

  [[nodiscard]] bool sameIdentity(const ElementRef& other) const {
    if (backing_.index() != other.backing_.index()) return false;
    if constexpr (requires { Access::hashable; }) {
      if constexpr (Access::hashable) {
        if (const auto* resource = std::get_if<ResourceBacking>(&backing_)) {
          const auto& other_resource = std::get<ResourceBacking>(other.backing_);
          return resource->owner.get() == other_resource.owner.get() &&
                 resource->context.sameIdentity(other_resource.context) && token_ == other.token_;
        }
        return std::get<DetachedBacking>(backing_).owner.ptr() ==
                   std::get<DetachedBacking>(other.backing_).owner.ptr() &&
               token_ == other.token_;
      }
    }
    if (index() != other.index()) return false;
    if (const auto* resource = std::get_if<ResourceBacking>(&backing_)) {
      const auto& other_resource = std::get<ResourceBacking>(other.backing_);
      return resource->owner.get() == other_resource.owner.get() &&
             resource->context.value() == other_resource.context.value();
    }
    return std::get<DetachedBacking>(backing_).owner.ptr() == std::get<DetachedBacking>(other.backing_).owner.ptr();
  }

  [[nodiscard]] Py_hash_t identityHash() const {
    if (const auto* resource = std::get_if<ResourceBacking>(&backing_)) {
      return valueHash(reinterpret_cast<std::uintptr_t>(resource->owner.get()),
                       resource->context.identityHash(),
                       reinterpret_cast<std::uintptr_t>(token_.get()));
    }
    const auto& detached = std::get<DetachedBacking>(backing_);
    return valueHash(reinterpret_cast<std::uintptr_t>(detached.owner.ptr()),
                     reinterpret_cast<std::uintptr_t>(token_.get()));
  }

 private:
  struct ResourceBacking {
    std::shared_ptr<Owner> owner;
    CollectionContext context;
  };

  struct DetachedBacking {
    nb::object owner;
    std::optional<std::size_t> revision;
  };

  using DetachedParent = typename DetachedParentType<Access>::Type;

  [[nodiscard]] DetachedParent& detachedOwner() const {
    return nb::cast<DetachedParent&>(std::get<DetachedBacking>(backing_).owner);
  }

  [[nodiscard]] static std::optional<std::size_t> detachedRevision(const nb::object& owner) {
    if constexpr (DetachedParentType<Access>::available &&
                  requires(const DetachedParent& value) { Access::revision(value); }) {
      return Access::revision(nb::cast<const DetachedParent&>(owner));
    }
    return std::nullopt;
  }

  void validateBacking() const {
    if (const auto* resource = std::get_if<ResourceBacking>(&backing_)) {
      (void)resource->context.value();
      return;
    }
    const auto& detached = std::get<DetachedBacking>(backing_);
    if constexpr (DetachedParentType<Access>::available &&
                  requires(const DetachedParent& value) { Access::revision(value); }) {
      if (detached.revision && *detached.revision != Access::revision(detachedOwner())) throwInvalidReference();
    }
  }

  std::variant<ResourceBacking, DetachedBacking> backing_;
  std::shared_ptr<ElementToken> token_;
};

template <class Access>
class ElementCollection {
 public:
  using Owner = typename Access::Owner;

  ElementCollection(std::shared_ptr<Owner> owner, std::size_t context = 0)
      : backing_(ResourceBacking{std::move(owner), CollectionContext(context)}) {}
  ElementCollection(std::shared_ptr<Owner> owner, std::size_t context, CollectionTracker& context_tracker)
      : backing_(ResourceBacking{std::move(owner), CollectionContext(context, context_tracker.track(context))}) {}
  explicit ElementCollection(nb::object owner) : backing_(std::move(owner)) {}

  [[nodiscard]] std::size_t size() const {
    if (const auto* resource = std::get_if<ResourceBacking>(&backing_)) {
      return Access::size(*resource->owner, resource->context.value());
    }
    if constexpr (DetachedParentType<Access>::available) {
      using DetachedParent = typename DetachedParentType<Access>::Type;
      return Access::size(nb::cast<const DetachedParent&>(std::get<nb::object>(backing_)));
    }
    throw nb::type_error("this collection does not support detached values");
  }

  [[nodiscard]] ElementRef<Access> at(std::int64_t requested) const {
    const auto index = sequenceIndex(requested, size());
    if (const auto* detached = std::get_if<nb::object>(&backing_)) return {*detached, index};
    const auto& resource = std::get<ResourceBacking>(backing_);
    const auto context = resource.context.value();
    if constexpr (requires { Access::elementIndex(*resource.owner, context, index); }) {
      const auto element = Access::elementIndex(*resource.owner, context, index);
      auto& tracker = Access::tracker(*resource.owner, context);
      if constexpr (requires { Access::hashable; }) {
        if constexpr (Access::hashable) {
          return {resource.owner, tracker.trackCanonical(element), resource.context};
        }
      }
      return {resource.owner, tracker.track(element), resource.context};
    } else {
      auto& tracker = Access::tracker(*resource.owner, context);
      if constexpr (requires { Access::hashable; }) {
        if constexpr (Access::hashable) {
          return {resource.owner, tracker.trackCanonical(index), resource.context};
        }
      }
      return {resource.owner, tracker.track(index), resource.context};
    }
  }

  [[nodiscard]] const std::shared_ptr<Owner>& owner() const { return std::get<ResourceBacking>(backing_).owner; }
  [[nodiscard]] std::size_t context() const { return std::get<ResourceBacking>(backing_).context.value(); }

 private:
  struct ResourceBacking {
    std::shared_ptr<Owner> owner;
    CollectionContext context;
  };
  std::variant<ResourceBacking, nb::object> backing_;
};

template <class Access>
class ElementIterator {
 public:
  explicit ElementIterator(ElementCollection<Access> collection) : collection_(std::move(collection)) {}

  [[nodiscard]] ElementRef<Access> next() {
    if (index_ >= collection_.size()) throw nb::stop_iteration();
    return collection_.at(static_cast<std::int64_t>(index_++));
  }

 private:
  ElementCollection<Access> collection_;
  std::size_t index_ = 0;
};

struct ElementIdentityNames {
  const char* reference_property = "index";
};

template <class Access>
nb::class_<ElementRef<Access>> bindElementCollection(nb::module_& module, const char* reference_name,
                                                     const char* collection_name, const char* public_reference_name,
                                                     ElementIdentityNames identity = {}) {
  const std::string iterator_name = std::string("_") + collection_name + "Iterator";
  const std::string slice_signature =
      std::string("def __getitem__(self, slice: slice) -> list[") + reference_name + "]";
  const std::string iterator_signature = std::string("def __iter__(self) -> Iterator[") + reference_name + "]";
  std::string public_collection_name = public_reference_name;
  public_collection_name.resize(public_collection_name.size() - 3);
  public_collection_name += "Collection";
  std::string public_value_name = public_reference_name;
  public_value_name.resize(public_value_name.size() - 3);
  if constexpr (requires { Access::collection_value_name; }) public_value_name = Access::collection_value_name;
  const std::string extend_signature =
      std::string("def extend(self, values: Iterable[") + public_value_name + "]) -> None";
  nb::class_<ElementIterator<Access>>(module, iterator_name.c_str())
      .def("__iter__", [](ElementIterator<Access>& self) -> ElementIterator<Access>& { return self; })
      .def("__next__", &ElementIterator<Access>::next);
  auto collection = nb::class_<ElementCollection<Access>>(module, collection_name, [] {
    if constexpr (requires { typename Access::Key; }) {
      return "A live keyed collection of resource elements. Structural edits may invalidate saved references.";
    }
    return "A live sequence of resource elements. Structural edits may invalidate saved references.";
  }());
  collection.def("__len__", &ElementCollection<Access>::size)
      .def(
          "__iter__",
          [](const ElementCollection<Access>& self) { return ElementIterator<Access>(self); },
          nb::sig(iterator_signature.c_str()))
      .def("__repr__", [public_collection_name](const ElementCollection<Access>& self) {
        return std::string("<") + public_collection_name + " len=" + std::to_string(self.size()) + ">";
      });
  if constexpr (requires { typename Access::Key; }) {
    using Key = typename Access::Key;
    collection
        .def(
            "__getitem__",
            [](const ElementCollection<Access>& self, const Key& key) {
              return self.at(static_cast<std::int64_t>(Access::positionForKey(*self.owner(), self.context(), key)));
            },
            nb::arg("key"))
        .def(
            "__contains__",
            [](const ElementCollection<Access>& self, const Key& key) {
              try {
                (void)Access::positionForKey(*self.owner(), self.context(), key);
                return true;
              } catch (const nb::python_error&) {
                if (!PyErr_ExceptionMatches(PyExc_KeyError)) throw;
                PyErr_Clear();
                return false;
              }
            },
            nb::arg("key"))
        .def("__contains__", &sequenceContains<ElementCollection<Access>>, nb::arg("value"));
    if constexpr (requires(typename Access::Owner& owner, std::size_t context, const Key& key) {
                    Access::removeKey(owner, context, key);
                  }) {
      collection.def(
          "__delitem__",
          [](const ElementCollection<Access>& self, const Key& key) {
            Access::removeKey(*self.owner(), self.context(), key);
          },
          nb::arg("key"));
    }
  } else {
    collection.def("__getitem__", &ElementCollection<Access>::at, nb::arg("index"))
        .def(
            "__getitem__",
            [](const ElementCollection<Access>& self, const nb::slice& slice) {
              auto [start, stop, step, length] = slice.compute(self.size());
              (void)stop;
              nb::list result;
              for (std::size_t index = 0; index < length; ++index) {
                result.append(self.at(start));
                start += step;
              }
              return result;
            },
            nb::arg("slice"),
            nb::sig(slice_signature.c_str()));
    using CollectionValue = typename CollectionValueType<Access>::Type;
    if constexpr (CollectionValueType<Access>::custom) {
      collection.def(
          "__setitem__",
          [](const ElementCollection<Access>& self, std::int64_t requested, const CollectionValue& value) {
            Access::setCollection(*self.owner(), self.context(), sequenceIndex(requested, self.size()), value);
          },
          nb::arg("index"),
          nb::arg("value"));
    } else if constexpr (requires(typename Access::Owner& owner,
                                  std::size_t context,
                                  std::size_t index,
                                  const typename Access::Value& value) { Access::set(owner, context, index, value); }) {
      collection.def(
          "__setitem__",
          [](const ElementCollection<Access>& self, std::int64_t index, const typename Access::Value& value) {
            auto element = self.at(index);
            element.set(value);
          },
          nb::arg("index"),
          nb::arg("value"));
    }
    if constexpr (CollectionValueType<Access>::custom) {
      collection.def(
          "append",
          [](const ElementCollection<Access>& self, const CollectionValue& value) {
            Access::appendCollection(*self.owner(), self.context(), value);
          },
          nb::arg("value"));
    } else if constexpr (requires(typename Access::Owner& owner,
                                  std::size_t context,
                                  const typename Access::Value& value) { Access::append(owner, context, value); }) {
      collection.def(
          "append",
          [](const ElementCollection<Access>& self, const typename Access::Value& value) {
            Access::append(*self.owner(), self.context(), value);
          },
          nb::arg("value"));
    }
    if constexpr (CollectionValueType<Access>::custom) {
      collection.def(
          "extend",
          [](const ElementCollection<Access>& self, const nb::iterable& values) {
            const auto materialized = materializeIterable(values);
            for (nb::handle value : materialized) {
              Access::appendCollection(*self.owner(), self.context(), nb::cast<CollectionValue>(value));
            }
          },
          nb::arg("values"),
          nb::sig(extend_signature.c_str()));
    } else if constexpr (requires(typename Access::Owner& owner,
                                  std::size_t context,
                                  const std::vector<typename Access::Value>& values) {
                           Access::extend(owner, context, values);
                         }) {
      collection.def(
          "extend",
          [](const ElementCollection<Access>& self, const nb::iterable& values) {
            std::vector<typename Access::Value> materialized;
            for (nb::handle value : values) materialized.push_back(nb::cast<typename Access::Value>(value));
            Access::extend(*self.owner(), self.context(), materialized);
          },
          nb::arg("values"),
          nb::sig(extend_signature.c_str()));
    } else if constexpr (requires(typename Access::Owner& owner,
                                  std::size_t context,
                                  const typename Access::Value& value) { Access::append(owner, context, value); }) {
      collection.def(
          "extend",
          [](const ElementCollection<Access>& self, const nb::iterable& values) {
            const auto materialized = materializeIterable(values);
            for (nb::handle value : materialized) {
              Access::append(*self.owner(), self.context(), nb::cast<typename Access::Value>(value));
            }
          },
          nb::arg("values"),
          nb::sig(extend_signature.c_str()));
    }
    if constexpr (requires(typename Access::Owner& owner, std::size_t context, std::size_t index) {
                    Access::remove(owner, context, index);
                  }) {
      collection.def(
          "__delitem__",
          [](const ElementCollection<Access>& self, std::int64_t requested) {
            Access::remove(*self.owner(), self.context(), sequenceIndex(requested, self.size()));
          },
          nb::arg("index"));
    }
  }
  if constexpr (requires(const typename Access::Owner& owner, std::size_t context, std::string_view name) {
                  Access::positionForName(owner, context, name);
                }) {
    collection.def(
        "by_name",
        [](const ElementCollection<Access>& self, std::string_view name) {
          return self.at(static_cast<std::int64_t>(Access::positionForName(*self.owner(), self.context(), name)));
        },
        nb::arg("name"));
  }
  if constexpr (requires(const typename Access::Owner& owner, std::size_t context, std::string_view path) {
                  Access::positionForPath(owner, context, path);
                }) {
    collection.def(
        "by_path",
        [](const ElementCollection<Access>& self, std::string_view path) {
          return self.at(static_cast<std::int64_t>(Access::positionForPath(*self.owner(), self.context(), path)));
        },
        nb::arg("path"));
  }
  if constexpr (requires(typename Access::Owner& owner, std::size_t context) { Access::clear(owner, context); }) {
    collection.def("clear",
                   [](const ElementCollection<Access>& self) { Access::clear(*self.owner(), self.context()); });
  }
  if constexpr (requires(const typename Access::Owner& owner, std::size_t context) {
                  Access::validate(owner, context);
                }) {
    collection.def("validate", [](const ElementCollection<Access>& self) {
      nb::gil_scoped_release release;
      Access::validate(*self.owner(), self.context());
    });
  }
  if constexpr (requires(typename Access::Owner& owner, std::size_t context) { Access::compact(owner, context); }) {
    collection.def("compact", [](const ElementCollection<Access>& self) {
      return Access::compact(*self.owner(), self.context());
    });
  }
  if constexpr (requires(typename Access::Owner& owner, std::size_t context, std::string_view path) {
                  Access::rebase(owner, context, path);
                }) {
    collection.def(
        "rebase",
        [](const ElementCollection<Access>& self, std::string_view path) {
          Access::rebase(*self.owner(), self.context(), path);
        },
        nb::arg("path"));
  }
  if constexpr (requires { typename Access::AddValue; }) {
    collection.def(
        "add",
        [](const ElementCollection<Access>& self, const typename Access::AddValue& value) {
          const auto key = Access::add(*self.owner(), self.context(), value);
          return self.at(static_cast<std::int64_t>(Access::positionForKey(*self.owner(), self.context(), key)));
        },
        nb::arg("value"));
  }
  if constexpr (requires { typename Access::Key; })
    registerCollection(collection);
  else
    bindSequenceProtocol(collection, reference_name);
  auto reference = nb::class_<ElementRef<Access>>(
      module, reference_name, "A live element reference. copy() returns an independent authoring value.");
  if (identity.reference_property) reference.def_prop_ro(identity.reference_property, &ElementRef<Access>::index);
  if constexpr (requires(const typename Access::Owner& owner, std::size_t context, std::size_t index) {
                  Access::snapshot(owner, context, index);
                }) {
    reference.def(
        "copy",
        [](const ElementRef<Access>& self) { return Access::snapshot(self.owner(), self.context(), self.index()); },
        "Return an independent copy of the referenced value.");
  } else {
    reference.def("copy", &ElementRef<Access>::copy, "Return an independent copy of the referenced value.");
  }
  reference
      .def(
          "__eq__",
          [](const ElementRef<Access>& self, nb::handle other) {
            if (!nb::isinstance<ElementRef<Access>>(other)) return false;
            try {
              return self.sameIdentity(nb::cast<const ElementRef<Access>&>(other));
            } catch (const nb::python_error&) {
              PyErr_Clear();
              return false;
            }
          },
          nb::is_operator())
      .def("__repr__", [public_reference_name, identity](const ElementRef<Access>& self) {
        try {
          std::string result = std::string("<") + public_reference_name;
          if (identity.reference_property) {
            result += " ";
            result += identity.reference_property;
            result += "=";
            result += std::to_string(self.index());
          }
          if (const auto label = self.displayLabel(); label && !label->value.empty()) {
            result += " ";
            result += label->field;
            result += "=";
            result += nb::repr(nb::cast(label->value)).c_str();
          }
          return result + ">";
        } catch (const nb::python_error&) {
          PyErr_Clear();
          return std::string("<") + public_reference_name + " invalid>";
        }
      });
  if constexpr (requires { Access::hashable; }) {
    if constexpr (Access::hashable)
      reference.def("__hash__", &ElementRef<Access>::identityHash);
    else
      reference.attr("__hash__") = nb::none();
  } else {
    reference.attr("__hash__") = nb::none();
  }
  return reference;
}

template <class Access>
using AccessValue = typename Access::Value;

template <class Access, class Field>
void bindElementField(nb::class_<ElementRef<Access>>& binding, const char* name, Field AccessValue<Access>::* member) {
  binding.def_prop_rw(
      name,
      [member](const ElementRef<Access>& self) { return self.copy().*member; },
      [member](ElementRef<Access>& self, const Field& value) {
        auto record = self.copy();
        record.*member = value;
        self.set(record);
      });
}

template <class Access, class Index>
void bindElementOptionalIndexField(nb::class_<ElementRef<Access>>& binding, const char* name,
                                   Index AccessValue<Access>::* member, Index absent) {
  const std::string getter_signature = std::string("def ") + name + "(self) -> int | None";
  const std::string setter_signature = std::string("def ") + name + "(self, value: int | None, /) -> None";
  binding.def_prop_rw(
      name,
      [member, absent](const ElementRef<Access>& self) {
        const Index value = self.copy().*member;
        return value == absent ? std::nullopt : std::optional<Index>{value};
      },
      [member, absent](ElementRef<Access>& self, const std::optional<Index>& value) {
        auto record = self.copy();
        record.*member = value.value_or(absent);
        self.set(record);
      },
      nb::for_getter(nb::sig(getter_signature.c_str())),
      nb::for_setter(nb::sig(setter_signature.c_str())));
}

template <class Access, class Field>
class ElementMemberRef {
 public:
  using Member = Field AccessValue<Access>::*;

  ElementMemberRef(ElementRef<Access> parent, Member member) : parent_(std::move(parent)), member_(member) {}

  [[nodiscard]] Field copy() const { return parent_.copy().*member_; }

  void set(const Field& value) {
    auto parent = parent_.copy();
    parent.*member_ = value;
    parent_.set(parent);
  }

  [[nodiscard]] bool sameIdentity(const ElementMemberRef& other) const {
    return member_ == other.member_ && parent_.sameIdentity(other.parent_);
  }

 private:
  ElementRef<Access> parent_;
  Member member_;
};

template <class Access, class Field>
nb::class_<ElementMemberRef<Access, Field>> bindElementMemberRef(nb::module_& module, const char* name,
                                                                 const char* public_name) {
  auto reference =
      nb::class_<ElementMemberRef<Access, Field>>(module, name, "A live reference to a field of a resource element.");
  reference.def("copy", &ElementMemberRef<Access, Field>::copy)
      .def(
          "__eq__",
          [](const ElementMemberRef<Access, Field>& self, const ElementMemberRef<Access, Field>& other) {
            try {
              return self.sameIdentity(other);
            } catch (const nb::python_error&) {
              PyErr_Clear();
              return false;
            }
          },
          nb::is_operator())
      .def("__repr__", [public_name](const ElementMemberRef<Access, Field>& self) {
        try {
          (void)self.copy();
          return std::string("<") + public_name + ">";
        } catch (const nb::python_error&) {
          PyErr_Clear();
          return std::string("<") + public_name + " invalid>";
        }
      });
  reference.attr("__hash__") = nb::none();
  return reference;
}

template <class Access, class Field>
void bindElementMember(nb::class_<ElementRef<Access>>& parent, const char* name, Field AccessValue<Access>::* member) {
  parent.def_prop_rw(
      name,
      [member](const ElementRef<Access>& self) { return ElementMemberRef<Access, Field>(self, member); },
      [member](ElementRef<Access>& self, const Field& value) {
        auto record = self.copy();
        record.*member = value;
        self.set(record);
      });
}

template <class Access, class Field, class Member>
void bindElementMemberField(nb::class_<ElementMemberRef<Access, Field>>& binding, const char* name,
                            Member Field::* member) {
  binding.def_prop_rw(
      name,
      [member](const ElementMemberRef<Access, Field>& self) { return self.copy().*member; },
      [member](ElementMemberRef<Access, Field>& self, const Member& value) {
        auto record = self.copy();
        record.*member = value;
        self.set(record);
      });
}

template <class Access>
class NestedElementRef {
 public:
  using ParentAccess = typename Access::ParentAccess;
  using Parent = typename ParentAccess::Value;
  using Value = typename Access::Value;

  NestedElementRef(ElementRef<ParentAccess> parent, std::size_t index) : parent_(std::move(parent)), index_(index) {}
  NestedElementRef(nb::object parent, std::size_t index) : parent_(std::move(parent)), index_(index) {}

  [[nodiscard]] std::size_t index() const {
    if (index_ >= Access::size(parentCopy())) throwInvalidReference();
    return index_;
  }

  [[nodiscard]] Value copy() const { return Access::get(parentCopy(), index()); }

  void set(const Value& value) {
    auto parent = parentCopy();
    Access::set(parent, index(), value);
    if (auto* resource = std::get_if<ElementRef<ParentAccess>>(&parent_)) {
      resource->set(parent);
    } else {
      nb::cast<Parent&>(std::get<nb::object>(parent_)) = std::move(parent);
    }
  }

  [[nodiscard]] bool resourceBacked() const noexcept {
    return std::holds_alternative<ElementRef<ParentAccess>>(parent_);
  }
  [[nodiscard]] typename ParentAccess::Owner& owner() const {
    return std::get<ElementRef<ParentAccess>>(parent_).owner();
  }
  [[nodiscard]] std::size_t parentIndex() const { return std::get<ElementRef<ParentAccess>>(parent_).index(); }
  [[nodiscard]] bool sameIdentity(const NestedElementRef& other) const {
    if (parent_.index() != other.parent_.index() || index() != other.index()) return false;
    if (const auto* resource = std::get_if<ElementRef<ParentAccess>>(&parent_)) {
      return resource->sameIdentity(std::get<ElementRef<ParentAccess>>(other.parent_));
    }
    return std::get<nb::object>(parent_).ptr() == std::get<nb::object>(other.parent_).ptr();
  }

 private:
  [[nodiscard]] Parent parentCopy() const {
    if (const auto* resource = std::get_if<ElementRef<ParentAccess>>(&parent_)) return resource->copy();
    return nb::cast<const Parent&>(std::get<nb::object>(parent_));
  }

  std::variant<ElementRef<ParentAccess>, nb::object> parent_;
  std::size_t index_;
};

template <class Access>
class NestedElementCollection {
 public:
  using ParentAccess = typename Access::ParentAccess;

  explicit NestedElementCollection(ElementRef<ParentAccess> parent) : parent_(std::move(parent)) {}
  explicit NestedElementCollection(nb::object parent) : parent_(std::move(parent)) {}

  [[nodiscard]] std::size_t size() const {
    if (const auto* resource = std::get_if<ElementRef<ParentAccess>>(&parent_)) return Access::size(resource->copy());
    return Access::size(nb::cast<const typename ParentAccess::Value&>(std::get<nb::object>(parent_)));
  }
  [[nodiscard]] NestedElementRef<Access> at(std::int64_t requested) const {
    const auto index = sequenceIndex(requested, size());
    if (const auto* resource = std::get_if<ElementRef<ParentAccess>>(&parent_)) return {*resource, index};
    return {std::get<nb::object>(parent_), index};
  }
  void set(std::int64_t requested, const typename Access::Value& value) {
    auto element = at(requested);
    element.set(value);
  }

 private:
  std::variant<ElementRef<ParentAccess>, nb::object> parent_;
};

template <class Access, class Field>
class NestedElementMemberRef {
 public:
  using Parent = typename Access::Value;
  using Member = Field Parent::*;

  NestedElementMemberRef(NestedElementRef<Access> parent, Member member)
      : parent_(std::move(parent)), member_(member) {}

  [[nodiscard]] Field copy() const { return parent_.copy().*member_; }

  void set(const Field& value) {
    auto parent = parent_.copy();
    parent.*member_ = value;
    parent_.set(parent);
  }

  [[nodiscard]] bool sameIdentity(const NestedElementMemberRef& other) const {
    return member_ == other.member_ && parent_.sameIdentity(other.parent_);
  }

 private:
  NestedElementRef<Access> parent_;
  Member member_;
};

template <class Access, class Field>
nb::class_<NestedElementMemberRef<Access, Field>> bindNestedElementMemberRef(nb::module_& module, const char* name,
                                                                             const char* public_name) {
  auto reference = nb::class_<NestedElementMemberRef<Access, Field>>(
      module, name, "A live reference to a field of a nested authoring element.");
  reference.def("copy", &NestedElementMemberRef<Access, Field>::copy)
      .def(
          "__eq__",
          [](const NestedElementMemberRef<Access, Field>& self, const NestedElementMemberRef<Access, Field>& other) {
            try {
              return self.sameIdentity(other);
            } catch (const nb::python_error&) {
              PyErr_Clear();
              return false;
            }
          },
          nb::is_operator())
      .def("__repr__", [public_name](const NestedElementMemberRef<Access, Field>& self) {
        try {
          (void)self.copy();
          return std::string("<") + public_name + ">";
        } catch (const nb::python_error&) {
          PyErr_Clear();
          return std::string("<") + public_name + " invalid>";
        }
      });
  reference.attr("__hash__") = nb::none();
  return reference;
}

template <class Access, class Field, class Member>
void bindNestedElementMemberField(nb::class_<NestedElementMemberRef<Access, Field>>& binding, const char* name,
                                  Member Field::* member) {
  binding.def_prop_rw(
      name,
      [member](const NestedElementMemberRef<Access, Field>& self) { return self.copy().*member; },
      [member](NestedElementMemberRef<Access, Field>& self, const Member& value) {
        auto record = self.copy();
        record.*member = value;
        self.set(record);
      });
}

template <class Access>
class NestedElementIterator {
 public:
  explicit NestedElementIterator(NestedElementCollection<Access> collection) : collection_(std::move(collection)) {}

  [[nodiscard]] NestedElementRef<Access> next() {
    if (index_ >= collection_.size()) throw nb::stop_iteration();
    return collection_.at(static_cast<std::int64_t>(index_++));
  }

 private:
  NestedElementCollection<Access> collection_;
  std::size_t index_ = 0;
};

template <class Access>
nb::class_<NestedElementRef<Access>> bindNestedElementCollection(nb::module_& module, const char* reference_name,
                                                                 const char* collection_name,
                                                                 const char* public_reference_name) {
  const std::string iterator_name = std::string("_") + collection_name + "Iterator";
  const std::string slice_signature =
      std::string("def __getitem__(self, slice: slice) -> list[") + reference_name + "]";
  const std::string iterator_signature = std::string("def __iter__(self) -> Iterator[") + reference_name + "]";
  nb::class_<NestedElementIterator<Access>>(module, iterator_name.c_str())
      .def("__iter__", [](NestedElementIterator<Access>& self) -> NestedElementIterator<Access>& { return self; })
      .def("__next__", &NestedElementIterator<Access>::next);
  auto collection = nb::class_<NestedElementCollection<Access>>(
      module, collection_name, "A live fixed-size sequence nested in an authoring value or resource element.");
  collection.def("__len__", &NestedElementCollection<Access>::size)
      .def("__getitem__", &NestedElementCollection<Access>::at, nb::arg("index"))
      .def("__setitem__", &NestedElementCollection<Access>::set, nb::arg("index"), nb::arg("value"))
      .def(
          "__getitem__",
          [](const NestedElementCollection<Access>& self, const nb::slice& slice) {
            auto [start, stop, step, length] = slice.compute(self.size());
            (void)stop;
            nb::list result;
            for (std::size_t index = 0; index < length; ++index) {
              result.append(self.at(start));
              start += step;
            }
            return result;
          },
          nb::arg("slice"),
          nb::sig(slice_signature.c_str()))
      .def(
          "__iter__",
          [](const NestedElementCollection<Access>& self) { return NestedElementIterator<Access>(self); },
          nb::sig(iterator_signature.c_str()));
  bindSequenceProtocol(collection, reference_name);
  auto reference = nb::class_<NestedElementRef<Access>>(
      module, reference_name, "A live reference to an element of a nested authoring sequence.");
  reference.def_prop_ro("index", &NestedElementRef<Access>::index)
      .def("copy", &NestedElementRef<Access>::copy, "Return an independent copy of the referenced value.")
      .def(
          "__eq__",
          [](const NestedElementRef<Access>& self, nb::handle other) {
            if (!nb::isinstance<NestedElementRef<Access>>(other)) return false;
            try {
              return self.sameIdentity(nb::cast<const NestedElementRef<Access>&>(other));
            } catch (const nb::python_error&) {
              PyErr_Clear();
              return false;
            }
          },
          nb::is_operator())
      .def("__repr__", [public_reference_name](const NestedElementRef<Access>& self) {
        try {
          return std::string("<") + public_reference_name + " index=" + std::to_string(self.index()) + ">";
        } catch (const nb::python_error&) {
          PyErr_Clear();
          return std::string("<") + public_reference_name + " invalid>";
        }
      });
  reference.attr("__hash__") = nb::none();
  return reference;
}

template <class Access>
void bindNestedCollection(nb::class_<ElementRef<typename Access::ParentAccess>>& parent, const char* name) {
  parent.def_prop_ro(name, [](const ElementRef<typename Access::ParentAccess>& self) {
    return NestedElementCollection<Access>(self);
  });
}

template <class Access>
using NestedAccessValue = typename Access::Value;

template <class Access, class Field>
void bindNestedElementField(nb::class_<NestedElementRef<Access>>& binding, const char* name,
                            Field NestedAccessValue<Access>::* member) {
  binding.def_prop_rw(
      name,
      [member](const NestedElementRef<Access>& self) { return self.copy().*member; },
      [member](NestedElementRef<Access>& self, const Field& value) {
        auto record = self.copy();
        record.*member = value;
        self.set(record);
      });
}

template <class Value>
class ReadOnlySequence {
 public:
  ReadOnlySequence() : values_(std::make_shared<std::vector<Value>>()) {}
  explicit ReadOnlySequence(std::vector<Value> values)
      : values_(std::make_shared<std::vector<Value>>(std::move(values))) {}
  explicit ReadOnlySequence(std::shared_ptr<std::vector<Value>> values) : values_(std::move(values)) {}

  [[nodiscard]] std::size_t size() const noexcept { return indices_ ? indices_->size() : values_->size(); }
  [[nodiscard]] const Value& at(std::int64_t requested) const {
    const auto index = sequenceIndex(requested, size());
    return (*values_)[indices_ ? (*indices_)[index] : index];
  }
  [[nodiscard]] ReadOnlySequence slice(const nb::slice& slice) const {
    auto [start, stop, step, length] = slice.compute(size());
    (void)stop;
    std::vector<std::size_t> indices;
    indices.reserve(length);
    for (std::size_t index = 0; index < length; ++index) {
      indices.push_back(indices_ ? (*indices_)[start] : start);
      start += step;
    }
    return ReadOnlySequence(values_, std::move(indices));
  }
  [[nodiscard]] ReadOnlySequence reversed() const {
    std::vector<std::size_t> indices;
    indices.reserve(size());
    for (std::size_t index = size(); index > 0; --index) {
      indices.push_back(indices_ ? (*indices_)[index - 1] : index - 1);
    }
    return ReadOnlySequence(values_, std::move(indices));
  }
  [[nodiscard]] bool contains(nb::handle value) const {
    for (std::size_t index = 0; index < size(); ++index) {
      if (equal(index, value)) return true;
    }
    return false;
  }
  [[nodiscard]] std::size_t count(nb::handle value) const {
    std::size_t result = 0;
    for (std::size_t index = 0; index < size(); ++index) {
      if (equal(index, value)) ++result;
    }
    return result;
  }
  [[nodiscard]] std::size_t find(nb::handle value, std::int64_t start, const std::optional<std::int64_t>& stop) const {
    const auto [first, last] = sequenceBounds(start, stop.value_or(std::numeric_limits<std::int64_t>::max()), size());
    for (std::size_t index = first; index < last; ++index) {
      if (equal(index, value)) return index;
    }
    throw nb::value_error("value is not in sequence");
  }
  void reserve(std::size_t count) { values_->reserve(count); }
  void append(Value value) { values_->push_back(std::move(value)); }

 private:
  ReadOnlySequence(std::shared_ptr<std::vector<Value>> values, std::vector<std::size_t> indices)
      : values_(std::move(values)), indices_(std::move(indices)) {}

  [[nodiscard]] bool equal(std::size_t index, nb::handle value) const {
    const auto element = nb::cast(at(static_cast<std::int64_t>(index)), nb::rv_policy::reference);
    const int result = PyObject_RichCompareBool(element.ptr(), value.ptr(), Py_EQ);
    if (result < 0) throw nb::python_error();
    return result != 0;
  }

  std::shared_ptr<std::vector<Value>> values_;
  std::optional<std::vector<std::size_t>> indices_;
};

template <class Value>
class ReadOnlyIterator {
 public:
  explicit ReadOnlyIterator(ReadOnlySequence<Value> sequence) : sequence_(std::move(sequence)) {}

  [[nodiscard]] const Value& next() {
    if (index_ >= sequence_.size()) throw nb::stop_iteration();
    return sequence_.at(static_cast<std::int64_t>(index_++));
  }

 private:
  ReadOnlySequence<Value> sequence_;
  std::size_t index_ = 0;
};

template <class Value>
void bindReadOnlySequence(nb::module_& module, const char* name, const char* value_name, const char* public_name) {
  const std::string iterator_name = std::string("_") + name + "Iterator";
  const std::string iterator_signature = std::string("def __iter__(self) -> Iterator[") + value_name + "]";
  const std::string reversed_signature = std::string("def __reversed__(self) -> Iterator[") + value_name + "]";
  nb::class_<ReadOnlyIterator<Value>>(module, iterator_name.c_str())
      .def("__iter__", [](ReadOnlyIterator<Value>& self) -> ReadOnlyIterator<Value>& { return self; })
      .def("__next__", &ReadOnlyIterator<Value>::next, nb::rv_policy::reference_internal);
  auto sequence = nb::class_<ReadOnlySequence<Value>>(
      module, name, "An immutable sequence returned with conversion sidecars or inspection data.");
  sequence.def("__len__", &ReadOnlySequence<Value>::size)
      .def("__getitem__", &ReadOnlySequence<Value>::at, nb::arg("index"), nb::rv_policy::reference_internal)
      .def("__getitem__", &ReadOnlySequence<Value>::slice, nb::arg("slice"))
      .def(
          "__iter__",
          [](const ReadOnlySequence<Value>& self) { return ReadOnlyIterator<Value>(self); },
          nb::sig(iterator_signature.c_str()))
      .def("__contains__", &ReadOnlySequence<Value>::contains, nb::arg("value"))
      .def("count", &ReadOnlySequence<Value>::count, nb::arg("value"))
      .def(
          "index", &ReadOnlySequence<Value>::find, nb::arg("value"), nb::arg("start") = 0, nb::arg("stop") = nb::none())
      .def(
          "__reversed__",
          [](const ReadOnlySequence<Value>& self) { return ReadOnlyIterator<Value>(self.reversed()); },
          nb::sig(reversed_signature.c_str()))
      .def("__repr__", [public_name](const ReadOnlySequence<Value>& self) {
        return std::string("<") + public_name + " len=" + std::to_string(self.size()) + ">";
      });
  registerSequence(sequence);
}

}  // namespace pistoris::python
