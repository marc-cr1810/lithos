#ifndef BLOCK_H
#define BLOCK_H

#include <cstdint>
#include <filesystem>
#include <iostream>
#include <memory>
#include <nlohmann/json.hpp>
#include <string>
#include <tuple>
#include <unordered_map>
#include <vector>

#include "../core/LangRegistry.h"
#include "../debug/Logger.h"
#include "../render/Model.h"
#include "../render/ModelLoader.h"
#include "../render/TextureAtlas.h"
#include <glm/glm.hpp>
#include <random>

// Block ID type - dynamic assignment at runtime (matches Vintage Story's
// ushort)
using block_id = uint16_t;  // 0-65,535 blocks
constexpr block_id AIR = 0; // Air block always gets ID 0

class World; // Forward declaration
#include "behaviors/BlockBehavior.h"

class Block {
public:
  Block(block_id id, const std::string &name) : id(id), name(name) {
    for (int i = 0; i < 6; ++i) {
      textureNames[i] = "pink"; // fallback
      uMin[i] = 0.0f;
      vMin[i] = 0.0f;
      uMax[i] = 1.0f;
      vMax[i] = 1.0f;
    }
  }
  virtual ~Block() {}

  block_id getId() const { return id; }
  void setId(block_id newId) { id = newId; }
  const std::string &getName() const { return name; }

  void setResourceId(const std::string &resId) { resourceId = resId; }
  const std::string &getResourceId() const { return resourceId; }

  // Get localized display name
  std::string getDisplayName() const { return Lang(resourceId); }

  // Texture Configuration
  void setTexture(const std::string &texName) {
    for (int i = 0; i < 6; ++i)
      textureNames[i] = texName;
  }

  void setTexture(int face, const std::string &texName) {
    if (face >= 0 && face < 6)
      textureNames[face] = texName;
  }

  void setFaceRotation(int face, int rotation) {
    if (face >= 0 && face < 6) {
      faceRotation[face] = rotation;
    }
  }

