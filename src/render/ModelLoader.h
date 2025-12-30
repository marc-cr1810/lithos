#ifndef MODEL_LOADER_H
#define MODEL_LOADER_H

#include "Model.h"
#include <filesystem>
#include <memory>
#include <string>
#include <unordered_map>

class ModelLoader {
public:
  static std::shared_ptr<Model> loadModel(const std::filesystem::path &path);

private:
  static std::unordered_map<std::string, std::shared_ptr<Model>> cache;
};

#endif
