#ifndef TREE_DECORATOR_H
#define TREE_DECORATOR_H

#include "WorldDecorator.h"

#include "decorators/TreeGenConfig.h"
#include <random>
#include <unordered_map>

class TreeDecorator : public WorldDecorator {
public:
  virtual void Decorate(Chunk &chunk, WorldGenerator &generator,
                        const struct ChunkColumn &column) override;

private:
  struct ChunkNeighborhood {
    Chunk *chunks[3][3];
    World *world;
  };

  // Build the selected tree
  void GenerateTree(Chunk &chunk, int x, int y, int z,
                    const TreeStructure &tree, std::mt19937 &rng,
                    const struct ChunkNeighborhood &hood);

  void BuildSegment(Chunk *chunk, int x, int y, int z,
                    const TreeSegment &segment, glm::vec3 treeOrigin, float dx,
                    float dy, float dz, float angleVertStart,
                    float angleHorStart, float width, float progress, int depth,
                    int &totalSegments, const TreeStructure &tree,
                    std::mt19937 &rng, const struct ChunkNeighborhood &hood);

  float GrowBranches(Chunk *chunk, int x, int y, int z, int branchQuantity,
                     const TreeSegment &branchSeg, int newDepth, float curWidth,
                     float branchWidthMultiplierStart, float currentSequence,
                     float angleHor, float dx, float dy, float dz,
                     glm::vec3 treeOrigin, float trunkOffsetX,
                     float trunkOffsetZ, int &totalSegments,
                     const TreeStructure &tree, std::mt19937 &rng,
                     const struct ChunkNeighborhood &hood);
};

#endif
