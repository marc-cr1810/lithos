#include "Landform.h"
#include "../../debug/Logger.h"
#include <algorithm>
#include <fstream>
#include <iostream>
#include <nlohmann/json.hpp>
#include <random>

float LandformVariant::GetDensityThreshold(int y) const {
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
  return lower->threshold + t * (upper->threshold - lower->threshold);
}

float Landform::GetDensityThreshold(int y) const {
  if (yKeys.empty())
    return 0.0f;
  // Fallback to variant logic? No, Landform has its own yKeys.
  if (yKeys.size() == 1)
    return yKeys[0].threshold;

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

Landform LandformRegistry::Select(int worldX, int worldZ, float temp,
                                  float humid) const {
  // VS-style: Seed RNG with world position (NoiseLandform.cs:128)
  // InitPositionSeed(xpos, zpos) -> uses position hash for deterministic random
  uint32_t seed = static_cast<uint32_t>(worldX) * 1619 +
                  static_cast<uint32_t>(worldZ) * 31337;
  std::mt19937 rng(seed);
  std::uniform_real_distribution<float> dist(0.0f, 1.0f);

  // Convert our climate values to VS's expected ranges:
  // - Temperature: VS landforms use Celsius scale directly (minTemp=-50,
  // maxTemp=50)
  // - Rain: VS uses 0-255 integer range
  // Our temp is already in Celsius, just need to convert humid (-1 to 1) to
  // rain (0-255)
  int vsRain = static_cast<int>(
      std::min(255.0f, std::max(0.0f, (humid + 1.0f) * 127.5f)));

  // 1. Filter candidates by climate and calculate total weight
  std::vector<const Landform *> candidates;
  float totalWeight = 0.0f;

  for (const auto &lf : landforms) {
    float weight = lf.weight;

    if (lf.useClimate) {
      // VS: if outside climate range, weight = 0 (NoiseLandform.cs:140)
      // Temperature is in Celsius, compare directly
      if (temp < lf.minTemp || temp > lf.maxTemp)
        weight = 0.0f;
      // Rain is 0-255 integer scale
      if (vsRain < lf.minRain || vsRain > lf.maxRain)
        weight = 0.0f;
    }

    if (weight > 0.0f) {
      candidates.push_back(&lf);
      totalWeight += weight;
    }
  }

  // Fallback if all filtered out
  if (candidates.empty() || totalWeight == 0.0f) {
    LOG_INFO("Landform fallback at ({}, {}): no valid candidates (temp={}, "
             "humid={}, vsRain={})",
             worldX, worldZ, temp, humid, vsRain);

    // DEBUG: Show first few landform climate ranges for debugging
    /*
    static bool shownRanges = false;
    if (!shownRanges && !landforms.empty()) {
      shownRanges = true;
      LOG_INFO("First 5 landform climate ranges:");
      for (size_t i = 0; i < std::min(size_t(5), landforms.size()); i++) {
        const auto &lf = landforms[i];
        LOG_INFO("  {}: useClimate={}, temp=[{}, {}], rain=[{}, {}]", lf.name,
                 lf.useClimate, lf.minTemp, lf.maxTemp, lf.minRain, lf.maxRain);
      }
    }
    */

    return landforms.empty() ? Landform{} : landforms[0];
  }

  // 2. Weighted random selection (VS: NoiseLandform.cs:147-152)
  float roll = dist(rng) * totalWeight;

  // DEBUG: Log selection (only occasionally to avoid spam)
  /*
  static int debugCounter = 0;
  bool shouldLog = (++debugCounter % 1000 == 0);

  if (shouldLog) {
    LOG_INFO("Landform selection at cell ({}, {}): {} candidates, "
             "totalWeight={}, roll={}, temp={}, humid={}",
             worldX, worldZ, candidates.size(), totalWeight, roll, temp, humid);
    for (const auto *lf : candidates) {
      LOG_INFO("  - {}: weight={}", lf->name, lf->weight);
    }
  }
  */

  for (const auto *lf : candidates) {
    roll -= lf->weight;
    if (roll <= 0.0f) {
      // Selected! Now handle variants/mutations
      Landform result = *lf;

      if (false) { // Disabled
        LOG_INFO("  => Selected: {}", result.name);
      }

      // TODO: Implement mutation system if needed
      // For now, just return the base landform
      return result;
    }
  }

  // Shouldn't reach here, but fallback to last candidate
  return candidates.empty() ? landforms[0] : *candidates.back();
}

static void ParseVariant(const nlohmann::json &j, LandformVariant &v) {
  if (j.contains("name"))
    v.nameSuffix = j["name"]; // Simplification
  else if (j.contains("code"))
    v.nameSuffix = j["code"];

  if (j.contains("weight"))
    v.weight = j["weight"];

  if (j.contains("minTemp")) {
    v.minTemp = j["minTemp"];
    v.useClimate = true;
  }
  if (j.contains("maxTemp")) {
    v.maxTemp = j["maxTemp"];
    v.useClimate = true;
  }
  if (j.contains("minRain")) {
    v.minRain = j["minRain"];
    v.useClimate = true;
  }
  if (j.contains("maxRain")) {
    v.maxRain = j["maxRain"];
    v.useClimate = true;
  }

  // Vintage Story uses thresholds in [0, 1] where:
  // - 1.0 = very solid (low terrain, underwater)
  // - 0.0 = air (high terrain, above surface)
  // Our density system uses: if (noise + threshold > 0) = solid
  // - Negative threshold = easier to be solid
  // - Positive threshold = harder to be solid (air)
  // Conversion: Convert VS [0,1] to our [-1,1] range, inverted
  if (j.contains("terrainYKeyPositions") &&
      j.contains("terrainYKeyThresholds")) {
    std::vector<float> pos = j["terrainYKeyPositions"];
    std::vector<float> th = j["terrainYKeyThresholds"];
    for (size_t i = 0; i < pos.size() && i < th.size(); ++i) {
      int y = (int)(pos[i] * 320.0f);
      // CRITICAL: VS inverts the JSON values! LandformVariant.cs line 162:
      // TerrainYThresholds[y] = 1 - GameMath.Lerp(...)
      // So JSON 1.0 (solid) becomes VS 0.0, JSON 0.0 (air) becomes VS 1.0
      // Then: VS uses noise - threshold > 0 for solid
      // We use: noise + threshold > 0 for air
      // After inversion: VS 0.0 = solid (low terrain), VS 1.0 = air (high)
      // Our formula: noise <= -ourThreshold for solid
      // So: VS 0.0 → ourThreshold should make it easy to be solid (negative)
      //     VS 1.0 → ourThreshold should make it hard to be solid (positive)
      // Since VS inverts: actualVS = 1 - jsonValue
      // Then map to [-1,1]: actualVS * 2 - 1
      // Then negate for our system: -(actualVS * 2 - 1)
      // Substituting: -(( 1 - th[i]) * 2 - 1) = -((2 - 2*th[i]) - 1) = -(1 -
      // 2*th[i]) = 2*th[i] - 1
      float convertedThreshold = th[i] * 2.0f - 1.0f;
      v.yKeys.push_back({y, convertedThreshold});
    }
  }
}

void LandformRegistry::LoadFromJson(const std::string &path) {
  std::ifstream file(path);
  if (!file.is_open()) {
    LOG_ERROR("Failed to open {}", path);
    return;
  }

  nlohmann::json root;
  try {
    root = nlohmann::json::parse(file, nullptr, true, true);
  } catch (const std::exception &e) {
    LOG_ERROR("JSON Error: {}", e.what());
    return;
  }

  landforms.clear();

  // Root should have "landforms" array or just be the array?
  // Vintage Story: { "code": "...", ... } is one item.
  // landforms.json usually array of objects.
  // Or object with "landforms"?
  // Assuming root is array or object with variants?
  // My previous assumption was "variants".
  // Check landforms.json content?
  // It's likely VS format: { "code": "...", "variants": [...] } ?
  // No, file likely contains list of landforms.
  // If root is object and has "variants", implies single landform file?
  // Ah, `landforms.json` typically combines all.
  // Let's assume it has "variants" key which is the list.

  if (root.contains("variants")) {
    for (const auto &j : root["variants"]) {
      Landform lf;
      if (j.contains("code"))
        lf.name = j["code"];
      if (j.contains("weight"))
        lf.weight = j["weight"];

      // Climate
      if (j.contains("minTemp")) {
        lf.minTemp = j["minTemp"];
        lf.useClimate = true;
      }
      if (j.contains("maxTemp")) {
        lf.maxTemp = j["maxTemp"];
        lf.useClimate = true;
      }
      if (j.contains("minRain")) {
        lf.minRain = j["minRain"];
        lf.useClimate = true;
      }
      if (j.contains("maxRain")) {
        lf.maxRain = j["maxRain"];
        lf.useClimate = true;
      }

      // Octaves
      if (j.contains("terrainOctaves")) {
        std::vector<float> amps = j["terrainOctaves"].get<std::vector<float>>();
        std::vector<float> thresholds;
        if (j.contains("terrainOctaveThresholds")) {
          thresholds = j["terrainOctaveThresholds"].get<std::vector<float>>();
        }
        for (size_t i = 0; i < amps.size(); i++) {
          float t = (i < thresholds.size()) ? thresholds[i] : 0.0f;
          lf.terrainOctaves.push_back({amps[i], t});
        }
      }

      // YKeys (Main Landform)
      LandformVariant tempVar;
      ParseVariant(j, tempVar);
      lf.yKeys = tempVar.yKeys;

      // Mutations (recursive variants)
      if (j.contains("mutations")) {
        for (const auto &m : j["mutations"]) {
          LandformVariant v;
          ParseVariant(m, v);
          // Add mutation as variant? Or separate Landform?
          // VS mutations are variants selected by mutation chance.
          // I map them to variants for now.
          // But Landform has `variants` (vector).
          lf.variants.push_back(v);
        }
      }

      Register(lf);
    }
  }

  LOG_INFO("Loaded {} landforms from JSON.", landforms.size());

  // DEBUG: Show climate ranges of first landform to verify defaults are loaded
  /*
  if (!landforms.empty()) {
    const auto &first = landforms[0];
    LOG_INFO("First landform '{}': useClimate={}, temp=[{}, {}], rain=[{}, {}]",
             first.name, first.useClimate, first.minTemp, first.maxTemp,
             first.minRain, first.maxRain);
  }
  */
}

LandformRegistry::LandformRegistry() {
  // Empty
}
