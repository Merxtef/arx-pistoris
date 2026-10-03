// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#pragma once

#include "doctest/doctest.h"

#include "arx_pistoris/model.hpp"
#include "arx_pistoris/texture.h"

#include "support/equivalence.h"

#include <algorithm>
#include <cstddef>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace test_support {
namespace model_equivalence_detail {

inline std::string texturePathKey(ArxStringView value) {
  std::string key(equivalence::stringView(value));
  for (char& character : key) {
    if (character == '\\') character = '/';
    if (character >= 'A' && character <= 'Z') character = static_cast<char>(character - 'A' + 'a');
  }
  return key;
}

inline void checkNormalsEqual(const ArxVector3& lhs, const ArxVector3& rhs) {
  CHECK(lhs.x == doctest::Approx(rhs.x).epsilon(1e-5));
  CHECK(lhs.y == doctest::Approx(rhs.y).epsilon(1e-5));
  CHECK(lhs.z == doctest::Approx(rhs.z).epsilon(1e-5));
}

inline bool vertexEquivalent(const ArxModelVertex& lhs, const ArxModelVertex& rhs, float epsilon) {
  return lhs.bone == rhs.bone && equivalence::vectorEquivalent(lhs.position, rhs.position, epsilon);
}

inline bool faceCornersEquivalent(const ArxModelFace& lhs, const ArxModelFace& rhs,
                                  std::span<const ArxModelVertex> lhs_vertices,
                                  std::span<const ArxModelVertex> rhs_vertices, std::size_t rhs_offset, float epsilon,
                                  bool compare_normals) {
  for (std::size_t corner = 0; corner < 3; ++corner) {
    const ArxModelCorner& lhs_corner = lhs.corners[corner];
    const ArxModelCorner& rhs_corner = rhs.corners[(corner + rhs_offset) % 3];
    if (lhs_corner.vertex >= lhs_vertices.size() || rhs_corner.vertex >= rhs_vertices.size()) return false;
    if (!vertexEquivalent(lhs_vertices[lhs_corner.vertex], rhs_vertices[rhs_corner.vertex], epsilon)) return false;
    if (compare_normals && !equivalence::vectorEquivalent(lhs_corner.normal, rhs_corner.normal, epsilon)) return false;
    if (!equivalence::floatEquivalent(lhs_corner.u, rhs_corner.u, epsilon) ||
        !equivalence::floatEquivalent(lhs_corner.v, rhs_corner.v, epsilon))
      return false;
  }
  return true;
}

inline bool faceEquivalent(const ArxModelFace& lhs, const ArxModelFace& rhs,
                           std::span<const ArxModelVertex> lhs_vertices, std::span<const ArxModelVertex> rhs_vertices,
                           float epsilon, bool compare_normals) {
  if (lhs.texture != rhs.texture || lhs.flags != rhs.flags ||
      !equivalence::floatEquivalent(lhs.transval, rhs.transval, epsilon))
    return false;
  if (compare_normals && !equivalence::vectorEquivalent(lhs.normal, rhs.normal, epsilon)) return false;
  return faceCornersEquivalent(lhs, rhs, lhs_vertices, rhs_vertices, 0, epsilon, compare_normals) ||
         faceCornersEquivalent(lhs, rhs, lhs_vertices, rhs_vertices, 1, epsilon, compare_normals) ||
         faceCornersEquivalent(lhs, rhs, lhs_vertices, rhs_vertices, 2, epsilon, compare_normals);
}

inline ArxVector3 faceAnchor(const ArxModelFace& face, std::span<const ArxModelVertex> vertices) {
  ArxVector3 result = vertices[face.corners[0].vertex].position;
  for (std::size_t corner = 1; corner < 3; ++corner) {
    const ArxVector3 position = vertices[face.corners[corner].vertex].position;
    result.x = std::min(result.x, position.x);
    result.y = std::min(result.y, position.y);
    result.z = std::min(result.z, position.z);
  }
  return result;
}

}  // namespace model_equivalence_detail

struct ModelEquivalenceOptions {
  bool compare_external_texture_extensions = true;
  bool compare_encoded_images = true;
  bool compare_normals = true;
  bool compare_geometry_order = true;
  bool compare_vertex_multiplicity = true;
  float comparison_epsilon = 0.0f;
};

