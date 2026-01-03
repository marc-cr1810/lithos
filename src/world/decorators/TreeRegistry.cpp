#include "TreeRegistry.h"
#include "../../debug/Logger.h"
#include <fstream>
#include <nlohmann/json.hpp>

using json = nlohmann::json;

TreeRegistry &TreeRegistry::Get() {
  static TreeRegistry instance;
  return instance;
}

void TreeRegistry::LoadConfigs(const std::string &path) {
  if (loaded)
    return;
  loaded = true;

  // 1. Load Main Config
  std::ifstream f(path);
  if (f.is_open()) {
    try {
      json j;
      f >> j;
      treeGenConfig = j.get<TreeGenConfig>();
      // LOG_INFO("Loaded treeGenConfig with {} tree generators",
      //          treeGenConfig.treegens.size());
    } catch (const json::parse_error &e) {
      LOG_ERROR("Failed to parse treegen.json: {}", e.what());
    }
  } else {
    LOG_ERROR("Could not open {}", path);
    return;
  }

  // 2. Load Individual Trees
  for (const auto &gen : treeGenConfig.treegens) {
    std::string treePath = "assets/worldgen/trees/" + gen.generator + ".json";
    std::ifstream tf(treePath);
    if (tf.is_open()) {
      try {
        json j;
        tf >> j;
        TreeStructure tree = j.get<TreeStructure>();
        loadedTrees[gen.generator] = tree;
        // LOG_INFO("Loaded tree definition: {}", gen.generator);
      } catch (const json::parse_error &e) {
        LOG_ERROR("Failed to parse tree {}: {}", gen.generator, e.what());
      }
    } else {
      LOG_ERROR("Could not open tree file: {}", treePath);
    }
  }
}

const TreeGenerator *TreeRegistry::SelectTree(float temp, float rain,
                                              float fert, float forest,
                                              float height, std::mt19937 &rng) {
  std::vector<const TreeGenerator *> candidates;
  float totalWeight = 0.0f;

  for (const auto &gen : treeGenConfig.treegens) {
    if (gen.IsSuitable(temp, rain, fert, forest, height)) {
      candidates.push_back(&gen);
      totalWeight += gen.weight;
    }
  }

  if (candidates.empty())
    return nullptr;

  std::uniform_real_distribution<float> dist(0.0f, totalWeight);
  float r = dist(rng);
  float accum = 0.0f;

  for (const auto *gen : candidates) {
    accum += gen->weight;
    if (r <= accum)
      return gen;
  }

  return candidates.back();
}

const TreeStructure *
TreeRegistry::GetTreeStructure(const std::string &name) const {
  auto it = loadedTrees.find(name);
  if (it != loadedTrees.end()) {
    return &it->second;
  }
  return nullptr;
}
