#pragma once

#include <condition_variable>
#include <mutex>
#include <optional>
#include <queue>


namespace Lithos {

/**
 * Thread-safe queue for passing data between threads.
 * Used for chunk mesh requests and results.
 */
template <typename T> class ThreadSafeQueue {
public:
  ThreadSafeQueue() = default;
  ~ThreadSafeQueue() = default;

  // Non-copyable
  ThreadSafeQueue(const ThreadSafeQueue &) = delete;
  ThreadSafeQueue &operator=(const ThreadSafeQueue &) = delete;

  /**
   * Push item to queue (thread-safe).
   * Wakes up one waiting thread.
   */
  void push(T item) {
    {
      std::lock_guard<std::mutex> lock(mutex);
      queue.push(std::move(item));
    }
    cv.notify_one();
  }

  /**
   * Try to pop item (non-blocking).
   * Returns empty optional if queue is empty.
   */
  std::optional<T> tryPop() {
    std::lock_guard<std::mutex> lock(mutex);
    if (queue.empty()) {
      return std::nullopt;
    }
    T item = std::move(queue.front());
    queue.pop();
    return item;
  }

  /**
   * Pop item (blocking).
   * Waits until an item is available.
   */
  T pop() {
    std::unique_lock<std::mutex> lock(mutex);
    cv.wait(lock, [this] { return !queue.empty() || shouldStop; });

    if (shouldStop && queue.empty()) {
      return T{}; // Return default-constructed item
    }

    T item = std::move(queue.front());
    queue.pop();
    return item;
  }

  /**
   * Check if queue is empty (snapshot).
   */
  bool empty() const {
    std::lock_guard<std::mutex> lock(mutex);
    return queue.empty();
  }

  /**
   * Get queue size (snapshot).
   */
  size_t size() const {
    std::lock_guard<std::mutex> lock(mutex);
    return queue.size();
  }

  /**
   * Signal queue to stop (for clean shutdown).
   */
  void stop() {
    {
      std::lock_guard<std::mutex> lock(mutex);
      shouldStop = true;
    }
    cv.notify_all();
  }

private:
  mutable std::mutex mutex;
  std::condition_variable cv;
  std::queue<T> queue;
  bool shouldStop = false;
};

} // namespace Lithos
