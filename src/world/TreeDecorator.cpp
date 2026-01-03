#include "TreeDecorator.h"
#include "../debug/Logger.h"
#include "../debug/Profiler.h"
#include "Block.h"
#include "ChunkColumn.h"
#include "World.h"
#include "WorldGenRegion.h"
#include "WorldGenerator.h"
#include "decorators/TreeRegistry.h"
#include <algorithm>
#include <cstdlib>
#include <fstream>
#include <glm/glm.hpp>
#include <iostream>
#include <vector>

// Helper for neighbor caching

void TreeDecorator::GenerateTree(Chunk &chunk, int x, int y, int z,
                                 const TreeStructure &tree, std::mt19937 &rng,
                                 const ChunkNeighborhood &hood) {
  // CRASH GUARD: Verify World pointer
  // Determine World Limits safely
  int maxHeight = 320; // Default for benchmark
  if (chunk.getWorld()) {
    maxHeight = chunk.getWorld()->config.worldHeight;
  }

  // Basic root position check
  if (y + tree.yOffset < 0 || y + tree.yOffset >= maxHeight)
    return;

  // Start with Trunks (Level 0)
  if (tree.trunks.empty()) {
    // LOG_WARN("GenerateTree: No trunks defined for tree.");
    return;
  }

  // Initial State
  // VS: Origin is integer coordinate (or whatever was passed), offsets handle
  // +0.5
  glm::vec3 treeOrigin(x, y + tree.yOffset, z);

  // Pick a trunk template
  if (tree.trunks.empty())
    return;
  std::uniform_int_distribution<int> trunkDist(0, tree.trunks.size() - 1);
  const TreeSegment &rootSeg = tree.trunks[trunkDist(rng)];

  // VS: Calculate initial trunk width
  // size = sizeMultiplier + sizeVar
  float baseSize = tree.sizeMultiplier;
  if (!tree.sizeVar.dist.empty() && tree.sizeVar.dist != "none") {
    baseSize += tree.sizeVar.Sample(rng);
  }
  float width = baseSize * rootSeg.widthMultiplier;

  // VS: Use trunk's own angles if specified, otherwise default upward
  float rootAngleVert = 0.0f;
  float rootAngleHori = 0.0f;

  // VS: Use trunk's own angles if specified, otherwise default upward
  if (!rootSeg.angleVert.dist.empty() && rootSeg.angleVert.dist != "none") {
    rootAngleVert = rootSeg.angleVert.Sample(rng);
    rootAngleHori =
        (!rootSeg.angleHori.dist.empty() && rootSeg.angleHori.dist != "none")
            ? rootSeg.angleHori.Sample(rng)
            : 0.0f;
  }

  // VS: dx/dz from root segment
  float rootDx = rootSeg.dx;
  float rootDz = rootSeg.dz;

  int totalSegments = 0;
  BuildSegment(&chunk, x, y, z, rootSeg, treeOrigin, rootDx, 0.0f, rootDz,
               rootAngleVert, rootAngleHori, width, 0.0f, 0, totalSegments,
               tree, rng, hood);
}

