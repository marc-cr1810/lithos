#ifndef BLOCK_H
#define BLOCK_H

#include <cstdint>
#include <iostream>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

#include "../render/TextureAtlas.h" // Include full definition for resolveUVs

// Keep enum for IDs, useful for generation and serialization
enum BlockType {
  AIR = 0,
  DIRT = 1,
  GRASS = 2,
  STONE = 3,
  WOOD = 4,
  LEAVES = 5,
  COAL_ORE = 6,
  IRON_ORE = 7,
  GLOWSTONE = 8,
  WATER = 9,
  LAVA = 10,
  SAND = 11,
  GRAVEL = 12,
  SNOW = 13,
  ICE = 14,
  CACTUS = 15,
  PINE_WOOD = 16,
  PINE_LEAVES = 17,
  TALL_GRASS = 18,
  DEAD_BUSH = 19,
  ROSE = 20,
  DRY_SHORT_GRASS = 21,
  DRY_TALL_GRASS = 22,
  OBSIDIAN = 23,
  COBBLESTONE = 24,
  WOOD_PLANKS = 25
};

class World; // Forward declaration

class Block {
public:
  Block(uint8_t id, const std::string &name) : id(id), name(name) {
    for (int i = 0; i < 6; ++i) {
      textureNames[i] = "pink"; // fallback
      uMin[i] = 0.0f;
      vMin[i] = 0.0f;
      uMax[i] = 1.0f;
      vMax[i] = 1.0f;
    }
  }
  virtual ~Block() {}

  uint8_t getId() const { return id; }
  const std::string &getName() const { return name; }

  // Texture Configuration
  void setTexture(const std::string &texName) {
    for (int i = 0; i < 6; ++i)
      textureNames[i] = texName;
  }

  void setTexture(int face, const std::string &texName) {
    if (face >= 0 && face < 6)
      textureNames[face] = texName;
  }

  // Overlay Configuration
  void setOverlayTexture(int face, const std::string &texName) {
    if (face >= 0 && face < 6)
      overlayTextureNames[face] = texName;
  }

  bool hasOverlay(int faceDir) const {
    if (faceDir < 0 || faceDir >= 6)
      return false;
    return !overlayTextureNames[faceDir].empty();
  }

  // Resolve UVs from Atlas
  virtual void resolveUVs(const TextureAtlas &atlas) {
    for (int i = 0; i < 6; ++i) {
      if (textureNames[i].empty())
        continue;

      // 1. Resolve Base Textures
      float u, v;
      if (atlas.GetTextureUV(textureNames[i], u, v)) {
        uMin[i] = u;
        vMin[i] = v;
        textureVariants[i].push_back({u, v});
      }

      // Check for variants name_0, name_1, ... name_64
      // We scan a fixed range to allow for gaps (e.g. grass_0, grass_2)
      for (int counter = 0; counter <= 64; ++counter) {
        std::string variantName =
            textureNames[i] + "_" + std::to_string(counter);
        if (atlas.GetTextureUV(variantName, u, v)) {
          textureVariants[i].push_back({u, v});
        }
      }

      // 2. Resolve Overlay Textures
      if (!overlayTextureNames[i].empty()) {
        float u, v;
        if (atlas.GetTextureUV(overlayTextureNames[i], u, v)) {
          overlayVariants[i].push_back({u, v});
        }
        // Check for variants
        for (int counter = 0; counter <= 64; ++counter) {
          std::string variantName =
              overlayTextureNames[i] + "_" + std::to_string(counter);
          if (atlas.GetTextureUV(variantName, u, v)) {
            overlayVariants[i].push_back({u, v});
          }
        }
      }
    }
  }

  // Visuals
  virtual void getTextureUV(int faceDir, float &u, float &v) const {
    if (faceDir >= 0 && faceDir < 6) {
      u = uMin[faceDir];
      v = vMin[faceDir];
    } else {
      u = 0;
      v = 0;
    }
  }

