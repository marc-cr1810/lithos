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
  float terrainScale = 0.0025f;
  int seaLevel = 60;
  int surfaceDepth = 4;

  // Noise Scales
  float tempScale = 0.002f;
  float humidityScale = 0.002f;
  float landformScale = 0.0005f;
  float climateScale = 0.0001f;
  float geologicScale = 0.001f;

  // Key is landform name: "oceans", "plains", "hills", "mountains", "valleys"
  std::map<std::string, LandformConfigOverride> landformOverrides;

  // Caves
  float caveFrequency = 0.015f;
  float caveThreshold = 0.55f;
  bool enableCaves = true;
  bool enableRavines = true;
  int ravineDepth = 40;
  float caveEntranceNoise = 0.2f;

  // Decorators
  bool enableOre = true;
  bool enableTrees = true;
  bool enableFlora = true;

  // Densities (per chunk or roll)
  int coalAttempts = 10;
  int ironAttempts = 5;
  float oakDensity = 5.0f;    // Roll %
  float pineDensity = 2.0f;   // Roll %
  float cactusDensity = 1.0f; // Roll %
  float floraDensity = 10.0f; // Grass roll %

  WorldGenConfig() {
    // Initialize with default values seen in WorldGenerator.cpp
    landformOverrides["oceans"] = {45.0f, 10.0f};
    landformOverrides["plains"] = {64.0f, 12.0f};
    landformOverrides["hills"] = {68.0f, 25.0f};
    landformOverrides["mountains"] = {82.0f, 60.0f};
    landformOverrides["valleys"] = {58.0f, 7.0f};
  }
};
