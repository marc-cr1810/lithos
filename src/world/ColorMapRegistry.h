#ifndef COLOR_MAP_REGISTRY_H
#define COLOR_MAP_REGISTRY_H

#include <GL/glew.h>
#include <glm/glm.hpp>
#include <nlohmann/json.hpp>
#include <string>
#include <unordered_map>
#include <vector>

class ColorMapRegistry {
public:
  struct ColorMap {
    std::string code;
    int width;
    int height;
    std::vector<unsigned char> data; // RGBA or RGB
    int channels;
    int padding = 0;
    bool loadIntoBlockTextureAtlas = false;
    unsigned int textureID = 0;
    int runtimeIndex = 0;
    glm::vec4 atlasUVRect = glm::vec4(0.0f); // uMin, vMin, uWidth, vHeight
  };

  static ColorMapRegistry &Get();

  void LoadColorMaps(const std::string &configPath);

  // Looks up color based on map code (e.g. "climatePlantTint") and UV
  // (temp/humid) temp/humid should be 0.0-1.0
  glm::vec3 GetColor(const std::string &mapCode, float temp, float humid) const;

  // Get GL Texture ID for a map code
  unsigned int GetTextureID(const std::string &mapCode) const;

  // Get unique integer ID for a map code (for vertex data)
  int GetMapIndex(const std::string &mapCode) const;

  // Set/Get Atlas UV Rect
  void SetAtlasUVRect(const std::string &mapCode, const glm::vec4 &rect);
  glm::vec4 GetAtlasUVRect(const std::string &mapCode) const;

  // Bind all textures to units starting at baseUnit
  void BindTextures(int baseUnit);

  const std::unordered_map<std::string, ColorMap> &GetMaps() const {
    return maps;
  }

  const std::vector<std::string> &GetIndexedCodes() const {
    return mapIndexToCode;
  }

  // Get all registered map codes (ordered by runtime index)
  const std::vector<std::string> &GetMapCodes() const { return mapIndexToCode; }

private:
  ColorMapRegistry() = default;

  std::unordered_map<std::string, ColorMap> maps;
  std::vector<std::string> mapIndexToCode; // To look up map by index

  // Resolve property value based on variant using byType patterns
  // Helper to load texture data from disk
  bool LoadTexture(const std::string &path, ColorMap &outMap);
};

#endif
