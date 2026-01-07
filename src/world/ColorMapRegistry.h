#ifndef COLOR_MAP_REGISTRY_H
#define COLOR_MAP_REGISTRY_H

#include <glm/glm.hpp>
#include <nlohmann/json.hpp>
#include <string>
#include <unordered_map>
#include <vector>


class ColorMapRegistry {
public:
  static ColorMapRegistry &Get();

  void LoadColorMaps(const std::string &configPath);

  // Looks up color based on map code (e.g. "climatePlantTint") and UV
  // (temp/humid) temp/humid should be 0.0-1.0
  glm::vec3 GetColor(const std::string &mapCode, float temp, float humid) const;

private:
  ColorMapRegistry() = default;

  struct ColorMap {
    std::string code;
    int width;
    int height;
    std::vector<unsigned char> data; // RGBA or RGB
    int channels;
  };

  std::unordered_map<std::string, ColorMap> maps;

  // Helper to load texture data from disk
  bool LoadTexture(const std::string &path, ColorMap &outMap);
};

#endif
