#pragma once
#include <map>
#include <string>

struct LandformConfigOverride {
  float baseHeight;
  float heightVariation;
};

struct WorldGenConfig {
  int seed = 0;
  float globalScale = 1.0f;
  int seaLevel = 60;

  // Key is landform name: "oceans", "plains", "hills", "mountains", "valleys"
  std::map<std::string, LandformConfigOverride> landformOverrides;

  float caveFrequency = 0.015f;
  float caveThreshold = 0.55f;

  WorldGenConfig() {
    // Initialize with default values seen in WorldGenerator.cpp
    landformOverrides["oceans"] = {45.0f, 10.0f};
    landformOverrides["plains"] = {64.0f, 12.0f};
    landformOverrides["hills"] = {68.0f, 25.0f};
    landformOverrides["mountains"] = {82.0f, 60.0f};
    landformOverrides["valleys"] = {58.0f, 7.0f};
  }
};
