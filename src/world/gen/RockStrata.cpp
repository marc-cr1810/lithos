#include "RockStrata.h"
#include <cmath>
#include <cstdlib>

RockStrataRegistry &RockStrataRegistry::Get() {
  static RockStrataRegistry instance;
  return instance;
}

void RockStrataRegistry::Register(const GeologicProvince &province) {
  provinces.push_back(province);
}

const GeologicProvince *RockStrataRegistry::GetProvince(float noise) {
  if (provinces.empty())
    return nullptr;

  // Map noise (-1 to 1) to index
  float t = (noise + 1.0f) * 0.5f;
  size_t index = (size_t)(t * provinces.size());
  if (index >= provinces.size())
    index = provinces.size() - 1;

  return &provinces[index];
}

BlockType RockStrataRegistry::GetStrataBlock(int x, int y, int z, int surfaceY,
                                             float provinceNoise, int seed) {
  const GeologicProvince *prov = GetProvince(provinceNoise);
  if (!prov)
    return BlockType::STONE;

  int currentDepth = surfaceY - y;
  if (currentDepth < 0)
    return BlockType::AIR; // Should catch this before

  // Simple deterministic pseudo-random for thickness variation based on
  // X/Z/Seed to avoid straight lines
  auto getNoise = [&](int id) {
    int n = x * 374761393 + z * 668265263 + seed + id;
    n = (n ^ (n >> 13)) * 1274126177;
    return (float)((n & 0x7FFFFFFF) / (float)0x7FFFFFFF); // 0..1
  };

  int depthAccumulator = 0;
  // 1. Sedimentary (Top)
  for (size_t i = 0; i < prov->sedimentary.size(); ++i) {
    const auto &layer = prov->sedimentary[i];
    float noise = getNoise(i * 10);
    int thickness =
        layer.baseThickness + (int)(noise * layer.thicknessVariation);

    if (currentDepth >= depthAccumulator &&
        currentDepth < depthAccumulator + thickness) {
      return layer.block;
    }
    depthAccumulator += thickness;
  }

  // 2. Metamorphic (Middle)
  for (size_t i = 0; i < prov->metamorphic.size(); ++i) {
    const auto &layer = prov->metamorphic[i];
    float noise = getNoise(i * 20 + 500);
    int thickness =
        layer.baseThickness + (int)(noise * layer.thicknessVariation);

    if (currentDepth >= depthAccumulator &&
        currentDepth < depthAccumulator + thickness) {
      return layer.block;
    }
    depthAccumulator += thickness;
  }

  // 3. Igneous (Bottom/Base)
  // Everything below is igneous
  return prov->igneousBlock;
}

RockStrataRegistry::RockStrataRegistry() {
  // 1. Cratons / Shields (Ancient, Stable, Igneous/Metamorphic Surface)
  {
    GeologicProvince p;
    p.name = "Craton Shield";
    // Exposed basement
    p.sedimentary = {}; // Very little to no sedimentary
    p.metamorphic = {{BlockType::GNEISS, 20, 10}, {BlockType::SLATE, 10, 5}};
    p.igneousBlock = BlockType::GRANITE;
    Register(p);
  }

  // 2. Platforms (Sedimentary over Craton)
  {
    GeologicProvince p;
    p.name = "Platform";
    p.sedimentary = {{BlockType::SANDSTONE, 10, 5},
                     {BlockType::LIMESTONE, 15, 8},
                     {BlockType::CLAYSTONE, 10, 4}};
    p.metamorphic = {{BlockType::SHALE, 8, 3}};
    p.igneousBlock = BlockType::GRANITE;
    Register(p);
  }

  // 3. Orogens / Fold Belts (Mountain roots, Uplift)
  {
    GeologicProvince p;
    p.name = "Orogen";
    p.sedimentary = {{BlockType::LIMESTONE, 5, 2}};
    // Twisted metamorphic layers
    p.metamorphic = {{BlockType::SLATE, 20, 10},
                     {BlockType::WHITE_MARBLE, 10, 5}, // Marble veins
                     {BlockType::PHYLLITE, 15, 5}};
    p.igneousBlock = BlockType::DIORITE;
    Register(p);
  }

  // 4. Basins (Thick Sedimentary)
  {
    GeologicProvince p;
    p.name = "Sedimentary Basin";
    p.sedimentary = {{BlockType::SANDSTONE, 20, 10},
                     {BlockType::SHALE, 15, 5},
                     {BlockType::COAL_ORE, 2, 1}, // Seams
                     {BlockType::LIMESTONE, 20, 10}};
    p.metamorphic = {{BlockType::SLATE, 5, 2}};
    p.igneousBlock = BlockType::ANDESITE;
    Register(p);
  }

  // 5. Large Igneous Provinces (LIPs) - Flood Basalts
  {
    GeologicProvince p;
    p.name = "Flood Basalt (LIP)";
    p.sedimentary = {{BlockType::BASALT, 15, 5}, // Layers of flow
                     {BlockType::SCORIA, 5, 2}};
    p.metamorphic = {};
    p.igneousBlock = BlockType::BASALT;
    Register(p);
  }

  // 6. Extended Regions (Faulted)
  {
    GeologicProvince p;
    p.name = "Extended Crust";
    p.sedimentary = {{BlockType::CONGLOMERATE, 10, 5},
                     {BlockType::SANDSTONE, 10, 5}};
    p.metamorphic = {{BlockType::GNEISS, 10, 5}};
    p.igneousBlock = BlockType::RHYOLITE;
    Register(p);
  }

  // 7. Volcanic Arcs (Subduction zones)
  {
    GeologicProvince p;
    p.name = "Volcanic Arc";
    p.sedimentary = {{BlockType::TUFF, 10, 5}, {BlockType::ANDESITE, 10, 5}};
    p.metamorphic = {{BlockType::SLATE, 5, 2}};
    p.igneousBlock = BlockType::ANDESITE;
    Register(p);
  }

  // 8. Metallogenic Province (Ore rich)
  {
    GeologicProvince p;
    p.name = "Metallogenic";
    p.sedimentary = {{BlockType::LIMESTONE, 10, 5}};
    p.metamorphic = {{BlockType::PHYLLITE, 10, 5},
                     {BlockType::IRON_ORE, 2, 1}, // Richer ores
                     {BlockType::GOLD_ORE, 1, 0}};
    p.igneousBlock = BlockType::GRANITE;
    Register(p);
  }
}
