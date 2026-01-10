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

  // Helper: Rotate point around center (0.5, 0.5, 0.5)
  auto rotatePoint = [&](glm::vec3 p, int rx, int ry, int rz) -> glm::vec3 {
    if (rx == 0 && ry == 0 && rz == 0)
      return p;

    glm::vec3 local = p - glm::vec3(0.5f);
    glm::mat4 matrix = glm::mat4(1.0f);
    if (rx != 0)
      matrix = glm::rotate(matrix, glm::radians((float)rx), glm::vec3(1, 0, 0));
    if (ry != 0)
      matrix = glm::rotate(matrix, glm::radians((float)ry), glm::vec3(0, 1, 0));
    if (rz != 0)
      matrix = glm::rotate(matrix, glm::radians((float)rz), glm::vec3(0, 0, 1));

    return glm::vec3(matrix * glm::vec4(local, 1.0f)) + glm::vec3(0.5f);
  };

  // Helper: Rotate normal
  auto rotateNormal = [&](glm::vec3 n, int rx, int ry, int rz) -> glm::vec3 {
    if (rx == 0 && ry == 0 && rz == 0)
      return n;

    glm::mat4 matrix = glm::mat4(1.0f);
    if (rx != 0)
      matrix = glm::rotate(matrix, glm::radians((float)rx), glm::vec3(1, 0, 0));
    if (ry != 0)
      matrix = glm::rotate(matrix, glm::radians((float)ry), glm::vec3(0, 1, 0));
    if (rz != 0)
      matrix = glm::rotate(matrix, glm::radians((float)rz), glm::vec3(0, 0, 1));

    return glm::vec3(matrix * glm::vec4(n, 0.0f));
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

  int brX = block->getRotateX();
  int brY = block->getRotateY();
  int brZ = block->getRotateZ();

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

      // Apply Block Rotation
      glm::vec3 rp0 = rotatePoint(p0, brX, brY, brZ);
      glm::vec3 rp1 = rotatePoint(p1, brX, brY, brZ);
      glm::vec3 rp2 = rotatePoint(p2, brX, brY, brZ);
      glm::vec3 rp3 = rotatePoint(p3, brX, brY, brZ);
      glm::vec3 rotatedNormal = rotateNormal(normal, brX, brY, brZ);

      // Determine new faceIdx for culling
      int rotatedFaceIdx = -1;
      if (rotatedNormal.z > 0.5f)
        rotatedFaceIdx = 0;
      else if (rotatedNormal.z < -0.5f)
        rotatedFaceIdx = 1;
      else if (rotatedNormal.x < -0.5f)
        rotatedFaceIdx = 2;
      else if (rotatedNormal.x > 0.5f)
        rotatedFaceIdx = 3;
      else if (rotatedNormal.y > 0.5f)
        rotatedFaceIdx = 4;
      else if (rotatedNormal.y < -0.5f)
        rotatedFaceIdx = 5;

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
      }

      glm::vec2 uv0(localU1, localV2);
      glm::vec2 uv1(localU2, localV2);
      glm::vec2 uv2(localU2, localV1);
      glm::vec2 uv3(localU1, localV1);

      // Calculate Overlay Flags (use original faceIdx for tint logic?)
      // Actually VS usually expects the logic to stay with the model face,
      // but if the block is rotated, should the tint stay with the "Top" face
      // or the "Physical" face?
      // In VS, tint usually stays with the face defined in the blocktype.
      float overlayFlag = 0.0f;
      if (block->shouldTint(faceIdx, 0))
        overlayFlag += 4.0f;
      if (block->hasOverlay(faceIdx))
        overlayFlag += 1.0f;
      if (block->shouldTint(faceIdx, 1))
        overlayFlag += 2.0f;

      // Store Vertices
      geom.positions.push_back(rp0);
      geom.positions.push_back(rp1);
      geom.positions.push_back(rp2);
      geom.positions.push_back(rp3);

      geom.uvs.push_back(uv0);
      geom.uvs.push_back(uv1);
      geom.uvs.push_back(uv2);
      geom.uvs.push_back(uv3);

      geom.normals.push_back(rotatedNormal);
      geom.normals.push_back(rotatedNormal);
      geom.normals.push_back(rotatedNormal);
      geom.normals.push_back(rotatedNormal);

      glm::vec4 origin(uMin, vMin, uW, vH);
      geom.texOrigins.push_back(origin);
      geom.texOrigins.push_back(origin);
      geom.texOrigins.push_back(origin);
      geom.texOrigins.push_back(origin);

      geom.overlayFlags.push_back(overlayFlag);
      geom.overlayFlags.push_back(overlayFlag);
      geom.overlayFlags.push_back(overlayFlag);
      geom.overlayFlags.push_back(overlayFlag);

      geom.faceInfo.push_back({rotatedFaceIdx, elem.shade});
      geom.faceInfo.push_back({rotatedFaceIdx, elem.shade});
      geom.faceInfo.push_back({rotatedFaceIdx, elem.shade});
      geom.faceInfo.push_back({rotatedFaceIdx, elem.shade});

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
