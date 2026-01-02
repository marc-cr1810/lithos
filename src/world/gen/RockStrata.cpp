#include "RockStrata.h"
#include "../../debug/Logger.h"
#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <fstream>
#include <nlohmann/json.hpp>

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
                                             float strataNoise,
                                             float distortion, int seed) {
  const GeologicProvince *prov = GetProvince(provinceNoise);
  if (!prov) {
    // std::cout << "Null Province!" << std::endl;
    return BlockType::STONE;
  }
  // std::cout << "Prov: " << prov->name << std::endl;

  // Distortion: Bend the layers!
  // distortion is typically -1 to 1 (noise).
  // We scale it to have significant vertical impact (e.g. +/- 40 blocks)
  // But wait, the caller might pass scaled distortion.
  // Let's assume distortion is the raw noise value or pre-scaled?
  // Caller (WorldGen) has 'upheaval' which is used for height shift.
  // If we utilize the SAME upheaval, the strata will follow the terrain bumps,
  // which is good. But we might want INDEPENDENT folding. For now, let's just
  // apply it directly to Y.

  // Warp the Y coordinate used for layer lookup
  // If we move up (higher Y), we should see deeper layers?
  // If Y=10 and warp=+10, we look up layer at Y=20.
  // This effectively pushes the strata DOWN at that location.
  // Wait, if layer is at Depth 10.
  // if Y=60 (Surface). Depth=0.
  // Y lookup should be relative to surface?
  // The original code used: currentDepth = surfaceY - y;
  // This means strata follows the terrain surface perfectly parallel.
  // To make it look like REAL geology (independent of surface erosion),
  // we should look up based on ABOSLUTE Y, not Depth from Surface.
  // VS uses Absolute Y with Upheaval.

  // NEW LOGIC: Absolute Y Mapping + Distortion
  // But we must ensure the top layer matches the surface block type roughly?
  // No, real geology doesn't guarantee top layer is sandstone. It could be
  // granite if eroded. HOWEVER, Lithos expects Top Soil (Grass/Dirt) to be
  // placed by WorldGen separately. This function places the ROCK below.

  // Let's try Absolute Y Strata.
  // 0 = Bedrock. 320 = Sky.
  // Layers stack from bottom up? Or top down from some "Simulated Max Height"?
  // Let's stack from Bottom Up (Igneous -> Metamorphic -> Sedimentary).
  // distortedY = y + distortion * 50.0f; // Scale distortion

  // But wait, if we switch to Absolute Y, we break the "Sedimentary Basin" feel
  // where it fills valleys. VS Hybrid approach: Some layers follow surface,
  // some are absolute? Let's stick to "Depth from Surface" but modulate the
  // Depth calculation. distortedDepth = (surfaceY - y) + distortion * 40.0f; If
  // distortion is positive, depth increases -> we see deeper layers
  // (erosion/uplift). If distortion is negative, depth decreases -> we see top
  // layers (depression).

  float distortionScale = 60.0f;
  int distortedDepth = (surfaceY - y) + (int)(distortion * distortionScale);

  // If distortedDepth < 0, it means we are "above" the simulated strata stack.
  // This can happen if upheaval pushes the strata so far down that we are
  // checking high up? Or rather, if we are exposed. We clamp to 0 (Top Layer).
  if (distortedDepth < 0)
    distortedDepth = 0;

  // Modulation: Use the coherent strataNoise passed from WorldGenerator.
  auto getNoise = [&](int id) {
    float n = (strataNoise + 1.0f) * 0.5f;
    return n;
  };

  int depthAccumulator = 0;

  // 1. Sedimentary (Top)
  for (size_t i = 0; i < prov->sedimentary.size(); ++i) {
    const auto &layer = prov->sedimentary[i];
    float noise = (strataNoise + 1.0f) * 0.5f;

    int thickness =
        layer.baseThickness + (int)(noise * layer.thicknessVariation);

    if (distortedDepth >= depthAccumulator &&
        distortedDepth < depthAccumulator + thickness) {
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

    if (distortedDepth >= depthAccumulator &&
        distortedDepth < depthAccumulator + thickness) {
      return layer.block;
    }
    depthAccumulator += thickness;
  }

  // 3. Igneous / Volcanic (Bottom up? Check deeper?)
  // If we fell through here, we are *below* the sedimentary/metamorphic stack.
  // The "distortedDepth" is how far we are *below the surface*.
  // Sed/Meta stack has a total thickness.

  // Total thickness of top layers
  int topStackThickness = depthAccumulator;

  // We are at depth "distortedDepth".
  // relative depth in igneous = distortedDepth - topStackThickness.
  int igneousDepth = distortedDepth - topStackThickness;

  if (igneousDepth < 0)
    return BlockType::STONE; // Should not trigger if logic above is correct

  // Traverse Igneous layers (Top Down relative to start of Igneous?)
  // rockstrata.json says "BottomUp" for some, "TopDown" for others.
  // Converting "BottomUp" to layer logic means: Thickness 100 means it occupies
  // 0..100? Let's simplified assumption: Igneous layers stack Top-Down *from
  // the bottom of Metamorphic*.

  for (size_t i = 0; i < prov->igneous.size(); ++i) {
    const auto &layer = prov->igneous[i];
    float noise = (strataNoise + 1.0f) * 0.5f;

    // Some igneous layers might be huge (Unitu) or small (dykes).
    // rockstrata.json: "lithos:basalt" (Igneous) amp=[10,5...] -> thickness
    // ~17. "lithos:granite" amp=[2.9...] -> thickness ~5. Wait, Granite is
    // usually the *Base*. It should ideally be Infinite. If rockstrata.json
    // gives it finite thickness, what's below? Maybe the last layer should be
    // infinite? Or we loop?

    int thickness =
        layer.baseThickness + (int)(noise * layer.thicknessVariation);

    if (igneousDepth < thickness) {
      return layer.block;
    }
    igneousDepth -= thickness;
  }

  // Fallback to the LAST igneous layer if we go deeper?
  // Or just STONE.
  if (!prov->igneous.empty()) {
    return prov->igneous.back().block;
  }

  return BlockType::STONE;
}

// Global Palette Storage (Private to this TU or member if moved to header, but
// static here is easier for now) Actually, these should probably be members of
// Registry to allow clearing/reloading properly. But since LoadStrataLayers and
// LoadProvinces are separate, we need state persistence. Let's add them as
// static members of the methods or file-scoped.
static std::vector<StrataLayer> g_Sedimentary;
static std::vector<StrataLayer> g_Metamorphic;
static std::vector<StrataLayer> g_Igneous;
static std::vector<StrataLayer> g_Volcanic;

void RockStrataRegistry::LoadStrataLayers(const std::string &path) {
  std::ifstream file(path);
  if (!file.is_open()) {
    LOG_ERROR("Failed to open rockstrata JSON: {}", path);
    return;
  }

  nlohmann::json root;
  try {
    root = nlohmann::json::parse(file, nullptr, true, true);
  } catch (const std::exception &e) {
    LOG_ERROR("JSON Parse Error (RockStrata): {}", e.what());
    return;
  }

  g_Sedimentary.clear();
  g_Metamorphic.clear();
  g_Igneous.clear();
  g_Volcanic.clear();

  if (root.contains("variants")) {
    for (const auto &j : root["variants"]) {
      std::string code;
      if (j.contains("blockcode"))
        code = j["blockcode"];
      else
        continue;

      Block *block = BlockRegistry::getInstance().getBlock(code);
      if (block->getId() == BlockType::AIR && code != "lithos:air") {
        // Warning log?
        continue;
      }

      StrataLayer layer;
      layer.block = (BlockType)block->getId();

      // Calc Thickness
      float sumAmp = 0.0f;
      if (j.contains("amplitudes")) {
        for (float a : j["amplitudes"])
          sumAmp += a;
      }
      layer.baseThickness = (int)sumAmp;
      if (layer.baseThickness < 2)
        layer.baseThickness = 2;
      layer.thicknessVariation = (int)(sumAmp * 0.5f);
      if (layer.thicknessVariation < 1)
        layer.thicknessVariation = 1;

      // Group
      std::string group = j.value("rockGroup", "Sedimentary");

      if (group == "Sedimentary")
        g_Sedimentary.push_back(layer);
      else if (group == "Metamorphic")
        g_Metamorphic.push_back(layer);
      else if (group == "Igneous")
        g_Igneous.push_back(layer);
      else if (group == "Volcanic")
        g_Volcanic.push_back(layer);
    }
  }
  LOG_INFO("Loaded Global Strata Layers: Sed={} Meta={} Ign={} Volc={}",
           g_Sedimentary.size(), g_Metamorphic.size(), g_Igneous.size(),
           g_Volcanic.size());
}

void RockStrataRegistry::LoadProvinces(const std::string &path) {
  std::ifstream file(path);
  if (!file.is_open()) {
    LOG_ERROR("Failed to open provinces JSON: {}", path);
    return;
  }

  nlohmann::json root;
  try {
    root = nlohmann::json::parse(file, nullptr, true, true);
  } catch (const std::exception &e) {
    LOG_ERROR("JSON Parse Error (Provinces): {}", e.what());
    return;
  }

  provinces.clear();

  if (root.contains("variants")) {
    for (const auto &j : root["variants"]) {
      GeologicProvince p;
      if (j.contains("code"))
        p.name = j["code"];

      // Check constraints
      bool allowSed = true;
      bool allowMeta = true;
      bool allowIgn = true; // Always true?
      bool allowVolc = true;

      if (j.contains("rockstrata")) {
        const auto &rs = j["rockstrata"];
        if (rs.contains("Sedimentary") &&
            rs["Sedimentary"].value("maxThickness", 100) == 0)
          allowSed = false;
        if (rs.contains("Metamorphic") &&
            rs["Metamorphic"].value("maxThickness", 100) == 0)
          allowMeta = false;
        if (rs.contains("Igneous") &&
            rs["Igneous"].value("maxThickness", 255) == 0)
          allowIgn = false;
        if (rs.contains("Volcanic") &&
            rs["Volcanic"].value("maxThickness", 100) == 0)
          allowVolc = false;
        // If Volcanic entry missing, assume false? VS config usually explicit.
        // geologicprovinces.json has "Volcanic" in some, "Igneous" in all.
        if (!rs.contains("Volcanic"))
          allowVolc = false;
      }

      if (allowSed)
        p.sedimentary = g_Sedimentary;
      if (allowMeta)
        p.metamorphic = g_Metamorphic;
      if (allowIgn)
        p.igneous = g_Igneous;
      // Also append Volcanic to Igneous? Or Separate?
      // Let's create a separate volcanic vector in struct or append to igneous?
      // If I append to igneous, they mix.
      // rockstrata.json says Volcanic is "BottomUp" or "TopDown"?
      // "lithos:kimberlite" (Volcanic) is "BottomUp".
      // "lithos:basalt" (Volcanic) is "TopDown".
      // "lithos:basalt" (Igneous) is "BottomUp".
      // Complex. For now, let's treat Volcanic as "Special Igneous" and add to
      // igneous list.
      if (allowVolc) {
        p.igneous.insert(p.igneous.end(), g_Volcanic.begin(), g_Volcanic.end());
      }

      Register(p);
    }
  }
  LOG_INFO("Loaded {} Geologic Provinces.", provinces.size());
}

RockStrataRegistry::RockStrataRegistry() {
  // Empty constructor - no hardcoded values
}
