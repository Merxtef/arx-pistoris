// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#include "room_distance_metadata.h"

#include "arx_pistoris/base/indices.h"
#include "arx_pistoris/base/math.h"
#include "arx_pistoris/base/math.hpp"
#include "arx_pistoris/runtime/types.h"

#include "cgltf/cgltf.h"
#include "coordinates.h"
#include "external/glb/node_graph.h"
#include "modules/rooms.h"
#include "nlohmann/json.hpp"
#include "utils/log.h"
#include "utils/math/mat4.h"
#include "utils/math/quat.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace pistoris::glb_level {
namespace {

constexpr std::string_view kRootKey = "arx_pistoris_room_distances";
constexpr std::string_view kRoomKey = "arx_pistoris_room";
constexpr std::string_view kPortalKey = "arx_pistoris_portal";
constexpr std::uint64_t kVersion = 1;
constexpr float kTopologyTolerance = 1.0f;
constexpr double kTransformTolerance = 1.0e-3;

struct EncodedPortal {
  RoomIndex room_1 = 0;
  RoomIndex room_2 = 0;
  PortalShape shape = PortalShape::kQuad;
  std::array<ArxVector3, 4> vertices{};
};

struct EncodedMetadata {
  std::size_t room_count = 0;
  std::vector<EncodedPortal> portals;
  RoomDistances distances;
};

struct MetadataScan {
  bool found = false;
  std::size_t room_records = 0;
  std::size_t portal_records = 0;
  std::vector<std::size_t> roots;
};

struct IdMap {
  std::vector<std::size_t> id_by_imported;
  std::vector<std::size_t> imported_by_id;
};

const nlohmann::json* member(const nlohmann::json& object, std::string_view name) {
  if (!object.is_object()) return nullptr;
  const auto found = object.find(name);
  return found == object.end() ? nullptr : &*found;
}

nlohmann::json parseExtras(const cgltf_extras& extras) {
  if (extras.data == nullptr) return nlohmann::json::value_t::discarded;
  return nlohmann::json::parse(extras.data, nullptr, false);
}

bool readUnsigned(const nlohmann::json& value, std::uint64_t& out) {
  if (value.is_number_unsigned()) {
    out = value.get<std::uint64_t>();
    return true;
  }
  if (!value.is_number_integer()) return false;
  const std::int64_t signed_value = value.get<std::int64_t>();
  if (signed_value < 0) return false;
  out = static_cast<std::uint64_t>(signed_value);
  return true;
}

bool readSize(const nlohmann::json& value, std::size_t& out) {
  std::uint64_t parsed = 0;
  if (!readUnsigned(value, parsed) || parsed > std::numeric_limits<std::size_t>::max()) return false;
  out = static_cast<std::size_t>(parsed);
  return true;
}

bool readFloat(const nlohmann::json& value, float& out) {
  if (!value.is_number()) return false;
  const double parsed = value.get<double>();
  if (!std::isfinite(parsed) || std::abs(parsed) > std::numeric_limits<float>::max()) return false;
  out = static_cast<float>(parsed);
  return true;
}

bool readVector(const nlohmann::json& value, ArxVector3& out) {
  return value.is_array() && value.size() == 3U && readFloat(value[0], out.x) && readFloat(value[1], out.y) &&
         readFloat(value[2], out.z);
}

bool readRoomIndex(const nlohmann::json& value, RoomIndex& out) {
  std::uint64_t parsed = 0;
  if (!readUnsigned(value, parsed) || parsed >= static_cast<std::uint64_t>(kInvalidRoomIndex)) return false;
  out = static_cast<RoomIndex>(parsed);
  return true;
}

bool readPortalIndex(const nlohmann::json& value, PortalIndex& out) {
  if (value.is_null()) {
    out = kInvalidPortalIndex;
    return true;
  }
  std::uint64_t parsed = 0;
  if (!readUnsigned(value, parsed) || parsed >= static_cast<std::uint64_t>(kInvalidPortalIndex)) return false;
  out = static_cast<PortalIndex>(parsed);
  return true;
}

bool readNodeId(const cgltf_extras& extras, std::string_view key, std::size_t& out) {
  const nlohmann::json root = parseExtras(extras);
  const nlohmann::json* record = member(root, key);
  const nlohmann::json* id = record != nullptr ? member(*record, "id") : nullptr;
  return id != nullptr && readSize(*id, out);
}

MetadataScan scanMetadata(const cgltf_data& data, const glb::NodeGraph& graph) {
  MetadataScan scan;
  for (std::size_t node_index : graph.preorder) {
    if (node_index >= data.nodes_count || !graph.reachable[node_index]) continue;
    const nlohmann::json extras = parseExtras(data.nodes[node_index].extras);
    if (!extras.is_object()) continue;
    if (extras.contains(kRootKey)) {
      scan.found = true;
      scan.roots.push_back(node_index);
    }
    if (extras.contains(kRoomKey)) {
      scan.found = true;
      ++scan.room_records;
    }
    if (extras.contains(kPortalKey)) {
      scan.found = true;
      ++scan.portal_records;
    }
  }
  return scan;
}

bool readPortal(const nlohmann::json& source, EncodedPortal& out) {
  const nlohmann::json* room_values = member(source, "rooms");
  const nlohmann::json* shape_value = member(source, "shape");
  const nlohmann::json* vertex_values = member(source, "vertices");
  if (room_values == nullptr || !room_values->is_array() || room_values->size() != 2U || shape_value == nullptr ||
      vertex_values == nullptr || !vertex_values->is_array())
    return false;
  if (!readRoomIndex((*room_values)[0], out.room_1) || !readRoomIndex((*room_values)[1], out.room_2)) return false;

  std::uint64_t shape = 0;
  if (!readUnsigned(*shape_value, shape) || (shape != 3U && shape != 4U) || vertex_values->size() != shape)
    return false;
  out.shape = shape == 3U ? PortalShape::kTriangle : PortalShape::kQuad;
  for (std::size_t vertex = 0; vertex < shape; ++vertex)
    if (!readVector((*vertex_values)[vertex], out.vertices[vertex])) return false;
  return true;
}

bool readDistance(const nlohmann::json& source, RoomDistance& out) {
  const nlohmann::json* distance = member(source, "distance");
  const nlohmann::json* portals = member(source, "portals");
  return distance != nullptr && portals != nullptr && portals->is_array() && portals->size() == 2U &&
         readFloat(*distance, out.distance) && readPortalIndex((*portals)[0], out.low_room_portal) &&
         readPortalIndex((*portals)[1], out.high_room_portal);
}

bool readMetadata(const cgltf_extras& extras, EncodedMetadata& out) {
  const nlohmann::json root = parseExtras(extras);
  const nlohmann::json* metadata = member(root, kRootKey);
  if (metadata == nullptr) return false;
  const nlohmann::json* version = member(*metadata, "version");
  const nlohmann::json* room_count = member(*metadata, "roomCount");
  const nlohmann::json* portal_count = member(*metadata, "portalCount");
  const nlohmann::json* portals = member(*metadata, "portals");
  const nlohmann::json* pairs = member(*metadata, "pairs");
  std::uint64_t parsed_version = 0;
  std::size_t parsed_portal_count = 0;
  if (version == nullptr || !readUnsigned(*version, parsed_version) || parsed_version != kVersion ||
      room_count == nullptr || !readSize(*room_count, out.room_count) || portal_count == nullptr ||
      !readSize(*portal_count, parsed_portal_count) || portals == nullptr || !portals->is_array() ||
      portals->size() != parsed_portal_count || pairs == nullptr || !pairs->is_array())
    return false;
  if (out.room_count > static_cast<std::size_t>(kInvalidRoomIndex) ||
      parsed_portal_count > static_cast<std::size_t>(kInvalidPortalIndex) ||
      pairs->size() != rooms::roomDistancePairCount(out.room_count))
    return false;

  out.portals.resize(portals->size());
  for (std::size_t index = 0; index < portals->size(); ++index)
    if (!readPortal((*portals)[index], out.portals[index])) return false;
  out.distances.resize(pairs->size());
  for (std::size_t index = 0; index < pairs->size(); ++index)
    if (!readDistance((*pairs)[index], out.distances[index])) return false;
  return true;
}

bool unitRigidTransform(const math::Mat4& world, const ImportUnits& units) {
  std::array<ArxVector3, 3> axes{};
  constexpr std::array<ArxVector3, 3> kBasis = {{{1.0f, 0.0f, 0.0f}, {0.0f, 1.0f, 0.0f}, {0.0f, 0.0f, 1.0f}}};
  for (std::size_t index = 0; index < axes.size(); ++index) {
    const std::optional<ArxVector3> converted = toArxVector(math::xformDir(world, kBasis[index]), units);
    if (!converted) return false;
    axes[index] = *converted;
  }
  auto dot = [](const ArxVector3& left, const ArxVector3& right) {
    return static_cast<double>(left.x) * right.x + static_cast<double>(left.y) * right.y +
           static_cast<double>(left.z) * right.z;
  };
  for (const ArxVector3& axis : axes)
    if (std::abs(dot(axis, axis) - 1.0) > kTransformTolerance) return false;
  return std::abs(dot(axes[0], axes[1])) <= kTransformTolerance &&
         std::abs(dot(axes[0], axes[2])) <= kTransformTolerance &&
         std::abs(dot(axes[1], axes[2])) <= kTransformTolerance;
}

ArxVector3 centroid(const EncodedPortal& portal) {
  ArxVector3 result{};
  const std::size_t count = rooms::portalVertexCount(portal.shape);
  for (std::size_t index = 0; index < count; ++index) result = result + portal.vertices[index];
  return result / static_cast<float>(count);
}

double distanceSquared(const ArxVector3& left, const ArxVector3& right) {
  const double x = static_cast<double>(left.x) - right.x;
  const double y = static_cast<double>(left.y) - right.y;
  const double z = static_cast<double>(left.z) - right.z;
  return x * x + y * y + z * z;
}

bool cyclicVerticesMatch(const EncodedPortal& expected, const Portal& actual, const ArxVector3& offset, bool mirrored) {
  const std::size_t count = rooms::portalVertexCount(expected.shape);
  const double tolerance_squared = static_cast<double>(kTopologyTolerance) * kTopologyTolerance;
  for (std::size_t shift = 0; shift < count; ++shift) {
    bool matches = true;
    for (std::size_t index = 0; index < count; ++index) {
      const std::size_t expected_index = mirrored ? (count - index) % count : index;
      if (distanceSquared(expected.vertices[expected_index] + offset, actual.vertices[(index + shift) % count]) >
          tolerance_squared) {
        matches = false;
        break;
      }
    }
    if (matches) return true;
  }
  return false;
}

bool transformPortals(std::vector<EncodedPortal>& portals, const math::Mat4& world, const ImportUnits& units) {
  for (EncodedPortal& portal : portals) {
    const std::size_t count = rooms::portalVertexCount(portal.shape);
    for (std::size_t vertex = 0; vertex < count; ++vertex) {
      const std::optional<ArxVector3> transformed = toArxPoint(math::xformPoint(world, portal.vertices[vertex]), units);
      if (!transformed) return false;
      portal.vertices[vertex] = *transformed;
    }
  }
  return true;
}

bool mapIds(const cgltf_data& data, std::span<const std::size_t> nodes, std::string_view key, std::size_t count,
            IdMap& out) {
  if (nodes.size() != count) return false;
  IdMap mapping;
  mapping.id_by_imported.resize(count);
  mapping.imported_by_id.assign(count, std::numeric_limits<std::size_t>::max());
  for (std::size_t imported = 0; imported < nodes.size(); ++imported) {
    const std::size_t node = nodes[imported];
    std::size_t id = 0;
    if (node >= data.nodes_count || !readNodeId(data.nodes[node].extras, key, id) || id >= count ||
        mapping.imported_by_id[id] != std::numeric_limits<std::size_t>::max())
      return false;
    mapping.id_by_imported[imported] = id;
    mapping.imported_by_id[id] = imported;
  }
  out = std::move(mapping);
  return true;
}

bool validateTopology(const EncodedMetadata& metadata, const std::vector<std::size_t>& room_id_by_imported,
                      const std::vector<std::size_t>& portal_by_id, const RoomsData& rooms, bool mirrored) {
  if (metadata.portals.empty()) return true;
  const std::size_t first_portal = portal_by_id.front();
  const ArxVector3 offset = rooms::portalCentroid(rooms.portals[first_portal]) - centroid(metadata.portals.front());
  for (std::size_t id = 0; id < metadata.portals.size(); ++id) {
    const EncodedPortal& expected = metadata.portals[id];
    const Portal& actual = rooms.portals[portal_by_id[id]];
    if (actual.shape != expected.shape || room_id_by_imported[actual.room_1] != expected.room_1 ||
        room_id_by_imported[actual.room_2] != expected.room_2 ||
        distanceSquared(rooms::portalCentroid(actual), centroid(expected) + offset) >
            static_cast<double>(kTopologyTolerance) * kTopologyTolerance ||
        !cyclicVerticesMatch(expected, actual, offset, mirrored))
      return false;
  }
  return true;
}

bool mapDistances(const EncodedMetadata& metadata, const std::vector<std::size_t>& room_id_by_imported,
                  const std::vector<std::size_t>& portal_by_id, const RoomsData& rooms, RoomDistances& out) {
  if (!rooms::hasCompleteRoomDistances(metadata.distances, metadata.room_count)) return false;
  RoomDistances mapped(rooms::roomDistancePairCount(rooms.definitions.size()));
  std::size_t target = 0;
  for (std::size_t high = 1; high < rooms.definitions.size(); ++high) {
    for (std::size_t low = 0; low < high; ++low, ++target) {
      std::size_t source_low = room_id_by_imported[low];
      std::size_t source_high = room_id_by_imported[high];
      bool reversed = false;
      if (source_high < source_low) {
        std::swap(source_low, source_high);
        reversed = true;
      }
      RoomDistance distance = metadata.distances[rooms::roomDistancePairIndex(source_low, source_high)];
      if (reversed) std::swap(distance.low_room_portal, distance.high_room_portal);
      auto map_portal = [&](PortalIndex& portal) {
        if (portal == kInvalidPortalIndex) return true;
        if (portal >= portal_by_id.size() || portal_by_id[portal] >= static_cast<std::size_t>(kInvalidPortalIndex))
          return false;
        portal = static_cast<PortalIndex>(portal_by_id[portal]);
        return true;
      };
      if (!map_portal(distance.low_room_portal) || !map_portal(distance.high_room_portal)) return false;
      mapped[target] = distance;
    }
  }
  if (rooms::validateRoomDistances(mapped, rooms) != rooms::Error::kNone) return false;
  out = std::move(mapped);
  return true;
}

void logDiscard(std::string_view reason) {
  log(ARX_LOG_WARN, "GLB -> Level: encoded room distances discarded: {}", reason);
  log(ARX_LOG_INFO, "GLB -> Level: encoded room distances discarded");
}

}  // namespace

