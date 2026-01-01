#include "ResourceManager.h"
#include "../debug/Logger.h"

void ResourceManager::LoadShader(const std::string &name,
                                 const std::string &vsPath,
                                 const std::string &fsPath) {
  if (m_Shaders.find(name) != m_Shaders.end()) {
    LOG_WARN("Shader '{}' already exists in ResourceManager. Skipping load.",
             name);
    return;
  }

  try {
    m_Shaders[name] = std::make_unique<Shader>(vsPath.c_str(), fsPath.c_str());
    LOG_INFO("Loaded Shader: '{}' from '{}' and '{}'", name, vsPath, fsPath);
  } catch (...) {
    LOG_ERROR("Failed to load Shader: '{}'", name);
  }
}

Shader *ResourceManager::GetShader(const std::string &name) {
  auto it = m_Shaders.find(name);
  if (it != m_Shaders.end()) {
    return it->second.get();
  }
  LOG_ERROR("Shader '{}' not found in ResourceManager!", name);
  return nullptr;
}

void ResourceManager::LoadTextureAtlas(const std::string &name,
                                       const std::string &dirPath,
                                       int tileSize) {
  if (m_TextureAtlases.find(name) != m_TextureAtlases.end()) {
    LOG_WARN(
        "TextureAtlas '{}' already exists in ResourceManager. Skipping load.",
        name);
    return;
  }

  // Default size 1024x1024 for now, could be dynamic
  int atlasSize = 1024;
  auto atlas = std::make_unique<TextureAtlas>(atlasSize, atlasSize, tileSize);

  // We assume TextureAtlas has a Load method that takes a directory
  try {
    atlas->Load(dirPath);

    // Setup texture from atlas data
    auto texture = std::make_unique<Texture>(
        atlas->GetWidth(), atlas->GetHeight(), atlas->GetData(), 4);
    m_Textures[name] = std::move(texture);

    m_TextureAtlases[name] = std::move(atlas);
    LOG_INFO("Loaded TextureAtlas and Texture: '{}' from '{}'", name, dirPath);
  } catch (...) {
    LOG_ERROR("Failed to load TextureAtlas: '{}'", name);
  }
}

TextureAtlas *ResourceManager::GetTextureAtlas(const std::string &name) {
  auto it = m_TextureAtlases.find(name);
  if (it != m_TextureAtlases.end()) {
    return it->second.get();
  }
  LOG_ERROR("TextureAtlas '{}' not found in ResourceManager!", name);
  return nullptr;
}

void ResourceManager::LoadTexture(const std::string &name,
                                  const std::string &path) {
  if (m_Textures.find(name) != m_Textures.end()) {
    LOG_WARN("Texture '{}' already exists in ResourceManager. Skipping load.",
             name);
    return;
  }

  try {
    auto texture = std::make_unique<Texture>(path.c_str());
    // Check if texture loaded successfully? Texture constructor logs error but
    // doesn't throw. It creates a pink texture on failure. We can check if
    // dimensions are 1x1? Or just trust it. For now, we trust the Texture class
    // logic.
    m_Textures[name] = std::move(texture);
    // LOG_INFO is handled inside Texture constructor for success/failure
  } catch (...) {
    LOG_ERROR("Failed to load Texture: '{}'", name);
  }
}

Texture *ResourceManager::GetTexture(const std::string &name) {
  auto it = m_Textures.find(name);
  if (it != m_Textures.end()) {
    return it->second.get();
  }
  LOG_ERROR("Texture '{}' not found in ResourceManager!", name);
  return nullptr;
}

void ResourceManager::Clear() {
  m_Shaders.clear();
  m_TextureAtlases.clear();
  m_Textures.clear();
}
