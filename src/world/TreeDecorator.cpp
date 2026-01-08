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
#include <random>
#include <vector>

static block_id WOOD = 0, LEAVES = 0, GRASS = 0, DIRT = 0, PODZOL = 0, MUD = 0,
                SAND = 0, GRAVEL = 0, COARSE_DIRT = 0, TERRA_PRETA = 0,
                PEAT = 0, CLAY = 0, CLAYSTONE = 0, SNOW = 0, SNOW_LAYER = 0;
static bool idsResolved = false;

static void resolveTreeIds() {
  if (idsResolved)
    return;
  auto &reg = BlockRegistry::getInstance();
  WOOD = reg.getBlockId("lithos:oak_log");
  LEAVES = reg.getBlockId("lithos:oak_leaves");
  GRASS = reg.getBlockId("lithos:grass");
  DIRT = reg.getBlockId("lithos:dirt");
  PODZOL = reg.getBlockId("lithos:podzol");
  MUD = reg.getBlockId("lithos:mud");
  SAND = reg.getBlockId("lithos:sand");
  GRAVEL = reg.getBlockId("gravel");
  COARSE_DIRT = reg.getBlockId("lithos:coarse_dirt");
  TERRA_PRETA = reg.getBlockId("lithos:terra_preta");
  PEAT = reg.getBlockId("lithos:peat");
  CLAY = reg.getBlockId("lithos:clay");
  CLAYSTONE = reg.getBlockId("lithos:rock_claystone");
  SNOW = reg.getBlockId("lithos:snow_block");
  SNOW_LAYER = reg.getBlockId("lithos:snow_layer");
  idsResolved = true;
}

// Helper for neighbor caching

void TreeDecorator::GenerateTree(WorldGenRegion *region, int x, int y, int z,
                                 const TreeStructure &tree, std::mt19937 &rng,
                                 const ChunkNeighborhood &hood,
                                 int targetChunkX, int targetChunkZ,
                                 WorldGenerator *generator) {
  // Determine World Limits safely
  int maxHeight = 320; // Default for benchmark
  if (region && region->getWorld()) {
    maxHeight = region->getWorld()->config.worldHeight;
  }

  // Use generator if passed, or try to get from world if possible (circular
  // dependency usually prevents this without casting) We prefer passing it
  // down.

  // Basic root position check
  if (y + tree.yOffset < 0 || y + tree.yOffset >= maxHeight)
    return;

  // Start with Trunks (Level 0)
  if (tree.trunks.empty()) {
    return;
  }

  // Initial State
  glm::vec3 treeOrigin(x, y + tree.yOffset, z);

  // Pick a trunk template
  if (tree.trunks.empty())
    return;
  std::uniform_int_distribution<int> trunkDist(0, tree.trunks.size() - 1);
  const TreeSegment &rootSeg = tree.trunks[trunkDist(rng)];

  // size = sizeMultiplier + sizeVar
  float baseSize = tree.sizeMultiplier;
  if (!tree.sizeVar.dist.empty() && tree.sizeVar.dist != "none") {
    baseSize += tree.sizeVar.Sample(rng);
  }
  float width = baseSize * rootSeg.widthMultiplier;

  float rootAngleVert = 0.0f;
  float rootAngleHori = 0.0f;

  if (!rootSeg.angleVert.dist.empty() && rootSeg.angleVert.dist != "none") {
    rootAngleVert = rootSeg.angleVert.Sample(rng);
    rootAngleHori =
        (!rootSeg.angleHori.dist.empty() && rootSeg.angleHori.dist != "none")
            ? rootSeg.angleHori.Sample(rng)
            : 0.0f;
  }

  float rootDx = rootSeg.dx;
  float rootDz = rootSeg.dz;

  int totalSegments = 0;
  BuildSegment(region, x, y, z, rootSeg, treeOrigin, rootDx, 0.0f, rootDz,
               rootAngleVert, rootAngleHori, width, 0.0f, 0, totalSegments,
               tree, rng, hood, targetChunkX, targetChunkZ, generator);
}

