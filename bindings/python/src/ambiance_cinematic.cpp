// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#include "binding_utils.h"
#include "bindings.h"
#include "resource_state.h"
#include "resource_types.h"

#include <cstddef>
#include <cstdint>
#include <exception>
#include <memory>
#include <nanobind/stl/string.h>
#include <nanobind/stl/string_view.h>
#include <nanobind/stl/vector.h>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <variant>
#include <vector>

namespace pistoris::python {
namespace {

struct CinematicLanguage {
  std::string name;
};

struct AmbianceTrackAccess {
  using Owner = PythonAmbiance;
  using Value = ArxAmbianceTrack;
  static std::size_t size(const Owner& owner, std::size_t) { return owner.trackCount(); }
  static CollectionTracker& tracker(Owner& owner, std::size_t) { return owner.tracking.tracks; }
  static Value get(const Owner& owner, std::size_t, std::size_t index) { return owner.tracks()[index]; }
};

std::size_t ambianceKeyCount(const PythonAmbiance& ambiance, AmbianceTrackIndex track);

bool isPannedTrack(const ArxAmbianceTrack& track) {
  return track.kind == static_cast<ArxAmbianceTrackKind>(AmbianceTrackKind::kPanned);
}

std::vector<ArxAmbiancePannedKey> pannedKeyValues(const PythonAmbiance& owner, AmbianceTrackIndex track) {
  const auto keys = unwrap(owner.pannedKeys(track));
  return {keys.begin(), keys.end()};
}

std::vector<ArxAmbiancePositionedKey> positionedKeyValues(const PythonAmbiance& owner, AmbianceTrackIndex track) {
  const auto keys = unwrap(owner.positionedKeys(track));
  return {keys.begin(), keys.end()};
}

void setPannedTrackValue(PythonAmbiance& owner, AmbianceTrackIndex track, SoundIndex sound,
                         const std::vector<ArxAmbiancePannedKey>& keys) {
  unwrap(owner.setPannedTrack(track, {sound, keys.data(), keys.size()}));
}

void setPositionedTrackValue(PythonAmbiance& owner, AmbianceTrackIndex track, SoundIndex sound,
                             const std::vector<ArxAmbiancePositionedKey>& keys) {
  unwrap(owner.setPositionedTrack(track, {sound, keys.data(), keys.size()}));
}

std::optional<std::string> ambianceSoundPath(const Ambiance& ambiance, SoundIndex sound) {
  if (sound == kNoSound) return std::nullopt;
  return copyString(ambiance.sounds()[sound].path);
}

SoundIndex ambianceSoundIndex(const Ambiance& ambiance, const std::optional<std::string>& path) {
  if (!path) return kNoSound;
  const std::string canonical = canonicalResourcePath(*path);
  const auto sounds = ambiance.sounds();
  for (std::size_t index = 0; index < sounds.size(); ++index) {
    if (copyString(sounds[index].path) == canonical) return static_cast<SoundIndex>(index);
  }
  throwMissingKey(canonical);
}

AmbiancePannedTrackValue pannedTrackValue(const PythonAmbiance& owner, AmbianceTrackIndex track) {
  return {ambianceSoundPath(owner, owner.tracks()[track].sound), pannedKeyValues(owner, track)};
}

AmbiancePositionedTrackValue positionedTrackValue(const PythonAmbiance& owner, AmbianceTrackIndex track) {
  return {ambianceSoundPath(owner, owner.tracks()[track].sound), positionedKeyValues(owner, track)};
}

template <class Source, class Destination>
void copyAmbianceKeyCommon(const Source& source, Destination& destination) {
  destination.start_delay_ms = source.start_delay_ms;
  destination.play_count = source.play_count;
  destination.delay_min_ms = source.delay_min_ms;
  destination.delay_max_ms = source.delay_max_ms;
  destination.volume = source.volume;
  destination.pitch = source.pitch;
}

class AmbianceKeyCollection;
void replaceAmbianceTrack(PythonAmbiance& owner, std::int64_t requested, nb::handle value);
AmbianceTrackIndex addAmbianceTrack(PythonAmbiance& owner, nb::handle value);

class AmbianceTrackRef {
 public:
  explicit AmbianceTrackRef(ElementRef<AmbianceTrackAccess> reference) : reference_(std::move(reference)) {}

  [[nodiscard]] std::size_t index() const { return reference_.index(); }
  [[nodiscard]] PythonAmbiance& owner() const { return reference_.owner(); }
  [[nodiscard]] ArxAmbianceTrack metadata() const { return reference_.copy(); }

  [[nodiscard]] std::optional<std::string> soundPath() const { return ambianceSoundPath(owner(), metadata().sound); }

  void setSoundPath(const std::optional<std::string>& sound) {
    const auto track = static_cast<AmbianceTrackIndex>(index());
    const auto current = metadata();
    const SoundIndex sound_index = ambianceSoundIndex(owner(), sound);
    if (isPannedTrack(current)) {
      setPannedTrackValue(owner(), track, sound_index, pannedKeyValues(owner(), track));
    } else {
      setPositionedTrackValue(owner(), track, sound_index, positionedKeyValues(owner(), track));
    }
  }

  [[nodiscard]] AmbianceTrackKind kind() const { return static_cast<AmbianceTrackKind>(metadata().kind); }

  void setKind(AmbianceTrackKind kind) {
    if (kind == this->kind()) return;

    const auto track = static_cast<AmbianceTrackIndex>(index());
    const auto current = metadata();
    if (kind == AmbianceTrackKind::kPositioned && isPannedTrack(current)) {
      const auto source = pannedKeyValues(owner(), track);
      std::vector<ArxAmbiancePositionedKey> converted(source.size());
      for (std::size_t index = 0; index < source.size(); ++index) {
        copyAmbianceKeyCommon(source[index], converted[index]);
      }
      setPositionedTrackValue(owner(), track, current.sound, converted);
    } else if (kind == AmbianceTrackKind::kPanned && !isPannedTrack(current)) {
      const auto source = positionedKeyValues(owner(), track);
      std::vector<ArxAmbiancePannedKey> converted(source.size());
      for (std::size_t index = 0; index < source.size(); ++index) {
        copyAmbianceKeyCommon(source[index], converted[index]);
      }
      setPannedTrackValue(owner(), track, current.sound, converted);
    } else {
      throw nb::value_error("unsupported ambiance track kind");
    }
    owner().tracking.spatial_automations.invalidate(track);
  }

  [[nodiscard]] nb::object copy() const {
    const auto track = static_cast<AmbianceTrackIndex>(index());
    if (isPannedTrack(metadata())) return nb::cast(pannedTrackValue(owner(), track));
    return nb::cast(positionedTrackValue(owner(), track));
  }

  [[nodiscard]] AmbianceKeyCollection keys() const;
  [[nodiscard]] bool sameIdentity(const AmbianceTrackRef& other) const {
    return reference_.sameIdentity(other.reference_);
  }

 private:
  ElementRef<AmbianceTrackAccess> reference_;
};

class AmbianceTrackCollection {
 public:
  explicit AmbianceTrackCollection(std::shared_ptr<PythonAmbiance> owner) : values_(std::move(owner)) {}

  [[nodiscard]] std::size_t size() const { return values_.size(); }
  [[nodiscard]] AmbianceTrackRef at(std::int64_t index) const { return AmbianceTrackRef(values_.at(index)); }
  void append(nb::handle value) const { (void)addAmbianceTrack(*values_.owner(), value); }
  void set(std::int64_t index, nb::handle value) const { replaceAmbianceTrack(*values_.owner(), index, value); }
  void remove(std::int64_t requested) const {
    const auto index = sequenceTargetIndex<AmbianceTrackIndex>(requested, size());
    auto& owner = *values_.owner();
    unwrap(owner.removeTrack(index));
    owner.tracking.tracks.remove(index);
    owner.tracking.keys.removeParent(index);
    owner.tracking.spatial_automations.removeParent(index);
  }
  void clear() const {
    auto& owner = *values_.owner();
    owner.clearTracks();
    owner.tracking.tracks.invalidate();
    owner.tracking.keys.invalidate();
    owner.tracking.spatial_automations.invalidate();
  }
  [[nodiscard]] std::size_t trimToMaster() const {
    auto& owner = *values_.owner();
    std::vector<std::size_t> key_counts;
    key_counts.reserve(owner.trackCount());
    for (std::size_t track = 0; track < owner.trackCount(); ++track) {
      key_counts.push_back(ambianceKeyCount(owner, static_cast<AmbianceTrackIndex>(track)));
    }
    const auto removed = unwrap(owner.trimTracksToMaster());
    for (std::size_t track = 0; track < key_counts.size(); ++track) {
      const std::size_t retained = ambianceKeyCount(owner, static_cast<AmbianceTrackIndex>(track));
      while (key_counts[track] > retained) {
        const std::size_t removed_key = --key_counts[track];
        owner.tracking.keys.removeElement(track, removed_key);
        owner.tracking.spatial_automations.removeElement(track, removed_key);
      }
    }
    return removed;
  }

 private:
  ElementCollection<AmbianceTrackAccess> values_;
};

class AmbianceTrackIterator {
 public:
  explicit AmbianceTrackIterator(AmbianceTrackCollection collection) : collection_(std::move(collection)) {}

  [[nodiscard]] AmbianceTrackRef next() {
    if (index_ >= collection_.size()) throw nb::stop_iteration();
    return collection_.at(static_cast<std::int64_t>(index_++));
  }

 private:
  AmbianceTrackCollection collection_;
  std::size_t index_ = 0;
};

enum class AmbianceAutomationChannel : std::uint8_t { kVolume, kPitch, kPan, kX, kY, kZ };

class AmbianceKeyRef {
 public:
  AmbianceKeyRef(AmbianceTrackRef track, std::shared_ptr<ElementToken> token)
      : backing_(std::move(track)), token_(std::move(token)) {}
  AmbianceKeyRef(const nb::object& track, bool panned, std::size_t index)
      : backing_(DetachedBacking{track, panned, detachedRevision(track, panned)}),
        token_(std::make_shared<ElementToken>(ElementToken{index})) {}

  [[nodiscard]] std::size_t index() const {
    validate();
    if (!token_->index) throwInvalidReference();
    return *token_->index;
  }

  [[nodiscard]] bool resourceBacked() const noexcept { return std::holds_alternative<AmbianceTrackRef>(backing_); }
  [[nodiscard]] AmbianceTrackRef& track() { return std::get<AmbianceTrackRef>(backing_); }
  [[nodiscard]] const AmbianceTrackRef& track() const { return std::get<AmbianceTrackRef>(backing_); }
  [[nodiscard]] bool isPanned() const {
    if (const auto* resource = std::get_if<AmbianceTrackRef>(&backing_)) return isPannedTrack(resource->metadata());
    return std::get<DetachedBacking>(backing_).panned;
  }

  [[nodiscard]] nb::object copy() const {
    if (const auto* resource = std::get_if<AmbianceTrackRef>(&backing_)) {
      const auto track_index = static_cast<AmbianceTrackIndex>(resource->index());
      if (isPanned()) return nb::cast(unwrap(resource->owner().pannedKeys(track_index))[index()]);
      return nb::cast(unwrap(resource->owner().positionedKeys(track_index))[index()]);
    }
    const auto& detached = std::get<DetachedBacking>(backing_);
    if (detached.panned) return nb::cast(nb::cast<const AmbiancePannedTrackValue&>(detached.track).keys[index()]);
    return nb::cast(nb::cast<const AmbiancePositionedTrackValue&>(detached.track).keys[index()]);
  }

  void set(nb::handle value) {
    const auto key_index = index();
    if (isPanned()) {
      if (!nb::isinstance<ArxAmbiancePannedKey>(value)) {
        throw nb::type_error("panned tracks require PannedKey values");
      }
      if (auto* resource = std::get_if<AmbianceTrackRef>(&backing_)) {
        const auto track_index = static_cast<AmbianceTrackIndex>(resource->index());
        auto keys = pannedKeyValues(resource->owner(), track_index);
        keys[key_index] = nb::cast<const ArxAmbiancePannedKey&>(value);
        setPannedTrackValue(resource->owner(), track_index, resource->metadata().sound, keys);
      } else {
        auto& track_value = nb::cast<AmbiancePannedTrackValue&>(std::get<DetachedBacking>(backing_).track);
        track_value.keys[key_index] = nb::cast<const ArxAmbiancePannedKey&>(value);
      }
      return;
    }
    if (!nb::isinstance<ArxAmbiancePositionedKey>(value)) {
      throw nb::type_error("positioned tracks require PositionedKey values");
    }
    if (auto* resource = std::get_if<AmbianceTrackRef>(&backing_)) {
      const auto track_index = static_cast<AmbianceTrackIndex>(resource->index());
      auto keys = positionedKeyValues(resource->owner(), track_index);
      keys[key_index] = nb::cast<const ArxAmbiancePositionedKey&>(value);
      setPositionedTrackValue(resource->owner(), track_index, resource->metadata().sound, keys);
    } else {
      auto& track_value = nb::cast<AmbiancePositionedTrackValue&>(std::get<DetachedBacking>(backing_).track);
      track_value.keys[key_index] = nb::cast<const ArxAmbiancePositionedKey&>(value);
    }
  }

