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

    // Parse extra params
    if (item.contains("padding")) {
      map.padding = item.value("padding", 0);
    }
    if (item.contains("loadIntoBlockTextureAtlas")) {
      map.loadIntoBlockTextureAtlas =
          item.value("loadIntoBlockTextureAtlas", false);
    }

    // ... (Initial setup same as before)
    if (LoadTexture(texturePath, map)) {
      // Assign runtime index
      map.runtimeIndex = mapIndexToCode.size() + 1; // 1-based (0 = no tint)
      mapIndexToCode.push_back(code);

      maps[code] = std::move(map);
      LOG_INFO("Loaded color map: {} (padding: {}, atlas: {}) ID: {}", code,
               maps[code].padding, maps[code].loadIntoBlockTextureAtlas,
               maps[code].textureID);
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

  // Store metadata
  outMap.width = w;
  outMap.height = h;
  outMap.channels = channels;

  // Keep CPU copy (optional? GetColor might still be used for CPU logic)
  size_t dataSize = w * h * channels;
  outMap.data.resize(dataSize);
  std::memcpy(outMap.data.data(), data, dataSize);

  // Generate OpenGL Texture ONLY if not being packed into Atlas
  if (!outMap.loadIntoBlockTextureAtlas) {
    glGenTextures(1, &outMap.textureID);
    glBindTexture(GL_TEXTURE_2D, outMap.textureID);
  }

  int format = (channels == 4) ? GL_RGBA : GL_RGB;

  if (outMap.padding > 0) {
    // Optimization: Strip padding!
    // Extract inner region: [padding, width-padding] x [padding,
    // height-padding]
    int innerW = w - 2 * outMap.padding;
    int innerH = h - 2 * outMap.padding;

    if (innerW <= 0 || innerH <= 0) {
      LOG_ERROR("Texture padding is too large for image size: {}", path);
      stbi_image_free(data);
      if (outMap.textureID != 0)
        glDeleteTextures(1, &outMap.textureID);
      return false;
    }

    // Allocate buffer for inner image
    std::vector<unsigned char> innerData;
    innerData.resize(innerW * innerH * channels);

    // Copy row by row
    for (int y = 0; y < innerH; ++y) {
      int srcY = y + outMap.padding;
      int srcOffset = (srcY * w + outMap.padding) * channels;
      int dstOffset = (y * innerW) * channels;
      std::memcpy(&innerData[dstOffset], &data[srcOffset], innerW * channels);
    }

    // Update outMap to reflect stripped data
    outMap.data = innerData;
    outMap.width = innerW;
    outMap.height = innerH;
    outMap.padding = 0; // Padding is gone now

    LOG_DEBUG("  -> Stripped padding: {}x{} -> {}x{}", w, h, innerW, innerH);

  } else {
    // No stripping needed, data is already in outMap.data?
    // Wait, lines 93-96 copied data to outMap.data.
    // So we use outMap.data.
  }

  // Upload if needed
  if (!outMap.loadIntoBlockTextureAtlas && outMap.textureID != 0) {
    glTexImage2D(GL_TEXTURE_2D, 0, format, outMap.width, outMap.height, 0,
                 format, GL_UNSIGNED_BYTE, outMap.data.data());

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    glBindTexture(GL_TEXTURE_2D, 0);
  }
  stbi_image_free(data);
  return true;
}

unsigned int ColorMapRegistry::GetTextureID(const std::string &mapCode) const {
  auto it = maps.find(mapCode);
  if (it != maps.end())
    return it->second.textureID;
  return 0;
}

int ColorMapRegistry::GetMapIndex(const std::string &mapCode) const {
  auto it = maps.find(mapCode);
  if (it != maps.end())
    return it->second.runtimeIndex;
  return 0; // 0 means no tint
}

void ColorMapRegistry::SetAtlasUVRect(const std::string &mapCode,
                                      const glm::vec4 &rect) {
  auto it = maps.find(mapCode);
  if (it != maps.end()) {
    it->second.atlasUVRect = rect;
  } else {
    LOG_WARN("Cannot set Atlas UV Rect: Map '{}' not found", mapCode);
  }
}

glm::vec4 ColorMapRegistry::GetAtlasUVRect(const std::string &mapCode) const {
  auto it = maps.find(mapCode);
  if (it != maps.end()) {
    return it->second.atlasUVRect;
  }
  return glm::vec4(0.0f);
}

void ColorMapRegistry::BindTextures(int baseUnit) {
  // Bind all known maps
  // Shader will expect: uniform sampler2D tintMaps[X];
  // tintMaps[0] maps to unit baseUnit
  // tintMaps[1] maps to unit baseUnit + 1
  // ...
  // Note: We need a consistent ordering.
  // mapIndexToCode gives us 0->code1 (index 1), 1->code2 (index 2)
  // MapIndex 1 => binds to slot 0? Or do we reserve index 0 for "none"?
  // In shader: valid indices are 1..N.
  // We can bind map 1 to unit 0, map 2 to unit 1...
  // access: texture(tintMaps[index - 1], uv).

  for (size_t i = 0; i < mapIndexToCode.size(); ++i) {
    glActiveTexture(GL_TEXTURE0 + baseUnit + i);
    std::string code = mapIndexToCode[i];
    glBindTexture(GL_TEXTURE_2D, maps[code].textureID);
  }
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

  temp = glm::clamp(temp, 0.0f, 1.0f);
  humid = glm::clamp(humid, 0.0f, 1.0f);

  int x, y;

  if (map.padding > 0) {
    // Assuming padding is symmetric (top/bottom/left/right)
    // Map range [0, 1] to [padding, width - padding - 1]
    // Inner width = width - 2*padding
    float innerW = (float)(map.width - 2 * map.padding);
    float innerH = (float)(map.height - 2 * map.padding);

    // Ensure we don't divide by zero if texture is smaller than padding
    // (unlikely but safe)
    if (innerW <= 0)
      innerW = 1.0f;
    if (innerH <= 0)
      innerH = 1.0f;

    // Note: width-1 for index, but here we want proper range scaling
    // Map 0 -> padding
    // Map 1 -> width - padding - 1
    // Range size is (width - padding - 1) - padding = width - 2*padding - 1
    // So scale factor is (innerW - 1)

    x = map.padding + (int)(temp * (innerW - 1.0f));
    y = map.padding + (int)((1.0f - humid) * (innerH - 1.0f));
  } else {
    x = (int)(temp * (map.width - 1));
    y = (int)((1.0f - humid) * (map.height - 1));
  }

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
