// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#include "binding_utils.h"
#include "bindings.h"
#include "resource_state.h"
#include "resource_types.h"

#include <cstddef>
#include <cstdint>
#include <memory>
#include <nanobind/stl/optional.h>
#include <nanobind/stl/string.h>
#include <nanobind/stl/string_view.h>
#include <nanobind/stl/vector.h>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace pistoris::python {
namespace {

struct LevelNativeOutput {  // NOLINT(bugprone-exception-escape) - MSVC unordered_map move may allocate
  Fts fts;
  Llf llf;
  Dlf dlf;
  ReadOnlySequence<TextureFile> texture_files;
};

struct LevelBytesOutput {
  std::vector<std::uint8_t> fts;
  std::vector<std::uint8_t> llf;
  std::vector<std::uint8_t> dlf;
  ReadOnlySequence<TextureFile> texture_files;
};

struct LevelImportOutput {
  std::shared_ptr<PythonLevel> level;
  std::vector<std::string> texture_source_paths;
  std::optional<ArxLevelGlbImportInfo> glb_info;
};

struct LevelGlbOutput {
  std::vector<std::uint8_t> glb;
  ArxLevelModelPreviewReport model_preview_report{};
};

struct RenderedMinimapValue {
  ArxVector2 projection_offset{};
  std::vector<std::uint8_t> encoded_image;
};

struct LevelMinimapValue {
  std::vector<std::uint8_t> encoded_image;
  ArxRect world_xz_bounds{};
};

struct LevelNativeInput {  // NOLINT(bugprone-exception-escape) - MSVC unordered_map move may allocate
  Fts fts;
  std::optional<Llf> llf;
  std::optional<Dlf> dlf;
};

LevelNativeInput readLevelNativeInput(nb::handle fts_data, nb::handle llf_data, nb::handle dlf_data) {
  const auto fts_bytes = byteSpan(fts_data);
  auto fts_result = [&] {
    nb::gil_scoped_release release;
    return readFts(fts_bytes);
  }();
  LevelNativeInput input{.fts = unwrap(std::move(fts_result)), .llf = {}, .dlf = {}};
  std::optional<DlfBundle> dlf_bundle;
  if (!dlf_data.is_none()) {
    const auto dlf_bytes = byteSpan(dlf_data);
    auto dlf_result = [&] {
      nb::gil_scoped_release release;
      return readDlf(dlf_bytes);
    }();
    dlf_bundle = unwrap(std::move(dlf_result));
    input.dlf = std::move(dlf_bundle->dlf);
  }
  if (!llf_data.is_none()) {
    const auto llf_bytes = byteSpan(llf_data);
    auto llf_result = [&] {
      nb::gil_scoped_release release;
      return readLlf(llf_bytes);
    }();
    input.llf = unwrap(std::move(llf_result));
  } else if (dlf_bundle && dlf_bundle->embedded_lighting) {
    input.llf = std::move(dlf_bundle->embedded_lighting);
  }
  return input;
}

TextureFile textureFile(const Level& level, const NativeTextureFile& value) {
  return {copyString(level.textures()[value.source_texture].path), value.resource_path, value.encoded_image};
}

std::optional<std::string> levelTexturePath(const Level& level, TextureIndex texture) {
  if (texture == kNoTexture) return std::nullopt;
  return copyString(level.textures()[texture].path);
}

TextureIndex levelTextureIndex(const Level& level, const std::optional<std::string>& path) {
  if (!path) return kNoTexture;
  const std::string canonical = canonicalResourcePath(*path);
  const auto textures = level.textures();
  for (std::size_t index = 0; index < textures.size(); ++index) {
    if (copyString(textures[index].path) == canonical) return static_cast<TextureIndex>(index);
  }
  throwMissingKey(canonical);
}

std::optional<std::string> levelRoomName(const Level& level, RoomIndex room) {
  if (room == kInvalidRoomIndex) return std::nullopt;
  return copyString(level.rooms()[room].name);
}

RoomIndex levelRoomIndex(const Level& level, const std::optional<std::string>& name) {
  if (!name) return kInvalidRoomIndex;
  const std::string canonical = canonicalIdentifier(*name);
  const auto rooms = level.rooms();
  for (std::size_t index = 0; index < rooms.size(); ++index) {
    if (copyString(rooms[index].name) == canonical) return static_cast<RoomIndex>(index);
  }
  throwMissingKey(canonical);
}

std::optional<std::string> levelPortalName(const Level& level, PortalIndex portal) {
  if (portal == kInvalidPortalIndex) return std::nullopt;
  return copyString(level.portals()[portal].name);
}

PortalIndex levelPortalIndex(const Level& level, const std::optional<std::string>& name) {
  if (!name) return kInvalidPortalIndex;
  const std::string canonical = canonicalIdentifier(*name);
  const auto portals = level.portals();
  for (std::size_t index = 0; index < portals.size(); ++index) {
    if (copyString(portals[index].name) == canonical) return static_cast<PortalIndex>(index);
  }
  throwMissingKey(canonical);
}

std::string levelAnchorName(const Level& level, AnchorIndex anchor) { return copyString(level.anchors()[anchor].name); }

AnchorIndex levelAnchorIndex(const Level& level, std::string_view name) {
  const std::string canonical = canonicalIdentifier(name);
  const auto anchors = level.anchors();
  for (std::size_t index = 0; index < anchors.size(); ++index) {
    if (copyString(anchors[index].name) == canonical) return static_cast<AnchorIndex>(index);
  }
  throwMissingKey(canonical);
}

void commitLevel(PythonLevel& owner, Level&& updated) { static_cast<Level&>(owner) = std::move(updated); }

bool sameLevelVertex(ArxLevelVertex left, ArxLevelVertex right) {
  return left.position.x == right.position.x && left.position.y == right.position.y &&
         left.position.z == right.position.z;
}

ArxLevelFace levelFaceValue(Level& level, const LevelFace& value,
                            const std::optional<ArxLevelFace>& current = std::nullopt) {
  ArxLevelFace result{};
  result.normal = value.normal.value_or(levelFaceNormal(value));
  result.texture = levelTextureIndex(level, value.texture);
  result.room = levelRoomIndex(level, value.room);
  result.flags = value.flags;
  result.transval = value.transval;
  for (std::size_t corner = 0; corner < value.corners.size(); ++corner) {
    const LevelCorner& source = value.corners[corner];
    VertexIndex vertex = kInvalidVertexIndex;
    if (current && sameLevelVertex(level.vertices()[current->corners[corner].vertex], source.vertex)) {
      vertex = current->corners[corner].vertex;
    } else {
      vertex = unwrap(level.addVertex(source.vertex));
    }
    result.corners[corner] = {vertex, source.normal, source.u, source.v, source.color};
  }
  return result;
}

LevelZone levelZone(const Level& level, ZoneIndex index, const ArxLevelZone& value) {
  LevelZone result;
  result.name = copyString(value.name);
  result.reference_y = value.reference_y;
  result.height_mode = value.height_mode;
  result.height = value.height;
  if (value.has_color) result.color = value.color;
  if (value.has_farclip) result.farclip = value.farclip;
  if (value.has_ambiance) result.ambiance = LevelZoneAmbiance{copyString(value.ambiance.name), value.ambiance.volume};
  const auto perimeter = unwrap(level.zonePerimeter(index));
  result.perimeter_xz.reserve(perimeter.size());
  for (std::size_t point = 0; point < perimeter.size(); ++point) result.perimeter_xz.push_back(perimeter[point]);
  return result;
}

LevelPath levelPath(const Level& level, PathIndex index, const ArxLevelPath& value) {
  LevelPath result;
  result.name = copyString(value.name);
  result.position = value.position;
  const auto nodes = unwrap(level.pathNodes(index));
  result.nodes.reserve(nodes.size());
  for (std::size_t node = 0; node < nodes.size(); ++node) result.nodes.push_back(nodes[node]);
  return result;
}

ArxLevelZoneInput zoneInput(const LevelZone& value, ArxLevelZone& storage, ArxLevelZoneAmbiance& ambiance) {
  storage.name = view(value.name);
  storage.perimeter_count = value.perimeter_xz.size();
  storage.reference_y = value.reference_y;
  storage.height_mode = value.height_mode;
  storage.height = value.height;
  if (value.color) {
    storage.has_color = 1;
    storage.color = *value.color;
  }
  if (value.farclip) {
    storage.has_farclip = 1;
    storage.farclip = *value.farclip;
  }
  if (value.ambiance) {
    ambiance.name = view(value.ambiance->name);
    ambiance.volume = value.ambiance->volume;
    storage.has_ambiance = 1;
    storage.ambiance = ambiance;
  }
  return {storage, value.perimeter_xz.data()};
}

struct LevelVertexAccess {
  using Owner = PythonLevel;
  using Value = ArxLevelVertex;
  static std::size_t size(const Owner& owner, std::size_t) { return owner.vertexCount(); }
  static CollectionTracker& tracker(Owner& owner, std::size_t) { return owner.tracking.vertices; }
  static Value get(const Owner& owner, std::size_t, std::size_t index) { return owner.vertices()[index]; }
  static void set(Owner& owner, std::size_t, std::size_t index, Value value) {
    unwrap(owner.setVertex(static_cast<VertexIndex>(index), value));
  }
  static void append(Owner& owner, std::size_t, const Value& value) { (void)unwrap(owner.addVertex(value)); }
  static void extend(Owner& owner, std::size_t, const std::vector<Value>& values) {
    (void)unwrap(owner.addVertices(values));
  }
  static void validate(const Owner& owner, std::size_t) { unwrap(owner.validateVertices()); }
  static std::size_t compact(Owner& owner, std::size_t) {
    const std::size_t removed = unwrap(owner.compactVertices());
    owner.tracking.vertices.invalidate();
    return removed;
  }
};

struct LevelTextureAccess {
  using Owner = PythonLevel;
  using Value = Texture;
  static constexpr const char* collection_value_name =  // NOLINT(readability-identifier-naming)
      "pistoris.Texture";
  static std::size_t size(const Owner& owner, std::size_t) { return owner.textureCount(); }
  static CollectionTracker& tracker(Owner& owner, std::size_t) { return owner.tracking.textures; }
  static Value get(const Owner& owner, std::size_t, std::size_t index) { return Value(owner.textures()[index]); }
  static void set(Owner& owner, std::size_t, std::size_t index, const Value& value) {
    unwrap(owner.setTexture(static_cast<TextureIndex>(index), value.asView()));
  }
  static void append(Owner& owner, std::size_t, const Value& value) { (void)unwrap(owner.addTexture(value.asView())); }
  static ElementDisplayLabel displayLabel(const Owner& owner, std::size_t, std::size_t index) {
    return {"path", copyString(owner.textures()[index].path)};
  }
  static void validate(const Owner& owner, std::size_t) { unwrap(owner.validateTextures()); }
  static std::size_t compact(Owner& owner, std::size_t) {
    const std::size_t removed = unwrap(owner.compactTextures());
    owner.tracking.textures.invalidate();
    return removed;
  }
  static void rebase(Owner& owner, std::size_t, std::string_view path) { unwrap(owner.rebaseTexturePaths(path)); }
};

struct LevelRoomAccess {
  using Owner = PythonLevel;
  using Value = LevelRoom;
  static std::size_t size(const Owner& owner, std::size_t) { return owner.roomCount(); }
  static CollectionTracker& tracker(Owner& owner, std::size_t) { return owner.tracking.rooms; }
  static Value get(const Owner& owner, std::size_t, std::size_t index) { return Value(owner.rooms()[index]); }
  static void set(Owner& owner, std::size_t, std::size_t index, const Value& value) {
    unwrap(owner.setRoom(static_cast<RoomIndex>(index), value.asValue()));
  }
  static void append(Owner& owner, std::size_t, const Value& value) { (void)unwrap(owner.addRoom(value.asValue())); }
  static void remove(Owner& owner, std::size_t, std::size_t index) {
    const auto room = static_cast<RoomIndex>(index);
    std::vector<std::size_t> removed_portals;
    const auto portals = owner.portals();
    for (std::size_t portal = 0; portal < portals.size(); ++portal) {
      if (portals[portal].room_front == room || portals[portal].room_back == room) removed_portals.push_back(portal);
    }
    unwrap(owner.removeRoom(room));
    owner.tracking.portals.removeMany(std::move(removed_portals));
    owner.tracking.rooms.remove(index);
  }
  static void validate(const Owner& owner, std::size_t) { unwrap(owner.validateRooms()); }
};

struct LevelPortalAccess {
  using Owner = PythonLevel;
  using Value = LevelPortal;
  static std::size_t size(const Owner& owner, std::size_t) { return owner.portalCount(); }
  static CollectionTracker& tracker(Owner& owner, std::size_t) { return owner.tracking.portals; }
  static Value get(const Owner& owner, std::size_t, std::size_t index) {
    const ArxLevelPortal source = owner.portals()[index];
    Value result;
    result.name = copyString(source.name);
    result.room_front = levelRoomName(owner, source.room_front);
    result.room_back = levelRoomName(owner, source.room_back);
    result.shape = source.shape;
    std::copy(std::begin(source.vertices), std::end(source.vertices), result.vertices.begin());
    return result;
  }
  static ArxLevelPortal native(const Owner& owner, const Value& value) {
    ArxLevelPortal result{};
    result.name = view(value.name);
    result.room_front = levelRoomIndex(owner, value.room_front);
    result.room_back = levelRoomIndex(owner, value.room_back);
    result.shape = value.shape;
    std::copy(value.vertices.begin(), value.vertices.end(), std::begin(result.vertices));
    return result;
  }
  static void set(Owner& owner, std::size_t, std::size_t index, const Value& value) {
    unwrap(owner.setPortal(static_cast<PortalIndex>(index), native(owner, value)));
  }
  static void append(Owner& owner, std::size_t, const Value& value) {
    (void)unwrap(owner.addPortal(native(owner, value)));
  }
  static void remove(Owner& owner, std::size_t, std::size_t index) {
    unwrap(owner.removePortal(static_cast<PortalIndex>(index)));
    owner.tracking.portals.remove(index);
  }
  static void validate(const Owner& owner, std::size_t) { unwrap(owner.validatePortals()); }
};

class LevelRoomDistanceRef {
 public:
  LevelRoomDistanceRef(ElementRef<LevelRoomAccess> room_a, ElementRef<LevelRoomAccess> room_b)
      : room_a_(std::move(room_a)), room_b_(std::move(room_b)) {}

  [[nodiscard]] RoomIndex roomAIndex() const { return static_cast<RoomIndex>(room_a_.index()); }
  [[nodiscard]] RoomIndex roomBIndex() const { return static_cast<RoomIndex>(room_b_.index()); }
  [[nodiscard]] const ElementRef<LevelRoomAccess>& roomA() const noexcept { return room_a_; }
  [[nodiscard]] const ElementRef<LevelRoomAccess>& roomB() const noexcept { return room_b_; }
  [[nodiscard]] std::size_t index() const {
    const std::size_t room_a = roomAIndex();
    const std::size_t room_b = roomBIndex();
    return room_b * (room_b - 1) / 2 + room_a;
  }
  [[nodiscard]] LevelRoomDistanceValue copy() const {
    const auto source = unwrap(room_a_.owner().roomDistance(roomAIndex(), roomBIndex()));
    return {source.distance,
            levelPortalName(room_a_.owner(), source.portal_a),
            levelPortalName(room_a_.owner(), source.portal_b)};
  }
  void set(const LevelRoomDistanceValue& value) {
    unwrap(room_a_.owner().setRoomDistance({roomAIndex(),
                                            roomBIndex(),
                                            value.distance,
                                            levelPortalIndex(room_a_.owner(), value.portal_a),
                                            levelPortalIndex(room_a_.owner(), value.portal_b)}));
  }
  void reset() { set({}); }
  [[nodiscard]] bool sameIdentity(const LevelRoomDistanceRef& other) const {
    return room_a_.sameIdentity(other.room_a_) && room_b_.sameIdentity(other.room_b_);
  }

