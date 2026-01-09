#include "BlockBehaviorPillar.h"
#include "../Block.h"
#include "BlockBehaviorRegistrar.h"
#include <string>

static BlockBehaviorRegistrar registrar("Pillar", [](Block *block) {
  return std::make_shared<BlockBehaviorPillar>(block);
});

BlockBehaviorPillar::BlockBehaviorPillar(Block *block) : BlockBehavior(block) {}

block_id BlockBehaviorPillar::getPlacedBlockID(
    World &world, int x, int y, int z, const glm::vec3 &playerPos,
    const glm::vec3 &playerHeading, int clickedFace, block_id currentId) const {

  // Use member 'block' from base class
  std::string baseName = block->getResourceId();

  // Determine the axis state based on the clicked face
  std::string state = "ud"; // Default to Up/Down

  if (clickedFace == 4 || clickedFace == 5) {
    state = "ud";
  } else if (clickedFace == 0 || clickedFace == 1) {
    state = "ns";
  } else if (clickedFace == 2 || clickedFace == 3) {
    state = "we";
  }

  // Construct the new variant name
  // Strip existing suffix and append the new one.
  std::string newName = baseName;

  if (newName.length() > 3) {
    std::string suffix = newName.substr(newName.length() - 3);
    if (suffix == "_ud" || suffix == "_ns" || suffix == "_we") {
      newName = newName.substr(0, newName.length() - 3);
    }
  }

  newName += "_" + state;

  return BlockRegistry::getInstance().getBlockId(newName);
}