void TreeDecorator::BuildSegment(Chunk *chunk, int x, int y, int z,
                                 const TreeSegment &segment,
                                 glm::vec3 treeOrigin, float dx, float dy,
                                 float dz, float angleVerStart,
                                 float angleHorStart, float width,
                                 float progress, int depth, int &totalSegments,
                                 const TreeStructure &tree, std::mt19937 &rng,
                                 const ChunkNeighborhood &hood) {

  if (!chunk)
    return;

  // VS: Prevent infinite recursion
  if (depth > 30)
    return;

  // EMERGENCY: Per-tree iteration counter to prevent infinite loops
  if (totalSegments > 5000) {
    LOG_ERROR("TreeDecorator: Emergency segment limit reached (5000) for tree "
              "at {},{},{}",
              treeOrigin.x, treeOrigin.y, treeOrigin.z);
    return;
  }
  totalSegments++;

  World *world = chunk->getWorld();
  int maxHeight = world ? world->config.worldHeight : 320;

  float sizeMultiplier = tree.sizeMultiplier;
  if (sizeMultiplier <= 0.0f)
    sizeMultiplier = 1.0f;

  glm::ivec3 chunkOrigin = chunk->chunkPosition * 32;

  // VS: Initialize deltas from base position (NOT direction vector!)
  // Note: angles are now passed in, matching VS logic

  // VS: Initialize deltas from base position (NOT direction vector!)
  // VS: Initialize deltas - ALREADY PASSED IN

  // Determine Max Length based on Width Loss (VS Logic)
  // VS: sequencesPerIteration = 1f / (curWidth / widthloss)
  // So total length in blocks approx width / widthLoss
  float calculatedMaxLen =
      (segment.widthLoss > 0.0001f) ? (width / segment.widthLoss) : 200.0f;
  // Clamp to reasonable bounds to prevent infinite loops or tiny segments
  if (calculatedMaxLen > 200)
    calculatedMaxLen = 200;
  if (calculatedMaxLen < 2)
    calculatedMaxLen = 2;

  float totaldistance = calculatedMaxLen; // VS uses curWidth / widthloss

  // Initialize branch spawning state
  float lastRelDistance = 0.0f;
  float nextBranchDistance = segment.branchStart.Sample(rng);
  float currentSpacing = segment.branchSpacing.Sample(rng);

  float branchQuantityStart = segment.branchQuantity.Sample(rng);
  float branchWidthMultiplierStart = segment.branchWidthMultiplier.Sample(rng);

  // VS: Use randomWidthLoss if specified, otherwise base widthLoss
  float widthloss = (!segment.randomWidthLoss.dist.empty() &&
                     segment.randomWidthLoss.dist != "none")
                        ? segment.randomWidthLoss.Sample(rng)
                        : segment.widthLoss;

  // VS: If widthloss is essentially zero, segment can't progress - exit early
  if (widthloss < 0.000001f) {
    return; // Can't make progress
  }

  float curWidth = width; // Track current width separately

  // VS: Sample dieAt threshold ONCE (not every iteration!)
  float dieAtThreshold = segment.dieAt.Sample(rng);

  // Offset logic handled by passed dx/dy/dz now

  // Resolve Block IDs
  Block *logBlock =
      BlockRegistry::getInstance().getBlock(tree.treeBlocks.logBlockCode);
  if (!logBlock || logBlock->getId() == AIR)
    logBlock = BlockRegistry::getInstance().getBlock(WOOD);
  if (!logBlock)
    logBlock = BlockRegistry::getInstance().getBlock(AIR);

  Block *leavesBlock =
      BlockRegistry::getInstance().getBlock(tree.treeBlocks.leavesBlockCode);
  if (!leavesBlock || leavesBlock->getId() == AIR)
    leavesBlock = BlockRegistry::getInstance().getBlock(LEAVES);
  if (!leavesBlock)
    leavesBlock = BlockRegistry::getInstance().getBlock(AIR);

  BlockType logId = static_cast<BlockType>(logBlock->getId());
  BlockType leavesId = static_cast<BlockType>(leavesBlock->getId());

  // Resolve Branchy Leaves for structure
  Block *branchyBlock = BlockRegistry::getInstance().getBlock(
      tree.treeBlocks.leavesBranchyBlockCode);
  BlockType branchyId = (branchyBlock && branchyBlock->getId() != AIR)
                            ? (BlockType)branchyBlock->getId()
                            : logId;

  // VS: Build trunk segment block IDs (for multi-textured trunks)
  std::vector<BlockType> trunkSegmentBlockIds;
  if (!tree.treeBlocks.trunkSegmentBase.empty() &&
      !tree.treeBlocks.trunkSegmentVariants.empty()) {
    for (const std::string &variant : tree.treeBlocks.trunkSegmentVariants) {
      std::string blockCode =
          tree.treeBlocks.trunkSegmentBase + variant + "-ud";
      Block *segBlock = BlockRegistry::getInstance().getBlock(blockCode);
      if (segBlock && segBlock->getId() != AIR) {
        trunkSegmentBlockIds.push_back((BlockType)segBlock->getId());
      } else {
        trunkSegmentBlockIds.push_back(logId); // Fallback
      }
    }
  }

  bool alive = true;

  int iteration = 0;
  float sequencesPerIteration = 1.0f / (curWidth / widthloss);

  float currentSequence;
  float angleVer, angleHor;
  float ddrag;
  float sinAngleVer, cosAngleHor, sinAngleHor;
  float trunkOffsetX, trunkOffsetZ;

  while (curWidth > 0 && iteration++ < 5000) {
    if (iteration >= 4999) {
      LOG_WARN("BuildSegment: Iteration {} reached at depth {}, curWidth={}, "
               "widthloss={}",
               iteration, depth, curWidth, widthloss);
    }
    curWidth -= widthloss;

    // VS widthlossCurve dampening - critical for proper taper
    if (segment.widthlossCurve + curWidth / 20.0f < 1.0f) {
      widthloss *= (segment.widthlossCurve + curWidth / 20.0f);
    }

    // VS: If widthloss becomes too small, segment can't progress - stop
    if (widthloss < 0.000001f) {
      break;
    }

    currentSequence = sequencesPerIteration * (iteration - 1);

    if (curWidth < dieAtThreshold)
      break;

    // VS: Evolve angles each iteration
    angleVer = segment.angleVertEvolve.Apply(angleVerStart, currentSequence);
    angleHor = segment.angleHoriEvolve.Apply(angleHorStart, currentSequence);

    sinAngleVer = std::sin(angleVer);
    cosAngleHor = std::cos(angleHor);
    sinAngleHor = std::sin(angleHor);

    // VS: Trunk offset for branch spawning
    trunkOffsetX = std::clamp(0.7f * sinAngleVer * cosAngleHor, -0.5f, 0.5f);
    trunkOffsetZ = std::clamp(0.7f * sinAngleVer * sinAngleHor, -0.5f, 0.5f);

    // VS: Gravity drag based on horizontal distance
    ddrag = segment.gravityDrag * std::sqrt(dx * dx + dz * dz);

    // VS: Update deltas (NOT direction vector!)
    dx += sinAngleVer * cosAngleHor / std::max(1.0f, std::abs(ddrag));
    dy += std::min(1.0f, std::max(-1.0f, std::cos(angleVer) - ddrag));
    dz += sinAngleVer * sinAngleHor / std::max(1.0f, std::abs(ddrag));

    // VS: Determine Block ID based on Width
    BlockType currentSegmentBlockId;
    if (segment.segment != 0 && curWidth >= 0.3f &&
        !trunkSegmentBlockIds.empty()) {
      int idx = segment.segment - 1;
      if (idx >= 0 && idx < (int)trunkSegmentBlockIds.size()) {
        currentSegmentBlockId = trunkSegmentBlockIds[idx];
      } else {
        currentSegmentBlockId = logId;
      }
    } else if (segment.NoLogs || curWidth <= 0.3f) {
      // Use leaf gradient
      if (curWidth > 0.1f) {
        currentSegmentBlockId = branchyId;
      } else {
        currentSegmentBlockId = leavesId;
      }
    } else {
      // Normal log
      currentSegmentBlockId = logId;
    }

    // VS: Position = treeOrigin + deltas
    glm::vec3 currentPos(treeOrigin.x + dx, treeOrigin.y + dy,
                         treeOrigin.z + dz);
    // 1. Place Log
    glm::ivec3 bPos = glm::vec3(currentPos);
    if (bPos.y >= 0 && bPos.y < maxHeight) {
      int lx = bPos.x - chunkOrigin.x;
      int ly = bPos.y - chunkOrigin.y;
      int lz = bPos.z - chunkOrigin.z;

      // Bounds Check: confine to local chunk
      if (lx >= 0 && lx < 32 && lz >= 0 && lz < 32) {
        BlockType currentType =
            (BlockType)chunk->getBlock(lx, ly, lz).getType();
        Block *currentBlock =
            BlockRegistry::getInstance().getBlock(currentType);

        if (currentBlock->isSolid() && !currentBlock->isReplaceable() &&
            currentType != currentSegmentBlockId && currentType != logId &&
            currentType != branchyId && currentType != leavesId) {
          alive = false;
          break;
        }

        // Replace if air or replaceable
        if (currentBlock->isReplaceable() || currentType == AIR) {
          chunk->setBlock(lx, ly, lz, currentSegmentBlockId);
        }
      } else {
        // Neighbor placement disabled
      }
    }

    // VS: Calculate relative distance from deltas
    float reldistance = std::sqrt(dx * dx + dy * dy + dz * dz) / totaldistance;

    // VS: Branch spawning check
    if (reldistance < nextBranchDistance)
      continue;

    if (depth < 3 && !tree.branches.empty() &&
        reldistance > lastRelDistance + currentSpacing * (1.0f - reldistance)) {
      currentSpacing = segment.branchSpacing.Sample(rng);
      lastRelDistance = reldistance;

      // VS: Evolve branch quantity
      float branchQuantity = segment.branchQuantityEvolve.Apply(
          branchQuantityStart, currentSequence);
      int quantity = (int)(branchQuantity + 0.5f);
      if (quantity < 0)
        quantity = 0;

      int branchIdx = std::min(depth, (int)tree.branches.size() - 1);
      const TreeSegment &branchSeg = tree.branches[branchIdx];

      curWidth =
          GrowBranches(chunk, x, y, z, quantity, branchSeg, depth + 1, curWidth,
                       branchWidthMultiplierStart, currentSequence, angleHor,
                       dx, dy, dz, treeOrigin, trunkOffsetX, trunkOffsetZ,
                       totalSegments, tree, rng, hood);
    }
  } // End while loop
}

