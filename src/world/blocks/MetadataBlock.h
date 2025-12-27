#ifndef METADATA_BLOCK_H
#define METADATA_BLOCK_H

#include "../Block.h"
#include <unordered_map>

// Block that supports different textures based on metadata value
class MetadataBlock : public Block {
public:
  MetadataBlock(uint8_t id, const std::string &name) : Block(id, name) {}

  // Set texture for a specific metadata value (all faces)
  void setTextureForMetadata(uint8_t metadata, const std::string &texName) {
    metadataTextures[metadata][""] = texName; // Empty string means all faces
  }

  // Set texture for a specific metadata value and face
  void setTextureForMetadata(uint8_t metadata, int face,
                             const std::string &texName) {
    metadataTextures[metadata][std::to_string(face)] = texName;
  }

  // Override resolveUVs to handle metadata-based textures
  void resolveUVs(const TextureAtlas &atlas) override {
    // First resolve base textures (for metadata 0 or default)
    Block::resolveUVs(atlas);

    // Then resolve metadata-specific textures
    for (auto &metaPair : metadataTextures) {
      uint8_t meta = metaPair.first;
      auto &faceTextures = metaPair.second;

      // Check if there's an "all faces" texture
      auto allFacesIt = faceTextures.find("");
      if (allFacesIt != faceTextures.end()) {
        const std::string &texName = allFacesIt->second;
        for (int face = 0; face < 6; ++face) {
          resolveMetadataTexture(atlas, meta, face, texName);
        }
      }

      // Then resolve face-specific textures (these override all-faces)
      for (auto &facePair : faceTextures) {
        if (facePair.first.empty())
          continue; // Skip all-faces entry

        int face = std::stoi(facePair.first);
        const std::string &texName = facePair.second;
        resolveMetadataTexture(atlas, meta, face, texName);
      }
    }
  }

  // Override getTextureUV to use metadata-based textures
  void getTextureUV(int faceDir, float &u, float &v, int x, int y, int z,
                    uint8_t metadata, int layer = 0) const override {
    u = 0;
    v = 0;
    if (faceDir < 0 || faceDir >= 6)
      return;

    // Check if we have metadata-specific variants
    auto metaIt = metadataVariants.find(metadata);
    if (metaIt != metadataVariants.end()) {
      auto faceIt = metaIt->second.find(faceDir);
      if (faceIt != metaIt->second.end()) {
        const auto &variants = faceIt->second;
        if (!variants.empty()) {
          // Deterministic random selection
          int hash = (x * 73856093) ^ (y * 19349663) ^ (z * 83492791);
          int index = std::abs(hash) % variants.size();
          u = variants[index].first;
          v = variants[index].second;
          return;
        }
      }
    }

    // Fallback to base implementation
    Block::getTextureUV(faceDir, u, v, x, y, z, layer);
  }

protected:
  void resolveMetadataTexture(const TextureAtlas &atlas, uint8_t meta, int face,
                              const std::string &texName) {
    if (texName.empty())
      return;

    float u, v;
    if (atlas.GetTextureUV(texName, u, v)) {
      metadataVariants[meta][face].push_back({u, v});
    }

    // Check for variants
    for (int counter = 0; counter <= 64; ++counter) {
      std::string variantName = texName + "_" + std::to_string(counter);
      if (atlas.GetTextureUV(variantName, u, v)) {
        metadataVariants[meta][face].push_back({u, v});
      }
    }
  }

  // Map: metadata -> face key -> texture name
  std::unordered_map<uint8_t, std::unordered_map<std::string, std::string>>
      metadataTextures;

  // Map: metadata -> face -> variants (UV coordinates)
  std::unordered_map<
      uint8_t, std::unordered_map<int, std::vector<std::pair<float, float>>>>
      metadataVariants;
};

#endif