  template <class Field>
  [[nodiscard]] Field field(Field ArxAmbiancePannedKey::* panned, Field ArxAmbiancePositionedKey::* positioned) const {
    const auto value = copy();
    if (isPanned()) return nb::cast<const ArxAmbiancePannedKey&>(value).*panned;
    return nb::cast<const ArxAmbiancePositionedKey&>(value).*positioned;
  }

  template <class Field>
  void setField(Field ArxAmbiancePannedKey::* panned, Field ArxAmbiancePositionedKey::* positioned,
                const Field& value) {
    auto key = copy();
    if (isPanned()) {
      auto record = nb::cast<ArxAmbiancePannedKey>(key);
      record.*panned = value;
      set(nb::cast(record));
    } else {
      auto record = nb::cast<ArxAmbiancePositionedKey>(key);
      record.*positioned = value;
      set(nb::cast(record));
    }
  }

  [[nodiscard]] bool supports(AmbianceAutomationChannel channel) const {
    const bool panned = isPanned();
    if (channel == AmbianceAutomationChannel::kPan) return panned;
    if (channel == AmbianceAutomationChannel::kX || channel == AmbianceAutomationChannel::kY ||
        channel == AmbianceAutomationChannel::kZ) {
      return !panned;
    }
    return true;
  }

  [[nodiscard]] ArxAmbianceAutomation automation(AmbianceAutomationChannel channel) const {
    const auto value = copy();
    if (isPanned()) {
      const auto& key = nb::cast<const ArxAmbiancePannedKey&>(value);
      if (channel == AmbianceAutomationChannel::kVolume) return key.volume;
      if (channel == AmbianceAutomationChannel::kPitch) return key.pitch;
      if (channel == AmbianceAutomationChannel::kPan) return key.pan;
    } else {
      const auto& key = nb::cast<const ArxAmbiancePositionedKey&>(value);
      if (channel == AmbianceAutomationChannel::kVolume) return key.volume;
      if (channel == AmbianceAutomationChannel::kPitch) return key.pitch;
      if (channel == AmbianceAutomationChannel::kX) return key.x;
      if (channel == AmbianceAutomationChannel::kY) return key.y;
      if (channel == AmbianceAutomationChannel::kZ) return key.z;
    }
    throw nb::attribute_error("spatial automation is unavailable for this key kind");
  }

  void setAutomation(AmbianceAutomationChannel channel, const ArxAmbianceAutomation& value) {
    if (!supports(channel)) throw nb::attribute_error("spatial automation is unavailable for this key kind");

    auto key = copy();
    if (isPanned()) {
      auto record = nb::cast<ArxAmbiancePannedKey>(key);
      if (channel == AmbianceAutomationChannel::kVolume) record.volume = value;
      if (channel == AmbianceAutomationChannel::kPitch) record.pitch = value;
      if (channel == AmbianceAutomationChannel::kPan) record.pan = value;
      set(nb::cast(record));
    } else {
      auto record = nb::cast<ArxAmbiancePositionedKey>(key);
      if (channel == AmbianceAutomationChannel::kVolume) record.volume = value;
      if (channel == AmbianceAutomationChannel::kPitch) record.pitch = value;
      if (channel == AmbianceAutomationChannel::kX) record.x = value;
      if (channel == AmbianceAutomationChannel::kY) record.y = value;
      if (channel == AmbianceAutomationChannel::kZ) record.z = value;
      set(nb::cast(record));
    }
  }

  [[nodiscard]] bool sameIdentity(const AmbianceKeyRef& other) const {
    if (backing_.index() != other.backing_.index() || index() != other.index()) return false;
    if (const auto* resource = std::get_if<AmbianceTrackRef>(&backing_)) {
      return resource->index() == std::get<AmbianceTrackRef>(other.backing_).index() &&
             &resource->owner() == &std::get<AmbianceTrackRef>(other.backing_).owner();
    }
    return std::get<DetachedBacking>(backing_).track.ptr() == std::get<DetachedBacking>(other.backing_).track.ptr();
  }

 private:
  struct DetachedBacking {
    nb::object track;
    bool panned;
    std::size_t revision;
  };

  [[nodiscard]] static std::size_t detachedRevision(const nb::object& track, bool panned) {
    if (panned) return nb::cast<const AmbiancePannedTrackValue&>(track).keys_revision;
    return nb::cast<const AmbiancePositionedTrackValue&>(track).keys_revision;
  }

  void validate() const {
    if (const auto* resource = std::get_if<AmbianceTrackRef>(&backing_)) {
      (void)resource->index();
      return;
    }
    const auto& detached = std::get<DetachedBacking>(backing_);
    if (detached.revision != detachedRevision(detached.track, detached.panned)) throwInvalidReference();
    const auto size = detached.panned ? nb::cast<const AmbiancePannedTrackValue&>(detached.track).keys.size()
                                      : nb::cast<const AmbiancePositionedTrackValue&>(detached.track).keys.size();
    if (!token_->index || *token_->index >= size) throwInvalidReference();
  }

  std::variant<AmbianceTrackRef, DetachedBacking> backing_;
  std::shared_ptr<ElementToken> token_;
};

class AmbianceKeyCollection {
 public:
  explicit AmbianceKeyCollection(AmbianceTrackRef track) : backing_(std::move(track)) {}
  AmbianceKeyCollection(nb::object track, bool panned) : backing_(DetachedBacking{std::move(track), panned}) {}

  [[nodiscard]] std::size_t size() const {
    if (const auto* resource = std::get_if<AmbianceTrackRef>(&backing_)) {
      const auto track = static_cast<AmbianceTrackIndex>(resource->index());
      if (isPannedTrack(resource->metadata())) return unwrap(resource->owner().pannedKeys(track)).size();
      return unwrap(resource->owner().positionedKeys(track)).size();
    }
    const auto& detached = std::get<DetachedBacking>(backing_);
    if (detached.panned) return nb::cast<const AmbiancePannedTrackValue&>(detached.track).keys.size();
    return nb::cast<const AmbiancePositionedTrackValue&>(detached.track).keys.size();
  }

  [[nodiscard]] AmbianceKeyRef at(std::int64_t requested) const {
    const auto index = sequenceIndex(requested, size());
    if (const auto* resource = std::get_if<AmbianceTrackRef>(&backing_)) {
      const auto track = resource->index();
      return {*resource, resource->owner().tracking.keys[track].track(index)};
    }
    const auto& detached = std::get<DetachedBacking>(backing_);
    return {detached.track, detached.panned, index};
  }

  void set(std::int64_t index, nb::handle value) const { at(index).set(value); }
  void append(nb::handle value) const {
    if (auto* resource = std::get_if<AmbianceTrackRef>(&backing_)) {
      const auto track = static_cast<AmbianceTrackIndex>(resource->index());
      if (resource->kind() == AmbianceTrackKind::kPanned) {
        if (!nb::isinstance<ArxAmbiancePannedKey>(value))
          throw nb::type_error("panned tracks require PannedKey values");
        auto keys = pannedKeyValues(resource->owner(), track);
        keys.push_back(nb::cast<const ArxAmbiancePannedKey&>(value));
        setPannedTrackValue(resource->owner(), track, resource->metadata().sound, keys);
      } else {
        if (!nb::isinstance<ArxAmbiancePositionedKey>(value))
          throw nb::type_error("positioned tracks require PositionedKey values");
        auto keys = positionedKeyValues(resource->owner(), track);
        keys.push_back(nb::cast<const ArxAmbiancePositionedKey&>(value));
        setPositionedTrackValue(resource->owner(), track, resource->metadata().sound, keys);
      }
      return;
    }
    auto& detached = std::get<DetachedBacking>(backing_);
    if (detached.panned) {
      if (!nb::isinstance<ArxAmbiancePannedKey>(value)) throw nb::type_error("panned tracks require PannedKey values");
      auto& track = nb::cast<AmbiancePannedTrackValue&>(detached.track);
      track.keys.push_back(nb::cast<const ArxAmbiancePannedKey&>(value));
      ++track.keys_revision;
    } else {
      if (!nb::isinstance<ArxAmbiancePositionedKey>(value))
        throw nb::type_error("positioned tracks require PositionedKey values");
      auto& track = nb::cast<AmbiancePositionedTrackValue&>(detached.track);
      track.keys.push_back(nb::cast<const ArxAmbiancePositionedKey&>(value));
      ++track.keys_revision;
    }
  }
  void remove(std::int64_t requested) const {
    const auto index = sequenceIndex(requested, size());
    if (auto* resource = std::get_if<AmbianceTrackRef>(&backing_)) {
      const auto track = static_cast<AmbianceTrackIndex>(resource->index());
      if (resource->kind() == AmbianceTrackKind::kPanned) {
        auto keys = pannedKeyValues(resource->owner(), track);
        keys.erase(keys.begin() + static_cast<std::ptrdiff_t>(index));
        setPannedTrackValue(resource->owner(), track, resource->metadata().sound, keys);
      } else {
        auto keys = positionedKeyValues(resource->owner(), track);
        keys.erase(keys.begin() + static_cast<std::ptrdiff_t>(index));
        setPositionedTrackValue(resource->owner(), track, resource->metadata().sound, keys);
      }
      resource->owner().tracking.keys.removeElement(track, index);
      resource->owner().tracking.spatial_automations.removeElement(track, index);
      return;
    }
    auto& detached = std::get<DetachedBacking>(backing_);
    if (detached.panned) {
      auto& track = nb::cast<AmbiancePannedTrackValue&>(detached.track);
      track.keys.erase(track.keys.begin() + static_cast<std::ptrdiff_t>(index));
      ++track.keys_revision;
    } else {
      auto& track = nb::cast<AmbiancePositionedTrackValue&>(detached.track);
      track.keys.erase(track.keys.begin() + static_cast<std::ptrdiff_t>(index));
      ++track.keys_revision;
    }
  }
  void clear() const {
    if (auto* resource = std::get_if<AmbianceTrackRef>(&backing_)) {
      const auto track = static_cast<AmbianceTrackIndex>(resource->index());
      if (resource->kind() == AmbianceTrackKind::kPanned) {
        setPannedTrackValue(resource->owner(), track, resource->metadata().sound, {});
      } else {
        setPositionedTrackValue(resource->owner(), track, resource->metadata().sound, {});
      }
      resource->owner().tracking.keys.invalidate(track);
      resource->owner().tracking.spatial_automations.invalidate(track);
      return;
    }
    auto& detached = std::get<DetachedBacking>(backing_);
    if (detached.panned) {
      auto& track = nb::cast<AmbiancePannedTrackValue&>(detached.track);
      track.keys.clear();
      ++track.keys_revision;
    } else {
      auto& track = nb::cast<AmbiancePositionedTrackValue&>(detached.track);
      track.keys.clear();
      ++track.keys_revision;
    }
  }

 private:
  struct DetachedBacking {
    nb::object track;
    bool panned;
  };
  std::variant<AmbianceTrackRef, DetachedBacking> backing_;
};

AmbianceKeyCollection AmbianceTrackRef::keys() const { return AmbianceKeyCollection(*this); }

class AmbianceKeyIterator {
 public:
  explicit AmbianceKeyIterator(AmbianceKeyCollection collection) : collection_(std::move(collection)) {}

  [[nodiscard]] AmbianceKeyRef next() {
    if (index_ >= collection_.size()) throw nb::stop_iteration();
    return collection_.at(static_cast<std::int64_t>(index_++));
  }

 private:
  AmbianceKeyCollection collection_;
  std::size_t index_ = 0;
};

class AmbianceAutomationRef {
 public:
  AmbianceAutomationRef(AmbianceKeyRef key, AmbianceAutomationChannel channel,
                        std::shared_ptr<ElementToken> spatial_token = {})
      : key_(std::move(key)), channel_(channel), spatial_token_(std::move(spatial_token)) {}

  [[nodiscard]] ArxAmbianceAutomation copy() const {
    validate();
    return key_.automation(channel_);
  }

  void set(const ArxAmbianceAutomation& value) {
    validate();
    key_.setAutomation(channel_, value);
  }

  [[nodiscard]] bool sameIdentity(const AmbianceAutomationRef& other) const {
    validate();
    other.validate();
    return channel_ == other.channel_ && key_.sameIdentity(other.key_);
  }

 private:
  void validate() const {
    (void)key_.index();
    if (spatial_token_ && !spatial_token_->index) throwInvalidReference();
  }

