#include "StairBlock.h"
#include "../BlockRegistrar.h"

static BlockRegistrar registrar("StairBlock",
                                [](block_id id, const std::string &name) {
                                  return new StairBlock(id, name);
                                });

StairBlock::StairBlock(block_id id, const std::string &name) : Block(id, name) {
  // Base Solidity for Unrotated Stair (Metadata 0, Facing Z+?)
  // Let's assume Metadata 0 = North (Back of stair is North) or similar.
  // If standard: Back is Solid. Top has no block.
  // Bottom: Solid (5)
  // Top: Empty (4)
  // Front: Empty (Half)
  // Back: Solid (Full)
  // Sides: Empty (Triangle)

  // Default SideSolid to false except strict ones
  setSideSolid(false);
  setSideSolid(5, true); // Bottom always solid
  // Note: We'll refine this in isSideSolid override which ignores the array
  // effectively, or uses it as base.
  // Actually, let's just use the logic in isSideSolid entirely.
}

bool StairBlock::isOpaque() const { return false; }
bool StairBlock::isSolid() const { return true; }
Block::RenderShape StairBlock::getRenderShape() const {
  return RenderShape::MODEL; // Changed to MODEL to use JSON model
}

bool StairBlock::isSideSolid(int face, int metadata) const {
  // 0=Front(Z+), 1=Back(Z-), 2=Left(X-), 3=Right(X+), 4=Top(Y+), 5=Bottom(Y-)

  // Vertical faces always false for standard stairs?
  // Except bottom (5) is always true.
  if (face == 5)
    return true;
  if (face == 4)
    return false; // Top is half-step, so not a full solid face.

  // Horizontal faces depend on rotation.
  // Let's assume Metadata:
  // 0: Back is North (Z-) -> Face 1 is Solid
  // 1: Back is East (X+) -> Face 3 is Solid
  // 2: Back is South (Z+) -> Face 0 is Solid
  // 3: Back is West (X-) -> Face 2 is Solid

  int solidFace = -1;
  switch (metadata % 4) {
  case 0:
    solidFace = 1;
    break; // Z-
  case 1:
    solidFace = 3;
    break; // X+
  case 2:
    solidFace = 0;
    break; // Z+
  case 3:
    solidFace = 2;
    break; // X-
  }

  return face == solidFace;
}
