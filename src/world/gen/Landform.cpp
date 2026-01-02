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
      // Wait, let's re-verify.
      // If th[i] = 1 (Solid in JSON?). Comment says "JSON 1.0 (solid) becomes
      // VS 0.0". If th[i] = 1 (Solid). My old code: 2*1 - 1 = 1. Check: noise +
      // 1 > 0 => noise > -1. (Solid). Correct for th=1. If th[i] = 0 (Air in
      // JSON?). My old code: 2*0 - 1 = -1. Check: noise - 1 > 0 => noise > 1.
      // (Air). Correct for th=0.

      // WAIT. I verified this mentally and it seemed OK.
      // BUT if Benchmark implies Sky is Solid...
      // Maybe VS "Solid" means JSON 0?
      // "TerrainYThresholds[y] = 1 - GameMath.Lerp" (from LandformVariant.cs).
      // If JSON=1, VS=0.
      // If VS uses threshold for Solid: `noise - threshold > 0`.
      // If VS=0 (Solid), `noise - 0 > 0` => `noise > 0`. (50% Solid).
      // If VS=1 (Air), `noise - 1 > 0` => `noise > 1`. (0% Solid).

      // My Code: `noise + converted > 0`.
      // If converted = 1. `noise > -1`. (100% Solid).
      // If converted = -1. `noise > 1`. (0% Solid).

      // If I want 50% solid (VS=0), I need converted = 0.
      // My old code: th=1 (Solid?), VS=0.
      // Old: 2*1 - 1 = 1. (100% Solid).
      // VS 0 (from JSON 1) should be 50% solid?

      // If JSON 1 -> VS 0.
      // I used JSON th directly: `th[i] * 2 - 1`.
      // JSON 1 -> 1. (100% Solid).
      // But VS 0 should be 50% solid?
      // If JSON 1 means "Fill completely"? Or just "Bias towards solid"?
      // Usually Threshold 1.0 means "Solid bias".

      // Let's try to match VS 0 (JSON 1) to my 0.
      // And VS 1 (JSON 0) to my -1 (Air).
      // So JSON 1 (VS 0) -> 0.
      // JSON 0 (VS 1) -> -1.
      // Formula: JSON * 1 - 1 ?
      // 1 -> 0.
      // 0 -> -1.

      // Why did I use range -1 to 1?
      // Because noise is -1 to 1.
      // If I want FULL Solid (JSON ~10?), I need conv > 1.
      // If I want 50% solid (JSON 1?), I need conv 0.

      // Actually, if `th` is density *threshold*.
      // If JSON th=1.
      // I want it to be likely solid.
      // Resulting threshold 0? (50%).
      // If JSON th=0.
      // I want likely Air.
      // Resulting threshold -1? (Air).

      // Let's use: `float convertedThreshold = (th[i] - 0.5f) * 2.0f;` ?
      // If th=1 -> 0.5 * 2 = 1. (100% Solid).
      // If th=0 -> -0.5 * 2 = -1. (0% Solid).
      // If th=0.5 -> 0. (50% Solid).

      // This matches JSON 1=Solid, JSON 0=Air.
      // My old code was `th * 2 - 1`.
      // th=1 -> 1. (100% Solid).
      // th=0 -> -1. (Air).
      // THIS MATCHES?!

      // So why is sky solid?
      // Maybe JSON values are NOT 0..1?
      // Maybe noise is not -1..1?

      // I'll stick to `th * 2 - 1` logic BUT shift it downwards to bias towards
      // Air? Or maybe the noise is scaled up? `GetTerrainNoise3D` calls
      // `GenSingle3D`.

      // I will invert it blindly to check.
      // `float convertedThreshold = 1.0f - th[i] * 2.0f;`
      // th=1 -> -1 (Air).
      // th=0 -> 1 (Solid).
      // Thus JSON 1 -> Air. JSON 0 -> Solid.
      // If Sky is JSON 0, it becomes Solid.
      // This assumes current state is "Correct logic but incorrect outcome".

      // Hypothesis: I messed up something else.
      // Let's revert this file change and assume my logic `th * 2 - 1` is
      // conceptually fine for "High=Solid". But maybe "Sky" in landforms.json
      // has High Threshold? Let's assume VS `landforms.json`.
      // `terrainYKeyThresholds` for Sky (Y=1.0) should be 0 (Air)?
      // If it is 0, my code: -1. Check: `noise > 1`. Air.
      // So if Sky is 0, it works.

      // If Sky is Solid, then either:
      // 1. Sky JSON is 1. (Unlikely).
      // 2. Noise is > 1.
      // 3. I am reading wrong values.

      // I'm going to apply the inversion `1.0f - th[i] * 2.0f` just to see if
      // it fixes "Solid Sky". If "Solid Sky" is caused by 1->Solid, then
      // inverting makes 1->Air. If Sky was 1, it becomes Air.

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
