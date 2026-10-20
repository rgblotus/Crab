#include "crab/application.hpp"
#include "crab/core/log.hpp"
#include "crab/core/version.hpp"

#include <charconv>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>

namespace {

struct Options {
  crab::ApplicationConfig application;
  bool showHelp = false;
  bool showVersion = false;
  bool verbose = false;
  bool framesSpecified = false;
};

bool parseUnsigned(std::string_view text, std::uint64_t& value) {
  const char* begin = text.data();
  const char* end = begin + text.size();
  const auto result = std::from_chars(begin, end, value);
  return result.ec == std::errc{} && result.ptr == end;
}

bool parsePositiveInt(std::string_view text, int& value) {
  std::uint64_t parsed = 0;
  if (!parseUnsigned(text, parsed) || parsed == 0 || parsed > 100'000) {
    return false;
  }
  value = static_cast<int>(parsed);
  return true;
}

void printUsage(std::string_view program) {
  std::cout
    << "Crab " << crab::core::kEngineVersion << "\n\n"
    << "Usage: " << program << " [options]\n\n"
    << "Options:\n"
    << "  -h, --help              Show this help\n"
    << "      --version           Print the engine version\n"
    << "      --headless          Run without creating a window\n"
    << "      --no-vulkan         Do not request a Vulkan window (diagnostics)\n"
    << "      --frames N          Stop after N frames (0 = unlimited; headless needs N > 0)\n"
    << "      --width N           Window width in logical pixels (default: 1280)\n"
    << "      --height N          Window height in logical pixels (default: 720)\n"
    << "      --title TEXT        Window title (default: Crab)\n"
    << "  -v, --verbose           Enable debug logging\n";
}

bool requireValue(int argc, char** argv, int& index, std::string_view option,
                  std::string_view& value) {
  if (index + 1 >= argc) {
    std::cerr << "Crab: missing value for " << option << "\n";
    return false;
  }
  value = argv[++index];
  return true;
}

} // namespace

int main(int argc, char** argv) {
  Options options;

  for (int index = 1; index < argc; ++index) {
    const std::string_view argument = argv[index];
    std::string_view value;

    if (argument == "-h" || argument == "--help") {
      options.showHelp = true;
    } else if (argument == "--version") {
      options.showVersion = true;
    } else if (argument == "--headless") {
      options.application.headless = true;
    } else if (argument == "--no-vulkan") {
      options.application.vulkan = false;
    } else if (argument == "-v" || argument == "--verbose") {
      options.verbose = true;
    } else if (argument == "--frames") {
      if (!requireValue(argc, argv, index, argument, value) ||
          !parseUnsigned(value, options.application.maxFrames)) {
        std::cerr << "Crab: --frames expects a non-negative integer\n";
        return EXIT_FAILURE;
      }
      options.framesSpecified = true;
    } else if (argument == "--width") {
      if (!requireValue(argc, argv, index, argument, value) ||
          !parsePositiveInt(value, options.application.width)) {
        std::cerr << "Crab: --width expects a positive integer\n";
        return EXIT_FAILURE;
      }
    } else if (argument == "--height") {
      if (!requireValue(argc, argv, index, argument, value) ||
          !parsePositiveInt(value, options.application.height)) {
        std::cerr << "Crab: --height expects a positive integer\n";
        return EXIT_FAILURE;
      }
    } else if (argument == "--title") {
      if (!requireValue(argc, argv, index, argument, value)) {
        return EXIT_FAILURE;
      }
      options.application.title = std::string(value);
    } else {
      std::cerr << "Crab: unknown option '" << argument << "'\n";
      printUsage(argv[0]);
      return EXIT_FAILURE;
    }
  }

  if (options.showVersion) {
    std::cout << crab::core::kEngineName << " " << crab::core::kEngineVersion << " ("
              << crab::core::kBuildType << ", " << crab::core::kCompiler << ")\n";
    return EXIT_SUCCESS;
  }

  if (options.showHelp) {
    printUsage(argv[0]);
    return EXIT_SUCCESS;
  }

  if (options.application.headless && !options.framesSpecified) {
    options.application.maxFrames = 5;
  }
  if (options.application.headless && options.application.maxFrames == 0) {
    std::cerr
      << "Crab: --frames 0 would run forever in headless mode; use a positive frame count\n";
    return EXIT_FAILURE;
  }

  crab::core::LogConfig logConfig;
  if (options.verbose) {
    logConfig.level = crab::core::LogLevel::debug;
  }
  crab::core::initializeLogging(logConfig);

  int exitCode = EXIT_SUCCESS;
  {
    crab::Application application(std::move(options.application));
    if (!application.init()) {
      exitCode = EXIT_FAILURE;
    } else {
      exitCode = application.run();
    }
  }
  crab::core::shutdownLogging();

  return exitCode;
}
