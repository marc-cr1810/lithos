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
  if (provinces.empty())
    return BlockType::STONE;

  // Map noise (-1 to 1) to continuous index
  float t = (provinceNoise + 1.0f) * 0.5f;    // 0..1
  float scaledT = t * (provinces.size() - 1); // 0 .. N-1
  size_t index1 = (size_t)scaledT;
  size_t index2 = index1 + 1;
  if (index2 >= provinces.size())
    index2 = provinces.size() - 1;
  float blend = scaledT - index1; // 0..1

  const GeologicProvince *prov1 = &provinces[index1];
  const GeologicProvince *prov2 = &provinces[index2];

  // Blend Thickness Caps
  auto lerp = [](int a, int b, float f) { return (int)(a + (b - a) * f); };

  int surfCap = lerp(prov1->sedMaxThickness, prov2->sedMaxThickness, blend);
  int metaCap = lerp(prov1->metaMaxThickness, prov2->metaMaxThickness, blend);
  int ignCap = lerp(prov1->ignMaxThickness, prov2->ignMaxThickness, blend);

  // Distortion
  float distortionScale = 30.0f; // Reduced from 60.0f to reduce extreme folding
  int distortedDepth = (surfaceY - y) + (int)(distortion * distortionScale);
  if (distortedDepth < 0)
    distortedDepth = 0;

  int depthAccumulator = 0;

  // Select Dominant Province for Layer List
  const GeologicProvince *dominantProv = (blend < 0.5f) ? prov1 : prov2;

  // 1. Sedimentary (Top)
  int sedThicknessUsed = 0;
  for (size_t i = 0; i < dominantProv->sedimentary.size(); ++i) {
    if (sedThicknessUsed >= surfCap)
      break;

    const auto &layer = dominantProv->sedimentary[i];
    float noise = (strataNoise + 1.0f) * 0.5f;
    int thickness =
        layer.baseThickness + (int)(noise * layer.thicknessVariation);

    int available = surfCap - sedThicknessUsed;
    if (thickness > available)
      thickness = available;
    if (thickness <= 0)
      break;

    if (distortedDepth >= depthAccumulator &&
        distortedDepth < depthAccumulator + thickness) {
      return layer.block;
    }
    depthAccumulator += thickness;
    sedThicknessUsed += thickness;
  }

  // 2. Metamorphic (Middle)
  int metaThicknessUsed = 0;
  for (size_t i = 0; i < dominantProv->metamorphic.size(); ++i) {
    if (metaThicknessUsed >= metaCap)
      break;

    const auto &layer = dominantProv->metamorphic[i];
    float noise = (strataNoise + 1.0f) * 0.5f;
    int thickness =
        layer.baseThickness + (int)(noise * layer.thicknessVariation);

    int available = metaCap - metaThicknessUsed;
    if (thickness > available)
      thickness = available;
    if (thickness <= 0)
      break;

    if (distortedDepth >= depthAccumulator &&
        distortedDepth < depthAccumulator + thickness) {
      return layer.block;
    }
    depthAccumulator += thickness;
    metaThicknessUsed += thickness;
  }

  // 3. Igneous
  int topStackThickness = depthAccumulator;
  int igneousDepth = distortedDepth - topStackThickness;

  if (igneousDepth < 0)
    return BlockType::STONE;

  int ignThicknessUsed = 0;
  for (size_t i = 0; i < dominantProv->igneous.size(); ++i) {
    if (ignThicknessUsed >= ignCap)
      return BlockType::STONE;

    const auto &layer = dominantProv->igneous[i];
    float noise = (strataNoise + 1.0f) * 0.5f;
    int thickness =
        layer.baseThickness + (int)(noise * layer.thicknessVariation);

    // If this is the last layer, should we allow it to be infinite?
    // VS logic suggests some are infinite, but with caps, we might want to cap
    // it. If we want Bedrock/Mantle at bottom, capping implies something below.
    // Let's strict cap.

    if (igneousDepth < thickness) {
      return layer.block;
    }
    igneousDepth -= thickness;
    ignThicknessUsed += thickness;
  }

  // Fallback to last igneous if we are still within cap but out of layers?
  if (!dominantProv->igneous.empty() && ignThicknessUsed < ignCap) {
    return dominantProv->igneous.back().block;
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
        if (rs.contains("Sedimentary")) {
          p.sedMaxThickness = rs["Sedimentary"].value("maxThickness", 255);
          if (p.sedMaxThickness == 0)
            allowSed = false;
        } else {
          allowSed = false;
        }

        if (rs.contains("Metamorphic")) {
          p.metaMaxThickness = rs["Metamorphic"].value("maxThickness", 255);
          if (p.metaMaxThickness == 0)
            allowMeta = false;
        } else {
          allowMeta = false;
        }

        if (rs.contains("Igneous")) {
          p.ignMaxThickness = rs["Igneous"].value("maxThickness", 255);
          if (p.ignMaxThickness == 0)
            allowIgn = false;
        } else {
          allowIgn = false;
        }

        if (rs.contains("Volcanic")) {
          p.volcMaxThickness = rs["Volcanic"].value("maxThickness", 255);
          if (p.volcMaxThickness == 0)
            allowVolc = false;
        } else {
          allowVolc = false;
        }
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