  virtual void getTextureUV(int faceDir, float &u, float &v, int x, int y,
                            int z, int layer = 0) const {
    u = 0;
    v = 0;
    if (faceDir >= 0 && faceDir < 6) {
      const auto &variants =
          (layer == 0) ? textureVariants[faceDir] : overlayVariants[faceDir];
      if (!variants.empty()) {
        // Deterministic random selection
        int hash = (x * 73856093) ^ (y * 19349663) ^ (z * 83492791);
        int index = std::abs(hash) % variants.size();
        u = variants[index].first;
        v = variants[index].second;
      } else {
        // Fallback to defaults if no variants found (e.g. standard texture)
        // For layer 0, use uMin. For layer 1, we might not have a "default" if
        // variants are empty but overlay name set? Actually resolveUVs
        // populates variants even for single texture if name matches. But logic
        // above: 1. check name, add to variants. So variants should contain at
        // least one if texture exists. Except for overlay logic: I pushed back
        // overlayVariants.
        if (layer == 0) {
          u = uMin[faceDir];
          v = vMin[faceDir];
        } else {
          // If overlay variants empty but we are here, means no overlay.
          // Caller should check hasOverlay first.
        }
      }
    }
  }

  // Overload with metadata support for blocks that need it
  virtual void getTextureUV(int faceDir, float &u, float &v, int x, int y,
                            int z, uint8_t metadata, int layer = 0) const {
    // Default implementation ignores metadata and calls the regular version
    getTextureUV(faceDir, u, v, x, y, z, layer);
  }

  // Properties
  virtual bool isSolid() const { return true; }  // Collision
  virtual bool isOpaque() const { return true; } // Visualization/Light Blocking
  virtual uint8_t getEmission() const { return 0; } // Light source
  virtual bool isActive() const { return true; }    // Replaces != AIR check

  enum class RenderLayer { OPAQUE, CUTOUT, TRANSPARENT };
  virtual RenderLayer getRenderLayer() const { return RenderLayer::OPAQUE; }

  enum class RenderShape { CUBE, CROSS };
  virtual RenderShape getRenderShape() const { return RenderShape::CUBE; }

  // Events
  virtual void onPlace(World &world, int x, int y, int z) const {}
  virtual void onNeighborChange(World &world, int x, int y, int z, int nx,
                                int ny, int nz) const {}
  virtual void update(World &world, int x, int y, int z) const {}

  virtual void getColor(float &r, float &g, float &b) const {
    r = 1.0f;
    g = 1.0f;
    b = 1.0f;
  }

  virtual float getAlpha() const { return 1.0f; }

  // Layer-based Tinting
  // layer 0 = base, layer 1 = overlay
  virtual bool shouldTint(int faceDir, int layer) const {
    return true; // Default behavior
  }

protected:
  uint8_t id;
  std::string name;

  std::string textureNames[6];
  float uMin[6];
  float vMin[6];
  float uMax[6];
  float vMax[6];

  // Variants
  std::vector<std::pair<float, float>> textureVariants[6];

  // Overlay Support
  std::string overlayTextureNames[6];
  std::vector<std::pair<float, float>> overlayVariants[6];
};

// Singleton blocks
struct ChunkBlock {
  Block *block;
  uint8_t skyLight = 0;   // 0-15 Sun
  uint8_t blockLight = 0; // 0-15 Torches
  uint8_t metadata = 0;   // Extra data (flow level, rotation, etc)

  bool isActive() const { return block->isActive(); }
  bool isOpaque() const { return block->isOpaque(); }
  bool isSolid() const { return block->isSolid(); }
  uint8_t getEmission() const { return block->getEmission(); }
  uint8_t getType() const { return block->getId(); }
  Block::RenderLayer getRenderLayer() const { return block->getRenderLayer(); }
};

class BlockRegistry {
public:
  static BlockRegistry &getInstance();

  void registerBlock(Block *block);
  Block *getBlock(uint8_t id);

  // New: resolve all blocks
  void resolveUVs(const TextureAtlas &atlas) {
    for (auto &pair : blocks) {
      pair.second->resolveUVs(atlas);
    }
  }

private:
  BlockRegistry();
  ~BlockRegistry();

  std::unordered_map<uint8_t, Block *> blocks;
  Block *defaultBlock; // Air
};

#endif
