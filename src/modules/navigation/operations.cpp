// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#include "arx_pistoris/base/indices.h"

#include "modules/navigation.h"

#include <algorithm>
#include <cassert>
#include <cstddef>
#include <utility>
#include <vector>

namespace pistoris::navigation {
namespace {

bool connectionLess(const AnchorConnection& lhs, const AnchorConnection& rhs) noexcept {
  return lhs.first != rhs.first ? lhs.first < rhs.first : lhs.second < rhs.second;
}

bool sameConnection(const AnchorConnection& lhs, const AnchorConnection& rhs) noexcept {
  return lhs.first == rhs.first && lhs.second == rhs.second;
}

}  // namespace

void setAnchor(NavigationData& navigation, AnchorIndex index, Anchor anchor) noexcept {
  assert(static_cast<std::size_t>(index) < navigation.anchors.size());
  navigation.anchors[index] = std::move(anchor);
}

AnchorIndex addAnchor(NavigationData& navigation, Anchor anchor) {
  assert(navigation.anchors.size() < static_cast<std::size_t>(kInvalidAnchorIndex));
  const AnchorIndex index = static_cast<AnchorIndex>(navigation.anchors.size());
  navigation.anchors.push_back(std::move(anchor));
  return index;
}

void removeAnchor(NavigationData& navigation, AnchorIndex index) noexcept {
  assert(static_cast<std::size_t>(index) < navigation.anchors.size());
  navigation.anchors.erase(navigation.anchors.begin() + static_cast<std::ptrdiff_t>(index));
  std::erase_if(navigation.connections, [index](const AnchorConnection& connection) {
    return connection.first == index || connection.second == index;
  });
  for (AnchorConnection& connection : navigation.connections) {
    if (connection.first > index) --connection.first;
    if (connection.second > index) --connection.second;
  }
}

Error validateConnectionPlacement(const NavigationData& navigation, AnchorConnectionIndex index,
                                  const AnchorConnection& connection) noexcept {
  if (static_cast<std::size_t>(index) >= navigation.connections.size()) return Error::kBadIndex;
  const Error error = validateConnection(connection, navigation.anchors.size());
  if (error != Error::kNone) return error;
  if (index != 0) {
    const AnchorConnection& previous = navigation.connections[index - 1U];
    if (sameConnection(previous, connection)) return Error::kDuplicateConnection;
    if (!connectionLess(previous, connection)) return Error::kBadConnectionOrder;
  }
  if (static_cast<std::size_t>(index) + 1U < navigation.connections.size()) {
    const AnchorConnection& next = navigation.connections[index + 1U];
    if (sameConnection(connection, next)) return Error::kDuplicateConnection;
    if (!connectionLess(connection, next)) return Error::kBadConnectionOrder;
  }
  return Error::kNone;
}

Error validateConnectionInsertion(const NavigationData& navigation, const AnchorConnection& connection,
                                  AnchorConnectionIndex& out_index) noexcept {
  out_index = kInvalidAnchorConnectionIndex;
  if (navigation.connections.size() >= static_cast<std::size_t>(kInvalidAnchorConnectionIndex))
    return Error::kTooManyConnections;
  const Error error = validateConnection(connection, navigation.anchors.size());
  if (error != Error::kNone) return error;
  const auto found =
      std::lower_bound(navigation.connections.begin(), navigation.connections.end(), connection, connectionLess);
  if (found != navigation.connections.end() && sameConnection(*found, connection)) return Error::kDuplicateConnection;
  const std::size_t index = static_cast<std::size_t>(found - navigation.connections.begin());
  out_index = static_cast<AnchorConnectionIndex>(index);
  return Error::kNone;
}

void setConnection(NavigationData& navigation, AnchorConnectionIndex index, AnchorConnection connection) noexcept {
  assert(static_cast<std::size_t>(index) < navigation.connections.size());
  navigation.connections[index] = connection;
}

void insertConnection(NavigationData& navigation, AnchorConnectionIndex index, AnchorConnection connection) {
  assert(static_cast<std::size_t>(index) <= navigation.connections.size());
  navigation.connections.insert(navigation.connections.begin() + static_cast<std::ptrdiff_t>(index), connection);
}

void removeConnection(NavigationData& navigation, AnchorConnectionIndex index) noexcept {
  assert(static_cast<std::size_t>(index) < navigation.connections.size());
  navigation.connections.erase(navigation.connections.begin() + static_cast<std::ptrdiff_t>(index));
}

void replaceAnchors(NavigationData& navigation, std::vector<Anchor>&& anchors,
                    std::vector<AnchorConnection>&& connections) noexcept {
  navigation.anchors = std::move(anchors);
  navigation.connections = std::move(connections);
}

void replaceConnections(NavigationData& navigation, std::vector<AnchorConnection>&& connections) noexcept {
  navigation.connections = std::move(connections);
}

void clearAnchors(NavigationData& navigation) noexcept {
  navigation.anchors.clear();
  navigation.connections.clear();
}

void setSurface(NavigationData& navigation, NavSurface surface) noexcept { navigation.surface = std::move(surface); }

void clearSurface(NavigationData& navigation) noexcept { navigation.surface.reset(); }

}  // namespace pistoris::navigation
