#pragma once

#include <chrono>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

struct ProfileResult {
  std::string Name;
  long long Start;
  long long End;
  uint32_t ThreadID;
};

struct InstrumentationSession {
  std::string Name;
};

class Profiler {
public:
  Profiler(const Profiler &) = delete;
  Profiler(Profiler &&) = delete;

  void BeginSession(const std::string &name,
                    const std::string &filepath = "results.json");
  void EndSession();

  void WriteProfile(const ProfileResult &result);

  static Profiler &Get() {
    static Profiler instance;
    return instance;
  }

  // For runtime UI
  std::unordered_map<std::string, std::vector<float>> &GetResults() {
    return m_Results;
  }
  void ClearResults() { m_Results.clear(); }

private:
  Profiler();
  ~Profiler();

  InstrumentationSession *m_CurrentSession;
  std::mutex m_Lock;
  std::unordered_map<std::string, std::vector<float>>
      m_Results; // Name -> History
};

class ProfileTimer {
public:
  ProfileTimer(const char *name);
  ~ProfileTimer();

  void Stop();

private:
  const char *m_Name;
  std::chrono::time_point<std::chrono::high_resolution_clock> m_StartTimepoint;
  bool m_Stopped;
};

#if 1 // Enable Profiling
#define PROFILE_SCOPE(name) ProfileTimer timer##__LINE__(name)
#define PROFILE_FUNCTION() PROFILE_SCOPE(__FUNCSIG__)
#else
#define PROFILE_SCOPE(name)
#define PROFILE_FUNCTION()
#endif
