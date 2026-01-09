#ifndef BLOCK_BEHAVIOR_H
#define BLOCK_BEHAVIOR_H

#include <glm/glm.hpp>
#include <nlohmann/json.hpp>
#include <random>
#include <string>

// Forward declarations
class World;
class Block;
using block_id = uint16_t;

class BlockBehavior {
public:
  BlockBehavior(Block *block) : block(block) {}
  virtual ~BlockBehavior() = default;

  virtual void onLoaded(const nlohmann::json &properties) {}

  // Event hooks matching Block.h
  virtual void onPlace(World &world, int x, int y, int z) const {}
  virtual void onNeighborChange(World &world, int x, int y, int z, int nx,
                                int ny, int nz) const {}
  virtual void update(World &world, int x, int y, int z) const {}

  // Random Tick hook
  virtual void onRandomTick(World &world, int x, int y, int z,
                            std::mt19937 &rng) const {}

  // Placement modification hook
  // Returns: The block ID to place (default is currentId)
  virtual block_id getPlacedBlockID(World &world, int x, int y, int z,
                                    const glm::vec3 &playerPos,
                                    const glm::vec3 &playerHeading,
                                    int clickedFace, block_id currentId) const {
    return currentId;
  }

protected:
  Block *block;
};

#endif