  AmbianceKeyRef key_;
  AmbianceAutomationChannel channel_;
  std::shared_ptr<ElementToken> spatial_token_;
};

AmbianceAutomationRef commonAutomationRef(const AmbianceKeyRef& key, AmbianceAutomationChannel channel) {
  return {key, channel};
}

nb::object spatialAutomationRef(const AmbianceKeyRef& key, AmbianceAutomationChannel channel) {
  if (!key.supports(channel)) return nb::none();
  if (!key.resourceBacked()) return nb::cast(AmbianceAutomationRef(key, channel));
  const auto track = key.track().index();
  auto token = key.track().owner().tracking.spatial_automations[track].track(key.index());
  return nb::cast(AmbianceAutomationRef(key, channel, std::move(token)));
}

void replaceAmbianceTrack(PythonAmbiance& owner, std::int64_t requested, nb::handle value) {
  const auto index = sequenceTargetIndex<AmbianceTrackIndex>(requested, owner.trackCount());
  if (nb::isinstance<AmbiancePannedTrackValue>(value)) {
    const auto& track = nb::cast<const AmbiancePannedTrackValue&>(value);
    setPannedTrackValue(owner, index, ambianceSoundIndex(owner, track.sound), track.keys);
  } else if (nb::isinstance<AmbiancePositionedTrackValue>(value)) {
    const auto& track = nb::cast<const AmbiancePositionedTrackValue&>(value);
    setPositionedTrackValue(owner, index, ambianceSoundIndex(owner, track.sound), track.keys);
  } else {
    throw nb::type_error("track must be PannedTrack or PositionedTrack");
  }
  owner.tracking.keys.invalidate(index);
  owner.tracking.spatial_automations.invalidate(index);
}

AmbianceTrackIndex addAmbianceTrack(PythonAmbiance& owner, nb::handle value) {
  if (nb::isinstance<AmbiancePannedTrackValue>(value)) {
    const auto& track = nb::cast<const AmbiancePannedTrackValue&>(value);
    return unwrap(owner.addPannedTrack({ambianceSoundIndex(owner, track.sound), track.keys.data(), track.keys.size()}));
  }
  if (nb::isinstance<AmbiancePositionedTrackValue>(value)) {
    const auto& track = nb::cast<const AmbiancePositionedTrackValue&>(value);
    return unwrap(
        owner.addPositionedTrack({ambianceSoundIndex(owner, track.sound), track.keys.data(), track.keys.size()}));
  }
  throw nb::type_error("track must be PannedTrack or PositionedTrack");
}

struct AmbianceSoundAccess {
  using Owner = PythonAmbiance;
  using Value = Sound;
  static constexpr const char* collection_value_name =  // NOLINT(readability-identifier-naming)
      "pistoris.Sound";
  static std::size_t size(const Owner& owner, std::size_t) { return owner.soundCount(); }
  static CollectionTracker& tracker(Owner& owner, std::size_t) { return owner.tracking.sounds; }
  static Value get(const Owner& owner, std::size_t, std::size_t index) { return Value(owner.sounds()[index]); }
  static void set(Owner& owner, std::size_t, std::size_t index, const Value& value) {
    unwrap(owner.setSound(static_cast<SoundIndex>(index), value.asView()));
  }
  static void append(Owner& owner, std::size_t, const Value& value) { (void)unwrap(owner.addSound(value.asView())); }
  static void remove(Owner& owner, std::size_t, std::size_t index) {
    unwrap(owner.removeSound(static_cast<SoundIndex>(index)));
    owner.tracking.sounds.remove(index);
  }
  static ElementDisplayLabel displayLabel(const Owner& owner, std::size_t, std::size_t index) {
    return {"path", copyString(owner.sounds()[index].path)};
  }
  static std::size_t compact(Owner& owner, std::size_t) {
    const std::size_t removed = unwrap(owner.compactSounds());
    owner.tracking.sounds.invalidate();
    return removed;
  }
  static void rebase(Owner& owner, std::size_t, std::string_view path) { unwrap(owner.rebaseSoundPaths(path)); }
};

nb::object ambianceSoundReference(const std::shared_ptr<PythonAmbiance>& owner,
                                  const std::optional<std::string>& path) {
  if (!path) return nb::none();
  return nb::cast(ElementCollection<AmbianceSoundAccess>(owner).at(ambianceSoundIndex(*owner, path)));
}

std::optional<std::string> ambianceSoundReferencePath(const PythonAmbiance& owner, nb::handle value) {
  if (value.is_none()) return std::nullopt;
  const auto& sound = nb::cast<const ElementRef<AmbianceSoundAccess>&>(value);
  if (&sound.owner() != &owner) throw nb::value_error("sound belongs to a different ambiance");
  return copyString(owner.sounds()[sound.index()].path);
}

void commitCinematic(PythonCinematic& owner, Cinematic&& updated) {
  static_cast<Cinematic&>(owner) = std::move(updated);
}

SoundHandle cinematicSoundHandle(SoundKind kind, SoundIndex index) {
  SoundHandle result = kNoSoundHandle;
  checkStatus(soundHandle(kind, index, result));
  return result;
}

std::optional<std::string> cinematicSoundPath(const Cinematic& cinematic, SoundHandle handle) {
  if (handle == kNoSoundHandle) return std::nullopt;
  SoundKind kind = SoundKind::kEffect;
  SoundIndex index = kNoSound;
  checkStatus(soundHandleKind(handle, kind));
  checkStatus(soundHandleIndex(handle, index));
  return copyString(cinematic.sounds(kind)[index].path);
}

SoundHandle cinematicSoundByPath(const Cinematic& cinematic, SoundKind kind, std::string_view path) {
  const std::string canonical = canonicalResourcePath(path);
  const auto sounds = cinematic.sounds(kind);
  for (std::size_t index = 0; index < sounds.size(); ++index) {
    if (copyString(sounds[index].path) == canonical) return cinematicSoundHandle(kind, static_cast<SoundIndex>(index));
  }
  throwMissingKey(canonical);
}

LanguageId cinematicLanguageByName(const Cinematic& cinematic, std::string_view name) {
  const std::string canonical = canonicalLowerIdentifier(name);
  if (const auto language = cinematic.findLanguage(canonical)) return *language;
  throwMissingKey(canonical);
}

std::optional<std::string> cinematicLanguageName(const Cinematic& cinematic, LanguageId id) {
  if (id == kSoundEffects) return std::nullopt;
  for (const auto language : cinematic.languages()) {
    if (language.id == id) return copyString(language.name);
  }
  throwMissingKey(id);
}

struct CinematicIllustrationAccess {
  using Owner = PythonCinematic;
  using Value = CinematicIllustrationValue;
  static std::size_t size(const Owner& owner, std::size_t) { return owner.illustrationCount(); }
  static CollectionTracker& tracker(Owner& owner, std::size_t) { return owner.tracking.illustrations; }
  static Value get(const Owner& owner, std::size_t, std::size_t index) {
    const auto illustration = owner.illustrations()[index];
    Value result;
    result.subdivision_scale = illustration.subdivision_scale;
    if (illustration.texture != kNoTexture) {
      const auto texture = owner.textures()[illustration.texture];
      result.path = copyString(texture.path);
      result.external_image_extension = copyString(texture.external_image_extension);
      if (texture.encoded_image.data) {
        result.encoded_image.assign(texture.encoded_image.data,
                                    texture.encoded_image.data + texture.encoded_image.size);
      }
    }
    return result;
  }
  static void set(Owner& owner, std::size_t, std::size_t index, const Value& value) {
    Cinematic updated(owner);
    auto illustration = updated.illustrations()[index];
    const ArxTextureView texture{
        view(value.path), imageView(value.encoded_image), view(value.external_image_extension)};
    if (illustration.texture == kNoTexture) {
      illustration.texture = unwrap(updated.addTexture(texture));
    } else {
      unwrap(updated.setTexture(illustration.texture, texture));
    }
    illustration.subdivision_scale = value.subdivision_scale;
    unwrap(updated.setIllustration(static_cast<CinematicIllustrationIndex>(index), illustration));
    commitCinematic(owner, std::move(updated));
  }
  static void append(Owner& owner, std::size_t, const Value& value) {
    Cinematic updated(owner);
    const ArxTextureView texture{
        view(value.path), imageView(value.encoded_image), view(value.external_image_extension)};
    const TextureIndex texture_index = unwrap(updated.addTexture(texture));
    (void)unwrap(updated.addIllustration({texture_index, value.subdivision_scale}));
    commitCinematic(owner, std::move(updated));
  }
  static void remove(Owner& owner, std::size_t, std::size_t index) {
    unwrap(owner.removeIllustration(static_cast<CinematicIllustrationIndex>(index)));
    owner.tracking.illustrations.remove(index);
  }
  static void clear(Owner& owner, std::size_t) {
    owner.clearIllustrations();
    owner.tracking.illustrations.invalidate();
  }
  static std::size_t compact(Owner& owner, std::size_t) { return unwrap(owner.compactIllustrations()); }
  static void rebase(Owner& owner, std::size_t, std::string_view path) { unwrap(owner.rebaseTexturePaths(path)); }
};

struct CinematicKeyframeAccess {
  using Owner = PythonCinematic;
  using Value = CinematicKeyframeValue;
  static std::size_t size(const Owner& owner, std::size_t) { return owner.keyframeCount(); }
  static CollectionTracker& tracker(Owner& owner, std::size_t) { return owner.tracking.keyframes; }
  static Value get(const Owner& owner, std::size_t, std::size_t index) {
    const ArxCinematicKeyframe key = owner.keyframes()[index];
    Value result;
    result.frame = key.frame;
    result.camera_position = key.camera_position;
    result.camera_roll = key.camera_roll;
    result.color = key.color;
    result.secondary_color = key.secondary_color;
    result.flash_color = key.flash_color;
    result.flash_decay = key.flash_decay;
    if (key.light_active != 0) result.light = key.light;
    result.outgoing_speed = key.outgoing_speed;
    result.sound_path = cinematicSoundPath(owner, key.sound);
    if (key.sound != kNoSoundHandle) checkStatus(soundHandleKind(key.sound, result.sound_kind));
    result.interpolation = key.interpolation;
    result.base_effect = key.base_effect;
    result.post_effect = key.post_effect;
    result.crossfade = key.crossfade != 0;
    result.dream = key.dream != 0;
    return result;
  }
  static ArxCinematicKeyframe native(const Owner& owner, const Value& value, CinematicIllustrationIndex illustration) {
    ArxCinematicKeyframe result{};
    result.frame = value.frame;
    result.illustration = illustration;
    result.camera_position = value.camera_position;
    result.camera_roll = value.camera_roll;
    result.color = value.color;
    result.secondary_color = value.secondary_color;
    result.flash_color = value.flash_color;
    result.flash_decay = value.flash_decay;
    if (value.light) result.light = *value.light;
    result.outgoing_speed = value.outgoing_speed;
    result.sound = value.sound_path ? cinematicSoundByPath(owner, value.sound_kind, *value.sound_path) : kNoSoundHandle;
    result.interpolation = value.interpolation;
    result.base_effect = value.base_effect;
    result.post_effect = value.post_effect;
    result.crossfade = static_cast<std::uint8_t>(value.crossfade);
    result.dream = static_cast<std::uint8_t>(value.dream);
    result.light_active = static_cast<std::uint8_t>(value.light.has_value());
    return result;
  }
  static void set(Owner& owner, std::size_t, std::size_t index, const Value& value) {
    const bool had_light = owner.keyframes()[index].light_active != 0;
    unwrap(owner.setKeyframe(index, native(owner, value, owner.keyframes()[index].illustration)));
    if (had_light && !value.light) owner.tracking.keyframe_lights.invalidateIndex(index);
  }
  static void remove(Owner& owner, std::size_t, std::size_t index) {
    unwrap(owner.removeKeyframe(index));
    owner.tracking.keyframes.remove(index);
    owner.tracking.keyframe_lights.remove(index);
  }
  static void clear(Owner& owner, std::size_t) {
    owner.clearKeyframes();
    owner.tracking.keyframes.invalidate();
    owner.tracking.keyframe_lights.invalidate();
  }
};

class CinematicLightRef {
 public:
  CinematicLightRef(std::shared_ptr<PythonCinematic> owner, std::shared_ptr<ElementToken> token)
      : owner_(std::move(owner)), token_(std::move(token)) {}

  [[nodiscard]] ArxCinematicLight copy() const {
    const std::size_t index = keyframeIndex();
    const auto key = owner_->keyframes()[index];
    if (key.light_active == 0) throwInvalidReference();
    return key.light;
  }

  void set(const ArxCinematicLight& value) {
    const std::size_t index = keyframeIndex();
    auto key = owner_->keyframes()[index];
    if (key.light_active == 0) throwInvalidReference();
    key.light = value;
    unwrap(owner_->setKeyframe(index, key));
  }

  [[nodiscard]] bool sameIdentity(const CinematicLightRef& other) const {
    (void)copy();
    (void)other.copy();
    return owner_.get() == other.owner_.get() && token_ == other.token_;
  }

 private:
  [[nodiscard]] std::size_t keyframeIndex() const {
    if (!token_->index) throwInvalidReference();
    return *token_->index;
  }

