#include "LiquidBlock.h"
#include "../BlockRegistrar.h"
#include "../World.h"

static BlockRegistrar registrar("LiquidBlock",
                                [](uint8_t id, const std::string &name) {
                                  return new LiquidBlock(id, name);
                                });

// Metadata: 0 = Source/Full Strength, 1-7 = Decaying Flow

LiquidBlock::LiquidBlock(uint8_t id, const std::string &name)
    : Block(id, name) {}

bool LiquidBlock::isSolid() const { return false; }
bool LiquidBlock::isReplaceable() const { return true; }
bool LiquidBlock::isOpaque() const { return false; }
Block::RenderLayer LiquidBlock::getRenderLayer() const {
  return RenderLayer::TRANSPARENT;
}

float LiquidBlock::getAlpha() const { return 1.0f; }
uint8_t LiquidBlock::getEmission() const { return 0; }

void LiquidBlock::update(World &world, int x, int y, int z) const {
  // Simple fluid flow simulation
  ChunkBlock current = world.getBlock(x, y, z);
  int meta = current.metadata;

  // Flow down
  ChunkBlock below = world.getBlock(x, y - 1, z);
  if (below.block->isReplaceable() && below.getType() != id) {
    if (meta == 0) { // Source flows down as falling liquid (meta 8 usually, but
                     // here we simplify)
      world.setBlock(x, y - 1, z, (BlockType)id);
      world.setMetadata(x, y - 1, z, 0); // Propagate source-like downwards
    } else {
      world.setBlock(x, y - 1, z, (BlockType)id);
      world.setMetadata(x, y - 1, z, 8); // Falling stream
    }
  }

  // Flow sideways if on solid ground
  if (below.block->isSolid() || below.getType() == id) {
    if (meta < 7) { // 7 is max decay in this simple model
      trySpread(world, x + 1, y, z, meta + 1);
      trySpread(world, x - 1, y, z, meta + 1);
      trySpread(world, x, y, z + 1, meta + 1);
      trySpread(world, x, y, z - 1, meta + 1);
    }
  }

  checkMixing(world, x, y, z);
}

void LiquidBlock::onPlace(World &world, int x, int y, int z) const {
  world.scheduleBlockUpdate(x, y, z, 5); // Schedule flow
}

void LiquidBlock::onNeighborChange(World &world, int x, int y, int z, int nx,
                                   int ny, int nz) const {
  world.scheduleBlockUpdate(x, y, z, 5);
}

void LiquidBlock::trySpread(World &world, int x, int y, int z,
                            int newMeta) const {
  ChunkBlock target = world.getBlock(x, y, z);
  if (target.block->isReplaceable() && target.getType() != id) {
    world.setBlock(x, y, z, (BlockType)id);
    world.setMetadata(x, y, z, newMeta);
  } else if (target.getType() == id && target.metadata > newMeta) {
    // Strengthen flow if new path is shorter
    world.setBlock(x, y, z, (BlockType)id);
    world.setMetadata(x, y, z, newMeta);
  }
}

void LiquidBlock::checkMixing(World &world, int x, int y, int z) const {
  // cobblestone generator logic, etc
}
