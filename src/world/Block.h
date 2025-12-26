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
  ROSE = 20
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

  // Resolve UVs from Atlas
  virtual void resolveUVs(const TextureAtlas &atlas) {
    for (int i = 0; i < 6; ++i) {
      if (textureNames[i].empty())
        continue;

      float u, v; // min
      float um,
          vm; // max (not used by getTextureUV currently, but stored if needed)

      // We need getTextureUV to return min/max?
      // Atlas::GetTextureUV returns min, min.
      // Wait, TextureAtlas implementation stores uMin, vMin, uMax, vMax.
      // I only exposed GetTextureUV(name, uMin, vMin) in header.
      // I should probably update Block to just use uMin/vMin assuming uniform
      // size? But Atlas might change slot size. Let's rely on
      // TextureAtlas::GetTextureUV for now.

      if (atlas.GetTextureUV(textureNames[i], u, v)) {
        uMin[i] = u;
        vMin[i] = v;
      } else {
        // Fallback or keep 0
        // std::cerr << "Missing texture: " << textureNames[i] << " for block "
        // << name << std::endl;
      }
    }
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

  virtual void getColor(float &r, float &g, float &b) const {
    r = 1.0f;
    g = 1.0f;
    b = 1.0f;
  }

  virtual float getAlpha() const { return 1.0f; }

protected:
  uint8_t id;
  std::string name;

  std::string textureNames[6];
  float uMin[6];
  float vMin[6];
  float uMax[6];
  float vMax[6];
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