  std::shared_ptr<PythonCinematic> owner_;
  std::shared_ptr<ElementToken> token_;
};

template <class Field>
void bindCinematicLightField(nb::class_<CinematicLightRef>& binding, const char* name,
                             Field ArxCinematicLight::* member) {
  binding.def_prop_rw(
      name,
      [member](const CinematicLightRef& self) { return self.copy().*member; },
      [member](CinematicLightRef& self, const Field& value) {
        auto record = self.copy();
        record.*member = value;
        self.set(record);
      });
}

std::size_t soundKindIndex(SoundKind kind) { return kind == SoundKind::kEffect ? 0U : 1U; }

std::size_t languagePosition(const PythonCinematic& cinematic, LanguageId language) {
  const auto languages = cinematic.languages();
  std::size_t position = 0;
  while (position < languages.size() && languages[position].id < language) ++position;
  return position;
}

std::size_t soundEncodingPosition(const PythonCinematic& cinematic, SoundHandle sound, LanguageId language) {
  const auto encodings = cinematic.soundEncodings();
  for (std::size_t position = 0; position < encodings.size(); ++position) {
    if (encodings[position].sound == sound && encodings[position].language == language) return position;
  }
  return encodings.size();
}

std::size_t ambianceKeyCount(const PythonAmbiance& ambiance, AmbianceTrackIndex track) {
  const ArxAmbianceTrack value = ambiance.tracks()[track];
  return value.kind == static_cast<ArxAmbianceTrackKind>(AmbianceTrackKind::kPanned)
             ? unwrap(ambiance.pannedKeys(track)).size()
             : unwrap(ambiance.positionedKeys(track)).size();
}

template <SoundKind Kind>
struct CinematicSoundAccess {
  using Owner = PythonCinematic;
  using Value = CinematicSoundValue;
  using AddValue = std::string;
  using Key = std::string;
  static std::size_t size(const Owner& owner, std::size_t) { return owner.soundCount(Kind); }
  static CollectionTracker& tracker(Owner& owner, std::size_t) { return owner.tracking.sounds[soundKindIndex(Kind)]; }
  static Value get(const Owner& owner, std::size_t, std::size_t index) {
    const auto value = owner.sounds(Kind)[index];
    return {copyString(value.path)};
  }
  static void set(Owner& owner, std::size_t, std::size_t index, const Value& value) {
    unwrap(owner.setSoundPath(cinematicSoundHandle(Kind, static_cast<SoundIndex>(index)),
                              canonicalResourcePath(value.path)));
  }
  static Key add(Owner& owner, std::size_t, const AddValue& path) {
    const SoundHandle handle = unwrap(owner.addSound(Kind, canonicalResourcePath(path)));
    SoundIndex index = kNoSound;
    checkStatus(soundHandleIndex(handle, index));
    return copyString(owner.sounds(Kind)[index].path);
  }
  static std::size_t positionForKey(const Owner& owner, std::size_t, const Key& path) {
    const std::string canonical = canonicalResourcePath(path);
    const auto sounds = owner.sounds(Kind);
    for (std::size_t index = 0; index < sounds.size(); ++index) {
      if (copyString(sounds[index].path) == canonical) return index;
    }
    throwMissingKey(canonical);
  }
  static void removeKey(Owner& owner, std::size_t, const Key& path) {
    const std::size_t index = positionForKey(owner, 0, path);
    const SoundHandle handle = cinematicSoundHandle(Kind, static_cast<SoundIndex>(index));
    unwrap(owner.removeSound(handle));
    owner.tracking.sounds[soundKindIndex(Kind)].remove(index);
  }
  static ElementDisplayLabel displayLabel(const Owner& owner, std::size_t, std::size_t index) {
    return {"path", copyString(owner.sounds(Kind)[index].path)};
  }
  static std::size_t compact(Owner& owner, std::size_t) {
    const std::size_t removed = unwrap(owner.compactSounds(Kind));
    owner.tracking.sounds[soundKindIndex(Kind)].invalidate();
    return removed;
  }
  static void rebase(Owner& owner, std::size_t, std::string_view path) { unwrap(owner.rebaseSoundPaths(Kind, path)); }
};

using CinematicSoundEffectAccess = CinematicSoundAccess<SoundKind::kEffect>;
using CinematicSpeechAccess = CinematicSoundAccess<SoundKind::kSpeech>;

struct CinematicLanguageAccess {
  using Owner = PythonCinematic;
  using Value = CinematicLanguage;
  using AddValue = std::string;
  using Key = std::string;
  static std::size_t size(const Owner& owner, std::size_t) { return owner.languageCount(); }
  static CollectionTracker& tracker(Owner& owner, std::size_t) { return owner.tracking.languages; }
  static Value get(const Owner& owner, std::size_t, std::size_t index) {
    const auto value = owner.languages()[index];
    return {copyString(value.name)};
  }
  static void set(Owner& owner, std::size_t, std::size_t index, const Value& value) {
    unwrap(owner.setLanguage(owner.languages()[index].id, canonicalLowerIdentifier(value.name)));
  }
  static ElementDisplayLabel displayLabel(const Owner& owner, std::size_t, std::size_t index) {
    return {"name", copyString(owner.languages()[index].name)};
  }
  static Key add(Owner& owner, std::size_t, const AddValue& name) {
    const auto language = unwrap(owner.addLanguage(canonicalLowerIdentifier(name)));
    owner.tracking.languages.insert(languagePosition(owner, language));
    return copyString(owner.languages()[languagePosition(owner, language)].name);
  }
  static std::size_t positionForKey(const Owner& owner, std::size_t, const Key& name) {
    return languagePosition(owner, cinematicLanguageByName(owner, name));
  }
  static void removeKey(Owner& owner, std::size_t, const Key& name) {
    const std::size_t position = positionForKey(owner, 0, name);
    const LanguageId language = owner.languages()[position].id;
    unwrap(owner.removeLanguage(language));
    owner.tracking.languages.remove(position);
  }
};

class CinematicSpeechEncodingMap {
 public:
  explicit CinematicSpeechEncodingMap(ElementRef<CinematicSpeechAccess> speech) : speech_(std::move(speech)) {}

  [[nodiscard]] std::size_t size() const {
    std::size_t result = 0;
    const auto& owner = speech_.owner();
    const SoundHandle sound = handle();
    for (const auto language : owner.languages()) {
      if (soundEncodingPosition(owner, sound, language.id) != owner.soundEncodingCount()) ++result;
    }
    return result;
  }
  [[nodiscard]] nb::iterator iterator() const {
    nb::list result;
    const auto& owner = speech_.owner();
    const SoundHandle sound = handle();
    for (const auto language : owner.languages()) {
      if (soundEncodingPosition(owner, sound, language.id) != owner.soundEncodingCount()) {
        result.append(copyString(language.name));
      }
    }
    return nb::iter(result);
  }
  [[nodiscard]] nb::bytes at(std::string_view name) const {
    const auto found = position(name);
    if (!found) throwMissingKey(std::string(name));
    const auto audio = speech_.owner().soundEncodings()[*found].encoded_audio;
    return toBytes({audio.data, audio.size});
  }
  void set(std::string_view name, nb::handle data) {
    const LanguageId language = cinematicLanguageByName(speech_.owner(), name);
    const auto audio = byteSpan(data);
    unwrap(speech_.owner().setSoundData(handle(), language, {audio.data(), audio.size()}));
  }
  void remove(std::string_view name) {
    if (!position(name)) throwMissingKey(std::string(name));
    unwrap(speech_.owner().clearSoundData(handle(), cinematicLanguageByName(speech_.owner(), name)));
  }
  void clear() {
    std::vector<std::string> names;
    for (nb::handle name : iterator()) names.push_back(nb::cast<std::string>(name));
    for (const std::string& name : names) remove(name);
  }

 private:
  [[nodiscard]] SoundHandle handle() const {
    return cinematicSoundHandle(SoundKind::kSpeech, static_cast<SoundIndex>(speech_.index()));
  }
  [[nodiscard]] std::optional<std::size_t> position(std::string_view name) const {
    const auto& owner = speech_.owner();
    const auto language = owner.findLanguage(canonicalLowerIdentifier(name));
    if (!language) return std::nullopt;
    const std::size_t found = soundEncodingPosition(owner, handle(), *language);
    return found == owner.soundEncodingCount() ? std::nullopt : std::optional<std::size_t>{found};
  }

