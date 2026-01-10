#include "ChunkMeshWorker.h"
#include "Chunk.h"

namespace Lithos {

ChunkMeshWorker::ChunkMeshWorker() {}

ChunkMeshWorker::~ChunkMeshWorker() { stop(); }

void ChunkMeshWorker::start() {
  if (running.exchange(true)) {
    return; // Already running
  }

  workerThread = std::thread(&ChunkMeshWorker::workerLoop, this);
}

void ChunkMeshWorker::stop() {
  if (!running.exchange(false)) {
    return; // Already stopped
  }

  // Signal queues to stop
  requestQueue.stop();
  resultQueue.stop();

  // Wait for thread to finish
  if (workerThread.joinable()) {
    workerThread.join();
  }
}

void ChunkMeshWorker::queueRequest(ChunkMeshRequest request) {
  requestQueue.push(std::move(request));
}

std::optional<ChunkMeshResult> ChunkMeshWorker::tryGetResult() {
  return resultQueue.tryPop();
}

void ChunkMeshWorker::workerLoop() {
  while (running) {
    // Get next request (blocking)
    ChunkMeshRequest request = requestQueue.pop();

    // Check if we should stop
    if (!running)
      break;

    // Validate request
    if (!request.chunk) {
      continue;
    }

    // Generate mesh (CPU-side only, no GPU calls!)
    ChunkMeshResult result;
    result.chunkPos = request.chunkPos;

    // TODO: Call chunk->generateMeshData() once we separate it
    // For now, just mark as failure
    result.success = false;

    // Push result
    resultQueue.push(std::move(result));
  }
}

} // namespace Lithos