std::string roomDistanceRoomMetadataJson(std::size_t id) { return nlohmann::json{{kRoomKey, {{"id", id}}}}.dump(); }

std::string roomDistancePortalMetadataJson(std::size_t id) { return nlohmann::json{{kPortalKey, {{"id", id}}}}.dump(); }

std::string roomDistanceRootMetadataJson(const RoomsData& rooms) {
  nlohmann::json portals = nlohmann::json::array();
  for (const Portal& portal : rooms.portals) {
    nlohmann::json vertices = nlohmann::json::array();
    const std::size_t count = rooms::portalVertexCount(portal.shape);
    for (std::size_t index = 0; index < count; ++index) {
      const ArxVector3 encoded = math::rotate(kLevelGlbBasisRotation, portal.vertices[index]);
      vertices.push_back({encoded.x, encoded.y, encoded.z});
    }
    portals.push_back({{"rooms", {portal.room_1, portal.room_2}}, {"shape", count}, {"vertices", std::move(vertices)}});
  }

  nlohmann::json pairs = nlohmann::json::array();
  for (const RoomDistance& distance : rooms.distances) {
    auto portal = [](PortalIndex index) {
      return index == kInvalidPortalIndex ? nlohmann::json(nullptr) : nlohmann::json(index);
    };
    pairs.push_back({{"distance", distance.distance},
                     {"portals", {portal(distance.low_room_portal), portal(distance.high_room_portal)}}});
  }
  return nlohmann::json{{kRootKey,
                         {{"version", kVersion},
                          {"roomCount", rooms.definitions.size()},
                          {"portalCount", rooms.portals.size()},
                          {"portals", std::move(portals)},
                          {"pairs", std::move(pairs)}}}}
      .dump();
}

