#include "Tessellator.h"
#include <algorithm>
#include <glm/gtc/matrix_transform.hpp>

namespace Lithos {

Tessellator::Tessellator(ModelMeshCache &cache) : meshCache(cache) {}

const ModelMeshData *Tessellator::getMesh(const Block *block) {
  if (!block)
    return nullptr;
  return meshCache.getOrCreate(block);
}

ModelMeshData Tessellator::tessellateBlock(const Block *block) {
  ModelMeshData geom;

  if (!block)
    return geom;
  const Model *model = block->getModel();
  if (!model)
    return geom;

  // Helper: Rotate UVs
  auto rotateUVCoords = [](float &u, float &v, int rotation) {
    if (rotation == 90) {
      float tmp = u;
      u = 1.0f - v;
      v = tmp;
    } else if (rotation == 180) {
      u = 1.0f - u;
      v = 1.0f - v;
    } else if (rotation == 270) {
      float tmp = u;
      u = v;
      v = 1.0f - tmp;
    }
  };

  // Helper: Transform Vector
  auto transform = [&](const ModelElement &elem, glm::vec3 p) -> glm::vec3 {
    if (elem.hasRotation) {
      glm::vec3 local = p - elem.rotation.origin;
      float rad = glm::radians(elem.rotation.angle);
      float s = sin(rad), c = cos(rad);
      float nx = local.x, ny = local.y, nz = local.z;
      if (elem.rotation.axis == 'x') {
        ny = local.y * c - local.z * s;
        nz = local.y * s + local.z * c;
      } else if (elem.rotation.axis == 'y') {
        nx = local.x * c + local.z * s;
        nz = -local.x * s + local.z * c;
      } else if (elem.rotation.axis == 'z') {
        nx = local.x * c - local.y * s;
        ny = local.x * s + local.y * c;
      }
      return elem.rotation.origin + glm::vec3(nx, ny, nz);
    }
    return p;
  };

  uint32_t vertexOffset = 0;

  for (const auto &elem : model->elements) {
    glm::vec3 minP = elem.from;
    glm::vec3 maxP = elem.to;

    for (const auto &[faceIdx, faceProp] : elem.faces) {
      if (!faceProp.enabled)
        continue;

      // Geometry Generation (Quads)
      glm::vec3 p0, p1, p2, p3;
      glm::vec3 normal;

      if (faceIdx == 0) { // Z+ (South?) - Engine Convention: 0=Z+
        p0 = glm::vec3(minP.x, minP.y, maxP.z);
        p1 = glm::vec3(maxP.x, minP.y, maxP.z);
        p2 = glm::vec3(maxP.x, maxP.y, maxP.z);
        p3 = glm::vec3(minP.x, maxP.y, maxP.z);
        normal = glm::vec3(0, 0, 1);
      } else if (faceIdx == 1) { // Z-
        p0 = glm::vec3(maxP.x, minP.y, minP.z);
        p1 = glm::vec3(minP.x, minP.y, minP.z);
        p2 = glm::vec3(minP.x, maxP.y, minP.z);
        p3 = glm::vec3(maxP.x, maxP.y, minP.z);
        normal = glm::vec3(0, 0, -1);
      } else if (faceIdx == 2) { // X-
        p0 = glm::vec3(minP.x, minP.y, minP.z);
        p1 = glm::vec3(minP.x, minP.y, maxP.z);
        p2 = glm::vec3(minP.x, maxP.y, maxP.z);
        p3 = glm::vec3(minP.x, maxP.y, minP.z);
        normal = glm::vec3(-1, 0, 0);
      } else if (faceIdx == 3) { // X+
        p0 = glm::vec3(maxP.x, minP.y, maxP.z);
        p1 = glm::vec3(maxP.x, minP.y, minP.z);
        p2 = glm::vec3(maxP.x, maxP.y, minP.z);
        p3 = glm::vec3(maxP.x, maxP.y, maxP.z);
        normal = glm::vec3(1, 0, 0);
      } else if (faceIdx == 4) { // Y+
        p0 = glm::vec3(minP.x, maxP.y, maxP.z);
        p1 = glm::vec3(maxP.x, maxP.y, maxP.z);
        p2 = glm::vec3(maxP.x, maxP.y, minP.z);
        p3 = glm::vec3(minP.x, maxP.y, minP.z);
        normal = glm::vec3(0, 1, 0);
      } else { // Y-
        p0 = glm::vec3(minP.x, minP.y, minP.z);
        p1 = glm::vec3(maxP.x, minP.y, minP.z);
        p2 = glm::vec3(maxP.x, minP.y, maxP.z);
        p3 = glm::vec3(minP.x, minP.y, maxP.z);
        normal = glm::vec3(0, -1, 0);
      }

      // Apply Element Rotation
      p0 = transform(elem, p0);
      p1 = transform(elem, p1);
      p2 = transform(elem, p2);
      p3 = transform(elem, p3);

      // UV Resolution
      float uMin, vMin, uMax, vMax;
      block->getModelTextureUV(faceProp.texture, uMin, vMin, uMax, vMax);

      float uW = uMax - uMin;
      float vH = vMax - vMin;

      float localU1 = faceProp.uv[0];
      float localV1 = 1.0f - faceProp.uv[1];
      float localU2 = faceProp.uv[2];
      float localV2 = 1.0f - faceProp.uv[3];

      if (faceProp.rotation != 0) {
        rotateUVCoords(localU1, localV1, faceProp.rotation);
        rotateUVCoords(localU2, localV2, faceProp.rotation);
        // Note: uv[2]/uv[3] are max corners, so rotating them is independent or
        // coupled? Actually ModelFace::uv is just the box logic. Standard
        // blockbench export uses UV coordinates for corners. We'll trust
        // faceProp.rotation handling from Chunk.cpp
      }

      // Vertices order: P0, P1, P2, P3 (CCW or CW?)
      // Chunk pushVert: (u_p0... etc).
      // P0: u1, v2
      // P1: u2, v2
      // P2: u2, v1
      // P3: u1, v1
      // (Standard mapping)

      // geom.uvs should store normalized (0-1) coordinates relative to the
      // texture tile The shader handles the mapping using geom.texOrigins
      glm::vec2 uv0(localU1, localV2);
      glm::vec2 uv1(localU2, localV2);
      glm::vec2 uv2(localU2, localV1);
      glm::vec2 uv3(localU1, localV1);

      // Calculate Overlay Flags
      float overlayFlag = 0.0f;
      if (block->shouldTint(faceIdx, 0))
        overlayFlag += 4.0f;
      if (block->hasOverlay(faceIdx))
        overlayFlag += 1.0f;
      if (block->shouldTint(faceIdx, 1))
        overlayFlag += 2.0f;

      // Store Vertices
      // Order: P0, P1, P2, P3
      geom.positions.push_back(p0);
      geom.positions.push_back(p1);
      geom.positions.push_back(p2);
      geom.positions.push_back(p3);

      geom.uvs.push_back(uv0);
      geom.uvs.push_back(uv1);
      geom.uvs.push_back(uv2);
      geom.uvs.push_back(uv3);

      geom.normals.push_back(normal);
      geom.normals.push_back(normal);
      geom.normals.push_back(normal);
      geom.normals.push_back(normal);

      glm::vec4 origin(uMin, vMin, uW, vH);
      geom.texOrigins.push_back(origin);
      geom.texOrigins.push_back(origin);
      geom.texOrigins.push_back(origin);
      geom.texOrigins.push_back(origin);

      geom.overlayFlags.push_back(overlayFlag);
      geom.overlayFlags.push_back(overlayFlag);
      geom.overlayFlags.push_back(overlayFlag);
      geom.overlayFlags.push_back(overlayFlag);

      geom.faceInfo.push_back({faceIdx, elem.shade});
      geom.faceInfo.push_back({faceIdx, elem.shade});
      geom.faceInfo.push_back({faceIdx, elem.shade});
      geom.faceInfo.push_back({faceIdx, elem.shade});

      // Indices (Quads -> Triangles)
      // 0, 1, 2, 0, 2, 3
      geom.indices.push_back(vertexOffset + 0);
      geom.indices.push_back(vertexOffset + 1);
      geom.indices.push_back(vertexOffset + 2);
      geom.indices.push_back(vertexOffset + 0);
      geom.indices.push_back(vertexOffset + 2);
      geom.indices.push_back(vertexOffset + 3);

      vertexOffset += 4;
    }
  }

  return geom;
}

} // namespace Lithos
