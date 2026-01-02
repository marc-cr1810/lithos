#include <argparse/argparse.hpp>
#include <ctime>
#include <iostream>
#include <random>

#include "core/Application.h"
#include "debug/CrashHandler.h"
#include "debug/Logger.h"

int main(int argc, char *argv[]) {
  srand((unsigned int)time(NULL));
  // Argument Parsing
  argparse::ArgumentParser program("Lithos");

  program.add_argument("--width")
      .help("Window width")
      .default_value(1280)
      .scan<'i', int>();

  program.add_argument("--height")
      .help("Window height")
      .default_value(720)
      .scan<'i', int>();

  program.add_argument("--vsync")
      .help("Enable Vertical Sync")
      .default_value(false)
      .implicit_value(true);

  program.add_argument("--render-distance")
      .help("Chunk render distance")
      .default_value(8)
      .scan<'i', int>();

  program.add_argument("--fov")
      .help("Camera Field of View")
      .default_value(45.0f)
      .scan<'g', float>();

  program.add_argument("--seed").help("World generation seed").scan<'i', int>();

  try {
    program.parse_args(argc, argv);
  } catch (const std::runtime_error &err) {
    std::cerr << err.what() << std::endl;
    std::cerr << program;
    return 1;
  }

  // Init Core Systems
  Logger::Init();
  CrashHandler::Init();

  // Config
  AppConfig config;
  config.width = program.get<int>("--width");
  config.height = program.get<int>("--height");
  config.vsync = program.get<bool>("--vsync");
  config.renderDistance = program.get<int>("--render-distance");
  config.fov = program.get<float>("--fov");

  // Seed Handling
  if (program.present<int>("--seed")) {
    config.seed = program.get<int>("--seed");
  } else {
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> distr(0, 100000);
    config.seed = distr(gen);
  }

  LOG_INFO("Starting Lithos Engine... Seed: {}", config.seed);

  // Application Lifecycle
  Application app(config);
  app.Run();

  return 0;
}
