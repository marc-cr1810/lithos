#pragma once

#include "ChunkMeshData.h"
#include "core/ThreadSafeQueue.h"
#include <atomic>
#include <thread>


namespace Lithos {

/**
 * Worker thread for generating chunk meshes.
 * Runs in background, doesn't touch GPU.
 */
class ChunkMeshWorker {
public:
  ChunkMeshWorker();
  ~ChunkMeshWorker();

  // Non-copyable
  ChunkMeshWorker(const ChunkMeshWorker &) = delete;
  ChunkMeshWorker &operator=(const ChunkMeshWorker &) = delete;

  /**
   * Start the worker thread.
   */
  void start();

  /**
   * Stop the worker thread (blocks until finished).
   */
  void stop();

  /**
   * Queue a chunk for meshing.
   */
  void queueRequest(ChunkMeshRequest request);

  /**
   * Try to get a completed mesh (non-blocking).
   */
  std::optional<ChunkMeshResult> tryGetResult();

  /**
   * Check if any results are available.
   */
  bool hasResults() const { return !resultQueue.empty(); }

  /**
   * Get number of pending requests.
   */
  size_t getPendingCount() const { return requestQueue.size(); }

private:
  ThreadSafeQueue<ChunkMeshRequest> requestQueue;
  ThreadSafeQueue<ChunkMeshResult> resultQueue;

  std::thread workerThread;
  std::atomic<bool> running{false};

  /**
   * Worker thread main loop.
   */
  void workerLoop();
};

} // namespace Lithos