 private:
  ElementRef<LevelRoomAccess> room_a_;
  ElementRef<LevelRoomAccess> room_b_;
};

class LevelRoomDistanceCollection {
 public:
  explicit LevelRoomDistanceCollection(std::shared_ptr<PythonLevel> owner) : owner_(std::move(owner)) {}

  [[nodiscard]] std::size_t size() const { return owner_->roomDistanceCount(); }
  [[nodiscard]] LevelRoomDistanceRef at(std::int64_t requested) const {
    std::size_t first = sequenceIndex(requested, size());
    std::size_t second = 1;
    while (first >= second) first -= second++;
    return pair(static_cast<std::int64_t>(first), static_cast<std::int64_t>(second));
  }
  [[nodiscard]] LevelRoomDistanceRef pair(std::int64_t requested_a, std::int64_t requested_b) const {
    const std::size_t room_count = owner_->roomCount();
    const std::size_t room_a = sequenceIndex(requested_a, room_count);
    const std::size_t room_b = sequenceIndex(requested_b, room_count);
    if (room_a == room_b) throw nb::value_error("room distance requires two distinct rooms");
    const std::size_t low = std::min(room_a, room_b);
    const std::size_t high = std::max(room_a, room_b);
    ElementCollection<LevelRoomAccess> rooms(owner_);
    return {rooms.at(static_cast<std::int64_t>(low)), rooms.at(static_cast<std::int64_t>(high))};
  }
  [[nodiscard]] LevelRoomDistanceRef pair(const ElementRef<LevelRoomAccess>& room_a,
                                          const ElementRef<LevelRoomAccess>& room_b) const {
    if (&room_a.owner() != owner_.get() || &room_b.owner() != owner_.get()) {
      throw nb::value_error("rooms belong to a different level");
    }
    return pair(static_cast<std::int64_t>(room_a.index()), static_cast<std::int64_t>(room_b.index()));
  }
  void set(std::int64_t requested, const LevelRoomDistanceValue& value) const { at(requested).set(value); }
  void setPair(std::int64_t room_a, std::int64_t room_b, const LevelRoomDistanceValue& value) const {
    pair(room_a, room_b).set(value);
  }
  void setPair(const ElementRef<LevelRoomAccess>& room_a, const ElementRef<LevelRoomAccess>& room_b,
               const LevelRoomDistanceValue& value) const {
    pair(room_a, room_b).set(value);
  }
  void reset() const { owner_->clearRoomDistances(); }
  void validate() const {
    auto result = [&] {
      nb::gil_scoped_release release;
      return owner_->validateRoomDistances();
    }();
    unwrap(std::move(result));
  }
  void replace(nb::handle distances_source, nb::handle endpoints_source) const {
    ScalarBufferView distances(distances_source, 'f', sizeof(float));
    ScalarBufferView endpoints(endpoints_source, 'I', sizeof(std::uint32_t));
    const std::size_t count = size();
    distances.requireShape({static_cast<Py_ssize_t>(count)});
    endpoints.requireFlatOrShape(checkedBufferProduct(count, 2U), {static_cast<Py_ssize_t>(count), 2});
    unwrap(owner_->replaceRoomDistances(distances.asSpan<float>(), endpoints.asSpan<PortalIndex>()));
  }
  void copy(nb::handle distances_source, nb::handle endpoints_source) const {
    requireCopyDestinations({distances_source, endpoints_source});
    const std::size_t count = size();
    ScalarBufferOutput<float> distances(distances_source, count, {static_cast<Py_ssize_t>(count)});
    ScalarBufferOutput<PortalIndex> endpoints(
        endpoints_source, checkedBufferProduct(count, 2U), {static_cast<Py_ssize_t>(count), 2});
    unwrap(owner_->copyRoomDistances({.distances = distances.span(), .endpoint_portals = endpoints.span()}));
  }
  void generate(float portal_offset, float spacing, float height_offset, float max_link_distance) const {
    auto result = [&] {
      nb::gil_scoped_release release;
      return owner_->generateRoomDistances({.portal_side_offset = portal_offset,
                                            .sample_spacing = spacing,
                                            .sample_height_offset = height_offset,
                                            .max_link_distance = max_link_distance});
    }();
    unwrap(std::move(result));
  }

 private:
  std::shared_ptr<PythonLevel> owner_;
};

class LevelRoomDistanceIterator {
 public:
  explicit LevelRoomDistanceIterator(LevelRoomDistanceCollection collection) : collection_(std::move(collection)) {}

  [[nodiscard]] LevelRoomDistanceRef next() {
    if (index_ >= collection_.size()) throw nb::stop_iteration();
    return collection_.at(static_cast<std::int64_t>(index_++));
  }

 private:
  LevelRoomDistanceCollection collection_;
  std::size_t index_ = 0;
};

struct LevelAnchorAccess {
  using Owner = PythonLevel;
  using Value = LevelAnchor;
  static std::size_t size(const Owner& owner, std::size_t) { return owner.anchorCount(); }
  static CollectionTracker& tracker(Owner& owner, std::size_t) { return owner.tracking.anchors; }
  static Value get(const Owner& owner, std::size_t, std::size_t index) { return Value(owner.anchors()[index]); }
  static void set(Owner& owner, std::size_t, std::size_t index, const Value& value) {
    unwrap(owner.setAnchor(static_cast<AnchorIndex>(index), value.asValue()));
  }
  static void append(Owner& owner, std::size_t, const Value& value) { (void)unwrap(owner.addAnchor(value.asValue())); }
  static void remove(Owner& owner, std::size_t, std::size_t index) {
    const auto anchor = static_cast<AnchorIndex>(index);
    std::vector<std::size_t> removed_connections;
    const auto connections = owner.anchorConnections();
    for (std::size_t connection = 0; connection < connections.size(); ++connection) {
      if (connections[connection].first == anchor || connections[connection].second == anchor) {
        removed_connections.push_back(connection);
      }
    }
    unwrap(owner.removeAnchor(anchor));
    owner.tracking.anchor_connections.removeMany(std::move(removed_connections));
    owner.tracking.anchors.remove(index);
  }
  static void clear(Owner& owner, std::size_t) {
    owner.clearAnchors();
    owner.tracking.anchors.invalidate();
    owner.tracking.anchor_connections.invalidate();
  }
  static void validate(const Owner& owner, std::size_t) { unwrap(owner.validateAnchors()); }
};

struct LevelAnchorConnectionAccess {
  using Owner = PythonLevel;
  using Value = LevelAnchorConnection;
  static std::size_t size(const Owner& owner, std::size_t) { return owner.anchorConnectionCount(); }
  static CollectionTracker& tracker(Owner& owner, std::size_t) { return owner.tracking.anchor_connections; }
  static Value get(const Owner& owner, std::size_t, std::size_t index) {
    const ArxLevelAnchorConnection value = owner.anchorConnections()[index];
    return {levelAnchorName(owner, value.first), levelAnchorName(owner, value.second)};
  }
  static ArxLevelAnchorConnection native(const Owner& owner, const Value& value) {
    return {levelAnchorIndex(owner, value.first), levelAnchorIndex(owner, value.second)};
  }
  static void set(Owner& owner, std::size_t, std::size_t index, const Value& value) {
    unwrap(owner.setAnchorConnection(static_cast<AnchorConnectionIndex>(index), native(owner, value)));
  }
  static void append(Owner& owner, std::size_t, const Value& value) {
    const auto index = unwrap(owner.addAnchorConnection(native(owner, value)));
    owner.tracking.anchor_connections.insert(index);
  }
  static void remove(Owner& owner, std::size_t, std::size_t index) {
    unwrap(owner.removeAnchorConnection(static_cast<AnchorConnectionIndex>(index)));
    owner.tracking.anchor_connections.remove(index);
  }
  static void validate(const Owner& owner, std::size_t) { unwrap(owner.validateAnchorConnections()); }
};

void applyNavSurface(PythonLevel& owner, const std::vector<ArxLevelVertex>& vertices,
                     const std::vector<ArxLevelNavSurfaceTriangle>& triangles) {
  std::vector<float> positions;
  positions.reserve(checkedBufferProduct(vertices.size(), 3U));
  for (const ArxLevelVertex& vertex : vertices) {
    positions.push_back(vertex.position.x);
    positions.push_back(vertex.position.y);
    positions.push_back(vertex.position.z);
  }
  std::vector<NavSurfaceVertexIndex> indices;
  indices.reserve(checkedBufferProduct(triangles.size(), 3U));
  for (const ArxLevelNavSurfaceTriangle& triangle : triangles)
    indices.insert(indices.end(), std::begin(triangle.vertices), std::end(triangle.vertices));
  unwrap(owner.setNavSurface(positions, indices));
}

struct LevelNavVertexAccess {
  using Owner = PythonLevel;
  using Value = ArxLevelVertex;
  static std::size_t size(const Owner& owner, std::size_t) { return owner.navSurfaceInfo().vertex_count; }
  static CollectionTracker& tracker(Owner& owner, std::size_t) { return owner.tracking.nav_vertices; }
  static Value get(const Owner& owner, std::size_t, std::size_t index) { return owner.navSurfaceVertices()[index]; }
  static void set(Owner& owner, std::size_t, std::size_t index, Value value) {
    auto vertices = owner.navSurfaceVertices();
    auto triangles = owner.navSurfaceTriangles();
    std::vector<ArxLevelVertex> copied_vertices;
    copied_vertices.reserve(vertices.size());
    for (std::size_t current = 0; current < vertices.size(); ++current) copied_vertices.push_back(vertices[current]);
    std::vector<ArxLevelNavSurfaceTriangle> copied_triangles;
    copied_triangles.reserve(triangles.size());
    for (std::size_t current = 0; current < triangles.size(); ++current) {
      copied_triangles.push_back(triangles[current]);
    }
    copied_vertices[index] = value;
    applyNavSurface(owner, copied_vertices, copied_triangles);
  }
};

struct LevelNavTriangleAccess {
  using Owner = PythonLevel;
  using Value = LevelNavSurfaceTriangleValue;
  static std::size_t size(const Owner& owner, std::size_t) { return owner.navSurfaceInfo().triangle_count; }
  static CollectionTracker& tracker(Owner& owner, std::size_t) { return owner.tracking.nav_triangles; }
  static Value get(const Owner& owner, std::size_t, std::size_t index) {
    const auto triangle = owner.navSurfaceTriangles()[index];
    const auto vertices = owner.navSurfaceVertices();
    Value result;
    for (std::size_t corner = 0; corner < result.vertices.size(); ++corner) {
      result.vertices[corner] = vertices[triangle.vertices[corner]];
    }
    return result;
  }
  static void set(Owner& owner, std::size_t, std::size_t index, const Value& value) {
    auto vertices = owner.navSurfaceVertices();
    auto triangles = owner.navSurfaceTriangles();
    std::vector<ArxLevelVertex> copied_vertices;
    copied_vertices.reserve(vertices.size());
    for (std::size_t current = 0; current < vertices.size(); ++current) copied_vertices.push_back(vertices[current]);
    std::vector<ArxLevelNavSurfaceTriangle> copied_triangles;
    copied_triangles.reserve(triangles.size());
    for (std::size_t current = 0; current < triangles.size(); ++current) {
      copied_triangles.push_back(triangles[current]);
    }
    ArxLevelNavSurfaceTriangle converted{};
    for (std::size_t corner = 0; corner < value.vertices.size(); ++corner) {
      const NavSurfaceVertexIndex current = copied_triangles[index].vertices[corner];
      if (sameLevelVertex(copied_vertices[current], value.vertices[corner])) {
        converted.vertices[corner] = current;
      } else {
        converted.vertices[corner] = static_cast<NavSurfaceVertexIndex>(copied_vertices.size());
        copied_vertices.push_back(value.vertices[corner]);
      }
    }
    copied_triangles[index] = converted;
    applyNavSurface(owner, copied_vertices, copied_triangles);
  }
};

struct LevelLightAccess {
  using Owner = PythonLevel;
  using Value = LevelLight;
  static std::size_t size(const Owner& owner, std::size_t) { return owner.lightCount(); }
  static CollectionTracker& tracker(Owner& owner, std::size_t) { return owner.tracking.lights; }
  static Value get(const Owner& owner, std::size_t, std::size_t index) { return Value(owner.lights()[index]); }
  static void set(Owner& owner, std::size_t, std::size_t index, const Value& value) {
    unwrap(owner.setLight(static_cast<LightIndex>(index), value.asValue()));
  }
  static void append(Owner& owner, std::size_t, const Value& value) { (void)unwrap(owner.addLight(value.asValue())); }
  static void remove(Owner& owner, std::size_t, std::size_t index) {
    unwrap(owner.removeLight(static_cast<LightIndex>(index)));
    owner.tracking.lights.remove(index);
  }
  static void validate(const Owner& owner, std::size_t) { unwrap(owner.validateLights()); }
};

struct LevelEntityAccess {
  using Owner = PythonLevel;
  using Value = LevelEntity;
  static std::size_t size(const Owner& owner, std::size_t) { return owner.entityCount(); }
  static CollectionTracker& tracker(Owner& owner, std::size_t) { return owner.tracking.entities; }
  static Value get(const Owner& owner, std::size_t, std::size_t index) { return Value(owner.entities()[index]); }
  static void set(Owner& owner, std::size_t, std::size_t index, const Value& value) {
    unwrap(owner.setEntity(static_cast<EntityIndex>(index), value.asValue()));
  }
  static void append(Owner& owner, std::size_t, const Value& value) { (void)unwrap(owner.addEntity(value.asValue())); }
  static void remove(Owner& owner, std::size_t, std::size_t index) {
    unwrap(owner.removeEntity(static_cast<EntityIndex>(index)));
    owner.tracking.entities.remove(index);
  }
  static ElementDisplayLabel displayLabel(const Owner& owner, std::size_t, std::size_t index) {
    return {"class_path", copyString(owner.entities()[index].class_path)};
  }
  static void validate(const Owner& owner, std::size_t) { unwrap(owner.validateEntities()); }
};

struct LevelFogAccess {
  using Owner = PythonLevel;
  using Value = LevelFog;
  static std::size_t size(const Owner& owner, std::size_t) { return owner.fogCount(); }
  static CollectionTracker& tracker(Owner& owner, std::size_t) { return owner.tracking.fogs; }
  static Value get(const Owner& owner, std::size_t, std::size_t index) { return Value(owner.fogs()[index]); }
  static void set(Owner& owner, std::size_t, std::size_t index, const Value& value) {
    unwrap(owner.setFog(static_cast<FogIndex>(index), value.asValue()));
  }
  static void append(Owner& owner, std::size_t, const Value& value) { (void)unwrap(owner.addFog(value.asValue())); }
  static void remove(Owner& owner, std::size_t, std::size_t index) {
    unwrap(owner.removeFog(static_cast<FogIndex>(index)));
    owner.tracking.fogs.remove(index);
  }
  static void validate(const Owner& owner, std::size_t) { unwrap(owner.validateFogs()); }
};

struct LevelZoneAccess {
  using Owner = PythonLevel;
  using Value = LevelZone;
  static std::size_t size(const Owner& owner, std::size_t) { return owner.zoneCount(); }
  static CollectionTracker& tracker(Owner& owner, std::size_t) { return owner.tracking.zones; }
  static Value get(const Owner& owner, std::size_t, std::size_t index) {
    return levelZone(owner, static_cast<ZoneIndex>(index), owner.zones()[index]);
  }
  static void set(Owner& owner, std::size_t, std::size_t index, const Value& value) {
    ArxLevelZone storage{};
    ArxLevelZoneAmbiance ambiance{};
    unwrap(owner.setZone(static_cast<ZoneIndex>(index), zoneInput(value, storage, ambiance)));
  }
  static void append(Owner& owner, std::size_t, const Value& value) {
    ArxLevelZone storage{};
    ArxLevelZoneAmbiance ambiance{};
    (void)unwrap(owner.addZone(zoneInput(value, storage, ambiance)));
  }
  static void remove(Owner& owner, std::size_t, std::size_t index) {
    unwrap(owner.removeZone(static_cast<ZoneIndex>(index)));
    owner.tracking.zones.remove(index);
  }
  static ElementDisplayLabel displayLabel(const Owner& owner, std::size_t, std::size_t index) {
    return {"name", copyString(owner.zones()[index].name)};
  }
  static void validate(const Owner& owner, std::size_t) { unwrap(owner.validateZones()); }
};

struct LevelPathAccess {
  using Owner = PythonLevel;
  using Value = LevelPath;
  static std::size_t size(const Owner& owner, std::size_t) { return owner.pathCount(); }
  static CollectionTracker& tracker(Owner& owner, std::size_t) { return owner.tracking.paths; }
  static Value get(const Owner& owner, std::size_t, std::size_t index) {
    return levelPath(owner, static_cast<PathIndex>(index), owner.paths()[index]);
  }
  static void apply(Owner& owner, std::size_t index, const Value& value) {
    unwrap(owner.setPath(static_cast<PathIndex>(index),
                         {view(value.name), value.position, value.nodes.data(), value.nodes.size()}));
  }
  static void set(Owner& owner, std::size_t, std::size_t index, const Value& value) {
    apply(owner, index, value);
    owner.tracking.path_nodes.invalidate(index);
  }
  static void append(Owner& owner, std::size_t, const Value& value) {
    (void)unwrap(owner.addPath({view(value.name), value.position, value.nodes.data(), value.nodes.size()}));
  }
  static void remove(Owner& owner, std::size_t, std::size_t index) {
    unwrap(owner.removePath(static_cast<PathIndex>(index)));
    owner.tracking.paths.remove(index);
    owner.tracking.path_nodes.removeParent(index);
  }
  static ElementDisplayLabel displayLabel(const Owner& owner, std::size_t, std::size_t index) {
    return {"name", copyString(owner.paths()[index].name)};
  }
  static void validate(const Owner& owner, std::size_t) { unwrap(owner.validatePaths()); }
};

struct LevelPathNodeAccess {
  using Owner = PythonLevel;
  using Value = ArxLevelPathNode;
  using DetachedParent = LevelPath;
  static std::size_t size(const Owner& owner, std::size_t path) {
    return unwrap(owner.pathNodes(static_cast<PathIndex>(path))).size();
  }
  static CollectionTracker& tracker(Owner& owner, std::size_t path) { return owner.tracking.path_nodes[path]; }
  static Value get(const Owner& owner, std::size_t path, std::size_t index) {
    return unwrap(owner.pathNodes(static_cast<PathIndex>(path)))[index];
  }
  static void set(Owner& owner, std::size_t path, std::size_t index, const Value& value) {
    auto record = levelPath(owner, static_cast<PathIndex>(path), owner.paths()[path]);
    record.nodes[index] = value;
    LevelPathAccess::apply(owner, path, record);
  }
  static std::size_t size(const DetachedParent& path) { return path.nodes.size(); }
  static Value get(const DetachedParent& path, std::size_t index) { return path.nodes[index]; }
  static void set(DetachedParent& path, std::size_t index, const Value& value) { path.nodes[index] = value; }
  static std::size_t revision(const DetachedParent& path) { return path.nodes_revision; }
};

nb::object levelTextureReference(const std::shared_ptr<PythonLevel>& owner, const std::optional<std::string>& path) {
  if (!path) return nb::none();
  return nb::cast(ElementCollection<LevelTextureAccess>(owner).at(levelTextureIndex(*owner, path)));
}

std::optional<std::string> levelTextureReferencePath(const PythonLevel& owner, nb::handle value) {
  if (value.is_none()) return std::nullopt;
  const auto& texture = nb::cast<const ElementRef<LevelTextureAccess>&>(value);
  if (&texture.owner() != &owner) throw nb::value_error("texture belongs to a different level");
  return copyString(owner.textures()[texture.index()].path);
}

nb::object levelRoomReference(const std::shared_ptr<PythonLevel>& owner, const std::optional<std::string>& name) {
  if (!name) return nb::none();
  return nb::cast(ElementCollection<LevelRoomAccess>(owner).at(levelRoomIndex(*owner, name)));
}

std::optional<std::string> levelRoomReferenceName(const PythonLevel& owner, nb::handle value) {
  if (value.is_none()) return std::nullopt;
  const auto& room = nb::cast<const ElementRef<LevelRoomAccess>&>(value);
  if (&room.owner() != &owner) throw nb::value_error("room belongs to a different level");
  return copyString(owner.rooms()[room.index()].name);
}

nb::object levelPortalReference(const std::shared_ptr<PythonLevel>& owner, const std::optional<std::string>& name) {
  if (!name) return nb::none();
  return nb::cast(ElementCollection<LevelPortalAccess>(owner).at(levelPortalIndex(*owner, name)));
}

std::optional<std::string> levelPortalReferenceName(const PythonLevel& owner, nb::handle value) {
  if (value.is_none()) return std::nullopt;
  const auto& portal = nb::cast<const ElementRef<LevelPortalAccess>&>(value);
  if (&portal.owner() != &owner) throw nb::value_error("portal belongs to a different level");
  return copyString(owner.portals()[portal.index()].name);
}

nb::object levelAnchorReference(const std::shared_ptr<PythonLevel>& owner, std::string_view name) {
  return nb::cast(ElementCollection<LevelAnchorAccess>(owner).at(levelAnchorIndex(*owner, name)));
}

std::string levelAnchorReferenceName(const PythonLevel& owner, nb::handle value) {
  const auto& anchor = nb::cast<const ElementRef<LevelAnchorAccess>&>(value);
  if (&anchor.owner() != &owner) throw nb::value_error("anchor belongs to a different level");
  return copyString(owner.anchors()[anchor.index()].name);
}

class LevelNavSurfaceView {
 public:
  explicit LevelNavSurfaceView(std::shared_ptr<PythonLevel> owner) : owner_(std::move(owner)) {}
  [[nodiscard]] const std::shared_ptr<PythonLevel>& owner() const noexcept { return owner_; }

