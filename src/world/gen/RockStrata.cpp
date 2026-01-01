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
                                             float provinceNoise,
                                             float strataNoise, int seed) {
  const GeologicProvince *prov = GetProvince(provinceNoise);
  if (!prov)
    return BlockType::STONE;

  int currentDepth = surfaceY - y;
  if (currentDepth < 0)
    return BlockType::AIR; // Should catch this before

  // Modulation: Use the coherent strataNoise passed from WorldGenerator.
  // We use the layer index to "offset" the noise lookup so layers don't wobble
  // identically. Actually, wobbling identically looks like compression, which
  // is good! But strictly parallel layers look boring. Let's use (strataNoise +
  // i * 0.1) or similar. Or just use the same wobble. A wobble of thickness
  // means "this layer is thicker here". If layer 1 is thick, layer 2 is pushed
  // down. We calculate thickness. thickness = base + (int)(strataNoise *
  // variation).

  auto getNoise = [&](int id) {
    // Use the coherent noise, modulate by id slightly?
    // Actually, we want coherent visual.
    // If we use strataNoise for all layers, then all layers are thick/thin
    // together. That visually reinforces the "wave". But `strataNoise` is
    // -1..1. We map it to 0..1?
    float n = (strataNoise + 1.0f) * 0.5f;
    // Let's add a tiny offset base on id so they aren't PERFECTLY synced?
    // No, simple is smooth.
    return n;
  };

  int depthAccumulator = 0;
  // 1. Sedimentary (Top)
  for (size_t i = 0; i < prov->sedimentary.size(); ++i) {
    const auto &layer = prov->sedimentary[i];
    // Use the smooth noise. Maybe hash the noise with index to allow different
    // layers to swell differently? "Jagged" came from White Noise (no spatial
    // correlation). If we use strataNoise (spatially correlated), we solve
    // jaggedness. If we simply use strataNoise, all layers swell together. That
    // prevents layers from pinching out one another, which is safe. Let's just
    // use it directly.
    float noise = (strataNoise + 1.0f) * 0.5f;

    // Add simple variation per layer to avoid "copy-paste" look?
    // float layerVar = (float)((i * 37) % 10) / 10.0f;
    // noise = std::fmod(noise + layerVar, 1.0f); // Wrap around? No, jagged
    // discontinuities.

    // Simple: ALL layers follow the main warp.
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
    float noise = (strataNoise + 1.0f) * 0.5f;
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
