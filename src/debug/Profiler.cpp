#include "Profiler.h"
#include <thread>

Profiler::Profiler() : m_CurrentSession(nullptr) {}

Profiler::~Profiler() { EndSession(); }

void Profiler::BeginSession(const std::string &name,
                            const std::string &filepath) {}

void Profiler::EndSession() {}

void Profiler::WriteProfile(const ProfileResult &result) {
  std::lock_guard<std::mutex> lock(m_Lock);

  // Store latest result for UI
  float duration =
      (result.End - result.Start) * 0.001f; // Microseconds to Milliseconds

  auto &history = m_Results[result.Name];
  history.push_back(duration);
  if (history.size() > 100) {
    history.erase(history.begin());
  }
}

ProfileTimer::ProfileTimer(const char *name, bool active)
    : m_Name(name), m_Stopped(false), m_Active(active) {
  if (m_Active)
    m_StartTimepoint = std::chrono::high_resolution_clock::now();
}

ProfileTimer::~ProfileTimer() {
  if (!m_Stopped && m_Active)
    Stop();
}

void ProfileTimer::Stop() {
  if (!m_Active)
    return;
  auto endTimepoint = std::chrono::high_resolution_clock::now();

  long long start =
      std::chrono::time_point_cast<std::chrono::microseconds>(m_StartTimepoint)
          .time_since_epoch()
          .count();
  long long end =
      std::chrono::time_point_cast<std::chrono::microseconds>(endTimepoint)
          .time_since_epoch()
          .count();

  uint32_t threadID =
      (uint32_t)std::hash<std::thread::id>{}(std::this_thread::get_id());
  Profiler::Get().WriteProfile({m_Name, start, end, threadID});

  m_Stopped = true;
}
