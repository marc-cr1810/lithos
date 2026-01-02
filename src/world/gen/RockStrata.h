#pragma once
#include "../Block.h" // For BlockType
#include <string>
#include <vector>

struct StrataLayer {
  BlockType block;
  int baseThickness = 10;
  int thicknessVariation = 5;
};

struct GeologicProvince {
  std::string name;

  // Layers ordered from Top to Bottom
  std::vector<StrataLayer> sedimentary;
  std::vector<StrataLayer> metamorphic;
  std::vector<StrataLayer> igneous;  // Multiple igneous layers supported
  std::vector<StrataLayer> volcanic; // Volcanic layers

  // Thickness Caps for Blending
  int sedMaxThickness = 255;
  int metaMaxThickness = 255;
  int ignMaxThickness = 255;
  int volcMaxThickness = 255;
};

class RockStrataRegistry {
public:
  static RockStrataRegistry &Get();

  void Register(const GeologicProvince &province);

  void LoadStrataLayers(const std::string &path);
  void LoadProvinces(const std::string &path);

  // Returns the primary rock type for a given coordinate
  // distortion: vertical warp/upheaval to bend layers
  BlockType GetStrataBlock(int x, int y, int z, int surfaceY,
                           float provinceNoise, float strataNoise,
                           float distortion, int seed);

private:
  RockStrataRegistry();
  std::vector<GeologicProvince> provinces;

  const GeologicProvince *GetProvince(float noise);
};