  int getFaceRotation(int face) const {
    if (face >= 0 && face < 6) {
      return faceRotation[face];
    }
    return 0;
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
  void setModel(const std::filesystem::path &path) {
    customModel = ModelLoader::loadModel(path);
    if (customModel) {
      setRenderShape(RenderShape::MODEL);
    }
  }

  const Model *getModel() const { return customModel.get(); }

  // Resolve UVs from Atlas
  virtual void resolveUVs(const TextureAtlas &atlas) {
    for (int i = 0; i < 6; ++i) {
      if (textureNames[i].empty())
        continue;

      // 1. Resolve Base Textures
      float u, v, u2, v2;
      if (atlas.GetTextureUV(textureNames[i], u, v, u2, v2)) {
        uMin[i] = u;
        vMin[i] = v;
        uMax[i] = u2;
        vMax[i] = v2;
        textureVariants[i].push_back(
            {u, v, u2, v2}); // Needs struct/tuple update
      } else {
        LOG_RESOURCE_WARN("Block '{}' failed to resolve main texture '{}'",
                          name, textureNames[i]);
      }

      // Check for variants name_0, name_1, ... name_64
      // We scan a fixed range to allow for gaps (e.g. grass_0, grass_2)
      for (int counter = 0; counter <= 64; ++counter) {
        std::string variantName =
            textureNames[i] + "_" + std::to_string(counter);
        if (atlas.GetTextureUV(variantName, u, v, u2, v2)) {
          textureVariants[i].push_back({u, v, u2, v2});
        }
      }

      // 2. Resolve Overlay Textures
      if (!overlayTextureNames[i].empty()) {
        float u, v, u2, v2;
        if (atlas.GetTextureUV(overlayTextureNames[i], u, v, u2, v2)) {
          overlayVariants[i].push_back({u, v, u2, v2}); // Update vector type?
        }
        // Check for variants
        for (int counter = 0; counter <= 64; ++counter) {
          std::string variantName =
              overlayTextureNames[i] + "_" + std::to_string(counter);
          if (atlas.GetTextureUV(variantName, u, v, u2, v2)) {
            overlayVariants[i].push_back({u, v, u2, v2});
          }
        }
      }
    }

    // Resolve Model Textures
    if (customModel) {
      // First, resolve any textures defined in the model file
      for (const auto &[key, texParams] : customModel->textures) {
        std::string name = texParams;
        const std::string prefix = "assets:block/";
        if (name.compare(0, prefix.length(), prefix) == 0) {
          name = name.substr(prefix.length());
        } else {
          size_t lastSlash = name.find_last_of("/\\:");
          if (lastSlash != std::string::npos) {
            name = name.substr(lastSlash + 1);
          }
        }

        float u, v, u2, v2;
        if (atlas.GetTextureUV(name, u, v, u2, v2)) {
          modelTextureUVs[key] = {u, v, u2, v2};
        }
      }

      // Also populate common face names (north, south, etc.) from block's face
      // textures This allows models to use #north, #south without defining them
      // in the model file
      const char *faceNames[] = {"north", "south", "east",
                                 "west",  "up",    "down"};
      int faceIndices[] = {1, 0, 3, 2, 4, 5}; // Map to engine face indices

      for (int i = 0; i < 6; ++i) {
        // Only populate if not already defined by model
        if (modelTextureUVs.find(faceNames[i]) == modelTextureUVs.end()) {
          modelTextureUVs[faceNames[i]] = {
              uMin[faceIndices[i]], vMin[faceIndices[i]], uMax[faceIndices[i]],
              vMax[faceIndices[i]]};
        }
      }
    }
  }

  // Visuals
  virtual void getTextureUV(int faceDir, float &u, float &v, float &u2,
                            float &v2) const {
    if (faceDir >= 0 && faceDir < 6) {
      u = uMin[faceDir];
      v = vMin[faceDir];
      u2 = uMax[faceDir];
      v2 = vMax[faceDir];
    } else {
      u = 0;
      v = 0;
      u2 = 1;
      v2 = 1;
    }
  }

  virtual void getTextureUV(int faceDir, float &u, float &v, float &u2,
                            float &v2, int x, int y, int z,
                            int layer = 0) const {
    u = 0;
    v = 0;
    u2 = 1;
    v2 = 1;
    if (faceDir >= 0 && faceDir < 6) {
      const auto &variants =
          (layer == 0) ? textureVariants[faceDir] : overlayVariants[faceDir];
      if (!variants.empty()) {
        // Deterministic random selection
        int hash = (x * 73856093) ^ (y * 19349663) ^ (z * 83492791);
        int index = std::abs(hash) % variants.size();
        u = std::get<0>(variants[index]);
        v = std::get<1>(variants[index]);
        u2 = std::get<2>(variants[index]);
        v2 = std::get<3>(variants[index]);
      } else {
        // Fallback to defaults
        if (layer == 0) {
          u = uMin[faceDir];
          v = vMin[faceDir];
          u2 = uMax[faceDir];
          v2 = vMax[faceDir];
        }
      }
    }
  }

  // Overload with metadata support for blocks that need it
  virtual void getTextureUV(int faceDir, float &u, float &v, float &u2,
                            float &v2, int x, int y, int z, uint8_t metadata,
                            int layer = 0) const {
    // Default implementation ignores metadata and calls the regular version
    getTextureUV(faceDir, u, v, u2, v2, x, y, z, layer);
  }

  // Model Texture UV Lookup
  void getModelTextureUV(const std::string &key, float &u, float &v, float &u2,
                         float &v2) const {
    // Key might be "#0", or just "0"?
    std::string cleanKey = key;
    if (!cleanKey.empty() && cleanKey[0] == '#') {
      cleanKey = cleanKey.substr(1);
    }

    auto it = modelTextureUVs.find(cleanKey);
    if (it != modelTextureUVs.end()) { // Update map to store tuple/struct?
      u = std::get<0>(it->second);
      v = std::get<1>(it->second);
      u2 = std::get<2>(it->second);
      v2 = std::get<3>(it->second);
    } else {
      u = 0;
      v = 0;
      u2 = 1;
      v2 = 1;
    }
  }

  // Properties
  // Properties
  virtual bool isSolid() const { return isSolid_; }
  void setSolid(bool solid) { isSolid_ = solid; }

  virtual bool isSelectable() const { return isSolid_; }
  virtual bool isOpaque() const { return isOpaque_; }
  virtual uint8_t getEmission() const { return emission_; }
  void setEmission(uint8_t e) { emission_ = e; }
  virtual bool isReplaceable() const { return isReplaceable_; }
  void setReplaceable(bool r) { isReplaceable_ = r; }

  virtual float getResistance() const { return resistance; }
  void setResistance(float r) { resistance = r; }

  virtual bool isActive() const { return true; }
  virtual bool isLiquid() const { return false; }
  virtual bool isLog() const { return false; }
  virtual bool isLeaves() const { return false; }
  virtual uint8_t getLightDecay() const { return 1; }

  void addCreativeTab(const std::string &tab) { creativeTabs.push_back(tab); }
  const std::vector<std::string> &getCreativeTabs() const {
    return creativeTabs;
  }

  // Model Rotation (VS-style)
  void setRotation(int x, int y, int z) {
    rotateX = x;
    rotateY = y;
    rotateZ = z;
  }
  void getRotation(int &x, int &y, int &z) const {
    x = rotateX;
    y = rotateY;
    z = rotateZ;
  }
  int getRotateX() const { return rotateX; }
  int getRotateY() const { return rotateY; }
  int getRotateZ() const { return rotateZ; }

  enum class RenderLayer { OPAQUE, CUTOUT, TRANSPARENT };
  virtual RenderLayer getRenderLayer() const { return renderLayer; }
  void setRenderLayer(RenderLayer layer) { renderLayer = layer; }

  enum class RenderShape { CUBE, CROSS, SLAB_BOTTOM, STAIRS, MODEL, LAYERED };
  virtual RenderShape getRenderShape() const { return renderShape; }
  void setRenderShape(RenderShape shape) { renderShape = shape; }

  enum class TintTarget : uint8_t { All, None, Base, Overlay };
  void setTintTarget(TintTarget target) { tintTarget = target; }
  TintTarget getTintTarget() const { return tintTarget; }

  // Get the height of the block (used for layered blocks, default is 1.0 for
  // full blocks)
  virtual float getBlockHeight(uint8_t metadata) const { return 1.0f; }

  // Get axis-aligned bounding box for this block (min/max relative to block
  // position) Default is full cube (0,0,0) to (1,1,1) but uses getBlockHeight
  // for Y-max
  virtual void getAABB(uint8_t metadata, glm::vec3 &outMin,
                       glm::vec3 &outMax) const {
    outMin = glm::vec3(0.0f, 0.0f, 0.0f);
    outMax = glm::vec3(1.0f, getBlockHeight(metadata), 1.0f);
  }

  // Behaviors
  void addBehavior(std::shared_ptr<BlockBehavior> behavior) {
    behaviors.push_back(behavior);
  }

  // Events
  virtual void onPlace(World &world, int x, int y, int z) const {
    for (const auto &b : behaviors) {
      b->onPlace(world, x, y, z);
    }
  }
  virtual void onNeighborChange(World &world, int x, int y, int z, int nx,
                                int ny, int nz) const {
    for (const auto &b : behaviors) {
      b->onNeighborChange(world, x, y, z, nx, ny, nz);
    }
  }
  virtual void update(World &world, int x, int y, int z) const {
    for (const auto &b : behaviors) {
      b->update(world, x, y, z);
    }
  }

  virtual void onRandomTick(World &world, int x, int y, int z,
                            std::mt19937 &rng) const {
    for (size_t i = 0; i < behaviors.size(); ++i) {
      behaviors[i]->onRandomTick(world, x, y, z, rng);
    }
  }

  // Hook for modifying block ID before placement (e.g. rotation)
  virtual block_id getPlacedBlockID(World &world, int x, int y, int z,
                                    const glm::vec3 &playerPos,
                                    const glm::vec3 &playerHeading,
                                    int clickedFace) const {
    block_id placedId = id;
    for (size_t i = 0; i < behaviors.size(); ++i) {
      placedId = behaviors[i]->getPlacedBlockID(
          world, x, y, z, playerPos, playerHeading, clickedFace, placedId);
    }
    return placedId;
  }

  virtual float getAlpha() const { return 1.0f; }

  // Layer-based Tinting
  // layer 0 = base, layer 1 = overlay
  virtual bool shouldTint(int faceDir, int layer) const {
    if (climateColorMap.empty() || tintTarget == TintTarget::None)
      return false;
    if (tintTarget == TintTarget::All)
      return true;
    if (tintTarget == TintTarget::Base)
      return layer == 0;
    if (tintTarget == TintTarget::Overlay)
      return layer == 1;
    return true;
  }

  void setOpaque(bool o) { isOpaque_ = o; }

  // Attributes
  void setAttributes(const nlohmann::json &attr) { attributes = attr; }
  const nlohmann::json &getAttributes() const { return attributes; }

  // Climate Tinting
  void setClimateColorMap(const std::string &mapCode) {
    climateColorMap = mapCode;
  }
  const std::string &getClimateColorMap() const { return climateColorMap; }

  // Random Ticking
  bool isRandomTickable() const { return isRandomTickable_; }
  void setRandomTickable(bool tickable) { isRandomTickable_ = tickable; }

protected:
  block_id id;
  std::string name;
  std::string resourceId;
  bool isOpaque_ = true;
  bool isSolid_ = true;
  bool isReplaceable_ = false;
  bool isRandomTickable_ = false;
  float resistance = 1.0f;
  RenderLayer renderLayer = RenderLayer::OPAQUE;
  uint8_t emission_ = 0;
  std::vector<std::string> creativeTabs;

  int rotateX = 0;
  int rotateY = 0;
  int rotateZ = 0;

  std::string textureNames[6];
  bool sideSolid[6] = {true, true, true, true, true, true}; // Default to solid

public:
  void setSideSolid(int face, bool solid) {
    if (face >= 0 && face < 6)
      sideSolid[face] = solid;
  }

  void setSideSolid(bool solid) {
    for (int i = 0; i < 6; ++i)
      sideSolid[i] = solid;
  }

  virtual bool isSideSolid(int face, int metadata = 0) const {
    // Default: ignore metadata, use static property
    if (face >= 0 && face < 6)
      return sideSolid[face];
    return true;
  }
  float uMin[6] = {0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f};
  float vMin[6] = {0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f};
  float uMax[6] = {1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f};
  float vMax[6] = {1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f};
  int faceRotation[6] = {0, 0, 0, 0, 0, 0}; // 0, 90, 180, 270

  // Variants
  std::vector<std::tuple<float, float, float, float>> textureVariants[6];

  // Overlay Support
  std::string overlayTextureNames[6];
  std::vector<std::tuple<float, float, float, float>> overlayVariants[6];

  // Custom Model
  std::shared_ptr<Model> customModel;
  std::unordered_map<std::string, std::tuple<float, float, float, float>>
      modelTextureUVs;

  RenderShape renderShape = RenderShape::CUBE;

  nlohmann::json attributes;

  std::string climateColorMap;
  TintTarget tintTarget = TintTarget::All;

  std::vector<std::shared_ptr<BlockBehavior>> behaviors;
};

// Singleton blocks
struct ChunkBlock {
  block_id id = 0;
  uint8_t skyLight = 0;   // 0-15 Sun
  uint8_t blockLight = 0; // 0-15 Torches
  uint8_t metadata = 0;   // Extra data (flow level, rotation, etc)

