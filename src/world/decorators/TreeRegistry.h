#pragma once

#include "TreeGenConfig.h"
#include <map>
#include <random>
#include <string>


class TreeRegistry {
public:
  static TreeRegistry &Get();

  void LoadConfigs(const std::string &path);
  const TreeGenerator *SelectTree(float temp, float rain, float fert,
                                  float forest, float height,
                                  std::mt19937 &rng);
  const TreeStructure *GetTreeStructure(const std::string &name) const;
  const TreeGenConfig &GetConfig() const { return treeGenConfig; }

private:
  TreeRegistry() = default;

  bool loaded = false;
  TreeGenConfig treeGenConfig;
  std::map<std::string, TreeStructure> loadedTrees;
};
