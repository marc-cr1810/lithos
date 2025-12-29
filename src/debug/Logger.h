#pragma once

#include <memory>

// This ignores all warnings raised inside External headers
#pragma warning(push, 0)
#include <spdlog/fmt/ostr.h>
#include <spdlog/spdlog.h>

#pragma warning(pop)

class Logger {
public:
  static void Init();

  inline static std::shared_ptr<spdlog::logger> &GetMainLogger() {
    return s_MainLogger;
  }
  inline static std::shared_ptr<spdlog::logger> &GetRenderLogger() {
    return s_RenderLogger;
  }
  inline static std::shared_ptr<spdlog::logger> &GetResourceLogger() {
    return s_ResourceLogger;
  }
  inline static std::shared_ptr<spdlog::logger> &GetWorldLogger() {
    return s_WorldLogger;
  }
  inline static std::shared_ptr<spdlog::logger> &GetPhysicsLogger() {
    return s_PhysicsLogger;
  }

private:
  static std::shared_ptr<spdlog::logger> s_MainLogger;
  static std::shared_ptr<spdlog::logger> s_RenderLogger;
  static std::shared_ptr<spdlog::logger> s_ResourceLogger;
  static std::shared_ptr<spdlog::logger> s_WorldLogger;
  static std::shared_ptr<spdlog::logger> s_PhysicsLogger;
};

// Main Logger Macros
#define LOG_TRACE(...) ::Logger::GetMainLogger()->trace(__VA_ARGS__)
#define LOG_INFO(...) ::Logger::GetMainLogger()->info(__VA_ARGS__)
#define LOG_WARN(...) ::Logger::GetMainLogger()->warn(__VA_ARGS__)
#define LOG_ERROR(...) ::Logger::GetMainLogger()->error(__VA_ARGS__)
#define LOG_CRITICAL(...) ::Logger::GetMainLogger()->critical(__VA_ARGS__)

// Render Logger Macros
#define LOG_RENDER_TRACE(...) ::Logger::GetRenderLogger()->trace(__VA_ARGS__)
#define LOG_RENDER_INFO(...) ::Logger::GetRenderLogger()->info(__VA_ARGS__)
#define LOG_RENDER_WARN(...) ::Logger::GetRenderLogger()->warn(__VA_ARGS__)
#define LOG_RENDER_ERROR(...) ::Logger::GetRenderLogger()->error(__VA_ARGS__)
#define LOG_RENDER_CRITICAL(...)                                               \
  ::Logger::GetRenderLogger()->critical(__VA_ARGS__)

// Resource Logger Macros
#define LOG_RESOURCE_TRACE(...)                                                \
  ::Logger::GetResourceLogger()->trace(__VA_ARGS__)
#define LOG_RESOURCE_INFO(...) ::Logger::GetResourceLogger()->info(__VA_ARGS__)
#define LOG_RESOURCE_WARN(...) ::Logger::GetResourceLogger()->warn(__VA_ARGS__)
#define LOG_RESOURCE_ERROR(...)                                                \
  ::Logger::GetResourceLogger()->error(__VA_ARGS__)
#define LOG_RESOURCE_CRITICAL(...)                                             \
  ::Logger::GetResourceLogger()->critical(__VA_ARGS__)

// World Logger Macros
#define LOG_WORLD_TRACE(...) ::Logger::GetWorldLogger()->trace(__VA_ARGS__)
#define LOG_WORLD_INFO(...) ::Logger::GetWorldLogger()->info(__VA_ARGS__)
#define LOG_WORLD_WARN(...) ::Logger::GetWorldLogger()->warn(__VA_ARGS__)
#define LOG_WORLD_ERROR(...) ::Logger::GetWorldLogger()->error(__VA_ARGS__)
#define LOG_WORLD_CRITICAL(...)                                                \
  ::Logger::GetWorldLogger()->critical(__VA_ARGS__)

// Physics Logger Macros
#define LOG_PHYSICS_TRACE(...) ::Logger::GetPhysicsLogger()->trace(__VA_ARGS__)
#define LOG_PHYSICS_INFO(...) ::Logger::GetPhysicsLogger()->info(__VA_ARGS__)
#define LOG_PHYSICS_WARN(...) ::Logger::GetPhysicsLogger()->warn(__VA_ARGS__)
#define LOG_PHYSICS_ERROR(...) ::Logger::GetPhysicsLogger()->error(__VA_ARGS__)
#define LOG_PHYSICS_CRITICAL(...)                                              \
  ::Logger::GetPhysicsLogger()->critical(__VA_ARGS__)
