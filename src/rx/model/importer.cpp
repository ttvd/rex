#include <string.h> // memcpy

#include "rx/model/loader.h"

#include "rx/core/filesystem/file.h"
#include "rx/core/algorithm/max.h"
#include "rx/core/map.h"
#include "rx/core/log.h"

#include "rx/core/math/abs.h"

#include "rx/math/constants.h"
#include "rx/math/mat3x3.h"

RX_LOG("model/importer", logger);

namespace Rx::Model {

Importer::Importer(Memory::Allocator& _allocator)
  : m_allocator{_allocator}
  , m_meshes{allocator()}
  , m_elements{allocator()}
  , m_positions{allocator()}
  , m_coordinates{allocator()}
  , m_normals{allocator()}
  , m_tangents{allocator()}
  , m_blend_indices{allocator()}
  , m_blend_weights{allocator()}
  , m_frames{allocator()}
  , m_animations{allocator()}
  , m_joints{allocator()}
  , m_name{allocator()}
{
}

bool Importer::load(Stream* _stream) {
  m_name = _stream->name();

  if (!read(_stream)) {
    return false;
  }

  if (m_elements.is_empty() || m_positions.is_empty()) {
    return error("missing vertices");
  }

  // Ensure none of the elements go out of bounds.
  const Size vertices = m_positions.size();
  Uint32 max_element = 0;
  m_elements.each_fwd([&max_element](Uint32 _element) {
    max_element = Algorithm::max(max_element, _element);
  });

  if (max_element >= vertices) {
    return error("element %zu out of bounds", max_element);
  }

  if (m_elements.size() % 3 != 0) {
    return error("unfinished triangles");
  }

  logger->verbose("%zu triangles, %zu vertices, %zu meshes",
    m_elements.size() / 3, m_positions.size(), m_meshes.size());

  // Check for normals.
  if (m_normals.is_empty()) {
    logger->warning("missing normals");
    generate_normals();
  }

  // Check for tangents.
  if (m_tangents.is_empty()) {
    // Generating tangent vectors cannot be done unless the model contains
    // appropriate texture coordinates.
    if (m_coordinates.is_empty()) {
      return error("missing tangents and texture coordinates, bailing");
    } else {
      logger->warning("missing tangents, generating them");
      if (!generate_tangents()) {
        return error("'could not generate tangents, degenerate tangents formed");
      }
    }
  }

  // Ensure none of the normals, tangents or coordinates go out of bounds of
  // the loaded model.
  if (m_normals.size() != vertices) {
    logger->warning("too %s normals",
      m_normals.size() > vertices ? "many" : "few");
    m_normals.resize(vertices);
  }

  if (m_tangents.size() != vertices) {
    logger->warning("too %s tangents",
      m_tangents.size() > vertices ? "many" : "few");
    m_tangents.resize(vertices);
  }

  if (!m_coordinates.is_empty() && m_coordinates.size() != vertices) {
    logger->warning("too %s coordinates",
      m_coordinates.size() > vertices ? "many" : "few");
    m_coordinates.resize(vertices);
  }

  // Coalesce batches that share the same material.
  struct Batch {
    Size offset;
    Size count;
    Math::AABB bounds;
  };

  Map<String, Vector<Batch>> batches{allocator()};
  m_meshes.each_fwd([&](const Mesh& _mesh) {
    Math::AABB bounds;
    for (Size i{0}; i < _mesh.count; i++) {
      bounds.expand(m_positions[m_elements[_mesh.offset + i]]);
    }
    if (auto* find{batches.find(_mesh.material)}) {
      find->push_back({_mesh.offset, _mesh.count, bounds});
    } else {
      Vector<Batch> result{allocator()};
      result.emplace_back(_mesh.offset, _mesh.count, bounds);
      batches.insert(_mesh.material, Utility::move(result));
    }
  });

  Vector<Mesh> optimized_meshes{allocator()};
  Vector<Uint32> optimized_elements{allocator()};
  batches.each_pair([&](const String& _material_name, const Vector<Batch>& _batches) {
    Math::AABB bounds;
    const Size elements{optimized_elements.size()};
    _batches.each_fwd([&](const Batch& _batch) {
      const Size count{optimized_elements.size()};
      optimized_elements.resize(count + _batch.count);
      memcpy(optimized_elements.data() + count,
        m_elements.data() + _batch.offset, sizeof(Uint32) * _batch.count);
      bounds.expand(_batch.bounds);
    });
    const Size count{optimized_elements.size() - elements};
    optimized_meshes.push_back({elements, count, _material_name, bounds});
  });

  if (optimized_meshes.size() < m_meshes.size()) {
    logger->info("reduced %zu meshes to %zu", m_meshes.size(),
      optimized_meshes.size());
  }

  m_meshes = Utility::move(optimized_meshes);
  m_elements = Utility::move(optimized_elements);

  return true;
}

bool Importer::load(const String& _file_name) {
  if (Filesystem::File file{_file_name, "rb"}) {
    return load(&file);
  }
  return false;
}

void Importer::generate_normals() {
  const Size vertices{m_positions.size()};
  const Size elements{m_elements.size()};

  m_normals.resize(vertices);

  for (Size i{0}; i < elements; i += 3) {
    const Uint32 index0{m_elements[i + 0]};
    const Uint32 index1{m_elements[i + 1]};
    const Uint32 index2{m_elements[i + 2]};

    const Math::Vec3f p1p0{m_positions[index1] - m_positions[index0]};
    const Math::Vec3f p2p0{m_positions[index2] - m_positions[index0]};

    const Math::Vec3f normal{Math::normalize(Math::cross(p1p0, p2p0))};

    m_normals[index0] += normal;
    m_normals[index1] += normal;
    m_normals[index2] += normal;
  }

  for (Size i{0}; i < vertices; i++) {
    m_normals[i] = Math::normalize(m_normals[i]);
  }
}

bool Importer::generate_tangents() {
  const Size vertex_count{m_positions.size()};

  Vector<Math::Vec3f> tangents{allocator(), vertex_count};
  Vector<Math::Vec3f> bitangents{allocator(), vertex_count};

  for (Size i{0}; i < m_elements.size(); i += 3) {
    const Uint32 index0 = m_elements[i + 0];
    const Uint32 index1 = m_elements[i + 1];
    const Uint32 index2 = m_elements[i + 2];

    const Math::Mat3x3f triangle{m_positions[index0], m_positions[index1], m_positions[index2]};

    const Math::Vec2f uv0{m_coordinates[index1] - m_coordinates[index0]};
    const Math::Vec2f uv1{m_coordinates[index2] - m_coordinates[index0]};

    const Math::Vec3f q1{triangle.y - triangle.x};
    const Math::Vec3f q2{triangle.z - triangle.x};

    const Float32 det = uv0.s * uv1.t - uv1.s * uv0.t;

    if (Math::abs(det) <= Math::k_epsilon<Float32>) {
      return false;
    }

    const Float32 inv_det = 1.0f / det;

    const Math::Vec3f tangent{
      inv_det*(uv1.t*q1.x - uv0.t*q2.x),
      inv_det*(uv1.t*q1.y - uv0.t*q2.y),
      inv_det*(uv1.t*q1.z - uv0.t*q2.z)
    };

    const Math::Vec3f bitangent{
      inv_det*(-uv1.s*q1.x * uv0.s*q2.x),
      inv_det*(-uv1.s*q1.y * uv0.s*q2.y),
      inv_det*(-uv1.s*q1.z * uv0.s*q2.z)
    };

    tangents[index0] += tangent;
    tangents[index1] += tangent;
    tangents[index2] += tangent;

    bitangents[index0] += bitangent;
    bitangents[index1] += bitangent;
    bitangents[index2] += bitangent;
  }

  m_tangents.resize(vertex_count);

  for (Size i{0}; i < vertex_count; i++) {
    const Math::Vec3f& normal = m_normals[i];
    const Math::Vec3f& tangent = tangents[i];
    const Math::Vec3f& bitangent = bitangents[i];

    const auto real_tangent =
      Math::normalize(tangent - normal * Math::dot(normal, tangent));
    const auto real_bitangent =
            Math::dot(Math::cross(normal, tangent), bitangent) < 0.0f ? -1.0f : 1.0f;

    m_tangents[i] = {real_tangent.x, real_tangent.y, real_tangent.z, real_bitangent};
  }

  return true;
}

void Importer::write_log(Log::Level _level, String&& message_) const {
  if (m_name.is_empty()) {
    logger->write(_level, "%s", Utility::move(message_));
  } else {
    logger->write(_level, "%s: %s", m_name, Utility::move(message_));
  }
}

} // namespace rx::model