  ElementRef<CinematicSpeechAccess> speech_;
};

nb::object cinematicIllustrationReference(const std::shared_ptr<PythonCinematic>& owner,
                                          CinematicIllustrationIndex index) {
  if (index == kInvalidCinematicIllustrationIndex) return nb::none();
  return nb::cast(ElementCollection<CinematicIllustrationAccess>(owner).at(static_cast<std::int64_t>(index)));
}

CinematicIllustrationIndex cinematicIllustrationReferenceIndex(const PythonCinematic& owner, nb::handle value) {
  if (value.is_none()) return kInvalidCinematicIllustrationIndex;
  const auto& illustration = nb::cast<const ElementRef<CinematicIllustrationAccess>&>(value);
  if (&illustration.owner() != &owner) throw nb::value_error("illustration belongs to a different cinematic");
  return static_cast<CinematicIllustrationIndex>(illustration.index());
}

nb::object cinematicSoundReference(const std::shared_ptr<PythonCinematic>& owner, SoundHandle handle) {
  if (handle == kNoSoundHandle) return nb::none();
  SoundKind kind = SoundKind::kEffect;
  SoundIndex index = kNoSound;
  checkStatus(soundHandleKind(handle, kind));
  checkStatus(soundHandleIndex(handle, index));
  if (kind == SoundKind::kEffect) {
    return nb::cast(ElementCollection<CinematicSoundEffectAccess>(owner).at(static_cast<std::int64_t>(index)));
  }
  return nb::cast(ElementCollection<CinematicSpeechAccess>(owner).at(static_cast<std::int64_t>(index)));
}

SoundHandle cinematicSoundReferenceHandle(const PythonCinematic& owner, nb::handle value) {
  if (value.is_none()) return kNoSoundHandle;
  if (nb::isinstance<ElementRef<CinematicSoundEffectAccess>>(value)) {
    const auto& sound = nb::cast<const ElementRef<CinematicSoundEffectAccess>&>(value);
    if (&sound.owner() != &owner) throw nb::value_error("sound belongs to a different cinematic");
    return cinematicSoundHandle(SoundKind::kEffect, static_cast<SoundIndex>(sound.index()));
  }
  if (nb::isinstance<ElementRef<CinematicSpeechAccess>>(value)) {
    const auto& sound = nb::cast<const ElementRef<CinematicSpeechAccess>&>(value);
    if (&sound.owner() != &owner) throw nb::value_error("sound belongs to a different cinematic");
    return cinematicSoundHandle(SoundKind::kSpeech, static_cast<SoundIndex>(sound.index()));
  }
  throw nb::type_error("expected a cinematic sound effect or speech reference");
}

struct AmbianceNativeOutput {
  Amb amb;
  ReadOnlySequence<SoundFileValue> sound_files;
};

struct AmbianceBytesOutput {
  std::vector<std::uint8_t> amb;
  ReadOnlySequence<SoundFileValue> sound_files;
};

struct AmbianceGlbOutput {
  std::vector<std::uint8_t> glb;
  ReadOnlySequence<SoundFileValue> sound_files;
};

struct AmbianceImportOutput {
  std::shared_ptr<PythonAmbiance> ambiance;
  std::vector<SoundSourceReference> sound_sources;
};

struct CinematicNativeOutput {
  Cin cin;
  ReadOnlySequence<TextureFile> illustration_files;
  ReadOnlySequence<CinematicSoundFileValue> sound_files;
};

struct CinematicBytesOutput {
  std::vector<std::uint8_t> cin;
  ReadOnlySequence<TextureFile> illustration_files;
  ReadOnlySequence<CinematicSoundFileValue> sound_files;
};

struct CinematicGlbOutput {
  std::vector<std::uint8_t> glb;
  ReadOnlySequence<CinematicSoundFileValue> sound_files;
};

struct CinematicImportOutput {
  std::shared_ptr<PythonCinematic> cinematic;
  std::vector<std::string> illustration_source_paths;
  std::vector<CinematicSoundSourceReference> sound_sources;
};

SoundFileValue soundFile(const Ambiance& ambiance, const SoundFile& value) {
  return {copyString(ambiance.sounds()[value.source_sound].path), value.path, value.encoded_audio};
}

CinematicSoundFileValue soundFile(const Cinematic& cinematic, const CinematicSoundFile& value) {
  SoundKind kind = SoundKind::kEffect;
  SoundIndex index = kNoSound;
  checkStatus(soundHandleKind(value.source_sound, kind));
  checkStatus(soundHandleIndex(value.source_sound, index));
  return {kind,
          copyString(cinematic.sounds(kind)[index].path),
          cinematicLanguageName(cinematic, value.language),
          value.path,
          value.encoded_audio};
}

TextureFile textureFile(const Cinematic& cinematic, const NativeTextureFile& value) {
  return {copyString(cinematic.textures()[value.source_texture].path), value.resource_path, value.encoded_image};
}

SoundSourceReferenceValue soundSource(const Ambiance& ambiance, const SoundSourceReference& value) {
  return {copyString(ambiance.sounds()[value.sound].path), value.path};
}

CinematicSoundSourceReferenceValue soundSource(const Cinematic& cinematic, const CinematicSoundSourceReference& value) {
  SoundKind kind = SoundKind::kEffect;
  SoundIndex index = kNoSound;
  checkStatus(soundHandleKind(value.sound, kind));
  checkStatus(soundHandleIndex(value.sound, index));
  return {kind, copyString(cinematic.sounds(kind)[index].path), value.path};
}

void bindOutputs(nb::module_& module) {
  nb::class_<AmbianceNativeOutput>(module, "AmbianceNativeOutput")
      .def_ro("amb", &AmbianceNativeOutput::amb)
      .def_ro("sound_files", &AmbianceNativeOutput::sound_files);
  nb::class_<AmbianceBytesOutput>(module, "AmbianceBytesOutput")
      .def_prop_ro("amb", [](const AmbianceBytesOutput& value) { return toBytes(value.amb); })
      .def_ro("sound_files", &AmbianceBytesOutput::sound_files);
  nb::class_<AmbianceGlbOutput>(module, "AmbianceGlbOutput")
      .def_prop_ro("glb", [](const AmbianceGlbOutput& value) { return toBytes(value.glb); })
      .def_ro("sound_files", &AmbianceGlbOutput::sound_files);
  nb::class_<AmbianceImportOutput>(module, "AmbianceImport")
      .def_ro("ambiance", &AmbianceImportOutput::ambiance)
      .def_prop_ro(
          "sound_sources",
          [](const AmbianceImportOutput& value) {
            nb::list result;
            for (const auto& source : value.sound_sources) result.append(soundSource(*value.ambiance, source));
            return nb::tuple(result);
          },
          nb::sig("def sound_sources(self) -> tuple[SoundSourceReference, ...]"));
  nb::class_<CinematicNativeOutput>(module, "CinematicNativeOutput")
      .def_ro("cin", &CinematicNativeOutput::cin)
      .def_ro("illustration_files", &CinematicNativeOutput::illustration_files)
      .def_ro("sound_files", &CinematicNativeOutput::sound_files);
  nb::class_<CinematicBytesOutput>(module, "CinematicBytesOutput")
      .def_prop_ro("cin", [](const CinematicBytesOutput& value) { return toBytes(value.cin); })
      .def_ro("illustration_files", &CinematicBytesOutput::illustration_files)
      .def_ro("sound_files", &CinematicBytesOutput::sound_files);
  nb::class_<CinematicGlbOutput>(module, "CinematicGlbOutput")
      .def_prop_ro("glb", [](const CinematicGlbOutput& value) { return toBytes(value.glb); })
      .def_ro("sound_files", &CinematicGlbOutput::sound_files);
  nb::class_<CinematicImportOutput>(module, "CinematicImport")
      .def_ro("cinematic", &CinematicImportOutput::cinematic)
      .def_prop_ro(
          "illustration_source_paths",
          [](const CinematicImportOutput& value) { return snapshot(value.illustration_source_paths); },
          nb::sig("def illustration_source_paths(self) -> tuple[str, ...]"))
      .def_prop_ro(
          "sound_sources",
          [](const CinematicImportOutput& value) {
            nb::list result;
            for (const auto& source : value.sound_sources) {
              result.append(soundSource(*value.cinematic, source));
            }
            return nb::tuple(result);
          },
          nb::sig("def sound_sources(self) -> tuple[CinematicSoundSourceReference, ...]"));
  nb::class_<CinematicLanguage>(module, "CinematicLanguage")
      .def(nb::new_([](const std::string& name) { return new CinematicLanguage{canonicalLowerIdentifier(name)}; }),
           nb::kw_only(),
           nb::arg("name") = "")
      .def_prop_rw(
          "name",
          [](const CinematicLanguage& value) { return value.name; },
          [](CinematicLanguage& value, std::string_view name) { value.name = canonicalLowerIdentifier(name); });
}

void bindAmbianceReferences(nb::module_& module) {
  nb::class_<AmbianceTrackIterator>(module, "_AmbianceTrackCollectionIterator")
      .def("__iter__", [](AmbianceTrackIterator& self) -> AmbianceTrackIterator& { return self; })
      .def("__next__", &AmbianceTrackIterator::next);
  auto tracks =
      nb::class_<AmbianceTrackCollection>(module, "AmbianceTrackCollection", "A live sequence of ambiance tracks.");
  tracks.def("__len__", &AmbianceTrackCollection::size)
      .def("__getitem__", &AmbianceTrackCollection::at, nb::arg("index"))
      .def(
          "__getitem__",
          [](const AmbianceTrackCollection& self, const nb::slice& slice) {
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
          nb::sig("def __getitem__(self, slice: slice) -> list[AmbianceTrackRef]"))
      .def("__setitem__",
           &AmbianceTrackCollection::set,
           nb::arg("index"),
           nb::arg("value"),
           nb::sig("def __setitem__(self, index: int, value: AmbiancePannedTrack | AmbiancePositionedTrack) -> None"))
      .def("__delitem__", &AmbianceTrackCollection::remove, nb::arg("index"))
      .def("append",
           &AmbianceTrackCollection::append,
           nb::arg("value"),
           nb::sig("def append(self, value: AmbiancePannedTrack | AmbiancePositionedTrack) -> None"))
      .def(
          "extend",
          [](const AmbianceTrackCollection& self, const nb::iterable& values) {
            for (nb::handle value : materializeIterable(values)) self.append(value);
          },
          nb::arg("values"),
          nb::sig("def extend(self, values: Iterable[AmbiancePannedTrack | AmbiancePositionedTrack]) -> None"))
      .def("clear", &AmbianceTrackCollection::clear)
      .def("trim_to_master", &AmbianceTrackCollection::trimToMaster)
      .def(
          "__iter__",
          [](const AmbianceTrackCollection& self) { return AmbianceTrackIterator(self); },
          nb::sig("def __iter__(self) -> Iterator[AmbianceTrackRef]"))
      .def("__repr__", [](const AmbianceTrackCollection& self) {
        return std::string("<pistoris.ambiance.TrackCollection len=") + std::to_string(self.size()) + ">";
      });
  bindSequenceProtocol(tracks, "AmbianceTrackRef");
  auto track = nb::class_<AmbianceTrackRef>(
      module, "AmbianceTrackRef", "A live ambiance track reference. Changing kind preserves key identity.");
  track.def_prop_ro("index", &AmbianceTrackRef::index)
      .def("copy", &AmbianceTrackRef::copy, nb::sig("def copy(self) -> AmbiancePannedTrack | AmbiancePositionedTrack"))
      .def_prop_rw(
          "sound",
          [](const AmbianceTrackRef& self) {
            return ambianceSoundReference(self.owner().shared_from_this(), self.soundPath());
          },
          [](AmbianceTrackRef& self, nb::handle sound) {
            self.setSoundPath(ambianceSoundReferencePath(self.owner(), sound));
          },
          nb::for_getter(nb::sig("def sound(self) -> SoundRef | None")),
          nb::for_setter(nb::arg("value").none()),
          nb::for_setter(nb::sig("def sound(self, value: SoundRef | None, /) -> None")))
      .def_prop_rw("kind", &AmbianceTrackRef::kind, &AmbianceTrackRef::setKind)
      .def_prop_ro("keys", &AmbianceTrackRef::keys)
      .def(
          "__eq__",
          [](const AmbianceTrackRef& self, nb::handle other) {
            if (!nb::isinstance<AmbianceTrackRef>(other)) return false;
            try {
              return self.sameIdentity(nb::cast<const AmbianceTrackRef&>(other));
            } catch (const nb::python_error&) {
              PyErr_Clear();
              return false;
            }
          },
          nb::is_operator())
      .def("__repr__", [](const AmbianceTrackRef& self) {
        try {
          return std::string("<pistoris.ambiance.TrackRef index=") + std::to_string(self.index()) + ">";
        } catch (const nb::python_error&) {
          PyErr_Clear();
          return std::string("<pistoris.ambiance.TrackRef invalid>");
        }
      });
  track.attr("__hash__") = nb::none();

  nb::class_<AmbianceKeyIterator>(module, "_AmbianceKeyCollectionIterator")
      .def("__iter__", [](AmbianceKeyIterator& self) -> AmbianceKeyIterator& { return self; })
      .def("__next__", &AmbianceKeyIterator::next);
  auto keys = nb::class_<AmbianceKeyCollection>(
      module, "AmbianceKeyCollection", "A live sequence of keys in a resource or detached track.");
  keys.def("__len__", &AmbianceKeyCollection::size)
      .def("__getitem__", &AmbianceKeyCollection::at, nb::arg("index"))
      .def(
          "__getitem__",
          [](const AmbianceKeyCollection& self, const nb::slice& slice) {
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
          nb::sig("def __getitem__(self, slice: slice) -> list[AmbianceKeyRef]"))
      .def("__setitem__",
           &AmbianceKeyCollection::set,
           nb::arg("index"),
           nb::arg("value"),
           nb::sig("def __setitem__(self, index: int, value: AmbiancePannedKey | AmbiancePositionedKey) -> None"))
      .def("__delitem__", &AmbianceKeyCollection::remove, nb::arg("index"))
      .def("append",
           &AmbianceKeyCollection::append,
           nb::arg("value"),
           nb::sig("def append(self, value: AmbiancePannedKey | AmbiancePositionedKey) -> None"))
      .def(
          "extend",
          [](const AmbianceKeyCollection& self, const nb::iterable& values) {
            for (nb::handle value : materializeIterable(values)) self.append(value);
          },
          nb::arg("values"),
          nb::sig("def extend(self, values: Iterable[AmbiancePannedKey | AmbiancePositionedKey]) -> None"))
      .def("clear", &AmbianceKeyCollection::clear)
      .def(
          "__iter__",
          [](const AmbianceKeyCollection& self) { return AmbianceKeyIterator(self); },
          nb::sig("def __iter__(self) -> Iterator[AmbianceKeyRef]"))
      .def("__repr__", [](const AmbianceKeyCollection& self) {
        return std::string("<pistoris.ambiance.KeyCollection len=") + std::to_string(self.size()) + ">";
      });
  bindSequenceProtocol(keys, "AmbianceKeyRef");

  auto automation = nb::class_<AmbianceAutomationRef>(module, "AmbianceAutomationRef");
  automation.def("copy", &AmbianceAutomationRef::copy)
      .def_prop_rw(
          "first",
          [](const AmbianceAutomationRef& self) { return self.copy().first; },
          [](AmbianceAutomationRef& self, float value) {
            auto automation = self.copy();
            automation.first = value;
            self.set(automation);
          })
      .def_prop_rw(
          "second",
          [](const AmbianceAutomationRef& self) { return self.copy().second; },
          [](AmbianceAutomationRef& self, float value) {
            auto automation = self.copy();
            automation.second = value;
            self.set(automation);
          })
      .def_prop_rw(
          "interval_ms",
          [](const AmbianceAutomationRef& self) { return self.copy().interval_ms; },
          [](AmbianceAutomationRef& self, std::uint32_t value) {
            auto automation = self.copy();
            automation.interval_ms = value;
            self.set(automation);
          })
      .def_prop_rw(
          "mode",
          [](const AmbianceAutomationRef& self) { return static_cast<AmbianceAutomationMode>(self.copy().mode); },
          [](AmbianceAutomationRef& self, AmbianceAutomationMode mode) {
            auto automation = self.copy();
            automation.mode = static_cast<ArxAmbianceAutomationMode>(mode);
            self.set(automation);
          })
      .def(
          "__eq__",
          [](const AmbianceAutomationRef& self, nb::handle other) {
            if (!nb::isinstance<AmbianceAutomationRef>(other)) return false;
            try {
              return self.sameIdentity(nb::cast<const AmbianceAutomationRef&>(other));
            } catch (const nb::python_error&) {
              PyErr_Clear();
              return false;
            }
          },
          nb::is_operator())
      .def("__repr__", [](const AmbianceAutomationRef& self) {
        try {
          (void)self.copy();
          return std::string("<pistoris.ambiance.AutomationRef>");
        } catch (const nb::python_error&) {
          PyErr_Clear();
          return std::string("<pistoris.ambiance.AutomationRef invalid>");
        }
      });
  automation.attr("__hash__") = nb::none();

  auto key = nb::class_<AmbianceKeyRef>(module, "AmbianceKeyRef");
  key.def_prop_ro("index", &AmbianceKeyRef::index)
      .def("copy", &AmbianceKeyRef::copy, nb::sig("def copy(self) -> AmbiancePannedKey | AmbiancePositionedKey"))
      .def_prop_rw(
          "start_delay_ms",
          [](const AmbianceKeyRef& self) {
            return self.field(&ArxAmbiancePannedKey::start_delay_ms, &ArxAmbiancePositionedKey::start_delay_ms);
          },
          [](AmbianceKeyRef& self, std::uint32_t value) {
            self.setField(&ArxAmbiancePannedKey::start_delay_ms, &ArxAmbiancePositionedKey::start_delay_ms, value);
          })
      .def_prop_rw(
          "play_count",
          [](const AmbianceKeyRef& self) {
            return self.field(&ArxAmbiancePannedKey::play_count, &ArxAmbiancePositionedKey::play_count);
          },
          [](AmbianceKeyRef& self, std::uint32_t value) {
            self.setField(&ArxAmbiancePannedKey::play_count, &ArxAmbiancePositionedKey::play_count, value);
          })
      .def_prop_rw(
          "delay_min_ms",
          [](const AmbianceKeyRef& self) {
            return self.field(&ArxAmbiancePannedKey::delay_min_ms, &ArxAmbiancePositionedKey::delay_min_ms);
          },
          [](AmbianceKeyRef& self, std::uint32_t value) {
            self.setField(&ArxAmbiancePannedKey::delay_min_ms, &ArxAmbiancePositionedKey::delay_min_ms, value);
          })
      .def_prop_rw(
          "delay_max_ms",
          [](const AmbianceKeyRef& self) {
            return self.field(&ArxAmbiancePannedKey::delay_max_ms, &ArxAmbiancePositionedKey::delay_max_ms);
          },
          [](AmbianceKeyRef& self, std::uint32_t value) {
            self.setField(&ArxAmbiancePannedKey::delay_max_ms, &ArxAmbiancePositionedKey::delay_max_ms, value);
          })
      .def_prop_rw(
          "volume",
          [](const AmbianceKeyRef& self) { return commonAutomationRef(self, AmbianceAutomationChannel::kVolume); },
          [](AmbianceKeyRef& self, const ArxAmbianceAutomation& value) {
            self.setAutomation(AmbianceAutomationChannel::kVolume, value);
          })
      .def_prop_rw(
          "pitch",
          [](const AmbianceKeyRef& self) { return commonAutomationRef(self, AmbianceAutomationChannel::kPitch); },
          [](AmbianceKeyRef& self, const ArxAmbianceAutomation& value) {
            self.setAutomation(AmbianceAutomationChannel::kPitch, value);
          })
      .def_prop_rw(
          "pan",
          [](const AmbianceKeyRef& self) { return spatialAutomationRef(self, AmbianceAutomationChannel::kPan); },
          [](AmbianceKeyRef& self, const ArxAmbianceAutomation& value) {
            self.setAutomation(AmbianceAutomationChannel::kPan, value);
          },
          nb::for_getter(nb::sig("def pan(self) -> AmbianceAutomationRef | None")),
          nb::for_setter(nb::sig("def pan(self, value: AmbianceAutomation, /) -> None")))
      .def_prop_rw(
          "x",
          [](const AmbianceKeyRef& self) { return spatialAutomationRef(self, AmbianceAutomationChannel::kX); },
          [](AmbianceKeyRef& self, const ArxAmbianceAutomation& value) {
            self.setAutomation(AmbianceAutomationChannel::kX, value);
          },
          nb::for_getter(nb::sig("def x(self) -> AmbianceAutomationRef | None")),
          nb::for_setter(nb::sig("def x(self, value: AmbianceAutomation, /) -> None")))
      .def_prop_rw(
          "y",
          [](const AmbianceKeyRef& self) { return spatialAutomationRef(self, AmbianceAutomationChannel::kY); },
          [](AmbianceKeyRef& self, const ArxAmbianceAutomation& value) {
            self.setAutomation(AmbianceAutomationChannel::kY, value);
          },
          nb::for_getter(nb::sig("def y(self) -> AmbianceAutomationRef | None")),
          nb::for_setter(nb::sig("def y(self, value: AmbianceAutomation, /) -> None")))
      .def_prop_rw(
          "z",
          [](const AmbianceKeyRef& self) { return spatialAutomationRef(self, AmbianceAutomationChannel::kZ); },
          [](AmbianceKeyRef& self, const ArxAmbianceAutomation& value) {
            self.setAutomation(AmbianceAutomationChannel::kZ, value);
          },
          nb::for_getter(nb::sig("def z(self) -> AmbianceAutomationRef | None")),
          nb::for_setter(nb::sig("def z(self, value: AmbianceAutomation, /) -> None")))
      .def(
          "__eq__",
          [](const AmbianceKeyRef& self, nb::handle other) {
            if (!nb::isinstance<AmbianceKeyRef>(other)) return false;
            try {
              return self.sameIdentity(nb::cast<const AmbianceKeyRef&>(other));
            } catch (const nb::python_error&) {
              PyErr_Clear();
              return false;
            }
          },
          nb::is_operator())
      .def("__repr__", [](const AmbianceKeyRef& self) {
        try {
          return std::string("<pistoris.ambiance.KeyRef index=") + std::to_string(self.index()) + ">";
        } catch (const nb::python_error&) {
          PyErr_Clear();
          return std::string("<pistoris.ambiance.KeyRef invalid>");
        }
      });
  key.attr("__hash__") = nb::none();

  const auto builtins = nb::module_::import_("builtins");
  const auto panned_track_type = module.attr("AmbiancePannedTrack");
  const auto panned_keys_getter = nb::cpp_function(
      [](nb::pointer_and_handle<AmbiancePannedTrackValue> value) {
        return AmbianceKeyCollection(nb::borrow<nb::object>(value.h), true);
      },
      nb::is_method(),
      nb::is_getter(),
      nb::sig("def keys(self) -> AmbianceKeyCollection"));
  const auto panned_keys_setter = nb::cpp_function(
      [](AmbiancePannedTrackValue& value, const nb::iterable& keys) {
        std::vector<ArxAmbiancePannedKey> materialized;
        for (nb::handle key : keys) materialized.push_back(nb::cast<ArxAmbiancePannedKey>(key));
        value.keys = std::move(materialized);
        ++value.keys_revision;
      },
      nb::is_method(),
      nb::arg("value"),
      nb::sig("def keys(self, value: Iterable[AmbiancePannedKey], /) -> None"));
  panned_track_type.attr("keys") = builtins.attr("property")(panned_keys_getter, panned_keys_setter);

  const auto positioned_track_type = module.attr("AmbiancePositionedTrack");
  const auto positioned_keys_getter = nb::cpp_function(
      [](nb::pointer_and_handle<AmbiancePositionedTrackValue> value) {
        return AmbianceKeyCollection(nb::borrow<nb::object>(value.h), false);
      },
      nb::is_method(),
      nb::is_getter(),
      nb::sig("def keys(self) -> AmbianceKeyCollection"));
  const auto positioned_keys_setter = nb::cpp_function(
      [](AmbiancePositionedTrackValue& value, const nb::iterable& keys) {
        std::vector<ArxAmbiancePositionedKey> materialized;
        for (nb::handle key : keys) materialized.push_back(nb::cast<ArxAmbiancePositionedKey>(key));
        value.keys = std::move(materialized);
        ++value.keys_revision;
      },
      nb::is_method(),
      nb::arg("value"),
      nb::sig("def keys(self, value: Iterable[AmbiancePositionedKey], /) -> None"));
  positioned_track_type.attr("keys") = builtins.attr("property")(positioned_keys_getter, positioned_keys_setter);

  auto sound = bindElementCollection<AmbianceSoundAccess>(
      module, "AmbianceSoundRef", "AmbianceSoundCollection", "pistoris.ambiance.SoundRef");
  sound
      .def_prop_rw(
          "path",
          [](const ElementRef<AmbianceSoundAccess>& self) {
            return copyString(self.owner().sounds()[self.index()].path);
          },
          [](ElementRef<AmbianceSoundAccess>& self, std::string_view path) {
            unwrap(self.owner().setSoundPath(static_cast<SoundIndex>(self.index()), path));
          })
      .def_prop_rw(
          "encoded_audio",
          [](const ElementRef<AmbianceSoundAccess>& self) {
            const auto audio = self.owner().sounds()[self.index()].encoded_audio;
            return toOptionalBytes({audio.data, audio.size});
          },
          [](ElementRef<AmbianceSoundAccess>& self, nb::handle data) {
            if (data.is_none()) {
              unwrap(self.owner().clearSoundData(static_cast<SoundIndex>(self.index())));
              return;
            }
            const auto audio = byteSpan(data);
            unwrap(self.owner().setSoundData(static_cast<SoundIndex>(self.index()), {audio.data(), audio.size()}));
          },
          nb::for_getter(nb::sig("def encoded_audio(self) -> bytes | None")),
          nb::for_setter(nb::arg("value").none()),
          nb::for_setter(nb::sig("def encoded_audio(self, value: object | None, /) -> None")));
}

void bindCinematicReferences(nb::module_& module) {
  auto illustration = bindElementCollection<CinematicIllustrationAccess>(module,
                                                                         "CinematicIllustrationRef",
                                                                         "CinematicIllustrationCollection",
                                                                         "pistoris.cinematic.IllustrationRef",
                                                                         {.reference_property = nullptr});
  illustration
      .def_prop_rw(
          "path",
          [](const ElementRef<CinematicIllustrationAccess>& self) {
            const auto illustration_value = self.owner().illustrations()[self.index()];
            return illustration_value.texture == kNoTexture
                       ? std::string{}
                       : copyString(self.owner().textures()[illustration_value.texture].path);
          },
          [](ElementRef<CinematicIllustrationAccess>& self, std::string_view path) {
            const auto illustration_value = self.owner().illustrations()[self.index()];
            if (illustration_value.texture == kNoTexture) {
              auto value = self.copy();
              value.path = path;
              self.set(value);
            } else {
              unwrap(self.owner().setTexturePath(illustration_value.texture, path));
            }
          })
      .def_prop_rw(
          "encoded_image",
          [](const ElementRef<CinematicIllustrationAccess>& self) {
            const auto illustration_value = self.owner().illustrations()[self.index()];
            if (illustration_value.texture == kNoTexture) return nb::object(nb::none());
            const auto image = self.owner().textures()[illustration_value.texture].encoded_image;
            return toOptionalBytes({image.data, image.size});
          },
          [](ElementRef<CinematicIllustrationAccess>& self, nb::handle data) {
            const auto illustration_value = self.owner().illustrations()[self.index()];
            if (illustration_value.texture == kNoTexture) {
              auto value = self.copy();
              if (!data.is_none()) {
                const auto image = byteSpan(data);
                value.encoded_image.assign(image.begin(), image.end());
              }
              self.set(value);
            } else if (data.is_none()) {
              unwrap(self.owner().clearTextureImage(illustration_value.texture));
            } else {
              const auto image = byteSpan(data);
              unwrap(self.owner().setTextureImage(illustration_value.texture, {image.data(), image.size()}));
            }
          },
          nb::for_getter(nb::sig("def encoded_image(self) -> bytes | None")),
          nb::for_setter(nb::arg("value").none()),
          nb::for_setter(nb::sig("def encoded_image(self, value: object | None, /) -> None")))
      .def_prop_rw(
          "external_image_extension",
          [](const ElementRef<CinematicIllustrationAccess>& self) {
            const auto illustration_value = self.owner().illustrations()[self.index()];
            return illustration_value.texture == kNoTexture
                       ? std::string{}
                       : copyString(self.owner().textures()[illustration_value.texture].external_image_extension);
          },
          [](ElementRef<CinematicIllustrationAccess>& self, std::string_view extension) {
            const auto illustration_value = self.owner().illustrations()[self.index()];
            if (illustration_value.texture == kNoTexture) {
              auto value = self.copy();
              value.external_image_extension = extension;
              self.set(value);
            } else {
              unwrap(self.owner().setTextureExternalImageExtension(illustration_value.texture, extension));
            }
          })
      .def_prop_rw(
          "subdivision_scale",
          [](const ElementRef<CinematicIllustrationAccess>& self) {
            return self.owner().illustrations()[self.index()].subdivision_scale;
          },
          [](ElementRef<CinematicIllustrationAccess>& self, std::int32_t scale) {
            auto value = self.owner().illustrations()[self.index()];
            value.subdivision_scale = scale;
            unwrap(self.owner().setIllustration(static_cast<CinematicIllustrationIndex>(self.index()), value));
          });

  auto keyframe = bindElementCollection<CinematicKeyframeAccess>(
      module, "CinematicKeyframeRef", "CinematicKeyframeCollection", "pistoris.cinematic.KeyframeRef");
  bindElementField(keyframe, "frame", &CinematicKeyframeValue::frame);
  bindElementField(keyframe, "camera_position", &CinematicKeyframeValue::camera_position);
  bindElementField(keyframe, "camera_roll", &CinematicKeyframeValue::camera_roll);
  bindElementField(keyframe, "color", &CinematicKeyframeValue::color);
  bindElementField(keyframe, "secondary_color", &CinematicKeyframeValue::secondary_color);
  bindElementField(keyframe, "flash_color", &CinematicKeyframeValue::flash_color);
  bindElementField(keyframe, "flash_decay", &CinematicKeyframeValue::flash_decay);
  auto light = nb::class_<CinematicLightRef>(module, "CinematicLightRef", "A live reference to a keyframe light.");
  bindCinematicLightField(light, "position", &ArxCinematicLight::position);
  bindCinematicLightField(light, "fall_in", &ArxCinematicLight::fall_in);
  bindCinematicLightField(light, "fall_out", &ArxCinematicLight::fall_out);
  bindCinematicLightField(light, "color", &ArxCinematicLight::color);
  bindCinematicLightField(light, "intensity", &ArxCinematicLight::intensity);
  bindCinematicLightField(light, "random_intensity", &ArxCinematicLight::random_intensity);
  light.def("copy", &CinematicLightRef::copy)
      .def(
          "__eq__",
          [](const CinematicLightRef& self, nb::handle other) {
            if (!nb::isinstance<CinematicLightRef>(other)) return false;
            try {
              return self.sameIdentity(nb::cast<const CinematicLightRef&>(other));
            } catch (const nb::python_error&) {
              PyErr_Clear();
              return false;
            }
          },
          nb::is_operator())
      .def("__repr__", [](const CinematicLightRef& self) {
        try {
          (void)self.copy();
          return std::string("<pistoris.cinematic.LightRef>");
        } catch (const nb::python_error&) {
          PyErr_Clear();
          return std::string("<pistoris.cinematic.LightRef invalid>");
        }
      });
  light.attr("__hash__") = nb::none();
  bindElementField(keyframe, "outgoing_speed", &CinematicKeyframeValue::outgoing_speed);
  bindElementField(keyframe, "interpolation", &CinematicKeyframeValue::interpolation);
  bindElementField(keyframe, "base_effect", &CinematicKeyframeValue::base_effect);
  bindElementField(keyframe, "post_effect", &CinematicKeyframeValue::post_effect);
  keyframe
      .def_prop_rw(
          "light",
          [](const ElementRef<CinematicKeyframeAccess>& self) -> nb::object {
            if (!self.copy().light) return nb::none();
            auto owner = self.owner().shared_from_this();
            auto token = owner->tracking.keyframe_lights.trackCanonical(self.index());
            return nb::cast(CinematicLightRef(std::move(owner), std::move(token)));
          },
          [](ElementRef<CinematicKeyframeAccess>& self, const std::optional<ArxCinematicLight>& value) {
            auto key = self.copy();
            key.light = value;
            self.set(key);
          },
          nb::for_getter(nb::sig("def light(self) -> CinematicLightRef | None")),
          nb::for_setter(nb::arg("value").none()),
          nb::for_setter(nb::sig("def light(self, value: CinematicLight | None, /) -> None")))
      .def_prop_rw(
          "illustration",
          [](const ElementRef<CinematicKeyframeAccess>& self) {
            return cinematicIllustrationReference(self.owner().shared_from_this(),
                                                  self.owner().keyframes()[self.index()].illustration);
          },
          [](ElementRef<CinematicKeyframeAccess>& self, nb::handle illustration_value) {
            auto key = self.owner().keyframes()[self.index()];
            key.illustration = cinematicIllustrationReferenceIndex(self.owner(), illustration_value);
            unwrap(self.owner().setKeyframe(self.index(), key));
          },
          nb::for_getter(nb::sig("def illustration(self) -> IllustrationRef | None")),
          nb::for_setter(nb::arg("value").none()),
          nb::for_setter(nb::sig("def illustration(self, value: IllustrationRef | None, /) -> None")))
      .def_prop_rw(
          "sound",
          [](const ElementRef<CinematicKeyframeAccess>& self) {
            return cinematicSoundReference(self.owner().shared_from_this(),
                                           self.owner().keyframes()[self.index()].sound);
          },
          [](ElementRef<CinematicKeyframeAccess>& self, nb::handle sound_value) {
            auto key = self.owner().keyframes()[self.index()];
            key.sound = cinematicSoundReferenceHandle(self.owner(), sound_value);
            unwrap(self.owner().setKeyframe(self.index(), key));
          },
          nb::for_getter(nb::sig("def sound(self) -> SoundEffectRef | SpeechRef | None")),
          nb::for_setter(nb::arg("value").none()),
          nb::for_setter(nb::sig("def sound(self, value: SoundEffectRef | SpeechRef | None, /) -> None")));
  keyframe
      .def_prop_rw(
          "crossfade",
          [](const ElementRef<CinematicKeyframeAccess>& self) { return self.copy().crossfade; },
          [](ElementRef<CinematicKeyframeAccess>& self, bool value) {
            auto key = self.copy();
            key.crossfade = value;
            self.set(key);
          })
      .def_prop_rw(
          "dream",
          [](const ElementRef<CinematicKeyframeAccess>& self) { return self.copy().dream; },
          [](ElementRef<CinematicKeyframeAccess>& self, bool value) {
            auto key = self.copy();
            key.dream = value;
            self.set(key);
          });
  const auto keyframe_collection = module.attr("CinematicKeyframeCollection");
  keyframe_collection.attr("add") = nb::cpp_function(
      [](const ElementCollection<CinematicKeyframeAccess>& self,
         const CinematicKeyframeValue& value,
         const ElementRef<CinematicIllustrationAccess>& illustration_value) {
        if (&illustration_value.owner() != self.owner().get()) {
          throw nb::value_error("illustration belongs to a different cinematic");
        }
        const auto index = unwrap(self.owner()->addKeyframe(CinematicKeyframeAccess::native(
            *self.owner(), value, static_cast<CinematicIllustrationIndex>(illustration_value.index()))));
        self.owner()->tracking.keyframes.insert(index);
        self.owner()->tracking.keyframe_lights.insert(index);
        return self.at(static_cast<std::int64_t>(index));
      },
      nb::is_method(),
      nb::arg("keyframe"),
      nb::kw_only(),
      nb::arg("illustration"),
      nb::sig("def add(self, keyframe: CinematicKeyframe, *, illustration: CinematicIllustrationRef) -> "
              "CinematicKeyframeRef"));

  auto sound_effect = bindElementCollection<CinematicSoundEffectAccess>(module,
                                                                        "CinematicSoundEffectRef",
                                                                        "CinematicSoundEffectCollection",
                                                                        "pistoris.cinematic.SoundEffectRef",
                                                                        {.reference_property = nullptr});
  bindElementField(sound_effect, "path", &CinematicSoundValue::path);
  sound_effect.def_prop_rw(
      "encoded_audio",
      [](const ElementRef<CinematicSoundEffectAccess>& self) -> nb::object {
        const SoundHandle sound = cinematicSoundHandle(SoundKind::kEffect, static_cast<SoundIndex>(self.index()));
        const std::size_t position = soundEncodingPosition(self.owner(), sound, kSoundEffects);
        if (position == self.owner().soundEncodingCount()) return nb::none();
        const auto audio = self.owner().soundEncodings()[position].encoded_audio;
        return toBytes({audio.data, audio.size});
      },
      [](ElementRef<CinematicSoundEffectAccess>& self, nb::handle data) {
        const SoundHandle sound = cinematicSoundHandle(SoundKind::kEffect, static_cast<SoundIndex>(self.index()));
        if (data.is_none()) {
          unwrap(self.owner().clearSoundData(sound, kSoundEffects));
          return;
        }
        const auto audio = byteSpan(data);
        unwrap(self.owner().setSoundData(sound, kSoundEffects, {audio.data(), audio.size()}));
      },
      nb::for_getter(nb::sig("def encoded_audio(self) -> bytes | None")),
      nb::for_setter(nb::arg("value").none()),
      nb::for_setter(nb::sig("def encoded_audio(self, value: object | None, /) -> None")));

  auto speech = bindElementCollection<CinematicSpeechAccess>(module,
                                                             "CinematicSpeechRef",
                                                             "CinematicSpeechCollection",
                                                             "pistoris.cinematic.SpeechRef",
                                                             {.reference_property = nullptr});
  bindElementField(speech, "path", &CinematicSoundValue::path);
  speech.def_prop_ro("encodings",
                     [](const ElementRef<CinematicSpeechAccess>& self) { return CinematicSpeechEncodingMap(self); });

  auto speech_encodings = nb::class_<CinematicSpeechEncodingMap>(
      module, "CinematicSpeechEncodingMap", "A live mapping from registered language names to encoded speech audio.");
  speech_encodings.def("__len__", &CinematicSpeechEncodingMap::size)
      .def("__getitem__",
           &CinematicSpeechEncodingMap::at,
           nb::arg("language"),
           nb::sig("def __getitem__(self, language: str, /) -> bytes"))
      .def("__setitem__", &CinematicSpeechEncodingMap::set, nb::arg("language"), nb::arg("encoded_audio"))
      .def("__delitem__", &CinematicSpeechEncodingMap::remove, nb::arg("language"))
      .def("clear", &CinematicSpeechEncodingMap::clear)
      .def("__iter__", &CinematicSpeechEncodingMap::iterator, nb::sig("def __iter__(self) -> Iterator[str]"))
      .def("__repr__", [](const CinematicSpeechEncodingMap& self) {
        nb::dict result;
        for (nb::handle name : self.iterator()) result[name] = self.at(nb::cast<std::string>(name));
        return std::string(nb::repr(result).c_str());
      });
  registerMutableMapping(speech_encodings);
  registerMappingValueEquality(speech_encodings);

  auto language = bindElementCollection<CinematicLanguageAccess>(module,
                                                                 "CinematicLanguageRef",
                                                                 "CinematicLanguageCollection",
                                                                 "pistoris.cinematic.LanguageRef",
                                                                 {.reference_property = nullptr});
  bindElementField(language, "name", &CinematicLanguage::name);
}

void bindAmbiance(nb::module_& module) {
  bindAmbianceReferences(module);
  nb::class_<PythonAmbiance>(
      module,
      "Ambiance",
      "An editable layered-audio ambiance resource. Byte factories parse encoded input, include_sound_sources "
      "controls returned references, and conversion outputs keep sound sidecars explicit.")
      .def(nb::new_([] { return std::make_shared<PythonAmbiance>(); }))
      .def("copy",
           [](const PythonAmbiance& self) {
             return tracked<Ambiance, AmbianceTracking>(Ambiance(static_cast<const Ambiance&>(self)));
           })
      .def("reset",
           [](PythonAmbiance& self) {
             unwrap(self.reset());
             self.tracking.invalidate();
           })
      .def_static(
          "from_amb",
          [](const Amb& data, bool include_sound_sources, NativeTextMode mode) {
            AmbianceImportOutput output;
            auto result = [&] {
              nb::gil_scoped_release release;
              return Ambiance::importNative(data, include_sound_sources ? &output.sound_sources : nullptr, mode);
            }();
            output.ambiance = tracked<Ambiance, AmbianceTracking>(unwrap(std::move(result)));
            return output;
          },
          nb::arg("amb"),
          nb::kw_only(),
          nb::arg("include_sound_sources") = true,
          nb::arg("text_mode") = NativeTextMode::kAuto)
      .def_static(
          "from_amb_bytes",
          [](nb::handle data, bool include_sound_sources, NativeTextMode mode) {
            AmbianceImportOutput output;
            const auto bytes = byteSpan(data);
            auto parsed = [&] {
              nb::gil_scoped_release release;
              return readAmb(bytes);
            }();
            auto amb = unwrap(std::move(parsed));
            auto result = [&] {
              nb::gil_scoped_release release;
              return Ambiance::importNative(amb, include_sound_sources ? &output.sound_sources : nullptr, mode);
            }();
            output.ambiance = tracked<Ambiance, AmbianceTracking>(unwrap(std::move(result)));
            return output;
          },
          nb::arg("data"),
          nb::kw_only(),
          nb::arg("include_sound_sources") = true,
          nb::arg("text_mode") = NativeTextMode::kAuto)
      .def_static(
          "from_glb",
          [](nb::handle data, bool include_sound_sources, float units) {
            AmbianceImportOutput output;
            Ambiance::GlbImportOptions options{.arx_units_per_glb_unit = units};
            const auto bytes = byteSpan(data);
            auto result = [&] {
              nb::gil_scoped_release release;
              return Ambiance::importGlb(bytes, options, include_sound_sources ? &output.sound_sources : nullptr);
            }();
            output.ambiance = tracked<Ambiance, AmbianceTracking>(unwrap(std::move(result)));
            return output;
          },
          nb::arg("data"),
          nb::kw_only(),
          nb::arg("include_sound_sources") = true,
          nb::arg("arx_units_per_glb_unit") = 10.0f)
      .def(
          "to_amb",
          [](const PythonAmbiance& self, bool include_sidecars, NativeTextMode mode) {
            NativeAmbianceBakeOptions options{.include_sound_files = include_sidecars, .text_mode = mode};
            auto result = [&] {
              nb::gil_scoped_release release;
              return self.bakeNativeBundle(options);
            }();
            auto bundle = unwrap(std::move(result));
            AmbianceNativeOutput output;
            output.amb = std::move(bundle.amb);
            output.sound_files.reserve(bundle.sound_files.size());
            for (const auto& file : bundle.sound_files) output.sound_files.append(soundFile(self, file));
            return output;
          },
          nb::kw_only(),
          nb::arg("include_sidecars") = true,
          nb::arg("text_mode") = NativeTextMode::kAuto)
      .def(
          "to_amb_bytes",
          [](const PythonAmbiance& self, bool include_sidecars, NativeTextMode mode) {
            NativeAmbianceBakeOptions options{.include_sound_files = include_sidecars, .text_mode = mode};
            auto baked = [&] {
              nb::gil_scoped_release release;
              return self.bakeNativeBundle(options);
            }();
            auto bundle = unwrap(std::move(baked));
            AmbianceBytesOutput output;
            auto written = [&] {
              nb::gil_scoped_release release;
              return writeAmb(bundle.amb);
            }();
            output.amb = unwrap(std::move(written));
            output.sound_files.reserve(bundle.sound_files.size());
            for (const auto& file : bundle.sound_files) output.sound_files.append(soundFile(self, file));
            return output;
          },
          nb::kw_only(),
          nb::arg("include_sidecars") = true,
          nb::arg("text_mode") = NativeTextMode::kAuto)
      .def(
          "to_glb",
          [](const PythonAmbiance& self, float units, bool include_sidecars, const PythonModel* reference_model) {
            Ambiance::GlbExportOptions options{.arx_units_per_glb_unit = units};
            if (!include_sidecars) {
              AmbianceGlbOutput output;
              auto result = [&] {
                nb::gil_scoped_release release;
                return self.exportGlb(options, reference_model);
              }();
              output.glb = unwrap(std::move(result));
              return output;
            }
            auto result = [&] {
              nb::gil_scoped_release release;
              return self.exportGlbBundle(options, reference_model);
            }();
            auto bundle = unwrap(std::move(result));
            AmbianceGlbOutput output;
            output.glb = std::move(bundle.glb);
            output.sound_files.reserve(bundle.sound_files.size());
            for (const auto& file : bundle.sound_files) output.sound_files.append(soundFile(self, file));
            return output;
          },
          nb::kw_only(),
          nb::arg("arx_units_per_glb_unit") = 10.0f,
          nb::arg("include_sidecars") = true,
          nb::arg("reference_model") = nullptr)
      .def("validate",
           [](const PythonAmbiance& self) {
             auto result = [&] {
               nb::gil_scoped_release release;
               return self.validate();
             }();
             unwrap(std::move(result));
           })
      .def_prop_rw(
          "resource_path",
          [](const PythonAmbiance& self) { return std::string(self.resourcePath()); },
          [](PythonAmbiance& self, const std::string& value) { unwrap(self.setResourcePath(value)); })
      .def("__repr__",
           [](const PythonAmbiance& self) {
             return resourceRepr(
                 "Ambiance", self.resourcePath(), {{"tracks", self.trackCount()}, {"sounds", self.soundCount()}});
           })
      .def_prop_ro("tracks", [](PythonAmbiance& self) { return AmbianceTrackCollection(self.shared_from_this()); })
      .def_prop_ro("sounds",
                   [](PythonAmbiance& self) { return ElementCollection<AmbianceSoundAccess>(self.shared_from_this()); })
      .def_prop_rw(
          "master_track",
          [](PythonAmbiance& self) -> std::optional<AmbianceTrackRef> {
            if (self.trackCount() == 0) return std::nullopt;
            return AmbianceTrackCollection(self.shared_from_this()).at(self.masterTrack());
          },
          [](PythonAmbiance& self, const AmbianceTrackRef& track) {
            if (&track.owner() != &self) throw nb::value_error("master track belongs to a different ambiance");
            unwrap(self.setMasterTrack(static_cast<AmbianceTrackIndex>(track.index())));
          },
          nb::for_getter(nb::sig("def master_track(self) -> AmbianceTrackRef | None")),
          nb::for_setter(nb::sig("def master_track(self, value: AmbianceTrackRef, /) -> None")));
}

void bindCinematic(nb::module_& module) {
  bindCinematicReferences(module);
  nb::class_<PythonCinematic>(
      module,
      "Cinematic",
      "An editable cinematic timeline with illustrations and sounds. Byte factories parse encoded input, "
      "include_* options control returned references, and conversion outputs keep media sidecars explicit.")
      .def(nb::new_([] { return std::make_shared<PythonCinematic>(); }))
      .def("copy",
           [](const PythonCinematic& self) {
             return tracked<Cinematic, CinematicTracking>(Cinematic(static_cast<const Cinematic&>(self)));
           })
      .def("reset",
           [](PythonCinematic& self) {
             unwrap(self.reset());
             self.tracking.invalidate();
           })
      .def_static(
          "from_cin",
          [](const Cin& data, bool include_illustration_sources, bool include_sound_sources, NativeTextMode mode) {
            CinematicImportOutput output;
            auto result = [&] {
              nb::gil_scoped_release release;
              return Cinematic::importNative(data,
                                             include_illustration_sources ? &output.illustration_source_paths : nullptr,
                                             include_sound_sources ? &output.sound_sources : nullptr,
                                             mode);
            }();
            output.cinematic = tracked<Cinematic, CinematicTracking>(unwrap(std::move(result)));
            return output;
          },
          nb::arg("cin"),
          nb::kw_only(),
          nb::arg("include_illustration_sources") = true,
          nb::arg("include_sound_sources") = true,
          nb::arg("text_mode") = NativeTextMode::kAuto)
      .def_static(
          "from_cin_bytes",
          [](nb::handle data, bool include_illustration_sources, bool include_sound_sources, NativeTextMode mode) {
            CinematicImportOutput output;
            const auto bytes = byteSpan(data);
            auto parsed = [&] {
              nb::gil_scoped_release release;
              return readCin(bytes);
            }();
            auto cin = unwrap(std::move(parsed));
            auto result = [&] {
              nb::gil_scoped_release release;
              return Cinematic::importNative(cin,
                                             include_illustration_sources ? &output.illustration_source_paths : nullptr,
                                             include_sound_sources ? &output.sound_sources : nullptr,
                                             mode);
            }();
            output.cinematic = tracked<Cinematic, CinematicTracking>(unwrap(std::move(result)));
            return output;
          },
          nb::arg("data"),
          nb::kw_only(),
          nb::arg("include_illustration_sources") = true,
          nb::arg("include_sound_sources") = true,
          nb::arg("text_mode") = NativeTextMode::kAuto)
      .def_static(
          "from_glb",
          [](nb::handle data, bool include_sound_sources) {
            CinematicImportOutput output;
            const auto bytes = byteSpan(data);
            auto result = [&] {
              nb::gil_scoped_release release;
              return Cinematic::importGlb(bytes, include_sound_sources ? &output.sound_sources : nullptr);
            }();
            output.cinematic = tracked<Cinematic, CinematicTracking>(unwrap(std::move(result)));
            return output;
          },
          nb::arg("data"),
          nb::kw_only(),
          nb::arg("include_sound_sources") = true)
      .def(
          "to_cin",
          [](const PythonCinematic& self,
             bool include_illustration_sidecars,
             bool include_sound_sidecars,
             CinematicIllustrationFormat illustration_format,
             NativeTextMode mode) {
            NativeCinematicBakeOptions options{.include_illustration_files = include_illustration_sidecars,
                                               .include_sound_files = include_sound_sidecars,
                                               .illustration_format = static_cast<ArxImageFormat>(illustration_format),
                                               .text_mode = mode};
            auto result = [&] {
              nb::gil_scoped_release release;
              return self.bakeNativeBundle(options);
            }();
            auto bundle = unwrap(std::move(result));
            CinematicNativeOutput output;
            output.cin = std::move(bundle.cin);
            output.illustration_files.reserve(bundle.illustration_files.size());
            for (const auto& file : bundle.illustration_files)
              output.illustration_files.append(textureFile(self, file));
            output.sound_files.reserve(bundle.sound_files.size());
            for (const auto& file : bundle.sound_files) output.sound_files.append(soundFile(self, file));
            return output;
          },
          nb::kw_only(),
          nb::arg("include_illustration_sidecars") = true,
          nb::arg("include_sound_sidecars") = true,
          nb::arg("illustration_format") = CinematicIllustrationFormat::kAuto,
          nb::arg("text_mode") = NativeTextMode::kAuto)
      .def(
          "to_cin_bytes",
          [](const PythonCinematic& self,
             bool include_illustration_sidecars,
             bool include_sound_sidecars,
             CinematicIllustrationFormat illustration_format,
             NativeTextMode mode) {
            NativeCinematicBakeOptions options{.include_illustration_files = include_illustration_sidecars,
                                               .include_sound_files = include_sound_sidecars,
                                               .illustration_format = static_cast<ArxImageFormat>(illustration_format),
                                               .text_mode = mode};
            auto baked = [&] {
              nb::gil_scoped_release release;
              return self.bakeNativeBundle(options);
            }();
            auto bundle = unwrap(std::move(baked));
            CinematicBytesOutput output;
            auto written = [&] {
              nb::gil_scoped_release release;
              return writeCin(bundle.cin);
            }();
            output.cin = unwrap(std::move(written));
            output.illustration_files.reserve(bundle.illustration_files.size());
            for (const auto& file : bundle.illustration_files)
              output.illustration_files.append(textureFile(self, file));
            output.sound_files.reserve(bundle.sound_files.size());
            for (const auto& file : bundle.sound_files) output.sound_files.append(soundFile(self, file));
            return output;
          },
          nb::kw_only(),
          nb::arg("include_illustration_sidecars") = true,
          nb::arg("include_sound_sidecars") = true,
          nb::arg("illustration_format") = CinematicIllustrationFormat::kAuto,
          nb::arg("text_mode") = NativeTextMode::kAuto)
      .def(
          "to_glb",
          [](const PythonCinematic& self, bool include_sidecars) {
            if (!include_sidecars) {
              CinematicGlbOutput output;
              auto result = [&] {
                nb::gil_scoped_release release;
                return self.exportGlb();
              }();
              output.glb = unwrap(std::move(result));
              return output;
            }
            auto result = [&] {
              nb::gil_scoped_release release;
              return self.exportGlbBundle();
            }();
            auto bundle = unwrap(std::move(result));
            CinematicGlbOutput output;
            output.glb = std::move(bundle.glb);
            output.sound_files.reserve(bundle.sound_files.size());
            for (const auto& file : bundle.sound_files) output.sound_files.append(soundFile(self, file));
            return output;
          },
          nb::kw_only(),
          nb::arg("include_sidecars") = true)
      .def("validate",
           [](const PythonCinematic& self) {
             auto result = [&] {
               nb::gil_scoped_release release;
               return self.validate();
             }();
             unwrap(std::move(result));
           })
      .def_prop_rw(
          "resource_path",
          [](const PythonCinematic& self) { return std::string(self.resourcePath()); },
          [](PythonCinematic& self, const std::string& value) { unwrap(self.setResourcePath(value)); })
      .def("__repr__",
           [](const PythonCinematic& self) {
             return resourceRepr("Cinematic",
                                 self.resourcePath(),
                                 {{"keyframes", self.keyframeCount()}, {"illustrations", self.illustrationCount()}});
           })
      .def_prop_rw(
          "end_frame",
          &Cinematic::endFrame,
          [](PythonCinematic& self, std::int32_t end_frame) { unwrap(self.setTimeline(end_frame, self.fps())); })
      .def_prop_rw("fps",
                   &Cinematic::fps,
                   [](PythonCinematic& self, float fps) { unwrap(self.setTimeline(self.endFrame(), fps)); })
      .def_prop_ro(
          "illustrations",
          [](PythonCinematic& self) { return ElementCollection<CinematicIllustrationAccess>(self.shared_from_this()); })
      .def_prop_ro(
          "keyframes",
          [](PythonCinematic& self) { return ElementCollection<CinematicKeyframeAccess>(self.shared_from_this()); })
      .def_prop_ro(
          "sfx",
          [](PythonCinematic& self) { return ElementCollection<CinematicSoundEffectAccess>(self.shared_from_this()); })
      .def_prop_ro(
          "speech",
          [](PythonCinematic& self) { return ElementCollection<CinematicSpeechAccess>(self.shared_from_this()); })
      .def_prop_ro("languages", [](PythonCinematic& self) {
        return ElementCollection<CinematicLanguageAccess>(self.shared_from_this());
      });
}

}  // namespace

void bindAmbianceCinematic(nb::module_& module) {
  try {
    bindOutputs(module);
  } catch (const std::exception& error) {
    throw std::runtime_error(std::string("outputs: ") + error.what());
  }
  try {
    bindAmbiance(module);
  } catch (const std::exception& error) {
    throw std::runtime_error(std::string("Ambiance: ") + error.what());
  }
  try {
    bindCinematic(module);
  } catch (const std::exception& error) {
    throw std::runtime_error(std::string("Cinematic: ") + error.what());
  }
}

}  // namespace pistoris::python