  // Fast accessors wrapping the registry lookup
  // Fast accessors wrapping the registry lookup
  Block *getBlock() const;

  bool isActive() const;
  bool isOpaque() const;
  bool isSolid() const;
  bool isSelectable() const;
  uint8_t getEmission() const;
  block_id getType() const { return id; }
  Block::RenderLayer getRenderLayer() const;
  Block::RenderShape getRenderShape() const;
};

class BlockRegistry {
public:
  static BlockRegistry &getInstance();

  void registerBlock(Block *block);
  inline Block *getBlock(block_id id) {
    if (id < blockVector.size())
      return blockVector[id];
    return defaultBlock;
  }
  Block *getBlock(const std::string &resourceId);
  block_id getBlockId(const std::string &resourceId);

  struct CreativeTab {
    std::string code;
    int listOrder;
    std::vector<Block *> blocks;
  };

  const std::vector<CreativeTab> &getCreativeTabs() const {
    return creativeTabs;
  }
  void addBlockToTab(const std::string &tabCode, Block *block);

  // New: resolve all blocks
  void resolveUVs(const TextureAtlas &atlas) {
    for (auto &pair : blocks) {
      pair.second->resolveUVs(atlas);
    }
  }

private:
  BlockRegistry();
  ~BlockRegistry();

  std::vector<Block *> blockVector; // Fast O(1) by ID
  std::unordered_map<block_id, Block *> blocks;
  std::unordered_map<std::string, Block *> blocksByResourceId;
  std::vector<CreativeTab> creativeTabs;
  Block *defaultBlock; // Air
  block_id nextId = 1;
};

// Inline implementations for ChunkBlock
inline Block *ChunkBlock::getBlock() const {
  return BlockRegistry::getInstance().getBlock(id);
}

inline bool ChunkBlock::isActive() const { return getBlock()->isActive(); }
inline bool ChunkBlock::isOpaque() const { return getBlock()->isOpaque(); }
inline bool ChunkBlock::isSolid() const { return getBlock()->isSolid(); }
inline bool ChunkBlock::isSelectable() const {
  return getBlock()->isSelectable();
}
inline uint8_t ChunkBlock::getEmission() const {
  return getBlock()->getEmission();
}
inline Block::RenderLayer ChunkBlock::getRenderLayer() const {
  return getBlock()->getRenderLayer();
}

inline Block::RenderShape ChunkBlock::getRenderShape() const {
  return getBlock()->getRenderShape();
}

#endif
