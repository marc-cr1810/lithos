#include "ColorMapRegistry.h"
#include "../debug/Logger.h"
#include "../vendor/stb_image.h"
#include <algorithm>
#include <fstream>
#include <iostream>

ColorMapRegistry &ColorMapRegistry::Get() {
  static ColorMapRegistry instance;
  return instance;
}

void ColorMapRegistry::LoadColorMaps(const std::string &configPath) {
  std::ifstream file(configPath);
  if (!file.is_open()) {
    LOG_ERROR("Failed to open color map config: {}", configPath);
    return;
  }

  nlohmann::json j;
  try {
    file >> j;
  } catch (const std::exception &e) {
    LOG_ERROR("Failed to parse color maps JSON: {}", e.what());
    return;
  }

  if (!j.is_array()) {
    LOG_ERROR("Color map config root is not an array");
    return;
  }

  for (const auto &item : j) {
    std::string code = item.value("code", "");
    if (code.empty())
      continue;

    std::string baseTexture = "";
    if (item.contains("texture") && item["texture"].contains("base")) {
      baseTexture = item["texture"]["base"];
    }

    if (baseTexture.empty()) {
      LOG_WARN("Color map '{}' has no base texture", code);
      continue;
    }

    // Texture path relative to assets/textures
    // e.g. "environment/plant_tint" ->
    // "assets/textures/environment/plant_tint.png"
    std::string texturePath = "assets/textures/" + baseTexture + ".png";

    ColorMap map;
    map.code = code;

    if (LoadTexture(texturePath, map)) {
      maps[code] = std::move(map);
      LOG_INFO("Loaded color map: {}", code);
    }
  }
}

bool ColorMapRegistry::LoadTexture(const std::string &path, ColorMap &outMap) {
  int w, h, channels;
  unsigned char *data = stbi_load(path.c_str(), &w, &h, &channels, 0);

  if (!data) {
    LOG_ERROR("Failed to load color map texture: {}", path);
    return false;
  }

  outMap.width = w;
  outMap.height = h;
  outMap.channels = channels;

  size_t dataSize = w * h * channels;
  outMap.data.resize(dataSize);
  std::memcpy(outMap.data.data(), data, dataSize);

  stbi_image_free(data);
  return true;
}

glm::vec3 ColorMapRegistry::GetColor(const std::string &mapCode, float temp,
                                     float humid) const {
  auto it = maps.find(mapCode);
  if (it == maps.end()) {
    // Return white if map not found ?? Or maybe green?
    return glm::vec3(1.0f);
  }

  const ColorMap &map = it->second;
  if (map.data.empty())
    return glm::vec3(1.0f);

  // In VS/Minecraft, temp/humid usually map to UVs
  // Typically:
  // Temp (X-axis): 0 (Cold) -> 1 (Hot) (or vice versa, check VS)
  // Humid (Y-axis): 0 (Dry) -> 1 (Wet)
  // VS: "ClimateColorMap" defines how it maps.
  // Usually it's a triangle triangle distribution or direct mapping.
  // For now, assume direct UV mapping 0..1.
  // Flip Y because texture coordinates usually 0 at bottom, but image data 0 at
  // top? Actually stbi loads top-to-bottom. UV (0,0) is usually bottom-left in
  // OpenGL, but images are stored top-left. Let's assume standard UV: u=temp,
  // v=humid. And handle the Y-flip if needed. For plant tint: usually
  // bottom-right is hot/dry? Let's implement simple 0-1 clamping and sampling.

  temp = glm::clamp(temp, 0.0f, 1.0f);
  humid = glm::clamp(humid, 0.0f, 1.0f);

  // Flip V for image sampling (0 is top in data, 0 is bottom in UV)
  // float u = temp;
  // float v = 1.0f - humid;
  // Wait, let's verify VS mapping later. For now assume (temp, 1-humid) is
  // typical for "foliage.png" style maps. Actually, let's just use (temp,
  // humid) directly and flip if it looks wrong. Minecraft foliage: x = temp y =
  // humidity * temp VS might be different. Let's stick to simple bilinear
  // sampling or nearest neighbor.

  int x = (int)(temp * (map.width - 1));
  int y = (int)((1.0f - humid) * (map.height - 1)); // Invert Y?

  x = std::clamp(x, 0, map.width - 1);
  y = std::clamp(y, 0, map.height - 1);

  int index = (y * map.width + x) * map.channels;

  if (index < 0 || index + 2 >= map.data.size())
    return glm::vec3(1.0f);

  float r = map.data[index] / 255.0f;
  float g = map.data[index + 1] / 255.0f;
  float b = map.data[index + 2] / 255.0f;

  return glm::vec3(r, g, b);
}
