#include "Benchmark.h"
#include "../world/Chunk.h"
#include "../world/ChunkColumn.h"
#include "../world/WorldGenerator.h"
#include "Logger.h"
#include "Profiler.h"
#include <chrono>
#include <mutex>  // Added for std::mutex
#include <thread> // Added for std::thread

static BenchmarkStatus s_Status;

BenchmarkStatus &GetBenchmarkStatus() { return s_Status; }

void StartBenchmarkAsync(const WorldGenConfig &config, int radius) {
  if (s_Status.isRunning)
    return; // Prevent multiple runs

  s_Status.isRunning = true;
  s_Status.isFinished = false;
  s_Status.progress = 0.0f;

  std::thread([config, radius]() {
    // Run logic similar to RunWorldGenBenchmark but updating progress
    BenchmarkResult result = {0};

    // Clear previous profiling data
    // Note: Profiler usually uses thread-local storage or needs thread safety.
    // Profiler::Get() singleton might need care. Assuming it is thread-safe
    // enough or we use it here. Actually, ProfileTimer uses `Profiler::Get()`
    // which returns a singleton. `WriteProfile` has a mutex. Safe.
    Profiler::Get().ClearResults();

    WorldGenerator generator(config);
    generator.EnableProfiling(true);
    if (config.fixedWorld) {
      generator.GenerateFixedMaps();
    }

    auto start = std::chrono::high_resolution_clock::now();
    int count = 0;
    int totalColumns = (radius * 2 + 1) * (radius * 2 + 1);
    int processedCols = 0;

    for (int cx = -radius; cx <= radius; ++cx) {
      for (int cz = -radius; cz <= radius; ++cz) {
        ChunkColumn column;
        generator.GenerateColumn(column, cx, cz);

        int chunksY = config.worldHeight / CHUNK_SIZE;
        for (int cy = 0; cy < chunksY; ++cy) {
          Chunk c;
          c.chunkPosition = glm::ivec3(cx, cy, cz);
          c.setWorld(nullptr);
          generator.GenerateChunk(c, column);
          count++;
        }

        processedCols++;
        s_Status.progress = (float)processedCols / totalColumns;
      }
    }

    auto end = std::chrono::high_resolution_clock::now();
    std::chrono::duration<float, std::milli> duration = end - start;

    result.totalTimeMs = duration.count();
    result.chunksGenerated = count;
    result.avgChunkTimeMs = (count > 0) ? (result.totalTimeMs / count) : 0.0f;

    // Gather granular data from Profiler
    auto &profilerResults = Profiler::Get().GetResults();
    std::lock_guard<std::mutex> lock(s_Status.resultMutex);

    for (const auto &kv : profilerResults) {
      const std::string &name = kv.first;
      const std::vector<float> &history = kv.second;

      float sum = 0.0f;
      for (float v : history)
        sum += v;

      if (!history.empty()) {
        result.stepAvgTimes[name] = sum / history.size();
      }
    }

    s_Status.result = result;
    s_Status.isFinished = true;
    s_Status.isRunning = false;
  }).detach();
}

// Keep existing synchronous function as a wrapper or just leave it for
// legacy/testing
BenchmarkResult RunWorldGenBenchmark(const WorldGenConfig &config, int radius) {
  BenchmarkResult result = {0};

  // Clear previous profiling data
  Profiler::Get().ClearResults();

  WorldGenerator generator(config);
  if (config.fixedWorld) {
    generator.GenerateFixedMaps();
  }

  auto start = std::chrono::high_resolution_clock::now();
  int count = 0;

  for (int cx = -radius; cx <= radius; ++cx) {
    for (int cz = -radius; cz <= radius; ++cz) {
      ChunkColumn column;
      generator.GenerateColumn(column, cx, cz);

      int chunksY = config.worldHeight / CHUNK_SIZE;
      for (int cy = 0; cy < chunksY; ++cy) {
        Chunk c;
        c.chunkPosition = glm::ivec3(cx, cy, cz);
        c.setWorld(nullptr);
        generator.GenerateChunk(c, column);
        count++;
      }
    }
  }

  auto end = std::chrono::high_resolution_clock::now();
  std::chrono::duration<float, std::milli> duration = end - start;

  result.totalTimeMs = duration.count();
  result.chunksGenerated = count;
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
      result.stepAvgTimes[name] = sum / history.size();
    }
  }

  return result;
}