float TreeDecorator::GrowBranches(
    Chunk *chunk, int x, int y, int z, int branchQuantity,
    const TreeSegment &branchSeg, int newDepth, float curWidth,
    float branchWidthMultiplierStart, float currentSequence, float angleHor,
    float dx, float dy, float dz, glm::vec3 treeOrigin, float trunkOffsetX,
    float trunkOffsetZ, int &totalSegments, const TreeStructure &tree,
    std::mt19937 &rng, const struct ChunkNeighborhood &hood) {

  float branchWidth;
  float prevHorAngle = 0.0f;
  float horAngle;
  // VS: Minimum angle distance to prevent clumping (PI/5 or var/5)
  float minHorangleDist =
      std::min(3.14159265f / 5.0f, branchSeg.branchHorizontalAngle.var / 5.0f);
  bool first = true;

  while (branchQuantity-- > 0) {
    // VS: Reduce parent trunk width for each branch
    curWidth *= branchSeg.branchWidthLossMul;

    // VS: Relative horizontal angle (relative to parent)
    horAngle = angleHor + branchSeg.branchHorizontalAngle.Sample(rng);

    // VS: Try to find a good angle 10 times
    int tries = 10;
    while (!first && std::abs(horAngle - prevHorAngle) < minHorangleDist &&
           tries-- > 0) {
      float newAngle = angleHor + branchSeg.branchHorizontalAngle.Sample(rng);
      if (std::abs(horAngle - prevHorAngle) <
          std::abs(newAngle - prevHorAngle)) {
        horAngle = newAngle;
      }
    }

    // VS: Branch Width
    // Check if evolution exists (simplified check here, assuming always
    // non-null in our struct if used)
    if (branchSeg.branchWidthMultiplierEvolve.transform !=
        MathUtils::TransformType::NONE) {
      branchWidth = curWidth * branchSeg.branchWidthMultiplierEvolve.Apply(
                                   branchWidthMultiplierStart, currentSequence);
    } else {
      // VS logic is intricate here, often just uses sample
      branchWidth = curWidth * branchSeg.branchWidthMultiplier.Sample(rng);
    }

    // Branch angles
    float branchAngleVer = branchSeg.branchVerticalAngle.Sample(rng);
    // Use the calculated horAngle (relative + spread)
    float branchAngleHor = horAngle;

    // Recursive call
    BuildSegment(chunk, x, y, z, branchSeg, treeOrigin, dx + trunkOffsetX, dy,
                 dz + trunkOffsetZ, branchAngleVer, branchAngleHor, branchWidth,
                 0, newDepth, totalSegments, tree, rng, hood);

    first = false;
    prevHorAngle = angleHor + horAngle; // VS: accumulates? No, wait.
    // VS: prevHorAngle = angleHor + horAngle;
    // Actually in VS code:
    // prevHorAngle = angleHor + horAngle; -> Wait, `int tries` loop checks
    // `horAngle` vs `prevHorAngle`. The `horAngle` calculated inside the loop
    // is `angleHor + random`. So `horAngle` IS the absolute angle (parent +
    // relative). So `prevHorAngle` should store the absolute angle of the last
    // branch. In VS code: `prevHorAngle = angleHor + horAngle;` -> wait,
    // `horAngle` was effectively `relative`. VS: `horAngle = angleHor + ...` ->
    // `horAngle` is absolute. VS line 227: `prevHorAngle = angleHor +
    // horAngle;` -> This looks like double addition if `horAngle` is already
    // absolute. Let's re-read VS code CAREFULLY. Line 193: `horAngle = angleHor
    // + ...` (Absolute) Line 227: `prevHorAngle = angleHor + horAngle;` ?? If
    // horAngle is absolute, why add angleHor again? Ah, wait. VS `horAngle`
    // variable might be reusing the name or I misread. Line 193: `horAngle =
    // angleHor + branch.branchHorizontalAngle.nextFloat(...)` So `horAngle` is
    // ABSOLUTE. Line 227: `prevHorAngle = angleHor + horAngle` -> This sets
    // prev to (Parent + Absolute). This implies `prevHorAngle` is stored in
    // some weird space or it's a bug in VS or I'm misinterpreting "angleHor".
    // If `angleHor` is 0, then prev = hor.
    // If `angleHor` is PI, prev = PI + (PI + rel) = 2PI + rel.
    // The check `Math.Abs(horAngle - prevHorAngle)` would be `Abs((PI+rel) -
    // (2PI+prevRel))`. This seems ... Odd. BUT: "The user's main objective is
    // to ... aligning with the Vintagestory (VS) implementation." I should copy
    // logic exactly even if it looks weird, unless it's strictly impossible.
    // HOWEVER: `prevHorAngle` is initialized to 0.
    // If I look at the loop:
    // 1st iter: horAngle = P + R1. prev = P + (P + R1) = 2P + R1.
    // 2nd iter: newHor = P + R2. check Abs(newHor - prev) = Abs(P+R2 - (2P+R1))
    // = Abs(R2 - P - R1). This effectively checks distance but shifts by P. If
    // correct, I will copy it. LET'S LOOK AT VS CODE AGAIN from the view. Line
    // 184: `float prevHorAngle = 0f;` Line 193: `horAngle = angleHor + ...`
    // Line 227: `prevHorAngle = angleHor + horAngle;`
    // YES. It is doing that. I will replicate it to be safe.

    // Actually, wait. In VS line 227: `prevHorAngle = angleHor + horAngle;`
    // But `horAngle` variable in implementation:
    // Line 193: `horAngle = angleHor + ...`
    // Re-reading closely.
    // Maybe `horAngle` in line 227 is being interpreted as relative?
    // No, `horAngle` is local variable.
    // I will replicate EXACTLY.
    prevHorAngle = angleHor + horAngle;
  }

  return curWidth;
}

