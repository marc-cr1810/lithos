#include "ModelLoader.h"
#include <fstream>
#include <iostream>
#include <nlohmann/json.hpp>


using json = nlohmann::json;

std::unordered_map<std::string, std::shared_ptr<Model>> ModelLoader::cache;

std::shared_ptr<Model> ModelLoader::loadModel(const std::string &path) {
  if (cache.find(path) != cache.end()) {
    return cache[path];
  }

  std::ifstream f(path);
  if (!f.is_open()) {
    std::cerr << "Failed to open model file: " << path << std::endl;
    return nullptr;
  }

  json j;
  try {
    f >> j;
  } catch (const std::exception &e) {
    std::cerr << "JSON parsing error in " << path << ": " << e.what()
              << std::endl;
    return nullptr;
  }

  auto model = std::make_shared<Model>();
  model->name = path;

  // Load textures
  if (j.contains("textures")) {
    for (auto &[key, value] : j["textures"].items()) {
      model->textures[key] = value.get<std::string>();
      // Add # prefix support if needed, or stripping it in logic
    }
  }

  // Load elements
  if (j.contains("elements")) {
    for (const auto &elemJson : j["elements"]) {
      ModelElement elem;

      // From/To
      // Blockbench often uses 0-16 range. Engine uses same?
      // Engine usually expects 0-1 range for full block, but let's check
      // geometry generation. Chunk.cpp greedy meshing implies blocks are size
      // 1x1x1. Vertices are pushed as integers/floats. Blockbench: 0..16. So we
      // need to scale by 1/16.

      std::vector<float> from = elemJson["from"];
      std::vector<float> to = elemJson["to"];

      elem.from = glm::vec3(from[0], from[1], from[2]) / 16.0f;
      elem.to = glm::vec3(to[0], to[1], to[2]) / 16.0f;

      // Rotation
      if (elemJson.contains("rotation")) {
        elem.hasRotation = true;
        auto &rotJson = elemJson["rotation"];
        std::vector<float> origin = rotJson["origin"];
        elem.rotation.origin =
            glm::vec3(origin[0], origin[1], origin[2]) / 16.0f;

        std::string axis = rotJson["axis"];
        elem.rotation.axis = axis[0];

        elem.rotation.angle = rotJson["angle"];
        if (rotJson.contains("rescale")) {
          elem.rotation.rescale = rotJson["rescale"];
        }
      }

      // Faces
      if (elemJson.contains("faces")) {
        for (auto &[dirStr, faceJson] : elemJson["faces"].items()) {
          ModelFace face;
          // UV
          // Blockbench UV is often 0..16.
          if (faceJson.contains("uv")) {
            std::vector<float> uv = faceJson["uv"];
            face.uv[0] = uv[0] / 16.0f;
            face.uv[1] = uv[1] / 16.0f;
            face.uv[2] = uv[2] / 16.0f;
            face.uv[3] = uv[3] / 16.0f;
          } else {
            // Default UV?
          }

          if (faceJson.contains("texture")) {
            face.texture = faceJson["texture"];
          }
          if (faceJson.contains("rotation")) {
            face.rotation = faceJson["rotation"];
          }
          if (faceJson.contains("cullface")) {
            // Parse cullface if needed
          }

          // Map direction string to engine face index
          int faceIdx = -1;
          if (dirStr == "north")
            faceIdx = 1; // Z- (Back) ? My Engine Back is 1. Back is -Z?
          // Let's verify Engine Convention again.
          // Chunk.cpp:
          // 0: "Z+" (Front)
          // 1: "Z-" (Back)
          // 2: "X-" (Left)
          // 3: "X+" (Right)
          // 4: "Y+" (Top)
          // 5: "Y-" (Bottom)

          // JSON "north" is -Z -> 1
          // JSON "south" is +Z -> 0
          // JSON "east"  is +X -> 3
          // JSON "west"  is -X -> 2
          // JSON "up"    is +Y -> 4
          // JSON "down"  is -Y -> 5

          // Wait, Blockbench north is -Z.
          // My Engine 0 is Z+ (Front). South is Z+.
          // So North(Z-) should be 1. Correct.

          if (dirStr == "north")
            faceIdx = 1;
          else if (dirStr == "south")
            faceIdx = 0;
          else if (dirStr == "east")
            faceIdx = 3;
          else if (dirStr == "west")
            faceIdx = 2;
          else if (dirStr == "up")
            faceIdx = 4;
          else if (dirStr == "down")
            faceIdx = 5;

          if (faceIdx != -1) {
            elem.faces[faceIdx] = face;
          }
        }
      }

      model->elements.push_back(elem);
    }
  }

  cache[path] = model;
  return model;
}
