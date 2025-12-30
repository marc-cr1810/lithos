#pragma once
#include <map>
#include <string>
#include <vector>

struct LandformConfigOverride {
  float baseHeight;
  float heightVariation;
  std::vector<float> octaveAmplitudes; // Overrides for 8 octaves
};

struct WorldGenConfig {
  int seed = 0;
  float globalScale = 1.0f;
  float terrainScale = 0.0025f;
  int seaLevel = 60;
  int surfaceDepth = 4;
  int worldHeight = 320; // World height in blocks (must be multiple of 32)

  // Noise Scales
  float tempScale = 0.003f;     // Balanced for moderate biome regions
  float humidityScale = 0.003f; // Balanced for moderate biome regions
  float landformScale =
      0.0015f; // Increased for more terrain variety (mountains, oceans, etc.)
  float climateScale = 0.0001f;
  float geologicScale = 0.001f;
  float biomeVariation = 0.25f; // Adds noise to break up smooth biome blobs

  // Key is landform name: "oceans", "plains", "hills", "mountains", "valleys"
  std::map<std::string, LandformConfigOverride> landformOverrides;

  // Caves
  float caveFrequency = 0.015f;
  float caveThreshold = 0.55f;
  bool enableCaves = true;
  bool enableRavines = true;
  int ravineDepth = 40;
  float caveEntranceNoise = 0.2f;

  // Hydrology
  bool enableRivers = true;
  float riverScale = 0.005f;
  float riverThreshold = 0.08f;
  float riverDepth = 15.0f;
  int lakeLevel = 62;

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
    landformOverrides["oceans"] = {
        35.0f,
        40.0f,
        {0.60f, 0.20f, 0.10f, 0.05f, 0.025f, 0.012f, 0.006f, 0.003f}};
    landformOverrides["plains"] = {
        66.0f,
        15.0f,
        {0.55f, 0.28f, 0.14f, 0.07f, 0.035f, 0.018f, 0.009f, 0.0045f}};
    landformOverrides["hills"] = {
        72.0f,
        40.0f,
        {0.45f, 0.38f, 0.28f, 0.2f, 0.12f, 0.07f, 0.035f, 0.018f}};
    landformOverrides["mountains"] = {
        100.0f, 180.0f, {0.38f, 0.45f, 0.5f, 0.42f, 0.28f, 0.2f, 0.14f, 0.07f}};
    landformOverrides["valleys"] = {
        55.0f,
        20.0f,
        {0.65f, 0.22f, 0.11f, 0.055f, 0.028f, 0.014f, 0.007f, 0.0035f}};
  }
};
