#include "LeavesBlock.h"
#include "../Block.h"
#include "../BlockRegistrar.h"

static BlockRegistrar registrar("LeavesBlock",
                                [](uint8_t id, const std::string &name) {
                                  return new LeavesBlock(id, name);
                                });

LeavesBlock::LeavesBlock(uint8_t id, const std::string &name)
    : PlantBlock(id, name) {
  // Leaves are solid (spawnable/collidable usually) but use cutout rendering
  isSolid_ = true;
  // renderLayer is inherited as CUTOUT from PlantBlock default, which is
  // correct
}
