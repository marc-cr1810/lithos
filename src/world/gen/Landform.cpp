#include "Landform.h"
#include <algorithm>
#include <iostream>

float Landform::GetDensityThreshold(int y) const {
  if (yKeys.empty())
    return 0.0f;
  if (yKeys.size() == 1)
    return yKeys[0].threshold;

  // Find keys surrounding y
  const YKey *lower = &yKeys.front();
  const YKey *upper = &yKeys.back();

  if (y <= lower->yLevel)
    return lower->threshold;
  if (y >= upper->yLevel)
    return upper->threshold;

  for (size_t i = 0; i < yKeys.size() - 1; ++i) {
    if (y >= yKeys[i].yLevel && y < yKeys[i + 1].yLevel) {
      lower = &yKeys[i];
      upper = &yKeys[i + 1];
      break;
    }
  }

  float t = (float)(y - lower->yLevel) / (float)(upper->yLevel - lower->yLevel);
  // Linear interpolation
  return lower->threshold + t * (upper->threshold - lower->threshold);
}

LandformRegistry &LandformRegistry::Get() {
  static LandformRegistry instance;
  return instance;
}

void LandformRegistry::Register(const Landform &landform) {
  landforms.push_back(landform);
}

const Landform *LandformRegistry::GetLandform(const std::string &name) const {
  for (const auto &lf : landforms) {
    if (lf.name == name)
      return &lf;
  }
  return nullptr;
}

Landform LandformRegistry::Select(float landformNoise, float temp,
                                  float humid) const {
  // 1. Filter candidates by climate
  std::vector<const Landform *> candidates;
  float totalWeight = 0.0f;

  for (const auto &lf : landforms) {
    if (lf.useClimate) {
      if (temp < lf.minTemp || temp > lf.maxTemp)
        continue;
      if (humid < lf.minRain || humid > lf.maxRain)
        continue;
    }
    candidates.push_back(&lf);
    totalWeight += lf.weight;
  }

  // Fallback
  if (candidates.empty()) {
    auto *plains = GetLandform("Plains");
    return plains ? *plains : landforms[0];
  }

  // 2. Select Main Landform
  const Landform *selected = candidates.back();
  float roll = (landformNoise + 1.0f) * 0.5f; // 0..1
  float targetWeight = roll * totalWeight;
  float currentWeight = 0.0f;

  for (const auto *lf : candidates) {
    currentWeight += lf->weight;
    if (targetWeight <= currentWeight) {
      selected = lf;
      break;
    }
  }

  // 3. Handle Mutation / Variants
  Landform result = *selected;

  // A. Scalar Mutation (legacy support)
  if (result.mutationChance > 0 && !result.mutationTarget.empty()) {
    float mutationRoll = (roll * 100.0f) - (int)(roll * 100.0f);
    if (mutationRoll < result.mutationChance) {
      const Landform *mutant = GetLandform(result.mutationTarget);
      if (mutant)
        result = *mutant;
    }
  }

  // B. Variant Selection (Sub-Landforms)
  if (!result.variants.empty()) {
    // Deterministic variant roll
    float variantRoll = (roll * 50.0f) - (int)(roll * 50.0f); // Different slice

    // Weighted selection for variants
    float varTotalWeight = 0.0f;
    for (const auto &v : result.variants)
      varTotalWeight += v.weight;

    float varTarget = variantRoll * varTotalWeight;
    float varCurrent = 0.0f;

    for (const auto &v : result.variants) {
      varCurrent += v.weight;
      if (varTarget <= varCurrent) {
        // Apply Variant
        result.name += " " + v.nameSuffix;
        if (!v.yKeys.empty()) {
          result.yKeys = v.yKeys;
        }
        break;
      }
    }
  }

  return result;
}

