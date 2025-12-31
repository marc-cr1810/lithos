#ifndef BENCHMARK_H
#define BENCHMARK_H

#include "../world/WorldGenConfig.h"
#include <map>
#include <string>

#include <atomic>
#include <mutex>

struct BenchmarkResult {
  float totalTimeMs;
  float avgChunkTimeMs;
  int chunksGenerated;
  std::map<std::string, float> stepAvgTimes;
};

struct BenchmarkStatus {
  std::atomic<bool> isRunning{false};
  std::atomic<float> progress{0.0f}; // 0.0 to 1.0
  bool isFinished{false}; // Set by main thread after polling isRunning=false
  BenchmarkResult result;
  std::mutex resultMutex;
};

// Returns synchronous result (blocking)
BenchmarkResult RunWorldGenBenchmark(const WorldGenConfig &config,
                                     int sideSize);

// Starts benchmark in a detached thread
void StartBenchmarkAsync(const WorldGenConfig &config, int sideSize);
BenchmarkStatus &GetBenchmarkStatus();

#endif // BENCHMARK_H
