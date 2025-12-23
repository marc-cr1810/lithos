#ifndef BLOCK_H
#define BLOCK_H

#include <cstdint>
#include <string>
#include <vector>
#include <unordered_map>
#include <memory>

// Keep enum for IDs, useful for generation and serialization
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
    GRAVEL = 12
};

class World; // Forward declaration

class Block {
public:
    Block(uint8_t id, const std::string& name) : id(id), name(name) {}
    virtual ~Block() {}

    uint8_t getId() const { return id; }
    const std::string& getName() const { return name; }

    // Properties
    virtual bool isSolid() const { return true; } // Collision
    virtual bool isOpaque() const { return true; } // Visualization/Light Blocking
    virtual uint8_t getEmission() const { return 0; } // Light source
    virtual bool isActive() const { return true; } // Replaces != AIR check
    
    enum class RenderLayer { OPAQUE, CUTOUT, TRANSPARENT };
    virtual RenderLayer getRenderLayer() const { return RenderLayer::OPAQUE; }
    
    // Events
    virtual void onPlace(World& world, int x, int y, int z) const {}
    virtual void onNeighborChange(World& world, int x, int y, int z, int nx, int ny, int nz) const {}
    virtual void update(World& world, int x, int y, int z) const {}
    
    // Visuals
    virtual void getTextureUV(int faceDir, float& uMin, float& vMin) const { 
        uMin = 0.0f; vMin = 0.0f; // Default (Magenta/Pink usually, or 0,0)
    }
    
    virtual void getColor(float& r, float& g, float& b) const { 
        r = 1.0f; g = 1.0f; b = 1.0f; 
    }
    
    virtual float getAlpha() const { return 1.0f; }

protected:
    uint8_t id;
    std::string name;
};

// Singleton blocks
struct ChunkBlock {
    Block* block;
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
    static BlockRegistry& getInstance();
    
    void registerBlock(Block* block);
    Block* getBlock(uint8_t id);

private:
    BlockRegistry();
    ~BlockRegistry();
    
    std::unordered_map<uint8_t, Block*> blocks;
    Block* defaultBlock; // Air
};

#endif