void restoreRoomDistanceMetadata(const cgltf_data& data, const glb::NodeGraph& graph,
                                 std::span<const std::size_t> room_nodes, std::span<const std::size_t> portal_nodes,
                                 const ImportUnits& units, RoomsData& rooms) {
  const MetadataScan scan = scanMetadata(data, graph);
  if (!scan.found) return;
  log(ARX_LOG_INFO, "GLB -> Level: encoded room distances found");
  if (scan.roots.size() != 1U) {
    logDiscard(scan.roots.empty() ? "snapshot block is missing" : "multiple snapshot blocks were found");
    return;
  }
  if (scan.room_records != room_nodes.size() || scan.portal_records != portal_nodes.size()) {
    logDiscard("object identity records are incomplete or misplaced");
    return;
  }

  EncodedMetadata metadata;
  const std::size_t root = scan.roots.front();
  if (!readMetadata(data.nodes[root].extras, metadata)) {
    logDiscard("snapshot structure or version is unsupported");
    return;
  }
  if (metadata.room_count != rooms.definitions.size() || metadata.portals.size() != rooms.portals.size() ||
      metadata.distances.size() != rooms::roomDistancePairCount(metadata.room_count)) {
    logDiscard("room, portal, or pair counts no longer match");
    return;
  }

  IdMap room_ids;
  IdMap portal_ids;
  if (!mapIds(data, room_nodes, kRoomKey, metadata.room_count, room_ids) ||
      !mapIds(data, portal_nodes, kPortalKey, metadata.portals.size(), portal_ids)) {
    logDiscard("room or portal identities are missing, duplicated, or out of range");
    return;
  }
  if (!unitRigidTransform(graph.world[root], units) || !transformPortals(metadata.portals, graph.world[root], units)) {
    logDiscard("metadata transform scales or shears the stored topology");
    return;
  }

  const bool mirrored = math::linearDeterminant(graph.world[root]) < 0.0;
  if (!validateTopology(metadata, room_ids.id_by_imported, portal_ids.imported_by_id, rooms, mirrored)) {
    logDiscard("portal topology no longer matches the stored snapshot");
    return;
  }

  RoomDistances distances;
  if (!mapDistances(metadata, room_ids.id_by_imported, portal_ids.imported_by_id, rooms, distances)) {
    logDiscard("stored pair or endpoint references are invalid");
    return;
  }
  rooms.distances = std::move(distances);
  log(ARX_LOG_INFO, "GLB -> Level: encoded room distances applied");
}

}  // namespace pistoris::glb_level