 private:
  std::shared_ptr<PythonLevel> owner_;
};

class LevelPlayerSpawnRef {
 public:
  LevelPlayerSpawnRef(std::shared_ptr<PythonLevel> owner, std::shared_ptr<ElementToken> token)
      : owner_(std::move(owner)), token_(std::move(token)) {}

  [[nodiscard]] ArxLevelPlayerSpawn copy() const {
    if (!token_->index) throwInvalidReference();
    const ArxLevelPlayerSpawn value = owner_->playerSpawn();
    if (value.is_usable == 0) throwInvalidReference();
    return value;
  }

  void set(ArxLevelPlayerSpawn value) {
    (void)copy();
    value.is_usable = 1;
    unwrap(owner_->setPlayerSpawn(value));
  }

  void validate() const {
    (void)copy();
    unwrap(owner_->validatePlayerSpawn());
  }

  [[nodiscard]] bool sameIdentity(const LevelPlayerSpawnRef& other) const {
    (void)copy();
    (void)other.copy();
    return owner_.get() == other.owner_.get() && token_ == other.token_;
  }

 private:
  std::shared_ptr<PythonLevel> owner_;
  std::shared_ptr<ElementToken> token_;
};

class LevelMinimapRef {
 public:
  explicit LevelMinimapRef(std::shared_ptr<PythonLevel> owner) : owner_(std::move(owner)) {}
  [[nodiscard]] const std::shared_ptr<PythonLevel>& owner() const noexcept { return owner_; }

 private:
  std::shared_ptr<PythonLevel> owner_;
};

class LevelLoadingScreenRef {
 public:
  explicit LevelLoadingScreenRef(std::shared_ptr<PythonLevel> owner) : owner_(std::move(owner)) {}
  [[nodiscard]] const std::shared_ptr<PythonLevel>& owner() const noexcept { return owner_; }