void TreeDecorator::BuildSegment(
    WorldGenRegion *region, int x, int y, int z, const TreeSegment &segment,
    glm::vec3 treeOrigin, float dx, float dy, float dz, float angleVerStart,
    float angleHorStart, float width, float progress, int depth,
    int &totalSegments, const TreeStructure &tree, std::mt19937 &rng,
    const ChunkNeighborhood &hood, int targetChunkX, int targetChunkZ,
    WorldGenerator *generator) {

  if (!region)
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

  World *world = region->getWorld();
  int maxHeight = world ? world->config.worldHeight : 320;

  float sizeMultiplier = tree.sizeMultiplier;
  if (sizeMultiplier <= 0.0f)
    sizeMultiplier = 1.0f;

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
  resolveTreeIds();
  // Resolve Block IDs - Optimized
  block_id logId = (block_id)tree.treeBlocks.resolvedLogBlockId;
  if (logId == AIR) {
    // Fallback
    logId = WOOD;
  }

  block_id leavesId = (block_id)tree.treeBlocks.resolvedLeavesBlockId;
  if (leavesId == AIR) {
    leavesId = LEAVES;
  }

  block_id branchyId = (block_id)tree.treeBlocks.resolvedLeavesBranchyBlockId;
  if (branchyId == AIR)
    branchyId = logId;

  // VS: Trunk segment block IDs
  const std::vector<block_id> &trunkSegmentIds =
      tree.treeBlocks.resolvedTrunkSegmentBlockIdList;

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
    block_id currentSegmentBlockId;
    if (segment.segment != 0 && curWidth >= 0.3f && !trunkSegmentIds.empty()) {
      int idx = segment.segment - 1;
      if (idx >= 0 && idx < (int)trunkSegmentIds.size()) {
        block_id tid = trunkSegmentIds[idx];
        currentSegmentBlockId = (tid != 0) ? (block_id)tid : logId;
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
    // 1. Place Log using world coordinates via region
    glm::ivec3 bPos = glm::vec3(currentPos);

    // Check if block is in target chunk
    // We only write blocks to the current chunk to ensure stateless generation
    int blockChunkX = (bPos.x >= 0) ? (bPos.x / CHUNK_SIZE)
                                    : ((bPos.x - CHUNK_SIZE + 1) / CHUNK_SIZE);
    int blockChunkZ = (bPos.z >= 0) ? (bPos.z / CHUNK_SIZE)
                                    : ((bPos.z - CHUNK_SIZE + 1) / CHUNK_SIZE);

    // PRE-PLACEMENT VALIDATION: Check if this location is valid BEFORE placing
    // This prevents orphaned leaf blocks when trees hit obstacles
    bool inTargetChunk =
        (blockChunkX == targetChunkX && blockChunkZ == targetChunkZ);

    if (bPos.y < 0 || bPos.y >= maxHeight) {
      alive = false;
      break;
    }

    // For any position, check if there's valid terrain support
    // Use actual block data for all chunks in the region (3x3 loaded)
    Block *checkBlock = region->getBlockPtr(bPos.x, bPos.y, bPos.z);
    if (!checkBlock) {
      alive = false;
      break;
    }

    block_id checkType = checkBlock->getId();

    // If we hit a solid, non-replaceable block that's not part of our tree,
    // stop
    if (checkBlock->isSolid() && !checkBlock->isReplaceable() &&
        checkType != currentSegmentBlockId && checkType != logId &&
        checkType != branchyId && checkType != leavesId) {
      alive = false;
      break;
    }

    // Additional validation: For leaf blocks, verify there's solid ground
    // nearby This prevents placing leaves over voids/cliffs
    if (currentSegmentBlockId == leavesId ||
        currentSegmentBlockId == branchyId) {
      // Check if there's solid ground within reasonable distance below
      bool hasSupport = false;
      int checkDist = std::min(10, bPos.y); // Check up to 10 blocks down

      for (int checkY = bPos.y - 1; checkY >= bPos.y - checkDist && checkY >= 0;
           checkY--) {
        Block *belowBlock = region->getBlockPtr(bPos.x, checkY, bPos.z);
        if (belowBlock && belowBlock->isSolid() &&
            !belowBlock->isReplaceable()) {
          // Found solid ground below
          if (belowBlock->getId() !=
              leavesId) { // Don't count other leaves as support
            hasSupport = true;
            break;
          }
        }
      }

      if (!hasSupport) {
        // No solid ground nearby, skip this leaf block
        continue; // Skip this iteration but keep the tree alive
      }
    }

    // Now actually place the block if we're in the target chunk
    if (inTargetChunk) {
      // Replace if air or replaceable
      if (checkBlock->isReplaceable() || checkType == AIR) {
        region->setBlock(bPos.x, bPos.y, bPos.z, currentSegmentBlockId);
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

      curWidth = GrowBranches(region, x, y, z, quantity, branchSeg, depth + 1,
                              curWidth, branchWidthMultiplierStart,
                              currentSequence, angleHor, dx, dy, dz, treeOrigin,
                              trunkOffsetX, trunkOffsetZ, totalSegments, tree,
                              rng, hood, targetChunkX, targetChunkZ, generator);
    }
  } // End while loop
}

float TreeDecorator::GrowBranches(
    WorldGenRegion *region, int x, int y, int z, int branchQuantity,
    const TreeSegment &branchSeg, int newDepth, float curWidth,
    float branchWidthMultiplierStart, float currentSequence, float angleHor,
    float dx, float dy, float dz, glm::vec3 treeOrigin, float trunkOffsetX,
    float trunkOffsetZ, int &totalSegments, const TreeStructure &tree,
    std::mt19937 &rng, const struct ChunkNeighborhood &hood, int targetChunkX,
    int targetChunkZ, WorldGenerator *generator) {

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
    // Recursive call
    BuildSegment(region, x, y, z, branchSeg, treeOrigin, dx + trunkOffsetX, dy,
                 dz + trunkOffsetZ, branchAngleVer, branchAngleHor, branchWidth,
                 0, newDepth, totalSegments, tree, rng, hood, targetChunkX,
                 targetChunkZ, generator);

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

// Chunk-based decoration (DEPRECATED - use region-based decoration)
// Legacy method kept for interface compatibility but does nothing
// All tree generation now uses region-based decoration for cross-chunk support
void TreeDecorator::Decorate(Chunk &chunk, WorldGenerator &generator,
                             const ChunkColumn &column) {
  // Intentionally empty - all decoration now happens via region-based Decorate
}

// Region-based decoration (cross-chunk tree generation)
void TreeDecorator::Decorate(WorldGenerator &generator, WorldGenRegion &region,
                             const ChunkColumn &column) {
  resolveTreeIds();
  PROFILE_SCOPE_CONDITIONAL("Decorator_Trees_Region",
                            generator.IsProfilingEnabled());

  World *world = region.getWorld();

  int targetX = region.getCenterX();
  int targetZ = region.getCenterZ();

  // To ensure seamless trees across chunk boundaries, we simulate tree
  // generation for the current chunk AND its neighbors. We only write blocks if
  // they fall into the current chunk (targetX, targetZ).

  int range = 1; // 1 chunk radius is usually enough for trees

  for (int ox = -range; ox <= range; ox++) {
    for (int oz = -range; oz <= range; oz++) {
      int cx = targetX + ox;
      int cz = targetZ + oz;

      int startX = cx * CHUNK_SIZE;
      int startZ = cz * CHUNK_SIZE;

      int seed = generator.GetSeed();
      std::mt19937 rng(seed + startX * 342 + startZ * 521);

      ChunkNeighborhood hood; // Dummy for now
      hood.world = world;

      const auto &config = TreeRegistry::Get().GetConfig();
      int attempts = (int)config.treesPerChunk.Sample(rng);
      attempts = attempts < 0 ? 0 : attempts;

      for (int i = 0; i < attempts; ++i) {
        int lx = std::uniform_int_distribution<int>(0, CHUNK_SIZE - 1)(rng);
        int lz = std::uniform_int_distribution<int>(0, CHUNK_SIZE - 1)(rng);

        int gx = startX + lx;
        int gz = startZ + lz;

        // Compute Climate Data on demand (since neighbors might not be
        // generated) Optimization: For current chunk (ox==0, oz==0), we could
        // use 'column'
        int height;
        float realTemp, rawRain, forest;

        if (ox == 0 && oz == 0) {
          height = column.getHeight(lx, lz);
          realTemp = column.temperatureMap[lx][lz];
          rawRain = column.humidityMap[lx][lz];
          forest = column.forestNoiseMap[lx][lz];
        } else {
          // For neighbors, we must compute from noise
          // (This is stateless and safe)
          height = generator.GetHeight(gx, gz);
          realTemp = generator.GetTemperature(gx, gz);
          rawRain = generator.GetHumidity(gx, gz);
          forest = generator.GetForestNoise(gx, gz);
        }

        if (height < generator.GetConfig().seaLevel)
          continue;

        // Surface block check:
        // We need to verify the surface block is actually solid.
        // Since caves are generated BEFORE decorators, we can check the actual
        // block rather than simulating cave generation.

        bool isSoil = true;
        block_id surfaceBlock = 0;

        // Get the surface block - this works for current chunk and neighbors in
        // 3x3 region
        surfaceBlock = region.getBlock(gx, height, gz);

        // Check if it's a valid soil type
        isSoil = (surfaceBlock == GRASS || surfaceBlock == DIRT ||
                  surfaceBlock == PODZOL || surfaceBlock == MUD ||
                  surfaceBlock == SAND || surfaceBlock == GRAVEL ||
                  surfaceBlock == COARSE_DIRT || surfaceBlock == TERRA_PRETA ||
                  surfaceBlock == PEAT || surfaceBlock == CLAY ||
                  surfaceBlock == CLAYSTONE || surfaceBlock == SNOW ||
                  surfaceBlock == SNOW_LAYER);

        if (!isSoil)
          continue;

        // Additional validation: verify it's actually solid (catches caves)
        // Only do this check if it doesn't interfere with neighbor chunks
        Block *surfaceBlockPtr = region.getBlockPtr(gx, height, gz);
        if (surfaceBlockPtr && !surfaceBlockPtr->isSolid()) {
          continue; // Surface was carved by cave or is otherwise not solid
        }

        float realRainNorm = (rawRain + 1.0f) * 0.5f;

        if (std::uniform_real_distribution<float>(0, 1)(rng) > forest)
          continue;

        const TreeGenerator *gen = TreeRegistry::Get().SelectTree(
            realTemp, realRainNorm, 100.0f, forest,
            (float)height / (float)generator.GetConfig().worldHeight, rng);

        if (gen) {
          const TreeStructure *structure =
              TreeRegistry::Get().GetTreeStructure(gen->generator);
          if (structure) {
            GenerateTree(&region, gx, height, gz, *structure, rng, hood,
                         targetX, targetZ, &generator);
          }
        }
      }
    }
  }
}
