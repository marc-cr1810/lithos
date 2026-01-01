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

  // Base igneous rock (fills from bottom up to the other layers)
  BlockType igneousBlock = BlockType::STONE; // Default granite/stone
};

class RockStrataRegistry {
public:
  static RockStrataRegistry &Get();

  void Register(const GeologicProvince &province);

  // Returns the primary rock type for a given coordinate
  // y: current height
  // surfaceY: height of the terrain surface
  // provinceNoise: -1 to 1 value selecting the province
  // seed: for thickness variation noise
  BlockType GetStrataBlock(int x, int y, int z, int surfaceY,
                           float provinceNoise, float strataNoise, int seed);

private:
  RockStrataRegistry();
  std::vector<GeologicProvince> provinces;

  const GeologicProvince *GetProvince(float noise);
};