LandformRegistry::LandformRegistry() {
  // Helper to make YKeys
  auto K = [](int y, float val) { return YKey{y, val}; };
  auto Oct = [](float amp) { return OctaveParam{amp, 0.0f}; };

  // --- 1. OCEAN / WETLANDS (LOW) ---
  {
    Landform lf;
    lf.name = "Ocean";
    lf.weight = 12.0f;
    lf.useClimate = false;
    lf.yKeys = {K(0, 1.0f), K(40, 0.0f), K(60, -1.0f)};
    lf.terrainOctaves = {Oct(0.2f)};
    lf.edgeBlendTarget =
        30.0f; // Blend to underwater level to avoid land bridges

    lf.variants.push_back({"Deep", 4.0f, {K(0, 1.f), K(20, 0.f), K(50, -1.f)}});
    lf.variants.push_back({"Warm", 3.0f});
    lf.variants.push_back({"Frozen", 3.0f});
    Register(lf);
  }
  {
    Landform lf;
    lf.name = "Swamp";
    lf.weight = 5.0f;
    lf.minTemp = 15.0f;
    lf.maxTemp = 40.0f;
    lf.minRain = 0.5f;
    lf.maxRain = 1.0f;
    lf.yKeys = {K(60, 1.0f), K(62, 0.0f), K(65, -1.0f)};
    lf.terrainOctaves = {Oct(0.1f)};
    lf.edgeBlendTarget = 60.0f; // Slightly below sea level for wateriness

    lf.variants.push_back({"Mangrove", 3.0f});
    lf.variants.push_back({"Bog", 3.0f});
    Register(lf);
  }

  // --- 2. PLAINS / FLATLANDS (MID-LOW) ---
  {
    Landform lf;
    lf.name = "Plains";
    lf.weight = 10.0f;
    // Celsius: -10 to 45 (Broad range)
    lf.minTemp = -10.0f;
    lf.maxTemp = 45.0f;
    lf.minRain = -0.5f;
    lf.maxRain = 0.5f;
    // Keys stay within typical plains range (60-80), preventing jumps to 256
    lf.yKeys = {K(60, 1.0f), K(64, 0.5f), K(70, -0.5f), K(80, -1.0f)};
    lf.terrainOctaves = {Oct(0.2f)};

    lf.variants.push_back({"Grazing", 5.0f});   // Default
    lf.variants.push_back({"Sunflower", 1.0f}); // Rare
    lf.variants.push_back(
        {"Plateau", 2.0f, {K(80, 1.f), K(85, 0.f), K(90, -1.f)}});
    Register(lf);
  }
  {
    Landform lf;
    lf.name = "Desert";
    lf.weight = 8.0f;
    lf.useClimate = true;
    // Hot and Dry
    lf.minTemp = 30.0f; // > 30C
    lf.maxTemp = 60.0f;
    lf.minRain = -1.0f;
    lf.maxRain = -0.5f;
    lf.yKeys = {K(60, 1.0f), K(68, 0.0f), K(85, -1.0f)};
    lf.terrainOctaves = {Oct(0.3f)};
    lf.foliageTint = {0.8f, 0.7f, 0.4f}; // Dried out look

    lf.variants.push_back({"Wastes", 4.0f});
    lf.variants.push_back({"Oasis", 0.5f}); // Very Rare
    Register(lf);
  }
  {
    Landform lf;
    lf.name = "Savanna";
    lf.weight = 8.0f;
    // Warm/Hot but not extreme Desert
    lf.minTemp = 20.0f;
    lf.maxTemp = 45.0f;
    lf.minRain = -0.2f;
    lf.maxRain = 0.3f;
    lf.yKeys = {K(62, 1.0f), K(68, 0.0f), K(80, -1.0f)};
    lf.terrainOctaves = {Oct(0.25f)};
    lf.variants.push_back({"Scrub", 3.0f});
    lf.variants.push_back(
        {"Shattered",
         1.0f,
         {K(60, 1.f), K(68, 0.f), K(100, 0.5f), K(120, -1.f)}});
    Register(lf);
  }
  {
    Landform lf;
    lf.name = "Tundra";
    lf.weight = 8.0f;
    // Cold
    lf.minTemp = -30.0f;
    lf.maxTemp = -5.0f;
    lf.yKeys = {K(60, 1.0f), K(65, 0.0f), K(75, -1.0f)};
    lf.terrainOctaves = {Oct(0.2f)};
    lf.variants.push_back({"Snowy", 4.0f});
    lf.variants.push_back(
        {"Spikes", 1.0f, {K(60, 1.f), K(70, 0.f), K(80, 0.5f), K(90, -1.f)}});
    Register(lf);
  }
  // Forest moved here as it is basically "Verdant Plains"
  {
    Landform lf;
    lf.name = "Forest";
    lf.weight = 10.0f;
    // Temperate
    lf.minTemp = 5.0f;
    lf.maxTemp = 25.0f;
    lf.minRain = 0.0f;
    lf.maxRain = 1.0f;
    lf.yKeys = {K(60, 1.0f), K(70, 0.0f), K(90, -1.0f)};
    lf.terrainOctaves = {Oct(0.4f)};
    lf.variants.push_back({"Birch", 3.0f});
    lf.variants.push_back({"Deep Woods", 2.0f});
    Register(lf);
  }

  // --- 3. HILLS / HIGHLANDS (MID-HIGH) ---
  {
    Landform lf;
    lf.name = "Hills";
    lf.weight = 10.0f;
    // Wide temp range
    lf.minTemp = -10.0f;
    lf.maxTemp = 35.0f;
    lf.yKeys = {K(60, 1.0f), K(70, 0.5f), K(90, 0.0f), K(120, -1.0f)};
    lf.terrainOctaves = {Oct(0.6f)};
    lf.variants.push_back({"Rolling", 5.0f});
    lf.variants.push_back({"Forested", 5.0f});
    Register(lf);
  }
  {
    Landform lf;
    lf.name = "Dunes";
    lf.weight = 5.0f;
    lf.minTemp = 30.0f;
    lf.maxTemp = 60.0f;
    lf.minRain = -1.0f;
    lf.maxRain = -0.5f;
    lf.yKeys = {K(60, 1.0f), K(75, 0.3f), K(100, -1.0f)};
    lf.terrainOctaves = {Oct(0.5f)};
    lf.variants.push_back({"Red Sand", 2.0f});
    lf.variants.push_back({"White Sand", 3.0f});
    Register(lf);
  }
  {
    Landform lf;
    lf.name = "Highlands";
    lf.weight = 5.0f;
    lf.minTemp = -15.0f;
    lf.maxTemp = 15.0f;
    lf.yKeys = {K(80, 1.0f), K(100, 0.0f), K(120, -0.2f), K(140, -1.0f)};
    lf.terrainOctaves = {Oct(1.2f)};
    Register(lf);
  }

  // --- 4. MOUNTAINS / BADLANDS (HIGH) ---
  {
    Landform lf;
    lf.name = "Mountains";
    lf.weight = 8.0f;
    // Colder generally
    lf.minTemp = -30.0f;
    lf.maxTemp = 20.0f;
    // Reduced max height to 240 to avoid clamping (256 limit)
    lf.yKeys = {K(60, 1.0f),  K(70, 0.8f),   K(100, 0.5f),
                K(160, 0.0f), K(200, -0.6f), K(240, -1.0f)};
    lf.terrainOctaves = {Oct(2.0f)};

    lf.variants.push_back({"Alpine", 5.0f});
    lf.variants.push_back(
        {"Jagged",
         3.0f,
         {K(60, 1.f), K(120, 0.5f), K(180, 0.f), K(240, -1.f)}});
    lf.variants.push_back({"Wooded", 3.0f});
    lf.variants.push_back({"Volcanic", 1.0f}); // Rare
    Register(lf);
  }
  {
    // Exotic
    Landform lf;
    lf.name = "Badlands";
    lf.weight = 3.0f;
    lf.minTemp = 25.0f;
    lf.maxTemp = 50.0f;
    lf.minRain = -1.0f;
    lf.maxRain = -0.5f;
    lf.yKeys = {K(60, 1.0f), K(80, 0.0f), K(200, -1.0f)};
    lf.terrainOctaves = {Oct(0.5f)};
    lf.variants.push_back(
        {"Eroded", 2.0f, {K(60, 1.f), K(70, 0.f), K(200, -1.f)}});
    lf.variants.push_back(
        {"Wooded Plateau", 2.0f, {K(60, 1.f), K(100, 0.f), K(120, -1.f)}});
    Register(lf);
  }
}