 private:
  std::shared_ptr<PythonLevel> owner_;
};

void invalidateLevelNavigation(PythonLevel& owner) {
  owner.tracking.nav_vertices.invalidate();
  owner.tracking.nav_triangles.invalidate();
  owner.tracking.anchors.invalidate();
  owner.tracking.anchor_connections.invalidate();
}

void bindOutputs(nb::module_& module) {
  nb::class_<ArxLevelGlbImportInfo>(module, "LevelGlbImportInfo")
      .def_ro("applied_arx_offset", &ArxLevelGlbImportInfo::applied_arx_offset);
  nb::class_<LevelNativeOutput>(module, "LevelNativeOutput")
      .def_ro("fts", &LevelNativeOutput::fts)
      .def_ro("llf", &LevelNativeOutput::llf)
      .def_ro("dlf", &LevelNativeOutput::dlf)
      .def_ro("texture_files", &LevelNativeOutput::texture_files);
  nb::class_<LevelBytesOutput>(module, "LevelBytesOutput")
      .def_prop_ro("fts", [](const LevelBytesOutput& value) { return toBytes(value.fts); })
      .def_prop_ro("llf", [](const LevelBytesOutput& value) { return toBytes(value.llf); })
      .def_prop_ro("dlf", [](const LevelBytesOutput& value) { return toBytes(value.dlf); })
      .def_ro("texture_files", &LevelBytesOutput::texture_files);
  nb::class_<LevelImportOutput>(module, "LevelImport")
      .def_ro("level", &LevelImportOutput::level)
      .def_prop_ro(
          "texture_source_paths",
          [](const LevelImportOutput& value) { return snapshot(value.texture_source_paths); },
          nb::sig("def texture_source_paths(self) -> tuple[str, ...]"))
      .def_ro("glb_info", &LevelImportOutput::glb_info);
  nb::class_<ArxLevelModelPreviewReport>(module, "LevelModelPreviewReport")
      .def_ro("mapped_models", &ArxLevelModelPreviewReport::mapped_models)
      .def_ro("previewed_entities", &ArxLevelModelPreviewReport::previewed_entities)
      .def_ro("skipped_anonymous_models", &ArxLevelModelPreviewReport::skipped_anonymous_models)
      .def_ro("skipped_unmappable_models", &ArxLevelModelPreviewReport::skipped_unmappable_models)
      .def_ro("skipped_duplicate_models", &ArxLevelModelPreviewReport::skipped_duplicate_models)
      .def_ro("skipped_invalid_models", &ArxLevelModelPreviewReport::skipped_invalid_models);
  nb::class_<LevelGlbOutput>(module, "LevelGlbOutput")
      .def_prop_ro("glb", [](const LevelGlbOutput& value) { return toBytes(value.glb); })
      .def_ro("model_preview_report", &LevelGlbOutput::model_preview_report);
  auto rendered_minimap = nb::class_<RenderedMinimapValue>(module, "LevelRenderedMinimap");
  rendered_minimap.def_ro("projection_offset", &RenderedMinimapValue::projection_offset)
      .def_prop_ro("encoded_image", [](const RenderedMinimapValue& value) { return toBytes(value.encoded_image); });
  bindRecordMediaField(rendered_minimap, "encoded_image", &RenderedMinimapValue::encoded_image, false);

  auto minimap = nb::class_<LevelMinimapValue>(module, "LevelMinimap");
  minimap
      .def(nb::new_([](nb::handle encoded_image, ArxRect world_xz_bounds) {
             const auto bytes = byteSpan(encoded_image);
             if (bytes.size() == 0) throw nb::value_error("encoded_image cannot be empty");
             return new LevelMinimapValue{std::vector<std::uint8_t>(bytes.begin(), bytes.end()), world_xz_bounds};
           }),
           nb::kw_only(),
           nb::arg("encoded_image"),
           nb::arg("world_xz_bounds"))
      .def_prop_ro("encoded_image", [](const LevelMinimapValue& value) { return toBytes(value.encoded_image); })
      .def_ro("world_xz_bounds", &LevelMinimapValue::world_xz_bounds);
  bindRecordMediaField(minimap, "encoded_image", &LevelMinimapValue::encoded_image, false);
}

void bindLevelReferences(nb::module_& module) {
  auto vertex = bindElementCollection<LevelVertexAccess>(
      module, "LevelVertexRef", "LevelVertexCollection", "pistoris.level.VertexRef");
  bindElementField(vertex, "position", &ArxLevelVertex::position);
  vertex
      .def(
          "replace",
          [](const ElementCollection<LevelVertexAccess>& self, nb::handle positions_source) {
            ScalarBufferView positions(positions_source, 'f', sizeof(float));
            (void)positions.tupleCount(3U);
            unwrap(self.owner()->replaceVertices(positions.asSpan<float>()));
            self.owner()->tracking.vertices.invalidate();
            self.owner()->tracking.faces.invalidate();
            invalidateLevelNavigation(*self.owner());
          },
          nb::is_method(),
          nb::arg("positions"),
          nb::sig("def replace(self, positions: object) -> None"))
      .def(
          "clear",
          [](const ElementCollection<LevelVertexAccess>& self) {
            self.owner()->clearVertices();
            self.owner()->tracking.vertices.invalidate();
            self.owner()->tracking.faces.invalidate();
            invalidateLevelNavigation(*self.owner());
          },
          nb::is_method());
  module.attr("LevelVertexCollection").attr("replace") = nb::cpp_function(
      [](const ElementCollection<LevelVertexAccess>& self, nb::handle source) {
        ScalarBufferView positions(source, 'f', sizeof(float));
        (void)positions.tupleCount(3U);
        unwrap(self.owner()->replaceVertices(positions.asSpan<float>()));
        self.owner()->tracking.vertices.invalidate();
        self.owner()->tracking.faces.invalidate();
        invalidateLevelNavigation(*self.owner());
      },
      nb::is_method(),
      nb::arg("positions"),
      nb::sig("def replace(self, positions: object) -> None"));
  module.attr("LevelVertexCollection").attr("clear") = nb::cpp_function(
      [](const ElementCollection<LevelVertexAccess>& self) {
        self.owner()->clearVertices();
        self.owner()->tracking.vertices.invalidate();
        self.owner()->tracking.faces.invalidate();
        invalidateLevelNavigation(*self.owner());
      },
      nb::is_method());
  if (PyObject_DelAttrString(vertex.ptr(), "replace") < 0) throw nb::python_error();
  if (PyObject_DelAttrString(vertex.ptr(), "clear") < 0) throw nb::python_error();

  auto face =
      bindElementCollection<LevelFaceAccess>(module, "LevelFaceRef", "LevelFaceCollection", "pistoris.level.FaceRef");
  using LevelFaceCornerVertexRef = NestedElementMemberRef<LevelFaceCornerAccess, ArxLevelVertex>;
  auto detached_vertex = bindNestedElementMemberRef<LevelFaceCornerAccess, ArxLevelVertex>(
      module, "LevelFaceCornerVertexRef", "pistoris.level.FaceCornerVertexRef");
  bindNestedElementMemberField(detached_vertex, "position", &ArxLevelVertex::position);
  auto corner = bindNestedElementCollection<LevelFaceCornerAccess>(
      module, "LevelFaceCornerRef", "LevelFaceCornerCollection", "pistoris.level.FaceCornerRef");
  corner.def_prop_rw(
      "vertex",
      [](const NestedElementRef<LevelFaceCornerAccess>& self) -> nb::object {
        if (!self.resourceBacked()) return nb::cast(LevelFaceCornerVertexRef(self, &LevelCorner::vertex));
        const VertexIndex index = self.owner().faces()[self.parentIndex()].corners[self.index()].vertex;
        return nb::cast(
            ElementCollection<LevelVertexAccess>(self.owner().shared_from_this()).at(static_cast<std::int64_t>(index)));
      },
      [](NestedElementRef<LevelFaceCornerAccess>& self, nb::handle value) {
        if (!self.resourceBacked()) {
          auto corner_value = self.copy();
          corner_value.vertex = nb::cast<ArxLevelVertex>(value);
          self.set(corner_value);
          return;
        }
        const auto& vertex = nb::cast<const ElementRef<LevelVertexAccess>&>(value);
        if (&vertex.owner() != &self.owner()) throw nb::value_error("vertex belongs to a different level");
        ArxLevelFace face_value = self.owner().faces()[self.parentIndex()];
        face_value.corners[self.index()].vertex = static_cast<VertexIndex>(vertex.index());
        unwrap(self.owner().setFace(static_cast<FaceIndex>(self.parentIndex()), face_value));
      },
      nb::for_getter(nb::sig("def vertex(self) -> LevelVertexRef | LevelFaceCornerVertexRef")),
      nb::for_setter(nb::sig("def vertex(self, value: LevelVertexRef | LevelVertex, /) -> None")));
  bindNestedElementField(corner, "normal", &LevelCorner::normal);
  bindNestedElementField(corner, "u", &LevelCorner::u);
  bindNestedElementField(corner, "v", &LevelCorner::v);
  corner.def_prop_rw(
      "color",
      [](const NestedElementRef<LevelFaceCornerAccess>& self) { return self.copy().color; },
      [](NestedElementRef<LevelFaceCornerAccess>& self, ArxColor3 color) {
        if (self.resourceBacked()) {
          unwrap(self.owner().setCornerColor(
              static_cast<FaceIndex>(self.parentIndex()), static_cast<std::uint8_t>(self.index()), color));
        } else {
          auto value = self.copy();
          value.color = color;
          self.set(value);
        }
      });
  bindNestedCollection<LevelFaceCornerAccess>(face, "corners");
  face.def_prop_rw(
      "texture",
      [](const ElementRef<LevelFaceAccess>& self) {
        return levelTextureReference(self.owner().shared_from_this(), self.copy().texture);
      },
      [](ElementRef<LevelFaceAccess>& self, nb::handle texture) {
        auto value = self.copy();
        value.texture = levelTextureReferencePath(self.owner(), texture);
        self.set(value);
      },
      nb::for_getter(nb::sig("def texture(self) -> TextureRef | None")),
      nb::for_setter(nb::arg("value").none()),
      nb::for_setter(nb::sig("def texture(self, value: TextureRef | None, /) -> None")));
  face.def_prop_rw(
      "room",
      [](const ElementRef<LevelFaceAccess>& self) {
        return levelRoomReference(self.owner().shared_from_this(), self.copy().room);
      },
      [](ElementRef<LevelFaceAccess>& self, nb::handle room) {
        auto value = self.copy();
        value.room = levelRoomReferenceName(self.owner(), room);
        self.set(value);
      },
      nb::for_getter(nb::sig("def room(self) -> RoomRef | None")),
      nb::for_setter(nb::arg("value").none()),
      nb::for_setter(nb::sig("def room(self, value: RoomRef | None, /) -> None")));
  face.def_prop_rw(
      "flags",
      [](const ElementRef<LevelFaceAccess>& self) { return static_cast<FaceTypeBitmask>(self.copy().flags); },
      [](ElementRef<LevelFaceAccess>& self, FaceTypeBitmask flags) {
        auto value = self.copy();
        value.flags = flags;
        self.set(value);
      });
  face.def_prop_rw(
      "normal",
      [](const ElementRef<LevelFaceAccess>& self) {
        const LevelFace value = self.copy();
        return value.normal.value_or(levelFaceNormal(value));
      },
      [](ElementRef<LevelFaceAccess>& self, ArxVector3 normal) {
        auto value = self.copy();
        value.normal = normal;
        self.set(value);
      });
  bindElementField(face, "transval", &LevelFace::transval);
  face.def(
          "replace",
          [](const ElementCollection<LevelFaceAccess>& self,
             nb::handle vertex_indices_source,
             nb::handle uvs_source,
             nb::handle corner_normals_source,
             nb::handle textures_source,
             nb::handle transvals_source,
             nb::handle corner_colors_source,
             nb::handle face_normals_source,
             nb::handle flags_source) {
            ScalarBufferView vertex_indices(vertex_indices_source, 'I', sizeof(std::uint32_t));
            const std::size_t count = vertex_indices.tupleCount(3U);
            const std::size_t corner_count = checkedBufferProduct(count, 3U);
            ScalarBufferView uvs(uvs_source, 'f', sizeof(float));
            uvs.requireFlatOrShape(checkedBufferProduct(corner_count, 2U), {static_cast<Py_ssize_t>(count), 3, 2});
            ScalarBufferView corner_normals(corner_normals_source, 'f', sizeof(float));
            corner_normals.requireFlatOrShape(checkedBufferProduct(corner_count, 3U),
                                              {static_cast<Py_ssize_t>(count), 3, 3});
            ScalarBufferView textures(textures_source, 'I', sizeof(TextureIndex));
            textures.requireShape({static_cast<Py_ssize_t>(count)});
            ScalarBufferView transvals(transvals_source, 'f', sizeof(float));
            transvals.requireShape({static_cast<Py_ssize_t>(count)});
            ScalarBufferView corner_colors(corner_colors_source, 'f', sizeof(float));
            if (!(corner_colors.ndim() == 1 && corner_colors.size() == 3U)) {
              corner_colors.requireFlatOrShape(checkedBufferProduct(corner_count, 3U),
                                               {static_cast<Py_ssize_t>(count), 3, 3});
            }
            std::optional<ScalarBufferView> face_normals;
            std::optional<ScalarBufferView> flags;
            std::span<const float> face_normal_values;
            std::span<const FaceType> face_flags;
            if (!face_normals_source.is_none()) {
              face_normals.emplace(face_normals_source, 'f', sizeof(float));
              face_normals->requireFlatOrShape(checkedBufferProduct(count, 3U), {static_cast<Py_ssize_t>(count), 3});
              face_normal_values = face_normals->asSpan<float>();
            }
            if (!flags_source.is_none()) {
              flags.emplace(flags_source, 'I', sizeof(FaceType));
              flags->requireShape({static_cast<Py_ssize_t>(count)});
              face_flags = flags->asSpan<FaceType>();
            }
            unwrap(self.owner()->replaceFaces(vertex_indices.asSpan<std::uint32_t>(),
                                              uvs.asSpan<float>(),
                                              corner_normals.asSpan<float>(),
                                              textures.asSpan<TextureIndex>(),
                                              transvals.asSpan<float>(),
                                              corner_colors.asSpan<float>(),
                                              face_normal_values,
                                              face_flags));
            self.owner()->tracking.faces.invalidate();
            invalidateLevelNavigation(*self.owner());
          },
          nb::is_method(),
          nb::arg("vertex_indices"),
          nb::arg("uvs"),
          nb::arg("corner_normals"),
          nb::arg("textures"),
          nb::arg("transvals"),
          nb::arg("corner_colors"),
          nb::arg("face_normals") = nb::none(),
          nb::arg("flags") = nb::none(),
          nb::sig("def replace(self, vertex_indices: object, uvs: object, corner_normals: object, textures: object, "
                  "transvals: object, corner_colors: object, face_normals: object | None = None, "
                  "flags: object | None = None) -> None"))
      .def(
          "replace_textures",
          [](const ElementCollection<LevelFaceAccess>& self, nb::handle textures_source) {
            ScalarBufferView textures(textures_source, 'I', sizeof(TextureIndex));
            textures.requireShape({static_cast<Py_ssize_t>(self.owner()->faceCount())});
            unwrap(self.owner()->replaceFaceTextures(textures.asSpan<TextureIndex>()));
          },
          nb::is_method(),
          nb::arg("textures"),
          nb::sig("def replace_textures(self, textures: object) -> None"))
      .def(
          "replace_rooms",
          [](const ElementCollection<LevelFaceAccess>& self, nb::handle rooms_source) {
            ScalarBufferView rooms(rooms_source, 'I', sizeof(RoomIndex));
            rooms.requireShape({static_cast<Py_ssize_t>(self.owner()->faceCount())});
            unwrap(self.owner()->replaceFaceRooms(rooms.asSpan<RoomIndex>()));
          },
          nb::is_method(),
          nb::arg("rooms"),
          nb::sig("def replace_rooms(self, rooms: object) -> None"))
      .def(
          "clear",
          [](const ElementCollection<LevelFaceAccess>& self) {
            self.owner()->clearFaces();
            self.owner()->tracking.faces.invalidate();
            invalidateLevelNavigation(*self.owner());
          },
          nb::is_method());

  module.attr("LevelFaceCollection").attr("replace") = face.attr("replace");
  module.attr("LevelFaceCollection").attr("replace_textures") = face.attr("replace_textures");
  module.attr("LevelFaceCollection").attr("replace_rooms") = face.attr("replace_rooms");
  module.attr("LevelFaceCollection").attr("clear") = face.attr("clear");
  for (const char* name : {"replace", "replace_textures", "replace_rooms", "clear"}) {
    if (PyObject_DelAttrString(face.ptr(), name) < 0) throw nb::python_error();
  }

  auto texture = bindElementCollection<LevelTextureAccess>(
      module, "LevelTextureRef", "LevelTextureCollection", "pistoris.level.TextureRef");
  texture
      .def(
          "clear",
          [](const ElementCollection<LevelTextureAccess>& self) {
            self.owner()->clearTextures();
            self.owner()->tracking.textures.invalidate();
          },
          nb::is_method())
      .def_prop_rw(
          "path",
          [](const ElementRef<LevelTextureAccess>& self) {
            return copyString(self.owner().textures()[self.index()].path);
          },
          [](ElementRef<LevelTextureAccess>& self, std::string_view path) {
            unwrap(self.owner().setTexturePath(static_cast<TextureIndex>(self.index()), path));
          })
      .def_prop_rw(
          "encoded_image",
          [](const ElementRef<LevelTextureAccess>& self) {
            const auto image = self.owner().textures()[self.index()].encoded_image;
            return toOptionalBytes({image.data, image.size});
          },
          [](ElementRef<LevelTextureAccess>& self, nb::handle data) {
            if (data.is_none()) {
              unwrap(self.owner().clearTextureImage(static_cast<TextureIndex>(self.index())));
              return;
            }
            const auto image = byteSpan(data);
            unwrap(self.owner().setTextureImage(static_cast<TextureIndex>(self.index()), {image.data(), image.size()}));
          },
          nb::for_getter(nb::sig("def encoded_image(self) -> bytes | None")),
          nb::for_setter(nb::arg("value").none()),
          nb::for_setter(nb::sig("def encoded_image(self, value: object | None, /) -> None")))
      .def_prop_rw(
          "external_image_extension",
          [](const ElementRef<LevelTextureAccess>& self) {
            return copyString(self.owner().textures()[self.index()].external_image_extension);
          },
          [](ElementRef<LevelTextureAccess>& self, std::string_view extension) {
            unwrap(self.owner().setTextureExternalImageExtension(static_cast<TextureIndex>(self.index()), extension));
          });
  module.attr("LevelTextureCollection").attr("clear") = texture.attr("clear");
  if (PyObject_DelAttrString(texture.ptr(), "clear") < 0) throw nb::python_error();

  auto room =
      bindElementCollection<LevelRoomAccess>(module, "LevelRoomRef", "LevelRoomCollection", "pistoris.level.RoomRef");
  bindElementField(room, "name", &LevelRoom::name);
  room.def(
      "clear",
      [](const ElementCollection<LevelRoomAccess>& self) {
        self.owner()->clearRooms();
        self.owner()->tracking.rooms.invalidate();
        self.owner()->tracking.portals.invalidate();
      },
      nb::is_method());
  module.attr("LevelRoomCollection").attr("clear") = room.attr("clear");
  if (PyObject_DelAttrString(room.ptr(), "clear") < 0) throw nb::python_error();

  auto portal = bindElementCollection<LevelPortalAccess>(
      module, "LevelPortalRef", "LevelPortalCollection", "pistoris.level.PortalRef");
  bindElementField(portal, "name", &LevelPortal::name);
  portal
      .def(
          "clear",
          [](const ElementCollection<LevelPortalAccess>& self) {
            self.owner()->clearPortals();
            self.owner()->tracking.portals.invalidate();
          },
          nb::is_method())
      .def_prop_rw(
          "room_front",
          [](const ElementRef<LevelPortalAccess>& self) {
            return levelRoomReference(self.owner().shared_from_this(), self.copy().room_front);
          },
          [](ElementRef<LevelPortalAccess>& self, nb::handle room_value) {
            auto value = self.copy();
            value.room_front = levelRoomReferenceName(self.owner(), room_value);
            self.set(value);
          },
          nb::for_getter(nb::sig("def room_front(self) -> RoomRef | None")),
          nb::for_setter(nb::arg("value").none()),
          nb::for_setter(nb::sig("def room_front(self, value: RoomRef | None, /) -> None")))
      .def_prop_rw(
          "room_back",
          [](const ElementRef<LevelPortalAccess>& self) {
            return levelRoomReference(self.owner().shared_from_this(), self.copy().room_back);
          },
          [](ElementRef<LevelPortalAccess>& self, nb::handle room_value) {
            auto value = self.copy();
            value.room_back = levelRoomReferenceName(self.owner(), room_value);
            self.set(value);
          },
          nb::for_getter(nb::sig("def room_back(self) -> RoomRef | None")),
          nb::for_setter(nb::arg("value").none()),
          nb::for_setter(nb::sig("def room_back(self, value: RoomRef | None, /) -> None")))
      .def_prop_rw(
          "shape",
          [](const ElementRef<LevelPortalAccess>& self) { return static_cast<PortalShape>(self.copy().shape); },
          [](ElementRef<LevelPortalAccess>& self, PortalShape shape) {
            auto value = self.copy();
            value.shape = static_cast<ArxPortalShape>(shape);
            self.set(value);
          })
      .def_prop_rw(
          "vertices",
          [](const ElementRef<LevelPortalAccess>& self) {
            nb::list result;
            for (const auto& value : self.copy().vertices) result.append(value);
            return nb::tuple(result);
          },
          [](ElementRef<LevelPortalAccess>& self, const nb::sequence& vertices) {
            if (nb::len(vertices) != 4) throw nb::value_error("array has the wrong length");
            auto value = self.copy();
            for (std::size_t index = 0; index < 4; ++index)
              value.vertices[index] = nb::cast<ArxVector3>(vertices[index]);
            self.set(value);
          },
          nb::for_getter(nb::sig("def vertices(self) -> tuple[Vector3, Vector3, Vector3, Vector3]")),
          nb::for_setter(nb::sig("def vertices(self, value: Sequence[Vector3], /) -> None")));
  module.attr("LevelPortalCollection").attr("clear") = portal.attr("clear");
  if (PyObject_DelAttrString(portal.ptr(), "clear") < 0) throw nb::python_error();
  const auto portal_collection = module.attr("LevelPortalCollection");
  portal_collection.attr("flatten") =
      nb::cpp_function([](const ElementCollection<LevelPortalAccess>& self) { unwrap(self.owner()->flattenPortals()); },
                       nb::is_method());

  nb::class_<LevelRoomDistanceIterator>(module, "_LevelRoomDistanceCollectionIterator")
      .def("__iter__", [](LevelRoomDistanceIterator& self) -> LevelRoomDistanceIterator& { return self; })
      .def("__next__", &LevelRoomDistanceIterator::next);
  auto room_distances = nb::class_<LevelRoomDistanceCollection>(
      module, "LevelRoomDistanceCollection", "A live sequence and pair-indexed view of room distances.");
  room_distances.def("__len__", &LevelRoomDistanceCollection::size)
      .def("__getitem__", &LevelRoomDistanceCollection::at, nb::arg("index"))
      .def(
          "__getitem__",
          [](const LevelRoomDistanceCollection& self, const nb::tuple& pair) {
            if (nb::len(pair) != 2) throw nb::value_error("room distance key must contain two rooms");
            return self.pair(nb::cast<const ElementRef<LevelRoomAccess>&>(pair[0]),
                             nb::cast<const ElementRef<LevelRoomAccess>&>(pair[1]));
          },
          nb::arg("pair"),
          nb::sig("def __getitem__(self, pair: tuple[LevelRoomRef, LevelRoomRef]) -> LevelRoomDistanceRef"))
      .def(
          "__getitem__",
          [](const LevelRoomDistanceCollection& self, const nb::slice& slice) {
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
          nb::sig("def __getitem__(self, slice: slice) -> list[LevelRoomDistanceRef]"))
      .def("__setitem__", &LevelRoomDistanceCollection::set, nb::arg("index"), nb::arg("value"))
      .def(
          "__setitem__",
          [](const LevelRoomDistanceCollection& self, const nb::tuple& pair, const LevelRoomDistanceValue& value) {
            if (nb::len(pair) != 2) throw nb::value_error("room distance key must contain two rooms");
            self.setPair(nb::cast<const ElementRef<LevelRoomAccess>&>(pair[0]),
                         nb::cast<const ElementRef<LevelRoomAccess>&>(pair[1]),
                         value);
          },
          nb::arg("pair"),
          nb::arg("value"),
          nb::sig("def __setitem__(self, pair: tuple[LevelRoomRef, LevelRoomRef], "
                  "value: LevelRoomDistance, /) -> None"))
      .def("reset", &LevelRoomDistanceCollection::reset)
      .def(
          "copy",
          &LevelRoomDistanceCollection::copy,
          nb::kw_only(),
          nb::arg("distances") = nb::none(),
          nb::arg("endpoint_portals") = nb::none(),
          nb::sig("def copy(self, *, distances: object | None = None, endpoint_portals: object | None = None) -> None"))
      .def("validate", &LevelRoomDistanceCollection::validate)
      .def("replace",
           &LevelRoomDistanceCollection::replace,
           nb::arg("distances"),
           nb::arg("endpoint_portals"),
           nb::sig("def replace(self, distances: object, endpoint_portals: object) -> None"))
      .def("generate",
           &LevelRoomDistanceCollection::generate,
           nb::kw_only(),
           nb::arg("portal_side_offset") = kDefaultRoomDistancePortalOffset,
           nb::arg("sample_spacing") = kDefaultRoomDistanceSampleSpacing,
           nb::arg("sample_height_offset") = kDefaultRoomDistanceSampleHeight,
           nb::arg("max_link_distance") = 0.0f)
      .def("__repr__",
           [](const LevelRoomDistanceCollection& self) {
             return std::string("<pistoris.level.RoomDistanceCollection len=") + std::to_string(self.size()) + ">";
           })
      .def(
          "__iter__",
          [](const LevelRoomDistanceCollection& self) { return LevelRoomDistanceIterator(self); },
          nb::sig("def __iter__(self) -> Iterator[LevelRoomDistanceRef]"));
  bindSequenceProtocol(room_distances, "LevelRoomDistanceRef");
  auto room_distance = nb::class_<LevelRoomDistanceRef>(
      module, "LevelRoomDistanceRef", "A live distance entry identified by two surviving rooms.");
  room_distance.def_prop_ro("room_a", &LevelRoomDistanceRef::roomA)
      .def_prop_ro("room_b", &LevelRoomDistanceRef::roomB)
      .def_prop_rw(
          "distance",
          [](const LevelRoomDistanceRef& self) { return self.copy().distance; },
          [](LevelRoomDistanceRef& self, float value) {
            auto distance = self.copy();
            distance.distance = value;
            self.set(distance);
          })
      .def_prop_rw(
          "portal_a",
          [](const LevelRoomDistanceRef& self) {
            return levelPortalReference(self.roomA().owner().shared_from_this(), self.copy().portal_a);
          },
          [](LevelRoomDistanceRef& self, nb::handle portal) {
            auto distance = self.copy();
            distance.portal_a = levelPortalReferenceName(self.roomA().owner(), portal);
            self.set(distance);
          },
          nb::for_getter(nb::sig("def portal_a(self) -> PortalRef | None")),
          nb::for_setter(nb::arg("value").none()),
          nb::for_setter(nb::sig("def portal_a(self, value: PortalRef | None, /) -> None")))
      .def_prop_rw(
          "portal_b",
          [](const LevelRoomDistanceRef& self) {
            return levelPortalReference(self.roomA().owner().shared_from_this(), self.copy().portal_b);
          },
          [](LevelRoomDistanceRef& self, nb::handle portal) {
            auto distance = self.copy();
            distance.portal_b = levelPortalReferenceName(self.roomA().owner(), portal);
            self.set(distance);
          },
          nb::for_getter(nb::sig("def portal_b(self) -> PortalRef | None")),
          nb::for_setter(nb::arg("value").none()),
          nb::for_setter(nb::sig("def portal_b(self, value: PortalRef | None, /) -> None")))
      .def("copy", &LevelRoomDistanceRef::copy)
      .def("reset", &LevelRoomDistanceRef::reset)
      .def(
          "__eq__",
          [](const LevelRoomDistanceRef& self, nb::handle other) {
            if (!nb::isinstance<LevelRoomDistanceRef>(other)) return false;
            try {
              return self.sameIdentity(nb::cast<const LevelRoomDistanceRef&>(other));
            } catch (const nb::python_error&) {
              PyErr_Clear();
              return false;
            }
          },
          nb::is_operator())
      .def("__repr__", [](const LevelRoomDistanceRef& self) {
        try {
          const std::string room_a = copyString(self.roomA().owner().rooms()[self.roomA().index()].name);
          const std::string room_b = copyString(self.roomB().owner().rooms()[self.roomB().index()].name);
          return std::string("<pistoris.level.RoomDistanceRef room_a=") + nb::repr(nb::cast(room_a)).c_str() +
                 " room_b=" + nb::repr(nb::cast(room_b)).c_str() + ">";
        } catch (const nb::python_error&) {
          PyErr_Clear();
          return std::string("<pistoris.level.RoomDistanceRef invalid>");
        }
      });
  room_distance.attr("__hash__") = nb::none();

  auto anchor = bindElementCollection<LevelAnchorAccess>(
      module, "LevelAnchorRef", "LevelAnchorCollection", "pistoris.level.AnchorRef");
  bindElementField(anchor, "position", &LevelAnchor::position);
  bindElementField(anchor, "radius", &LevelAnchor::radius);
  bindElementField(anchor, "height", &LevelAnchor::height);
  anchor.def_prop_rw(
      "flags",
      [](const ElementRef<LevelAnchorAccess>& self) { return static_cast<AnchorFlagBitmask>(self.copy().flags); },
      [](ElementRef<LevelAnchorAccess>& self, AnchorFlagBitmask flags) {
        auto value = self.copy();
        value.flags = static_cast<std::int16_t>(flags);
        self.set(value);
      });
  bindElementField(anchor, "name", &LevelAnchor::name);
  const auto anchor_collection = module.attr("LevelAnchorCollection");
  anchor_collection.attr("replace") = nb::cpp_function(
      [](const ElementCollection<LevelAnchorAccess>& self,
         nb::handle positions_source,
         nb::handle radii_source,
         nb::handle heights_source,
         nb::handle flags_source) {
        ScalarBufferView positions(positions_source, 'f', sizeof(float));
        const std::size_t count = positions.tupleCount(3U);
        ScalarBufferView radii(radii_source, 'f', sizeof(float));
        ScalarBufferView heights(heights_source, 'f', sizeof(float));
        ScalarBufferView flags(flags_source, 'I', sizeof(std::uint32_t));
        radii.requireShape({static_cast<Py_ssize_t>(count)});
        heights.requireShape({static_cast<Py_ssize_t>(count)});
        flags.requireShape({static_cast<Py_ssize_t>(count)});
        unwrap(self.owner()->replaceAnchors(
            positions.asSpan<float>(), radii.asSpan<float>(), heights.asSpan<float>(), flags.asSpan<std::uint32_t>()));
        self.owner()->tracking.anchors.invalidate();
        self.owner()->tracking.anchor_connections.invalidate();
      },
      nb::is_method(),
      nb::arg("positions"),
      nb::arg("radii"),
      nb::arg("heights"),
      nb::arg("flags"),
      nb::sig("def replace(self, positions: object, radii: object, heights: object, flags: object) -> None"));
  anchor_collection.attr("generate") = nb::cpp_function(
      [](const ElementCollection<LevelAnchorAccess>& self, float spacing, float radius, float height) {
        auto result = [&] {
          nb::gil_scoped_release release;
          return self.owner()->generateAnchors({.sample_spacing = spacing, .radius = radius, .height = height});
        }();
        unwrap(std::move(result));
        self.owner()->tracking.anchors.invalidate();
        self.owner()->tracking.anchor_connections.invalidate();
      },
      nb::is_method(),
      nb::kw_only(),
      nb::arg("sample_spacing") = 100.0f,
      nb::arg("radius") = kDefaultAnchorRadius,
      nb::arg("height") = kDefaultAnchorHeight);
  anchor_collection.attr("prune_islands") = nb::cpp_function(
      [](const ElementCollection<LevelAnchorAccess>& self, float ratio, std::uint32_t count) {
        auto result = [&] {
          nb::gil_scoped_release release;
          return self.owner()->pruneAnchorIslands(
              {.min_component_anchor_ratio = ratio, .min_component_anchor_count = count});
        }();
        unwrap(std::move(result));
        self.owner()->tracking.anchors.invalidate();
        self.owner()->tracking.anchor_connections.invalidate();
      },
      nb::is_method(),
      nb::kw_only(),
      nb::arg("min_component_anchor_ratio").sig("0.05") = 0.05f,
      nb::arg("min_component_anchor_count") = 1);

  auto connection = bindElementCollection<LevelAnchorConnectionAccess>(
      module, "LevelAnchorConnectionRef", "LevelAnchorConnectionCollection", "pistoris.level.AnchorConnectionRef");
  connection
      .def(
          "replace",
          [](const ElementCollection<LevelAnchorConnectionAccess>& self, nb::handle endpoints_source) {
            ScalarBufferView endpoints(endpoints_source, 'I', sizeof(AnchorIndex));
            (void)endpoints.tupleCount(2U);
            unwrap(self.owner()->replaceAnchorConnections(endpoints.asSpan<AnchorIndex>()));
            self.owner()->tracking.anchor_connections.invalidate();
          },
          nb::is_method(),
          nb::arg("endpoints"),
          nb::sig("def replace(self, endpoints: object) -> None"))
      .def(
          "clear",
          [](const ElementCollection<LevelAnchorConnectionAccess>& self) {
            self.owner()->clearAnchorConnections();
            self.owner()->tracking.anchor_connections.invalidate();
          },
          nb::is_method());
  module.attr("LevelAnchorConnectionCollection").attr("replace") = connection.attr("replace");
  module.attr("LevelAnchorConnectionCollection").attr("clear") = connection.attr("clear");
  if (PyObject_DelAttrString(connection.ptr(), "replace") < 0) throw nb::python_error();
  if (PyObject_DelAttrString(connection.ptr(), "clear") < 0) throw nb::python_error();
  connection
      .def_prop_rw(
          "first",
          [](const ElementRef<LevelAnchorConnectionAccess>& self) {
            return levelAnchorReference(self.owner().shared_from_this(), self.copy().first);
          },
          [](ElementRef<LevelAnchorConnectionAccess>& self, nb::handle anchor) {
            auto value = self.copy();
            value.first = levelAnchorReferenceName(self.owner(), anchor);
            self.set(value);
          },
          nb::for_getter(nb::sig("def first(self) -> AnchorRef")),
          nb::for_setter(nb::sig("def first(self, value: AnchorRef, /) -> None")))
      .def_prop_rw(
          "second",
          [](const ElementRef<LevelAnchorConnectionAccess>& self) {
            return levelAnchorReference(self.owner().shared_from_this(), self.copy().second);
          },
          [](ElementRef<LevelAnchorConnectionAccess>& self, nb::handle anchor) {
            auto value = self.copy();
            value.second = levelAnchorReferenceName(self.owner(), anchor);
            self.set(value);
          },
          nb::for_getter(nb::sig("def second(self) -> AnchorRef")),
          nb::for_setter(nb::sig("def second(self, value: AnchorRef, /) -> None")));
  const auto connection_collection = module.attr("LevelAnchorConnectionCollection");
  connection_collection.attr("generate") = nb::cpp_function(
      [](const ElementCollection<LevelAnchorConnectionAccess>& self,
         float max_distance,
         float max_step_distance,
         float max_step_up,
         float radius_scale,
         int max_steps) {
        auto result = [&] {
          nb::gil_scoped_release release;
          return self.owner()->generateAnchorConnections({.max_distance = max_distance,
                                                          .max_step_distance = max_step_distance,
                                                          .max_step_up = max_step_up,
                                                          .radius_scale = radius_scale,
                                                          .max_steps = max_steps});
        }();
        unwrap(std::move(result));
        self.owner()->tracking.anchor_connections.invalidate();
      },
      nb::is_method(),
      nb::kw_only(),
      nb::arg("max_distance") = 150.0f,
      nb::arg("max_step_distance") = 40.0f,
      nb::arg("max_step_up") = 55.0f,
      nb::arg("radius_scale").sig("0.9") = 0.9f,
      nb::arg("max_steps") = 100);

  auto nav_vertex = bindElementCollection<LevelNavVertexAccess>(
      module, "LevelNavVertexRef", "LevelNavVertexCollection", "pistoris.level.NavVertexRef");
  bindElementField(nav_vertex, "position", &ArxLevelVertex::position);
  auto nav_triangle = bindElementCollection<LevelNavTriangleAccess>(
      module, "LevelNavTriangleRef", "LevelNavTriangleCollection", "pistoris.level.NavTriangleRef");
  nav_triangle.def_prop_rw(
      "vertices",
      [](const ElementRef<LevelNavTriangleAccess>& self) {
        nb::list result;
        const auto triangle = self.owner().navSurfaceTriangles()[self.index()];
        ElementCollection<LevelNavVertexAccess> vertices(self.owner().shared_from_this());
        for (const auto vertex : triangle.vertices) result.append(vertices.at(vertex));
        return nb::tuple(result);
      },
      [](ElementRef<LevelNavTriangleAccess>& self, const nb::sequence& vertices) {
        if (nb::len(vertices) != 3) throw nb::value_error("array has the wrong length");
        const auto current_vertices = self.owner().navSurfaceVertices();
        const auto current_triangles = self.owner().navSurfaceTriangles();
        std::vector<ArxLevelVertex> copied_vertices(current_vertices.begin(), current_vertices.end());
        std::vector<ArxLevelNavSurfaceTriangle> copied_triangles(current_triangles.begin(), current_triangles.end());
        for (std::size_t index = 0; index < 3; ++index) {
          const auto& vertex = nb::cast<const ElementRef<LevelNavVertexAccess>&>(vertices[index]);
          if (&vertex.owner() != &self.owner()) throw nb::value_error("vertex belongs to a different level");
          copied_triangles[self.index()].vertices[index] = static_cast<NavSurfaceVertexIndex>(vertex.index());
        }
        applyNavSurface(self.owner(), copied_vertices, copied_triangles);
      },
      nb::for_getter(nb::sig("def vertices(self) -> tuple[NavVertexRef, NavVertexRef, NavVertexRef]")),
      nb::for_setter(nb::sig("def vertices(self, value: Sequence[NavVertexRef], /) -> None")));

  auto light = bindElementCollection<LevelLightAccess>(
      module, "LevelLightRef", "LevelLightCollection", "pistoris.level.LightRef");
  bindElementField(light, "name", &LevelLight::name);
  bindElementField(light, "position", &LevelLight::position);
  bindElementField(light, "color", &LevelLight::color);
  bindElementField(light, "fall_start", &LevelLight::fallstart);
  bindElementField(light, "fall_end", &LevelLight::fallend);
  bindElementField(light, "intensity", &LevelLight::intensity);
  bindElementField(light, "flicker", &LevelLight::flicker);
  bindElementField(light, "effect_radius", &LevelLight::effect_radius);
  bindElementField(light, "effect_frequency", &LevelLight::effect_frequency);
  bindElementField(light, "effect_size", &LevelLight::effect_size);
  bindElementField(light, "effect_speed", &LevelLight::effect_speed);
  bindElementField(light, "flare_size", &LevelLight::flare_size);
  light.def_prop_rw(
      "flags",
      [](const ElementRef<LevelLightAccess>& self) { return static_cast<LightFlagBitmask>(self.copy().flags); },
      [](ElementRef<LevelLightAccess>& self, LightFlagBitmask flags) {
        auto value = self.copy();
        value.flags = flags;
        self.set(value);
      });

  auto entity = bindElementCollection<LevelEntityAccess>(
      module, "LevelEntityRef", "LevelEntityCollection", "pistoris.level.EntityRef");
  bindElementField(entity, "class_path", &LevelEntity::class_path);
  bindElementField(entity, "ident", &LevelEntity::ident);
  bindElementField(entity, "position", &LevelEntity::position);
  bindElementField(entity, "rotation", &LevelEntity::rotation);
  bindElementField(entity, "name", &LevelEntity::name);

  auto fog =
      bindElementCollection<LevelFogAccess>(module, "LevelFogRef", "LevelFogCollection", "pistoris.level.FogRef");
  bindElementField(fog, "position", &LevelFog::position);
  bindElementField(fog, "color", &LevelFog::color);
  bindElementField(fog, "size", &LevelFog::size);
  bindElementField(fog, "directional", &LevelFog::directional);
  bindElementField(fog, "scale", &LevelFog::scale);
  bindElementField(fog, "rotation", &LevelFog::rotation);
  bindElementField(fog, "speed", &LevelFog::speed);
  bindElementField(fog, "rotate_speed", &LevelFog::rotate_speed);
  bindElementField(fog, "lifetime_ms", &LevelFog::lifetime_ms);
  bindElementField(fog, "frequency", &LevelFog::frequency);
  bindElementField(fog, "name", &LevelFog::name);

  auto zone =
      bindElementCollection<LevelZoneAccess>(module, "LevelZoneRef", "LevelZoneCollection", "pistoris.level.ZoneRef");
  bindElementField(zone, "name", &LevelZone::name);
  zone.def_prop_rw(
      "perimeter_xz",
      [](const ElementRef<LevelZoneAccess>& self) { return snapshot(self.copy().perimeter_xz); },
      [](ElementRef<LevelZoneAccess>& self, std::vector<ArxVector2> perimeter) {
        auto value = self.copy();
        value.perimeter_xz = std::move(perimeter);
        self.set(value);
      },
      nb::for_getter(nb::sig("def perimeter_xz(self) -> tuple[Vector2, ...]")),
      nb::for_setter(nb::sig("def perimeter_xz(self, value: Sequence[Vector2], /) -> None")));
  bindElementField(zone, "reference_y", &LevelZone::reference_y);
  zone.def_prop_rw(
      "height_mode",
      [](const ElementRef<LevelZoneAccess>& self) { return static_cast<ZoneHeightMode>(self.copy().height_mode); },
      [](ElementRef<LevelZoneAccess>& self, ZoneHeightMode mode) {
        auto value = self.copy();
        value.height_mode = static_cast<ArxZoneHeightMode>(mode);
        self.set(value);
      });
  bindElementField(zone, "height", &LevelZone::height);
  bindElementField(zone, "color", &LevelZone::color);
  bindElementField(zone, "far_clip", &LevelZone::farclip);
  bindElementField(zone, "ambiance", &LevelZone::ambiance);

  auto path =
      bindElementCollection<LevelPathAccess>(module, "LevelPathRef", "LevelPathCollection", "pistoris.level.PathRef");
  path.def_prop_rw(
      "name",
      [](const ElementRef<LevelPathAccess>& self) { return self.copy().name; },
      [](ElementRef<LevelPathAccess>& self, const std::string& name) {
        auto record = self.copy();
        record.name = name;
        LevelPathAccess::apply(self.owner(), self.index(), record);
      });
  path.def_prop_rw(
      "position",
      [](const ElementRef<LevelPathAccess>& self) { return self.copy().position; },
      [](ElementRef<LevelPathAccess>& self, ArxVector3 position) {
        auto record = self.copy();
        record.position = position;
        LevelPathAccess::apply(self.owner(), self.index(), record);
      });
  auto path_node = bindElementCollection<LevelPathNodeAccess>(
      module, "LevelPathNodeRef", "LevelPathNodeCollection", "pistoris.level.PathNodeRef");
  bindElementField(path_node, "relative_position", &ArxLevelPathNode::relative_position);
  path_node.def_prop_rw(
      "type",
      [](const ElementRef<LevelPathNodeAccess>& self) { return static_cast<PathNodeType>(self.copy().type); },
      [](ElementRef<LevelPathNodeAccess>& self, PathNodeType type) {
        auto value = self.copy();
        value.type = static_cast<ArxPathNodeType>(type);
        self.set(value);
      });
  bindElementField(path_node, "time_ms", &ArxLevelPathNode::time_ms);
  const auto path_value_type = module.attr("LevelPath");
  const auto path_nodes_getter = nb::cpp_function(
      [](nb::pointer_and_handle<LevelPath> value) {
        return ElementCollection<LevelPathNodeAccess>(nb::borrow<nb::object>(value.h));
      },
      nb::is_method(),
      nb::is_getter(),
      nb::sig("def nodes(self) -> LevelPathNodeCollection"));
  const auto path_nodes_setter = nb::cpp_function(
      [](LevelPath& value, std::vector<ArxLevelPathNode> nodes) {
        value.nodes = std::move(nodes);
        ++value.nodes_revision;
      },
      nb::is_method(),
      nb::arg("value"),
      nb::sig("def nodes(self, value: Sequence[LevelPathNode], /) -> None"));
  path_value_type.attr("nodes") =
      nb::module_::import_("builtins").attr("property")(path_nodes_getter, path_nodes_setter);
  path.def_prop_ro("nodes", [](const ElementRef<LevelPathAccess>& self) {
    return ElementCollection<LevelPathNodeAccess>(
        self.owner().shared_from_this(), self.index(), self.owner().tracking.paths);
  });

  nb::class_<LevelNavSurfaceView>(
      module, "LevelNavSurface", "The level navigation surface and its generation operations.")
      .def_prop_ro("info", [](const LevelNavSurfaceView& self) { return self.owner()->navSurfaceInfo(); })
      .def_prop_ro(
          "vertices",
          [](const LevelNavSurfaceView& self) { return ElementCollection<LevelNavVertexAccess>(self.owner()); },
          nb::sig("def vertices(self) -> LevelNavVertexCollection"))
      .def_prop_ro(
          "triangles",
          [](const LevelNavSurfaceView& self) { return ElementCollection<LevelNavTriangleAccess>(self.owner()); },
          nb::sig("def triangles(self) -> LevelNavTriangleCollection"))
      .def("validate",
           [](const LevelNavSurfaceView& self) {
             auto result = [&] {
               nb::gil_scoped_release release;
               return self.owner()->validateNavSurface();
             }();
             unwrap(std::move(result));
           })
      .def(
          "replace",
          [](const LevelNavSurfaceView& self, nb::handle positions_source, nb::handle triangle_indices_source) {
            ScalarBufferView positions(positions_source, 'f', sizeof(float));
            (void)positions.tupleCount(3U);
            ScalarBufferView triangle_indices(triangle_indices_source, 'I', sizeof(NavSurfaceVertexIndex));
            (void)triangle_indices.tupleCount(3U);
            unwrap(self.owner()->setNavSurface(positions.asSpan<float>(),
                                               triangle_indices.asSpan<NavSurfaceVertexIndex>()));
            self.owner()->tracking.nav_vertices.invalidate();
            self.owner()->tracking.nav_triangles.invalidate();
          },
          nb::arg("positions"),
          nb::arg("triangle_indices"),
          nb::sig("def replace(self, positions: object, triangle_indices: object) -> None"))
      .def("clear",
           [](const LevelNavSurfaceView& self) {
             self.owner()->clearNavSurface();
             self.owner()->tracking.nav_vertices.invalidate();
             self.owner()->tracking.nav_triangles.invalidate();
           })
      .def(
          "generate",
          [](const LevelNavSurfaceView& self,
             float radius,
             float height,
             float max_step_up,
             float clearance,
             float support_min_up_cos,
             FaceTypeBitmask ignore_flags) {
            Level::NavSurfaceGenOptions options;
            options.radius = radius;
            options.height = height;
            options.max_step_up = max_step_up;
            options.clearance = clearance;
            options.support_min_up_cos = support_min_up_cos;
            options.support_ignore_flags = static_cast<FaceType>(ignore_flags);
            auto result = [&] {
              nb::gil_scoped_release release;
              return self.owner()->generateNavSurface(options);
            }();
            unwrap(std::move(result));
            self.owner()->tracking.nav_vertices.invalidate();
            self.owner()->tracking.nav_triangles.invalidate();
          },
          nb::kw_only(),
          nb::arg("radius") = kDefaultAnchorRadius,
          nb::arg("height") = kDefaultAnchorHeight,
          nb::arg("max_step_up") = kDefaultNavSurfaceMaxStepUp,
          nb::arg("clearance") = kDefaultNavSurfaceClearance,
          nb::arg("support_min_up_cos").sig("0.5881717") = kDefaultNavSurfaceSupportMinUpCos,
          nb::arg("support_ignore_flags").sig("FaceFlag.TRANS | FaceFlag.WATER | FaceFlag.NOCOL | FaceFlag.LAVA") =
              static_cast<FaceTypeBitmask>(kDefaultNavSurfaceIgnoreFlags))
      .def(
          "from_floor",
          [](const LevelNavSurfaceView& self, float clearance, float support_min_up_cos, FaceTypeBitmask ignore_flags) {
            auto result = [&] {
              nb::gil_scoped_release release;
              return self.owner()->setNavSurfaceFromFloor(
                  {.clearance = clearance,
                   .support_min_up_cos = support_min_up_cos,
                   .support_ignore_flags = static_cast<FaceType>(ignore_flags)});
            }();
            unwrap(std::move(result));
            self.owner()->tracking.nav_vertices.invalidate();
            self.owner()->tracking.nav_triangles.invalidate();
          },
          nb::kw_only(),
          nb::arg("clearance") = kDefaultNavSurfaceClearance,
          nb::arg("support_min_up_cos").sig("0.5881717") = kDefaultNavSurfaceSupportMinUpCos,
          nb::arg("support_ignore_flags").sig("FaceFlag.TRANS | FaceFlag.WATER | FaceFlag.NOCOL | FaceFlag.LAVA") =
              static_cast<FaceTypeBitmask>(kDefaultNavSurfaceIgnoreFlags))
      .def(
          "prune_islands",
          [](const LevelNavSurfaceView& self, float ratio, double area) {
            auto result = [&] {
              nb::gil_scoped_release release;
              return self.owner()->pruneNavSurfaceIslands(
                  {.min_component_area_ratio = ratio, .min_component_area = area});
            }();
            unwrap(std::move(result));
            self.owner()->tracking.nav_vertices.invalidate();
            self.owner()->tracking.nav_triangles.invalidate();
          },
          nb::kw_only(),
          nb::arg("min_component_area_ratio").sig("0.05") = 0.05f,
          nb::arg("min_component_area") = 0.0)
      .def("__repr__", [](const LevelNavSurfaceView& self) {
        const ArxLevelNavSurfaceInfo info = self.owner()->navSurfaceInfo();
        return std::string("<pistoris.level.NavSurface present=") + (info.has_surface != 0U ? "True" : "False") +
               " vertices=" + std::to_string(info.vertex_count) + " triangles=" + std::to_string(info.triangle_count) +
               ">";
      });

  auto player_spawn =
      nb::class_<LevelPlayerSpawnRef>(module, "LevelPlayerSpawnRef", "A live reference to the level player spawn.");
  player_spawn
      .def_prop_rw(
          "position",
          [](const LevelPlayerSpawnRef& self) { return self.copy().position; },
          [](LevelPlayerSpawnRef& self, ArxVector3 position) {
            auto value = self.copy();
            value.position = position;
            self.set(value);
          })
      .def_prop_rw(
          "rotation",
          [](const LevelPlayerSpawnRef& self) { return self.copy().rotation; },
          [](LevelPlayerSpawnRef& self, ArxQuat rotation) {
            auto value = self.copy();
            value.rotation = rotation;
            self.set(value);
          })
      .def("copy", &LevelPlayerSpawnRef::copy, "Return an independent copy of the player spawn.")
      .def("validate", &LevelPlayerSpawnRef::validate)
      .def(
          "__eq__",
          [](const LevelPlayerSpawnRef& self, nb::handle other) {
            if (!nb::isinstance<LevelPlayerSpawnRef>(other)) return false;
            try {
              return self.sameIdentity(nb::cast<const LevelPlayerSpawnRef&>(other));
            } catch (const nb::python_error&) {
              PyErr_Clear();
              return false;
            }
          },
          nb::is_operator())
      .def("__repr__", [](const LevelPlayerSpawnRef& self) {
        try {
          const auto value = self.copy();
          return std::string("<pistoris.level.PlayerSpawnRef position=") + nb::repr(nb::cast(value.position)).c_str() +
                 ">";
        } catch (const nb::python_error&) {
          PyErr_Clear();
          return std::string("<pistoris.level.PlayerSpawnRef invalid>");
        }
      });
  player_spawn.attr("__hash__") = nb::none();

  nb::class_<LevelMinimapRef>(module, "LevelMinimapRef", "The level minimap and its rendering operations.")
      .def_prop_ro(
          "encoded_image",
          [](const LevelMinimapRef& self) -> nb::object {
            const auto image = self.owner()->minimap().encoded_image;
            if (image.size == 0) return nb::none();
            return toBytes({image.data, image.size});
          },
          nb::sig("def encoded_image(self) -> bytes | None"))
      .def_prop_ro("world_xz_bounds",
                   [](const LevelMinimapRef& self) -> std::optional<ArxRect> {
                     const auto value = self.owner()->minimap();
                     return value.encoded_image.size == 0 ? std::nullopt : std::optional{value.world_xz_bounds};
                   })
      .def("copy",
           [](const LevelMinimapRef& self) -> std::optional<LevelMinimapValue> {
             const auto value = self.owner()->minimap();
             if (value.encoded_image.size == 0) return std::nullopt;
             LevelMinimapValue result;
             result.encoded_image.assign(value.encoded_image.data, value.encoded_image.data + value.encoded_image.size);
             result.world_xz_bounds = value.world_xz_bounds;
             return result;
           })
      .def(
          "set",
          [](const LevelMinimapRef& self, nb::handle data, ArxRect world_xz_bounds) {
            const auto bytes = byteSpan(data);
            unwrap(self.owner()->setMinimap({bytes.data(), bytes.size()}, world_xz_bounds));
          },
          nb::arg("data"),
          nb::arg("world_xz_bounds"))
      .def(
          "set_from_projection",
          [](const LevelMinimapRef& self, nb::handle data, ArxVector2 projection_offset) {
            const auto bytes = byteSpan(data);
            unwrap(self.owner()->setMinimapFromProjection({bytes.data(), bytes.size()}, projection_offset));
          },
          nb::arg("data"),
          nb::arg("projection_offset"))
      .def("clear", [](const LevelMinimapRef& self) { self.owner()->clearMinimap(); })
      .def("validate", [](const LevelMinimapRef& self) { unwrap(self.owner()->validateMinimap()); })
      .def(
          "render",
          [](const LevelMinimapRef& self,
             level_images::MinimapRenderMode mode,
             const std::optional<ArxVector2>& projection_offset,
             ImageFormat format,
             ArxColor3 fill_color,
             const std::optional<ArxColor3>& border_color) {
            auto result = [&] {
              nb::gil_scoped_release release;
              return self.owner()->renderMinimap({.mode = mode,
                                                  .projection_offset = projection_offset,
                                                  .fill_color = fill_color,
                                                  .border_color = border_color,
                                                  .format = format});
            }();
            auto output = unwrap(std::move(result));
            return RenderedMinimapValue{output.projection_offset, std::move(output.encoded_image)};
          },
          nb::kw_only(),
          nb::arg("mode") = level_images::MinimapRenderMode::kPlain,
          nb::arg("projection_offset") = nb::none(),
          nb::arg("format") = ImageFormat::kPng,
          nb::arg("fill_color") = ArxColor3{},
          nb::arg("border_color") = nb::none())
      .def(
          "generate",
          [](const LevelMinimapRef& self,
             const MinimapSamplerValue* foreground,
             const MinimapSamplerValue* background,
             const MinimapSamplerValue* water,
             const MinimapSamplerValue* lava,
             ArxColor3 halo_color,
             std::uint32_t halo_radius) {
            Level::MinimapGenerationOptions options;
            if (foreground) options.foreground = foreground->asValue();
            if (background) options.background = background->asValue();
            if (water) options.water = water->asValue();
            if (lava) options.lava = lava->asValue();
            options.halo_color = halo_color;
            options.halo_radius = halo_radius;
            auto result = [&] {
              nb::gil_scoped_release release;
              return self.owner()->generateMinimap(options);
            }();
            unwrap(std::move(result));
          },
          nb::kw_only(),
          nb::arg("foreground") = nullptr,
          nb::arg("background") = nullptr,
          nb::arg("water") = nullptr,
          nb::arg("lava") = nullptr,
          nb::arg("halo_color") = ArxColor3{1.0f, 1.0f, 1.0f},
          nb::arg("halo_radius") = 5U)
      .def("__repr__", [](const LevelMinimapRef& self) {
        return std::string("<pistoris.level.MinimapRef present=") +
               (self.owner()->minimap().encoded_image.size != 0 ? "True>" : "False>");
      });

  nb::class_<LevelLoadingScreenRef>(
      module, "LevelLoadingScreenRef", "The level loading screen and its rendering operations.")
      .def_prop_rw(
          "encoded_image",
          [](const LevelLoadingScreenRef& self) -> nb::object {
            const auto image = self.owner()->loadingScreen();
            if (image.size == 0) return nb::none();
            return toBytes({image.data, image.size});
          },
          [](const LevelLoadingScreenRef& self, nb::handle data) {
            if (data.is_none()) {
              self.owner()->clearLoadingScreen();
              return;
            }
            const auto bytes = byteSpan(data);
            unwrap(self.owner()->setLoadingScreen({bytes.data(), bytes.size()}));
          },
          nb::for_getter(nb::sig("def encoded_image(self) -> bytes | None")),
          nb::for_setter(nb::arg("value").none()),
          nb::for_setter(nb::sig("def encoded_image(self, value: object | None, /) -> None")))
      .def(
          "copy",
          [](const LevelLoadingScreenRef& self) -> nb::object {
            const auto image = self.owner()->loadingScreen();
            if (image.size == 0) return nb::none();
            return toBytes({image.data, image.size});
          },
          nb::sig("def copy(self) -> bytes | None"))
      .def("clear", [](const LevelLoadingScreenRef& self) { self.owner()->clearLoadingScreen(); })
      .def("validate", [](const LevelLoadingScreenRef& self) { unwrap(self.owner()->validateLoadingScreen()); })
      .def(
          "render",
          [](const LevelLoadingScreenRef& self, level_images::LoadingScreenLayout layout, ImageFormat format) {
            auto result = [&] {
              nb::gil_scoped_release release;
              return self.owner()->renderLoadingScreen({.layout = layout, .format = format});
            }();
            return toBytes(unwrap(std::move(result)));
          },
          nb::kw_only(),
          nb::arg("layout") = level_images::LoadingScreenLayout::kOriginal,
          nb::arg("format") = ImageFormat::kPng)
      .def("__repr__", [](const LevelLoadingScreenRef& self) {
        return std::string("<pistoris.level.LoadingScreenRef present=") +
               (self.owner()->loadingScreen().size != 0 ? "True>" : "False>");
      });
}

void bindLevelConversions(nb::class_<PythonLevel>& binding) {
  binding
      .def_static(
          "from_native",
          [](const Fts& fts, const Llf* llf, const Dlf* dlf, bool include_texture_sources, NativeTextMode mode) {
            LevelImportOutput output;
            auto result = [&] {
              nb::gil_scoped_release release;
              return Level::importNative(
                  fts, llf, dlf, include_texture_sources ? &output.texture_source_paths : nullptr, mode);
            }();
            output.level = tracked<Level, LevelTracking>(unwrap(std::move(result)));
            return output;
          },
          nb::arg("fts"),
          nb::arg("llf") = nullptr,
          nb::arg("dlf") = nullptr,
          nb::kw_only(),
          nb::arg("include_texture_sources") = true,
          nb::arg("text_mode") = NativeTextMode::kAuto)
      .def_static(
          "from_native_bytes",
          [](nb::handle fts_data,
             nb::handle llf_data,
             nb::handle dlf_data,
             bool include_texture_sources,
             NativeTextMode mode) {
            auto input = readLevelNativeInput(fts_data, llf_data, dlf_data);
            LevelImportOutput output;
            auto result = [&] {
              nb::gil_scoped_release release;
              return Level::importNative(input.fts,
                                         input.llf ? &*input.llf : nullptr,
                                         input.dlf ? &*input.dlf : nullptr,
                                         include_texture_sources ? &output.texture_source_paths : nullptr,
                                         mode);
            }();
            output.level = tracked<Level, LevelTracking>(unwrap(std::move(result)));
            return output;
          },
          nb::arg("fts"),
          nb::arg("llf") = nb::none(),
          nb::arg("dlf") = nb::none(),
          nb::kw_only(),
          nb::arg("include_texture_sources") = true,
          nb::arg("text_mode") = NativeTextMode::kAuto)
      .def_static(
          "from_glb",
          [](nb::handle data, bool include_texture_sources, float units, const std::optional<ArxVector3>& offset) {
            LevelImportOutput output;
            Level::GlbImportOptions options;
            options.arx_units_per_glb_unit = units;
            options.arx_offset = offset;
            const auto bytes = byteSpan(data);
            ArxLevelGlbImportInfo info{};
            auto result = [&] {
              nb::gil_scoped_release release;
              return Level::importGlb(
                  bytes, options, &info, include_texture_sources ? &output.texture_source_paths : nullptr);
            }();
            output.level = tracked<Level, LevelTracking>(unwrap(std::move(result)));
            output.glb_info = info;
            return output;
          },
          nb::arg("data"),
          nb::kw_only(),
          nb::arg("include_texture_sources") = true,
          nb::arg("arx_units_per_glb_unit") = 100.0f,
          nb::arg("arx_offset") = nb::none())
      .def(
          "to_native",
          [](const PythonLevel& self,
             const std::string& level_name,
             const std::string& scene_path,
             bool include_sidecars,
             NativeTextMode mode,
             bool reconstruct_quads) {
            Level::NativeBakeOptions options{.level_name = level_name,
                                             .include_texture_files = include_sidecars,
                                             .text_mode = mode,
                                             .reconstruct_quads = reconstruct_quads,
                                             .dlf_scene_path = scene_path};
            auto result = [&] {
              nb::gil_scoped_release release;
              return self.bakeNativeBundle(options);
            }();
            auto bundle = unwrap(std::move(result));
            LevelNativeOutput output;
            output.fts = std::move(bundle.fts);
            output.llf = std::move(bundle.llf);
            output.dlf = std::move(bundle.dlf);
            output.texture_files.reserve(bundle.texture_files.size());
            for (const auto& file : bundle.texture_files) output.texture_files.append(textureFile(self, file));
            return output;
          },
          nb::kw_only(),
          nb::arg("level_name") = "",
          nb::arg("dlf_scene_path") = "",
          nb::arg("include_sidecars") = true,
          nb::arg("text_mode") = NativeTextMode::kAuto,
          nb::arg("reconstruct_quads") = true)
      .def(
          "to_native_bytes",
          [](const PythonLevel& self,
             const std::string& level_name,
             const std::string& scene_path,
             bool include_sidecars,
             NativeTextMode mode,
             bool reconstruct_quads,
             bool compress,
             bool embed_lighting,
             const std::string& signer) {
            Level::NativeBakeOptions options{.level_name = level_name,
                                             .include_texture_files = include_sidecars,
                                             .text_mode = mode,
                                             .reconstruct_quads = reconstruct_quads,
                                             .dlf_scene_path = scene_path};
            auto baked = [&] {
              nb::gil_scoped_release release;
              return self.bakeNativeBundle(options);
            }();
            auto bundle = unwrap(std::move(baked));
            LevelBytesOutput output;
            DlfWriteOptions dlf_options{.embedded_lighting = embed_lighting ? &bundle.llf : nullptr, .signer = signer};
            auto fts = [&] {
              nb::gil_scoped_release release;
              return writeFts(bundle.fts, compress);
            }();
            output.fts = unwrap(std::move(fts));
            auto llf = [&] {
              nb::gil_scoped_release release;
              return writeLlf(bundle.llf, LlfWriteOptions{.signer = signer}, compress);
            }();
            output.llf = unwrap(std::move(llf));
            auto dlf = [&] {
              nb::gil_scoped_release release;
              return writeDlf(bundle.dlf, dlf_options, compress);
            }();
            output.dlf = unwrap(std::move(dlf));
            output.texture_files.reserve(bundle.texture_files.size());
            for (const auto& file : bundle.texture_files) output.texture_files.append(textureFile(self, file));
            return output;
          },
          nb::kw_only(),
          nb::arg("level_name") = "",
          nb::arg("dlf_scene_path") = "",
          nb::arg("include_sidecars") = true,
          nb::arg("text_mode") = NativeTextMode::kAuto,
          nb::arg("reconstruct_quads") = true,
          nb::arg("compress") = true,
          nb::arg("embed_lighting") = false,
          nb::arg("signer") = "")
      .def(
          "to_dlf",
          [](const PythonLevel& self,
             const std::string& level_name,
             ArxVector3 target_fts_offset,
             const std::string& scene_path,
             NativeTextMode mode) {
            auto result = [&] {
              nb::gil_scoped_release release;
              return self.bakeDlf({.level_name = level_name,
                                   .target_fts_offset = target_fts_offset,
                                   .dlf_scene_path = scene_path,
                                   .text_mode = mode});
            }();
            return unwrap(std::move(result));
          },
          nb::kw_only(),
          nb::arg("level_name") = "",
          nb::arg("target_fts_offset") = ArxVector3{},
          nb::arg("dlf_scene_path") = "",
          nb::arg("text_mode") = NativeTextMode::kAuto)
      .def(
          "to_glb",
          [](const PythonLevel& self, const nb::sequence& model_previews, float units, ArxVector3 offset) {
            Level::GlbExportOptions options;
            options.arx_units_per_glb_unit = units;
            options.arx_offset = offset;
            const auto previews = borrowResourcePointers<Model, PythonModel>(model_previews);
            LevelGlbOutput output;
            auto result = [&] {
              nb::gil_scoped_release release;
              return self.exportGlb(previews.values, options, &output.model_preview_report);
            }();
            output.glb = unwrap(std::move(result));
            return output;
          },
          nb::arg("model_previews") = nb::tuple(),
          nb::kw_only(),
          nb::arg("arx_units_per_glb_unit") = 100.0f,
          nb::arg("arx_offset") = ArxVector3{},
          nb::sig("def to_glb(self, model_previews: Sequence[Model] = (), *, arx_units_per_glb_unit: float = 100.0, "
                  "arx_offset: Vector3 = ...) -> LevelGlbOutput"));
}

void bindLevelInspection(nb::class_<PythonLevel>& binding) {
  binding
      .def_prop_rw(
          "resource_path",
          [](const PythonLevel& self) { return std::string(self.resourcePath()); },
          [](PythonLevel& self, const std::string& value) { unwrap(self.setResourcePath(value)); })
      .def("__repr__",
           [](const PythonLevel& self) {
             return resourceRepr(
                 "Level", self.resourcePath(), {{"faces", self.faceCount()}, {"rooms", self.roomCount()}});
           })
      .def_prop_ro("bounds", &Level::bounds)
      .def_prop_ro("referenced_bounds", &Level::referencedBounds)
      .def_prop_ro("vertices",
                   [](PythonLevel& self) { return ElementCollection<LevelVertexAccess>(self.shared_from_this()); })
      .def_prop_ro("faces",
                   [](PythonLevel& self) { return ElementCollection<LevelFaceAccess>(self.shared_from_this()); })
      .def_prop_ro("textures",
                   [](PythonLevel& self) { return ElementCollection<LevelTextureAccess>(self.shared_from_this()); })
      .def("validate_geometry",
           [](PythonLevel& self) {
             auto result = [&] {
               nb::gil_scoped_release release;
               return self.validateGeometry();
             }();
             unwrap(std::move(result));
           })
      .def("validate_face_rooms",
           [](PythonLevel& self) {
             auto result = [&] {
               nb::gil_scoped_release release;
               return self.validateFaceRooms();
             }();
             unwrap(std::move(result));
           })
      .def("validate_corner_colors",
           [](PythonLevel& self) {
             auto result = [&] {
               nb::gil_scoped_release release;
               return self.validateCornerColors();
             }();
             unwrap(std::move(result));
           })
      .def(
          "generate_static_lighting",
          [](PythonLevel& self, ArxColor3 ambient, float factor, bool normals, bool shadows) {
            auto result = [&] {
              nb::gil_scoped_release release;
              return self.generateStaticLighting(
                  {.ambient_color = ambient, .global_factor = factor, .use_normals = normals, .use_shadows = shadows});
            }();
            unwrap(std::move(result));
          },
          nb::kw_only(),
          nb::arg("ambient_color") = kDefaultStaticLightingAmbientColor,
          nb::arg("global_factor").sig("0.85") = kDefaultStaticLightingGlobalFactor,
          nb::arg("use_normals") = true,
          nb::arg("use_shadows") = true)
      .def(
          "weld_vertices",
          [](PythonLevel& self, float radius, Level::PositionWeldMetric metric, Level::DegenerateFacePolicy policy) {
            auto result = [&] {
              nb::gil_scoped_release release;
              return self.weldVertices({.radius = radius, .metric = metric, .degenerate_faces = policy});
            }();
            unwrap(std::move(result));
            self.tracking.vertices.invalidate();
            self.tracking.faces.invalidate();
            invalidateLevelNavigation(self);
          },
          nb::kw_only(),
          nb::arg("radius").sig("0.0001") = 1.0e-4f,
          nb::arg("metric") = Level::PositionWeldMetric::kEuclidean,
          nb::arg("degenerate_faces") = Level::DegenerateFacePolicy::kPreserve)
      .def(
          "snap_to_portals",
          [](PythonLevel& self, float radius) {
            auto result = [&] {
              nb::gil_scoped_release release;
              return self.snapGeometryToPortals({.radius = radius});
            }();
            unwrap(std::move(result));
          },
          nb::kw_only(),
          nb::arg("radius") = kDefaultPortalSnapRadius)
      .def("reset_corner_colors", [](PythonLevel& self) { self.resetCornerColors(); })
      .def_prop_ro("rooms",
                   [](PythonLevel& self) { return ElementCollection<LevelRoomAccess>(self.shared_from_this()); })
      .def_prop_ro("portals",
                   [](PythonLevel& self) { return ElementCollection<LevelPortalAccess>(self.shared_from_this()); })
      .def_prop_ro("room_distances",
                   [](PythonLevel& self) { return LevelRoomDistanceCollection(self.shared_from_this()); })
      .def_prop_ro("anchors",
                   [](PythonLevel& self) { return ElementCollection<LevelAnchorAccess>(self.shared_from_this()); })
      .def_prop_ro(
          "anchor_connections",
          [](PythonLevel& self) { return ElementCollection<LevelAnchorConnectionAccess>(self.shared_from_this()); })
      .def_prop_ro("nav_surface", [](PythonLevel& self) { return LevelNavSurfaceView(self.shared_from_this()); })
      .def_prop_ro("lights",
                   [](PythonLevel& self) { return ElementCollection<LevelLightAccess>(self.shared_from_this()); })
      .def_prop_rw(
          "player_spawn",
          [](PythonLevel& self) -> nb::object {
            if (self.playerSpawn().is_usable == 0) return nb::none();
            auto token = self.tracking.player_spawn.trackCanonical(0);
            return nb::cast(LevelPlayerSpawnRef(self.shared_from_this(), std::move(token)));
          },
          [](PythonLevel& self, const std::optional<ArxLevelPlayerSpawn>& spawn) {
            if (!spawn) {
              self.clearPlayerSpawn();
              self.tracking.player_spawn.invalidate();
              return;
            }
            auto value = *spawn;
            value.is_usable = 1;
            unwrap(self.setPlayerSpawn(value));
          },
          nb::for_getter(nb::sig("def player_spawn(self) -> LevelPlayerSpawnRef | None")),
          nb::for_setter(nb::arg("value").none()),
          nb::for_setter(nb::sig("def player_spawn(self, value: LevelPlayerSpawn | None, /) -> None")))
      .def_prop_ro("entities",
                   [](PythonLevel& self) { return ElementCollection<LevelEntityAccess>(self.shared_from_this()); })
      .def_prop_ro("fogs", [](PythonLevel& self) { return ElementCollection<LevelFogAccess>(self.shared_from_this()); })
      .def_prop_ro("zones",
                   [](PythonLevel& self) { return ElementCollection<LevelZoneAccess>(self.shared_from_this()); })
      .def_prop_ro("paths",
                   [](PythonLevel& self) { return ElementCollection<LevelPathAccess>(self.shared_from_this()); })
      .def_prop_ro("minimap", [](PythonLevel& self) { return LevelMinimapRef(self.shared_from_this()); })
      .def_prop_ro("loading_screen", [](PythonLevel& self) { return LevelLoadingScreenRef(self.shared_from_this()); });
}

}  // namespace

std::size_t LevelFaceAccess::size(const Owner& owner, std::size_t) { return owner.faceCount(); }

CollectionTracker& LevelFaceAccess::tracker(Owner& owner, std::size_t) { return owner.tracking.faces; }

LevelFaceAccess::Value LevelFaceAccess::get(const Owner& owner, std::size_t, std::size_t index) {
  const ArxLevelFace source = owner.faces()[index];
  Value result;
  result.normal = source.normal;
  result.texture = levelTexturePath(owner, source.texture);
  result.room = levelRoomName(owner, source.room);
  result.flags = source.flags;
  result.transval = source.transval;
  for (std::size_t corner = 0; corner < result.corners.size(); ++corner) {
    result.corners[corner] = {owner.vertices()[source.corners[corner].vertex],
                              source.corners[corner].normal,
                              source.corners[corner].u,
                              source.corners[corner].v,
                              source.corners[corner].color};
  }
  return result;
}

void LevelFaceAccess::set(Owner& owner, std::size_t, std::size_t index, const Value& value) {
  Level updated(owner);
  const ArxLevelFace current = updated.faces()[index];
  unwrap(updated.setFace(static_cast<FaceIndex>(index), levelFaceValue(updated, value, current)));
  commitLevel(owner, std::move(updated));
}

void LevelFaceAccess::append(Owner& owner, std::size_t, const Value& value) {
  Level updated(owner);
  (void)unwrap(updated.addFace(levelFaceValue(updated, value)));
  commitLevel(owner, std::move(updated));
}

void LevelFaceAccess::remove(Owner& owner, std::size_t, std::size_t index) {
  unwrap(owner.removeFace(static_cast<FaceIndex>(index)));
  owner.tracking.faces.remove(index);
}

void LevelFaceAccess::validate(const Owner& owner, std::size_t) { unwrap(owner.validateFaces()); }

void bindLevelBulkCopies(nb::module_& module) {
  module.attr("LevelVertexCollection").attr("copy") = nb::cpp_function(
      [](const ElementCollection<LevelVertexAccess>& self, const nb::object& positions_source) {
        requireCopyDestinations({positions_source});
        const std::size_t count = self.owner()->vertexCount();
        ScalarBufferOutput<float> positions(
            positions_source, checkedBufferProduct(count, 3U), {static_cast<Py_ssize_t>(count), 3});
        unwrap(self.owner()->copyVertexPositions(positions.requiredSpan()));
      },
      nb::is_method(),
      nb::kw_only(),
      nb::arg("positions") = nb::none(),
      nb::sig("def copy(self, *, positions: object | None = None) -> None"));
  module.attr("LevelFaceCollection").attr("copy") = nb::cpp_function(
      [](const ElementCollection<LevelFaceAccess>& self,
         const nb::object& vertex_indices_source,
         const nb::object& uvs_source,
         const nb::object& corner_normals_source,
         const nb::object& textures_source,
         const nb::object& transvals_source,
         const nb::object& corner_colors_source,
         const nb::object& face_normals_source,
         const nb::object& flags_source) {
        requireCopyDestinations({vertex_indices_source,
                                 uvs_source,
                                 corner_normals_source,
                                 textures_source,
                                 transvals_source,
                                 corner_colors_source,
                                 face_normals_source,
                                 flags_source});
        const std::size_t count = self.owner()->faceCount();
        ScalarBufferOutput<std::uint32_t> vertex_indices(
            vertex_indices_source, checkedBufferProduct(count, 3U), {static_cast<Py_ssize_t>(count), 3});
        ScalarBufferOutput<float> uvs(
            uvs_source, checkedBufferProduct(count, 6U), {static_cast<Py_ssize_t>(count), 3, 2});
        ScalarBufferOutput<float> corner_normals(
            corner_normals_source, checkedBufferProduct(count, 9U), {static_cast<Py_ssize_t>(count), 3, 3});
        ScalarBufferOutput<TextureIndex> textures(textures_source, count, {static_cast<Py_ssize_t>(count)});
        ScalarBufferOutput<float> transvals(transvals_source, count, {static_cast<Py_ssize_t>(count)});
        ScalarBufferOutput<float> corner_colors(
            corner_colors_source, checkedBufferProduct(count, 9U), {static_cast<Py_ssize_t>(count), 3, 3});
        ScalarBufferOutput<float> face_normals(
            face_normals_source, checkedBufferProduct(count, 3U), {static_cast<Py_ssize_t>(count), 3});
        ScalarBufferOutput<FaceType> flags(flags_source, count, {static_cast<Py_ssize_t>(count)});
        unwrap(self.owner()->copyFaces({.vertex_indices = vertex_indices.span(),
                                        .uvs = uvs.span(),
                                        .corner_normals = corner_normals.span(),
                                        .textures = textures.span(),
                                        .transvals = transvals.span(),
                                        .corner_colors = corner_colors.span(),
                                        .face_normals = face_normals.span(),
                                        .flags = flags.span()}));
      },
      nb::is_method(),
      nb::kw_only(),
      nb::arg("vertex_indices") = nb::none(),
      nb::arg("uvs") = nb::none(),
      nb::arg("corner_normals") = nb::none(),
      nb::arg("textures") = nb::none(),
      nb::arg("transvals") = nb::none(),
      nb::arg("corner_colors") = nb::none(),
      nb::arg("face_normals") = nb::none(),
      nb::arg("flags") = nb::none(),
      nb::sig("def copy(self, *, vertex_indices: object | None = None, uvs: object | None = None, corner_normals: "
              "object | None = None, textures: object | None = None, transvals: object | None = None, corner_colors: "
              "object | None = None, face_normals: object | None = None, flags: object | None = None) -> None"));
  module.attr("LevelFaceCollection").attr("copy_textures") = nb::cpp_function(
      [](const ElementCollection<LevelFaceAccess>& self, const nb::object& textures_source) {
        requireCopyDestinations({textures_source});
        const std::size_t count = self.owner()->faceCount();
        ScalarBufferOutput<TextureIndex> textures(textures_source, count, {static_cast<Py_ssize_t>(count)});
        unwrap(self.owner()->copyFaceTextures(textures.requiredSpan()));
      },
      nb::is_method(),
      nb::kw_only(),
      nb::arg("textures") = nb::none(),
      nb::sig("def copy_textures(self, *, textures: object | None = None) -> None"));
  module.attr("LevelFaceCollection").attr("copy_rooms") = nb::cpp_function(
      [](const ElementCollection<LevelFaceAccess>& self, const nb::object& rooms_source) {
        requireCopyDestinations({rooms_source});
        const std::size_t count = self.owner()->faceCount();
        ScalarBufferOutput<RoomIndex> rooms(rooms_source, count, {static_cast<Py_ssize_t>(count)});
        unwrap(self.owner()->copyFaceRooms(rooms.requiredSpan()));
      },
      nb::is_method(),
      nb::kw_only(),
      nb::arg("rooms") = nb::none(),
      nb::sig("def copy_rooms(self, *, rooms: object | None = None) -> None"));
  module.attr("LevelAnchorCollection").attr("copy") = nb::cpp_function(
      [](const ElementCollection<LevelAnchorAccess>& self,
         const nb::object& positions_source,
         const nb::object& radii_source,
         const nb::object& heights_source,
         const nb::object& flags_source) {
        requireCopyDestinations({positions_source, radii_source, heights_source, flags_source});
        const std::size_t count = self.owner()->anchorCount();
        ScalarBufferOutput<float> positions(
            positions_source, checkedBufferProduct(count, 3U), {static_cast<Py_ssize_t>(count), 3});
        ScalarBufferOutput<float> radii(radii_source, count, {static_cast<Py_ssize_t>(count)});
        ScalarBufferOutput<float> heights(heights_source, count, {static_cast<Py_ssize_t>(count)});
        ScalarBufferOutput<std::uint32_t> flags(flags_source, count, {static_cast<Py_ssize_t>(count)});
        unwrap(self.owner()->copyAnchors(
            {.positions = positions.span(), .radii = radii.span(), .heights = heights.span(), .flags = flags.span()}));
      },
      nb::is_method(),
      nb::kw_only(),
      nb::arg("positions") = nb::none(),
      nb::arg("radii") = nb::none(),
      nb::arg("heights") = nb::none(),
      nb::arg("flags") = nb::none(),
      nb::sig("def copy(self, *, positions: object | None = None, radii: object | None = None, heights: object | None "
              "= None, flags: object | None = None) -> None"));
  module.attr("LevelAnchorConnectionCollection").attr("copy") = nb::cpp_function(
      [](const ElementCollection<LevelAnchorConnectionAccess>& self, const nb::object& endpoints_source) {
        requireCopyDestinations({endpoints_source});
        const std::size_t count = self.owner()->anchorConnectionCount();
        ScalarBufferOutput<AnchorIndex> endpoints(
            endpoints_source, checkedBufferProduct(count, 2U), {static_cast<Py_ssize_t>(count), 2});
        unwrap(self.owner()->copyAnchorConnections(endpoints.requiredSpan()));
      },
      nb::is_method(),
      nb::kw_only(),
      nb::arg("endpoints") = nb::none(),
      nb::sig("def copy(self, *, endpoints: object | None = None) -> None"));
  module.attr("LevelNavSurface").attr("copy") = nb::cpp_function(
      [](const LevelNavSurfaceView& self,
         const nb::object& positions_source,
         const nb::object& triangle_indices_source) {
        requireCopyDestinations({positions_source, triangle_indices_source});
        const std::size_t vertices = self.owner()->navSurfaceVertices().size();
        const std::size_t triangles = self.owner()->navSurfaceTriangles().size();
        ScalarBufferOutput<float> positions(
            positions_source, checkedBufferProduct(vertices, 3U), {static_cast<Py_ssize_t>(vertices), 3});
        ScalarBufferOutput<NavSurfaceVertexIndex> triangle_indices(
            triangle_indices_source, checkedBufferProduct(triangles, 3U), {static_cast<Py_ssize_t>(triangles), 3});
        unwrap(
            self.owner()->copyNavSurface({.positions = positions.span(), .triangle_indices = triangle_indices.span()}));
      },
      nb::is_method(),
      nb::kw_only(),
      nb::arg("positions") = nb::none(),
      nb::arg("triangle_indices") = nb::none(),
      nb::sig("def copy(self, *, positions: object | None = None, triangle_indices: object | None = None) -> None"));
}

void bindLevel(nb::module_& module) {
  bindOutputs(module);
  nb::enum_<level_images::MinimapRenderMode>(module, "MinimapRenderMode")
      .value("PLAIN", level_images::MinimapRenderMode::kPlain)
      .value("GAME", level_images::MinimapRenderMode::kGame);
  nb::enum_<level_images::LoadingScreenLayout>(module, "LoadingScreenLayout")
      .value("ORIGINAL", level_images::LoadingScreenLayout::kOriginal)
      .value("NORMAL", level_images::LoadingScreenLayout::kNormal)
      .value("FULLSCREEN", level_images::LoadingScreenLayout::kFullscreen);
  nb::enum_<Level::PositionWeldMetric>(module, "PositionWeldMetric")
      .value("EUCLIDEAN", Level::PositionWeldMetric::kEuclidean)
      .value("AXIS_ALIGNED", Level::PositionWeldMetric::kAxisAligned);
  nb::enum_<Level::DegenerateFacePolicy>(module, "DegenerateFacePolicy")
      .value("PRESERVE", Level::DegenerateFacePolicy::kPreserve)
      .value("REJECT", Level::DegenerateFacePolicy::kReject)
      .value("DISCARD", Level::DegenerateFacePolicy::kDiscard);
  module.def("level_projection_offset_from_mini_offset", [](ArxVector2 mini_offset) {
    ArxVector2 result{};
    checkStatus(level_images::projectionOffsetFromMiniOffset(mini_offset, result));
    return result;
  });
  module.def("level_mini_offset_from_projection_offset", [](ArxVector2 projection_offset) {
    ArxVector2 result{};
    checkStatus(level_images::miniOffsetFromProjectionOffset(projection_offset, result));
    return result;
  });
  module.def("level_projection_offset_for_level", [](std::uint32_t level, ArxVector2 stored_mini_offset) {
    ArxVector2 result{};
    checkStatus(level_images::projectionOffsetForLevel(level, stored_mini_offset, result));
    return result;
  });
  bindLevelReferences(module);
  bindLevelBulkCopies(module);
  auto binding =
      nb::class_<PythonLevel>(
          module,
          "Level",
          "An editable level resource with live geometry and gameplay views. Byte factories parse FTS/LLF/DLF "
          "input, include_texture_sources controls returned references, and conversion outputs keep sidecars explicit.")
          .def(nb::new_([] { return std::make_shared<PythonLevel>(); }))
          .def("copy",
               [](const PythonLevel& self) {
                 return tracked<Level, LevelTracking>(Level(static_cast<const Level&>(self)));
               })
          .def("reset",
               [](PythonLevel& self) {
                 unwrap(self.reset());
                 self.tracking.invalidate();
               })
          .def("validate", [](const PythonLevel& self) {
            auto result = [&] {
              nb::gil_scoped_release release;
              return self.validate();
            }();
            unwrap(std::move(result));
          });
  bindLevelConversions(binding);
  bindLevelInspection(binding);
}

}  // namespace pistoris::python
