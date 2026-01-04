#include "Benchmark.h"
#include "../world/Chunk.h"
#include "../world/ChunkColumn.h"
#include "../world/World.h"
#include "../world/WorldGenRegion.h"
#include "../world/WorldGenerator.h"
#include "Logger.h"
#include "Profiler.h"
#include <chrono>
#include <mutex>  // Added for std::mutex
#include <thread> // Added for std::thread

static BenchmarkStatus s_Status;

BenchmarkStatus &GetBenchmarkStatus() { return s_Status; }

// Core benchmark logic - shared by both sync and async versions
static void RunBenchmarkCore(World &benchmarkWorld, WorldGenerator &generator,
                             const WorldGenConfig &config, int sideSize,
                             BenchmarkResult &result,
                             std::atomic<float> *progress = nullptr) {
  auto start = std::chrono::high_resolution_clock::now();
  int count = 0;
  int totalColumns = sideSize * sideSize;
  int processedCols = 0;

  // Store generated chunks to pass back
  std::vector<std::shared_ptr<Chunk>> generatedChunks;
  generatedChunks.reserve(totalColumns * (config.worldHeight / CHUNK_SIZE));

  for (int cx = 0; cx < sideSize; ++cx) {
    for (int cz = 0; cz < sideSize; ++cz) {
      ChunkColumn column;
      generator.GenerateColumn(column, cx, cz);

      // Store column in world for region access
      {
        std::lock_guard<std::mutex> lock(benchmarkWorld.columnMutex);
        benchmarkWorld.columns[{cx, cz}] =
            std::make_unique<ChunkColumn>(column);
      }

      int chunksY = config.worldHeight / CHUNK_SIZE;
      for (int cy = 0; cy < chunksY; ++cy) {
        auto c = std::make_shared<Chunk>();
        c->chunkPosition = glm::ivec3(cx, cy, cz);
        c->setWorld(&benchmarkWorld);
        generator.GenerateChunk(*c, column);

        // Insert chunk into world for region access
        benchmarkWorld.insertChunk(c);

        generatedChunks.push_back(c);
        count++;
      }

      // Decoration step (now has access to chunks via benchmarkWorld)
      WorldGenRegion region(&benchmarkWorld, cx, cz);
      generator.Decorate(region, column);

      processedCols++;
      if (progress) {
        *progress = (float)processedCols / (float)totalColumns;
      }
    }
  }

  auto end = std::chrono::high_resolution_clock::now();
  std::chrono::duration<float, std::milli> duration = end - start;

  result.totalTimeMs = duration.count();
  result.chunksGenerated = count;
  result.generatedChunks = generatedChunks;
  result.avgChunkTimeMs = (count > 0) ? (result.totalTimeMs / count) : 0.0f;

  // Gather granular data from Profiler
  auto &profilerResults = Profiler::Get().GetResults();
  for (const auto &kv : profilerResults) {
    const std::string &name = kv.first;
    const std::vector<float> &history = kv.second;

    float sum = 0.0f;
    for (float v : history)
      sum += v;

    if (!history.empty()) {
      result.stepAvgTimes[name] = sum / (float)history.size();
    }
  }
}

void StartBenchmarkAsync(const WorldGenConfig &config, int sideSize) {
  if (s_Status.isRunning)
    return; // Prevent multiple runs

  s_Status.isRunning = true;
  s_Status.isFinished = false;
  s_Status.progress = 0.0f;

  std::thread([config, sideSize]() {
    auto result = std::make_unique<BenchmarkResult>();

    // Clear previous profiling data
    Profiler::Get().ClearResults();

    WorldGenerator generator(config);
    generator.EnableProfiling(true);
    if (config.fixedWorld) {
      generator.GenerateFixedMaps();
    }

    // Create World on heap so we can transfer ownership
    auto benchmarkWorld = std::make_unique<World>(config);

    // Run core benchmark logic with progress reporting
    RunBenchmarkCore(*benchmarkWorld, generator, config, sideSize, *result,
                     &s_Status.progress);

    // Transfer World ownership to result for preview reuse
    result->benchmarkWorld = std::move(benchmarkWorld);

    std::lock_guard<std::mutex> lock(s_Status.resultMutex);
    s_Status.result = std::move(result);
    s_Status.isFinished = true;
    s_Status.isRunning = false;
  }).detach();
}

// Keep existing synchronous function
BenchmarkResult RunWorldGenBenchmark(const WorldGenConfig &config,
                                     int sideSize) {
  BenchmarkResult result = {0};

  // Clear previous profiling data
  Profiler::Get().ClearResults();

  WorldGenerator generator(config);
  if (config.fixedWorld) {
    generator.GenerateFixedMaps();
  }

  // Create minimal World for region-based decoration to work
  World benchmarkWorld(config, true); // Silent mode for benchmarks

  // Run core benchmark logic
  RunBenchmarkCore(benchmarkWorld, generator, config, sideSize, result);

  return result;
}