// ======================================================================
// Public API - Decorator Interface
// ======================================================================

void TreeDecorator::Decorate(Chunk &chunk, WorldGenerator &generator,
                             const ChunkColumn &column) {
  PROFILE_SCOPE_CONDITIONAL("Decorator_Trees", generator.IsProfilingEnabled());

  glm::ivec3 cp = chunk.chunkPosition;

  // Cache Neighbors (One-time Global Lock)
  ChunkNeighborhood hood;
  hood.world = chunk.getWorld();
  hood.chunks[1][1] = &chunk;

  if (hood.world) {
    // We need to look up neighbors safely.
    // Use GenerationWorkerLoop context if
    // possible, but here we only have
    // generator. We can use
    // world->getChunk, which locks
    // worldMutex. Doing it 8 times starts
    // to be expensive, but less than 1000
    // times. BUT: getChunk is
    // std::shared_ptr return. We need raw
    // pointers for the cache (careful with
    // lifetime). Since we are in
    // Generation, and chunks persist until
    // Unload (main thread), and we hold
    // shared_ptrs in the World map... It
    // should be safe-ish if we hold
    // shared_ptrs locally?

    // Optimization: Lock ONCE and get
    // all 8. World::getChunk locks
    // individually. We can add a
    // World::getNeighbors(int x, int y, int
    // z, OutputArray) helper? Or just call
    // getChunk. 8 locks is infinitely
    // better than 1000 locks.

    // Optimization: Neighbor lookup
    // disabled for stability.
    // hood.world->getNeighbors(cp.x, cp.y,
    // cp.z, hood.chunks);
    hood.chunks[1][1] = &chunk;

  } else {
    // Benchmark mode: no neighbors
    for (int i = 0; i < 3; ++i)
      for (int j = 0; j < 3; ++j)
        if (i != 1 || j != 1)
          hood.chunks[i][j] = nullptr;
  }

  int seed = generator.GetSeed();

  int startX = cp.x * CHUNK_SIZE;
  int startZ = cp.z * CHUNK_SIZE;

  std::mt19937 rng(seed + startX * 342 + startZ * 521); // Distinct seed per
                                                        // chunk column

  // Try X attempts from Global Config
  const auto &config = TreeRegistry::Get().GetConfig();
  int attempts = (int)config.treesPerChunk.Sample(rng);
  attempts = attempts < 0 ? 0 : attempts;

  for (int i = 0; i < attempts; ++i) {
    int lx = std::uniform_int_distribution<int>(0, CHUNK_SIZE - 1)(rng);
    int lz = std::uniform_int_distribution<int>(0, CHUNK_SIZE - 1)(rng);

    int gx = startX + lx;
    int gz = startZ + lz;
    int height = column.getHeight(lx, lz);

    // Check bounds and sea level
    if (height < generator.GetConfig().seaLevel)
      continue;

    int cyStart = cp.y * CHUNK_SIZE;
    int cyEnd = cyStart + CHUNK_SIZE;

    // If the base of the tree is in this
    // chunk (or close enough that we should
    // start it)
    if (height >= cyStart && height < cyEnd) {
      // Check block
      BlockType surfaceBlock = AIR;
      int ly = height - cyStart;
      if (ly >= 0 && ly < CHUNK_SIZE) {
        surfaceBlock = (BlockType)chunk.getBlock(lx, ly, lz).getType();
      }

      // Updated Soil Check with extensive
      // list
      bool isSoil = (surfaceBlock == GRASS || surfaceBlock == DIRT ||
                     surfaceBlock == PODZOL || surfaceBlock == MUD ||
                     surfaceBlock == SAND || surfaceBlock == GRAVEL ||
                     surfaceBlock == COARSE_DIRT ||
                     surfaceBlock == TERRA_PRETA || surfaceBlock == PEAT ||
                     surfaceBlock == CLAY || surfaceBlock == CLAYSTONE ||
                     surfaceBlock == SNOW || surfaceBlock == SNOW_LAYER);

      if (!isSoil) {
        continue;
      }

      // Climate
      float realTemp = column.temperatureMap[lx][lz];
      float rawRain = column.humidityMap[lx][lz];
      float realRain = (rawRain + 1.0f) * 0.5f;
      float forest = column.forestNoiseMap[lx][lz]; // 0..1

      // Density scaling: Use forest value
      // as probability of placement
      if (std::uniform_real_distribution<float>(0, 1)(rng) > forest) {
        continue;
      }

      const TreeGenerator *gen = TreeRegistry::Get().SelectTree(
          realTemp, realRain, 100.0f, forest, (float)height / 256.0f, rng);
      if (gen) {
        const TreeStructure *structure =
            TreeRegistry::Get().GetTreeStructure(gen->generator);
        if (structure) {
          // VS: Tree generation starts FROM the passed coordinate, growing
          // upwards. So we pass the ground block (height), not the air block.
          // The algorithm typically steps y+1 for the first segment.
          GenerateTree(chunk, gx, height, gz, *structure, rng, hood);
        } else {
          LOG_ERROR("TreeFail: Structure not found for {}", gen->generator);
        }
      } else {
        // Debug log for failure (throttled)
        static int failCount = 0;
        if (failCount++ < 10) {
          // LOG_INFO("TreeFail: SelectTree returned null. T:{} R:{} F:{} H:{}",
          //          realTemp, realRain, forest, height);
        }
      }
    }
  }
}

// Region-based decoration (delegates to chunk-based for now)
void TreeDecorator::Decorate(WorldGenerator &generator, WorldGenRegion &region,
                             const ChunkColumn &column) {
  PROFILE_SCOPE_CONDITIONAL("Decorator_Trees_Region",
                            generator.IsProfilingEnabled());

  World *world = region.getWorld();
  int colX = region.getCenterX();
  int colZ = region.getCenterZ();

  for (int y = 0; y < 8; y++) {
    auto chunk = world->getChunk(colX, y, colZ);
    if (chunk) {
      Decorate(*chunk, generator, column);
    }
  }
}
