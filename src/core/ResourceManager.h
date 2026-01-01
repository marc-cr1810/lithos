#pragma once

#include "../render/Shader.h"
#include "../render/Texture.h"
#include "../render/TextureAtlas.h"
#include <memory>
#include <string>
#include <unordered_map>


class ResourceManager {
public:
  ResourceManager() = default;
  ~ResourceManager() = default;

  // Forbidden copy and assignment
  ResourceManager(const ResourceManager &) = delete;
  ResourceManager &operator=(const ResourceManager &) = delete;

  // Resource Loaders
  void LoadShader(const std::string &name, const std::string &vsPath,
                  const std::string &fsPath);
  Shader *GetShader(const std::string &name);

  void LoadTextureAtlas(const std::string &name, const std::string &dirPath,
                        int tileSize = 16);
  TextureAtlas *GetTextureAtlas(const std::string &name);

  void LoadTexture(const std::string &name, const std::string &path);
  Texture *GetTexture(const std::string &name);

  // Cleanup (Called in destructor usually, but can be manual)
  void Clear();

private:
  std::unordered_map<std::string, std::unique_ptr<Shader>> m_Shaders;
  std::unordered_map<std::string, std::unique_ptr<TextureAtlas>>
      m_TextureAtlases;
  std::unordered_map<std::string, std::unique_ptr<Texture>> m_Textures;
};
