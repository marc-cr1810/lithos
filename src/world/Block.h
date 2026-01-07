#ifndef BLOCK_H
#define BLOCK_H

#include <cstdint>
#include <filesystem>
#include <iostream>
#include <memory>
#include <nlohmann/json.hpp>
#include <string>
#include <unordered_map>
#include <vector>

#include "../core/LangRegistry.h"
#include "../render/Model.h"
#include "../render/ModelLoader.h"
#include "../render/TextureAtlas.h"
#include <glm/glm.hpp>

// Block ID type - dynamic assignment at runtime (matches Vintage Story's
// ushort)
using block_id = uint16_t;  // 0-65,535 blocks
constexpr block_id AIR = 0; // Air block always gets ID 0

class World; // Forward declaration

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

    // Resolve Model Textures
    if (customModel) {
      for (const auto &[key, texParams] : customModel->textures) {
        // texParams is string like "assets:block/plant/cactus/cactus_side"
        std::string name = texParams;
        // Strip "assets:block/" if it exists
        const std::string prefix = "assets:block/";
        if (name.compare(0, prefix.length(), prefix) == 0) {
          name = name.substr(prefix.length());
        } else {
          // Fallback for names without prefix - use just the basename
          size_t lastSlash = name.find_last_of("/\\:");
          if (lastSlash != std::string::npos) {
            name = name.substr(lastSlash + 1);
          }
        }

        float u, v;
        if (atlas.GetTextureUV(name, u, v)) {
          modelTextureUVs[key] = {u, v};
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

  // Model Texture UV Lookup
  void getModelTextureUV(const std::string &key, float &u, float &v) const {
    // Key might be "#0", or just "0"?
    // Blockbench: "texture": "#0"
    // Model Textures map: "0" -> "path"
    // So valid key in map is "0".
    // We should strip '#' if present.
    std::string cleanKey = key;
    if (!cleanKey.empty() && cleanKey[0] == '#') {
      cleanKey = cleanKey.substr(1);
    }

    auto it = modelTextureUVs.find(cleanKey);
    if (it != modelTextureUVs.end()) {
      u = it->second.first;
      v = it->second.second;
    } else {
      u = 0;
      v = 0;
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

  enum class RenderLayer { OPAQUE, CUTOUT, TRANSPARENT };
  virtual RenderLayer getRenderLayer() const { return renderLayer; }
  void setRenderLayer(RenderLayer layer) { renderLayer = layer; }

  enum class RenderShape { CUBE, CROSS, SLAB_BOTTOM, STAIRS, MODEL, LAYERED };
  virtual RenderShape getRenderShape() const { return renderShape; }
  void setRenderShape(RenderShape shape) { renderShape = shape; }

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

  // Events
  virtual void onPlace(World &world, int x, int y, int z) const {}
  virtual void onNeighborChange(World &world, int x, int y, int z, int nx,
                                int ny, int nz) const {}
  virtual void update(World &world, int x, int y, int z) const {}

  virtual float getAlpha() const { return 1.0f; }

  // Layer-based Tinting
  // layer 0 = base, layer 1 = overlay
  virtual bool shouldTint(int faceDir, int layer) const {
    if (climateColorMap.empty())
      return false;
    if (tintOverlayOnly)
      return layer == 1; // Only tint overlay
    return true;         // Tint everything
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

  void setTintOverlayOnly(bool overlayOnly) { tintOverlayOnly = overlayOnly; }
  bool isTintOverlayOnly() const { return tintOverlayOnly; }

protected:
  block_id id;
  std::string name;
  std::string resourceId;
  bool isOpaque_ = true;
  bool isSolid_ = true;
  bool isReplaceable_ = false;
  float resistance = 1.0f;
  RenderLayer renderLayer = RenderLayer::OPAQUE;
  uint8_t emission_ = 0;
  std::vector<std::string> creativeTabs;

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

  // Custom Model
  std::shared_ptr<Model> customModel;
  std::unordered_map<std::string, std::pair<float, float>> modelTextureUVs;

  RenderShape renderShape = RenderShape::CUBE;

  nlohmann::json attributes;

  std::string climateColorMap;
  bool tintOverlayOnly = false;
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
  bool isSelectable() const { return block->isSelectable(); }
  uint8_t getEmission() const { return block->getEmission(); }
  block_id getType() const { return block->getId(); }
  Block::RenderLayer getRenderLayer() const { return block->getRenderLayer(); }
};

class BlockRegistry {
public:
  static BlockRegistry &getInstance();

  void registerBlock(Block *block);
  Block *getBlock(block_id id);
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

  std::unordered_map<block_id, Block *> blocks;
  std::unordered_map<std::string, Block *> blocksByResourceId;
  std::vector<CreativeTab> creativeTabs;
  Block *defaultBlock; // Air
  block_id nextId = 1;
};

#endif
