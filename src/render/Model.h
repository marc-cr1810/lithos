#ifndef MODEL_H
#define MODEL_H

#include <glm/glm.hpp>
#include <string>
#include <unordered_map>
#include <vector>


struct ModelFace {
  float uv[4];         // [u1, v1, u2, v2]
  std::string texture; // e.g. "#0"
  int rotation = 0;    // 0, 90, 180, 270
  int cullFace = -1;   // -1 = none, 0-5 = direction
};

struct ModelRotation {
  glm::vec3 origin;
  char axis; // 'x', 'y', 'z'
  float angle;
  bool rescale = false;
};

struct ModelElement {
  glm::vec3 from;
  glm::vec3 to;
  ModelRotation rotation;
  bool hasRotation = false;

  // Faces: 0=north, 1=east, 2=south, 3=west, 4=up, 5=down
  // Note: Render/Chunk logic might use different mapping.
  // Chunk mapping: 0=Z+, 1=Z-, 2=X-, 3=X+, 4=Y+, 5=Y-
  // Blockbench "north" usually means -Z. "south" is +Z.
  // "east" is +X. "west" is -X. "up" is +Y. "down" is -Y.
  // My Engine:
  // 0: Z+ (Front) -> South?
  // 1: Z- (Back) -> North?
  // 2: X- (Left) -> West?
  // 3: X+ (Right) -> East?
  // 4: Y+ (Top) -> Up
  // 5: Y- (Bottom) -> Down

  // Key: Face Direction (0-5) using Engine Convention
  std::unordered_map<int, ModelFace> faces;
};

struct Model {
  std::string name;
  std::unordered_map<std::string, std::string> textures;
  std::vector<ModelElement> elements;
};

#endif