inline void checkModelsEquivalent(const pistoris::Model& lhs, const pistoris::Model& rhs,
                                  ModelEquivalenceOptions options = {}) {
  CHECK(lhs.resourcePath() == rhs.resourcePath());
  const bool vertex_counts_match = lhs.vertexCount() == rhs.vertexCount();
  const bool face_counts_match = lhs.faceCount() == rhs.faceCount();
  const bool texture_counts_match = lhs.textureCount() == rhs.textureCount();
  const bool bone_counts_match = lhs.boneCount() == rhs.boneCount();
  const bool action_counts_match = lhs.actionPointCount() == rhs.actionPointCount();
  const bool selection_counts_match = lhs.selectionCount() == rhs.selectionCount();
  if (options.compare_vertex_multiplicity) CHECK(lhs.vertexCount() == rhs.vertexCount());
  CHECK(lhs.faceCount() == rhs.faceCount());
  CHECK(lhs.textureCount() == rhs.textureCount());
  CHECK(lhs.boneCount() == rhs.boneCount());
  CHECK(lhs.actionPointCount() == rhs.actionPointCount());
  CHECK(lhs.selectionCount() == rhs.selectionCount());
  if ((options.compare_vertex_multiplicity && !vertex_counts_match) || !face_counts_match || !texture_counts_match ||
      !bone_counts_match || !action_counts_match || !selection_counts_match)
    return;

  const auto lhs_vertices_view = lhs.vertices();
  const auto rhs_vertices_view = rhs.vertices();
  const std::vector<ArxModelVertex> lhs_vertices(lhs_vertices_view.begin(), lhs_vertices_view.end());
  const std::vector<ArxModelVertex> rhs_vertices(rhs_vertices_view.begin(), rhs_vertices_view.end());
  const auto vertex_equivalent = [&](const ArxModelVertex& left, const ArxModelVertex& right) {
    return model_equivalence_detail::vertexEquivalent(left, right, options.comparison_epsilon);
  };
  if (options.compare_vertex_multiplicity) {
    CHECK(equivalence::spatialMultisetEquivalent<ArxModelVertex>(
        lhs_vertices,
        rhs_vertices,
        [](const ArxModelVertex& vertex) { return vertex.position; },
        vertex_equivalent,
        options.comparison_epsilon));
  } else {
    CHECK(equivalence::spatialSetEquivalent<ArxModelVertex>(
        lhs_vertices,
        rhs_vertices,
        [](const ArxModelVertex& vertex) { return vertex.position; },
        vertex_equivalent,
        options.comparison_epsilon));
  }

  const auto lhs_faces_view = lhs.faces();
  const auto rhs_faces_view = rhs.faces();
  const std::vector<ArxModelFace> lhs_faces(lhs_faces_view.begin(), lhs_faces_view.end());
  const std::vector<ArxModelFace> rhs_faces(rhs_faces_view.begin(), rhs_faces_view.end());
  if (options.compare_geometry_order) {
    for (std::size_t index = 0; index < lhs_faces.size(); ++index) {
      const ArxModelFace& lhs_face = lhs_faces[index];
      const ArxModelFace& rhs_face = rhs_faces[index];
      if (options.compare_normals) model_equivalence_detail::checkNormalsEqual(lhs_face.normal, rhs_face.normal);
      CHECK(lhs_face.texture == rhs_face.texture);
      CHECK(lhs_face.flags == rhs_face.flags);
      equivalence::checkFloatEqual(lhs_face.transval, rhs_face.transval, options.comparison_epsilon);
      for (std::size_t corner = 0; corner < 3; ++corner) {
        const bool lhs_vertex_valid = lhs_face.corners[corner].vertex < lhs_vertices.size();
        const bool rhs_vertex_valid = rhs_face.corners[corner].vertex < rhs_vertices.size();
        CHECK(lhs_vertex_valid);
        CHECK(rhs_vertex_valid);
        if (!lhs_vertex_valid || !rhs_vertex_valid) continue;
        CHECK(lhs_vertices[lhs_face.corners[corner].vertex].bone == rhs_vertices[rhs_face.corners[corner].vertex].bone);
        equivalence::checkVectorEqual(lhs_vertices[lhs_face.corners[corner].vertex].position,
                                      rhs_vertices[rhs_face.corners[corner].vertex].position,
                                      options.comparison_epsilon);
        if (options.compare_normals)
          model_equivalence_detail::checkNormalsEqual(lhs_face.corners[corner].normal, rhs_face.corners[corner].normal);
        equivalence::checkFloatEqual(
            lhs_face.corners[corner].u, rhs_face.corners[corner].u, options.comparison_epsilon);
        equivalence::checkFloatEqual(
            lhs_face.corners[corner].v, rhs_face.corners[corner].v, options.comparison_epsilon);
      }
    }
  } else {
    bool vertex_indices_valid = true;
    for (const ArxModelFace& face : lhs_faces)
      for (const ArxModelCorner& corner : face.corners) {
        const bool valid = corner.vertex < lhs_vertices.size();
        CHECK(valid);
        vertex_indices_valid = vertex_indices_valid && valid;
      }
    for (const ArxModelFace& face : rhs_faces)
      for (const ArxModelCorner& corner : face.corners) {
        const bool valid = corner.vertex < rhs_vertices.size();
        CHECK(valid);
        vertex_indices_valid = vertex_indices_valid && valid;
      }
    if (!vertex_indices_valid) return;
    const float coordinate_scale =
        std::max(equivalence::maxAbsCoordinate<ArxModelVertex>(
                     lhs_vertices, [](const ArxModelVertex& vertex) { return vertex.position; }),
                 equivalence::maxAbsCoordinate<ArxModelVertex>(
                     rhs_vertices, [](const ArxModelVertex& vertex) { return vertex.position; }));
    CHECK(equivalence::spatialMultisetEquivalent<ArxModelFace>(
        lhs_faces,
        rhs_faces,
        [&](const ArxModelFace& face) { return model_equivalence_detail::faceAnchor(face, lhs_vertices); },
        [&](const ArxModelFace& face) { return model_equivalence_detail::faceAnchor(face, rhs_vertices); },
        [&](const ArxModelFace& left, const ArxModelFace& right) {
          return model_equivalence_detail::faceEquivalent(
              left, right, lhs_vertices, rhs_vertices, options.comparison_epsilon, options.compare_normals);
        },
        options.comparison_epsilon,
        coordinate_scale));
  }

  const auto lhs_textures = lhs.textures();
  const auto rhs_textures = rhs.textures();
  for (std::size_t index = 0; index < lhs_textures.size(); ++index) {
    CHECK(model_equivalence_detail::texturePathKey(lhs_textures[index].path) ==
          model_equivalence_detail::texturePathKey(rhs_textures[index].path));
    if (options.compare_encoded_images)
      equivalence::checkImageViewsEqual(lhs_textures[index].encoded_image, rhs_textures[index].encoded_image);
    if (options.compare_external_texture_extensions)
      CHECK(equivalence::stringView(lhs_textures[index].external_image_extension) ==
            equivalence::stringView(rhs_textures[index].external_image_extension));
  }

  const auto lhs_bones = lhs.bones();
  const auto rhs_bones = rhs.bones();
  for (std::size_t index = 0; index < lhs_bones.size(); ++index) {
    CHECK(equivalence::stringView(lhs_bones[index].name) == equivalence::stringView(rhs_bones[index].name));
    equivalence::checkVectorEqual(lhs_bones[index].position, rhs_bones[index].position, options.comparison_epsilon);
    CHECK(lhs_bones[index].parent == rhs_bones[index].parent);
    equivalence::checkFloatEqual(
        lhs_bones[index].blob_shadow_size, rhs_bones[index].blob_shadow_size, options.comparison_epsilon);
  }

  const auto lhs_actions = lhs.actionPoints();
  const auto rhs_actions = rhs.actionPoints();
  for (std::size_t index = 0; index < lhs_actions.size(); ++index) {
    CHECK(equivalence::stringView(lhs_actions[index].name) == equivalence::stringView(rhs_actions[index].name));
    equivalence::checkVectorEqual(lhs_actions[index].position, rhs_actions[index].position, options.comparison_epsilon);
    CHECK(lhs_actions[index].bone == rhs_actions[index].bone);
  }

  const ArxModelOrigin lhs_origin = lhs.origin();
  const ArxModelOrigin rhs_origin = rhs.origin();
  CHECK(lhs_origin.bone == rhs_origin.bone);

  const auto lhs_selection_ids_view = lhs.selectionIds();
  const auto rhs_selection_ids_view = rhs.selectionIds();
  const std::vector<pistoris::SelectionId> lhs_selection_ids(lhs_selection_ids_view.begin(),
                                                             lhs_selection_ids_view.end());
  const std::vector<pistoris::SelectionId> rhs_selection_ids(rhs_selection_ids_view.begin(),
                                                             rhs_selection_ids_view.end());
  CHECK(lhs_selection_ids == rhs_selection_ids);
  if (lhs_selection_ids != rhs_selection_ids) return;
  for (pistoris::SelectionId id : lhs_selection_ids) {
    auto lhs_selection_result = lhs.selection(id);
    auto rhs_selection_result = rhs.selection(id);
    CHECK(lhs_selection_result);
    CHECK(rhs_selection_result);
    if (!lhs_selection_result || !rhs_selection_result) return;
    const ArxModelSelection lhs_selection = *lhs_selection_result;
    const ArxModelSelection rhs_selection = *rhs_selection_result;
    CHECK(equivalence::stringView(lhs_selection.name) == equivalence::stringView(rhs_selection.name));
    CHECK(lhs_selection.has_leading_vertex == rhs_selection.has_leading_vertex);
    if (lhs_selection.has_leading_vertex != 0 && rhs_selection.has_leading_vertex != 0) {
      equivalence::checkVectorEqual(
          lhs_selection.leading_position, rhs_selection.leading_position, options.comparison_epsilon);
      CHECK(lhs_selection.leading_bone == rhs_selection.leading_bone);
    }

    auto lhs_selected_result = lhs.selectionVertices(id);
    auto rhs_selected_result = rhs.selectionVertices(id);
    CHECK(lhs_selected_result);
    CHECK(rhs_selected_result);
    if (!lhs_selected_result || !rhs_selected_result) return;
    const std::size_t lhs_vertex_count = lhs_selected_result->size();
    const std::size_t rhs_vertex_count = rhs_selected_result->size();
    if (options.compare_vertex_multiplicity) {
      CHECK(lhs_vertex_count == rhs_vertex_count);
      if (lhs_vertex_count != rhs_vertex_count) return;
    }
    const std::vector<pistoris::VertexIndex> lhs_selected_vertices(lhs_selected_result->begin(),
                                                                   lhs_selected_result->end());
    const std::vector<pistoris::VertexIndex> rhs_selected_vertices(rhs_selected_result->begin(),
                                                                   rhs_selected_result->end());
    if (options.compare_geometry_order && options.compare_vertex_multiplicity) {
      CHECK(lhs_selected_vertices == rhs_selected_vertices);
    } else {
      std::vector<ArxModelVertex> lhs_selected;
      std::vector<ArxModelVertex> rhs_selected;
      lhs_selected.reserve(lhs_selected_vertices.size());
      rhs_selected.reserve(rhs_selected_vertices.size());
      for (pistoris::VertexIndex vertex : lhs_selected_vertices) {
        const bool valid = vertex < lhs_vertices.size();
        CHECK(valid);
        if (!valid) return;
        lhs_selected.push_back(lhs_vertices[vertex]);
      }
      for (pistoris::VertexIndex vertex : rhs_selected_vertices) {
        const bool valid = vertex < rhs_vertices.size();
        CHECK(valid);
        if (!valid) return;
        rhs_selected.push_back(rhs_vertices[vertex]);
      }
      if (options.compare_vertex_multiplicity) {
        CHECK(equivalence::spatialMultisetEquivalent<ArxModelVertex>(
            lhs_selected,
            rhs_selected,
            [](const ArxModelVertex& vertex) { return vertex.position; },
            vertex_equivalent,
            options.comparison_epsilon));
      } else {
        CHECK(equivalence::spatialSetEquivalent<ArxModelVertex>(
            lhs_selected,
            rhs_selected,
            [](const ArxModelVertex& vertex) { return vertex.position; },
            vertex_equivalent,
            options.comparison_epsilon));
      }
    }

    auto lhs_bones_result = lhs.selectionBones(id);
    auto rhs_bones_result = rhs.selectionBones(id);
    CHECK(lhs_bones_result);
    CHECK(rhs_bones_result);
    if (!lhs_bones_result || !rhs_bones_result) return;
    const std::size_t lhs_bone_count = lhs_bones_result->size();
    const std::size_t rhs_bone_count = rhs_bones_result->size();
    CHECK(lhs_bone_count == rhs_bone_count);
    if (lhs_bone_count != rhs_bone_count) return;
    const std::vector<pistoris::BoneIndex> lhs_selection_bones(lhs_bones_result->begin(), lhs_bones_result->end());
    const std::vector<pistoris::BoneIndex> rhs_selection_bones(rhs_bones_result->begin(), rhs_bones_result->end());
    CHECK(lhs_selection_bones == rhs_selection_bones);

    auto lhs_actions_result = lhs.selectionActionPoints(id);
    auto rhs_actions_result = rhs.selectionActionPoints(id);
    CHECK(lhs_actions_result);
    CHECK(rhs_actions_result);
    if (!lhs_actions_result || !rhs_actions_result) return;
    const std::size_t lhs_action_count = lhs_actions_result->size();
    const std::size_t rhs_action_count = rhs_actions_result->size();
    CHECK(lhs_action_count == rhs_action_count);
    if (lhs_action_count != rhs_action_count) return;
    const std::vector<pistoris::ActionPointIndex> lhs_selection_actions(lhs_actions_result->begin(),
                                                                        lhs_actions_result->end());
    const std::vector<pistoris::ActionPointIndex> rhs_selection_actions(rhs_actions_result->begin(),
                                                                        rhs_actions_result->end());
    CHECK(lhs_selection_actions == rhs_selection_actions);

    auto lhs_origin_result = lhs.selectionIncludesOrigin(id);
    auto rhs_origin_result = rhs.selectionIncludesOrigin(id);
    CHECK(lhs_origin_result);
    CHECK(rhs_origin_result);
    if (!lhs_origin_result || !rhs_origin_result) return;
    CHECK(*lhs_origin_result == *rhs_origin_result);
  }
}

}  // namespace test_support
