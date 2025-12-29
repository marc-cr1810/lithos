#include "Logger.h"
#include <filesystem>
#include <spdlog/sinks/basic_file_sink.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <vector>

std::shared_ptr<spdlog::logger> Logger::s_MainLogger;
std::shared_ptr<spdlog::logger> Logger::s_RenderLogger;
std::shared_ptr<spdlog::logger> Logger::s_ResourceLogger;
std::shared_ptr<spdlog::logger> Logger::s_WorldLogger;
std::shared_ptr<spdlog::logger> Logger::s_PhysicsLogger;

void Logger::Init() {
  if (!std::filesystem::exists("logs")) {
    std::filesystem::create_directory("logs");
  }

  std::vector<spdlog::sink_ptr> logSinks;
  logSinks.emplace_back(
      std::make_shared<spdlog::sinks::stdout_color_sink_mt>());
  logSinks.emplace_back(std::make_shared<spdlog::sinks::basic_file_sink_mt>(
      "logs/Lithos.log", true));

  logSinks[0]->set_pattern("%^[%T] [%n] :%$ %v");
  logSinks[1]->set_pattern("[%T] [%l] [%n] : %v");

  s_MainLogger =
      std::make_shared<spdlog::logger>("MAIN", begin(logSinks), end(logSinks));
  spdlog::register_logger(s_MainLogger);
  s_MainLogger->set_level(spdlog::level::info);
  s_MainLogger->flush_on(spdlog::level::trace);

  s_RenderLogger = std::make_shared<spdlog::logger>("RENDER", begin(logSinks),
                                                    end(logSinks));
  spdlog::register_logger(s_RenderLogger);
  s_RenderLogger->set_level(spdlog::level::info);
  s_RenderLogger->flush_on(spdlog::level::info);

  s_ResourceLogger = std::make_shared<spdlog::logger>(
      "RESOURCE", begin(logSinks), end(logSinks));
  spdlog::register_logger(s_ResourceLogger);
  s_ResourceLogger->set_level(spdlog::level::info);
  s_ResourceLogger->flush_on(spdlog::level::info);

  s_WorldLogger =
      std::make_shared<spdlog::logger>("WORLD", begin(logSinks), end(logSinks));
  spdlog::register_logger(s_WorldLogger);
  s_WorldLogger->set_level(spdlog::level::info);
  s_WorldLogger->flush_on(spdlog::level::info);

  s_PhysicsLogger = std::make_shared<spdlog::logger>("PHYSICS", begin(logSinks),
                                                     end(logSinks));
  spdlog::register_logger(s_PhysicsLogger);
  s_PhysicsLogger->set_level(spdlog::level::info);
  s_PhysicsLogger->flush_on(spdlog::level::info);
}
