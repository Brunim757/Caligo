#include "Application.h"

#include <algorithm>
#include <atomic>
#include <caligo/core/JobSystem.h>
#include <caligo/core/Log.h>
#include <caligo/core/Result.h>
#include <caligo/core/Time.h>
#include <caligo/editor/Editor.h>
#include <caligo/platform/File.h>
#include <caligo/platform/System.h>
#include <caligo/platform/Window.h>
#include <caligo/scene/Scene.h>
#include <caligo/script/ScriptHost.h>
#include <charconv>
#include <cmath>
#include <cstdio>
#include <filesystem>
#include <format>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace caligo {

namespace {

struct Options {
  bool headless = false;
  bool scriptSmoke = false;
  bool help = false;
  uint32_t frames = 0;
  LogLevel logLevel = LogLevel::Info;
  std::filesystem::path scenePath;
};

std::string PathText(const std::filesystem::path& path) {
  const std::u8string text = path.u8string();
  return std::string(reinterpret_cast<const char*>(text.data()), text.size());
}

uint32_t ScaledDimension(uint32_t value, float scale) {
  return static_cast<uint32_t>(static_cast<float>(value) * scale + 0.5f);
}

std::optional<uint32_t> ParsePositiveInteger(std::string_view text) {
  uint32_t value = 0;
  const char* begin = text.data();
  const char* end = text.data() + text.size();
  const std::from_chars_result result = std::from_chars(begin, end, value);
  if (result.ec != std::errc{} || result.ptr != end || value == 0) {
    return std::nullopt;
  }
  return value;
}

std::optional<LogLevel> ParseLogLevel(std::string_view text) {
  if (text == "trace") {
    return LogLevel::Trace;
  }
  if (text == "debug") {
    return LogLevel::Debug;
  }
  if (text == "info") {
    return LogLevel::Info;
  }
  if (text == "warn") {
    return LogLevel::Warn;
  }
  if (text == "error") {
    return LogLevel::Error;
  }
  return std::nullopt;
}

Result<Options> ParseOptions(int argc, char** argv) {
  Options options;
  for (int index = 1; index < argc; ++index) {
    const std::string_view argument = argv[index];
    if (argument == "--headless") {
      options.headless = true;
      continue;
    }
    if (argument == "--script-smoke") {
      options.scriptSmoke = true;
      continue;
    }
    if (argument == "--help" || argument == "-h") {
      options.help = true;
      continue;
    }
    if (argument == "--frames") {
      ++index;
      if (index >= argc) {
        return Error{"--frames requires a value", 1};
      }
      const std::optional<uint32_t> frames = ParsePositiveInteger(argv[index]);
      if (!frames.has_value()) {
        return Error{std::format("invalid frame count: {}", argv[index]), 2};
      }
      options.frames = *frames;
      continue;
    }
    if (argument == "--log-level") {
      ++index;
      if (index >= argc) {
        return Error{"--log-level requires a value", 3};
      }
      const std::optional<LogLevel> level = ParseLogLevel(argv[index]);
      if (!level.has_value()) {
        return Error{std::format("invalid log level: {}", argv[index]), 4};
      }
      options.logLevel = *level;
      continue;
    }
    if (argument == "--scene") {
      ++index;
      if (index >= argc) {
        return Error{"--scene requires a path", 6};
      }
      options.scenePath = argv[index];
      continue;
    }
    return Error{std::format("unknown argument: {}", argument), 5};
  }
  return options;
}

void PrintUsage() {
  std::printf("Caligo\n\n");
  std::printf("Usage: caligo [options]\n\n");
  std::printf("  --headless           Run without a window\n");
  std::printf("  --script-smoke       Validate the managed script runtime\n");
  std::printf("  --frames <count>     Stop after the given number of frames\n");
  std::printf("  --log-level <level>  trace, debug, info, warn, error\n");
  std::printf("  --scene <path>       Open a scene in the editor\n");
  std::printf("  --help               Show this message\n");
}

int RunScriptSmoke() {
  const Result<std::filesystem::path> executable = platform::ExecutableDirectory();
  if (!executable) {
    CALIGO_LOG_ERROR("script smoke could not locate the executable directory: {}",
                     executable.GetError().message);
    return 1;
  }

  script::ScriptHost host;
  const Result<void> initialized = host.Initialize(executable.Value());
  if (!initialized) {
    CALIGO_LOG_ERROR("script smoke could not start the script host: {}",
                     initialized.GetError().message);
    return 2;
  }

  const Result<std::vector<std::string>> types = host.ScriptTypes();
  if (!types) {
    CALIGO_LOG_ERROR("script smoke could not list script types: {}", types.GetError().message);
    host.Shutdown();
    return 3;
  }
  const std::vector<std::string>& names = types.Value();
  const auto hasType = [&names](std::string_view name) {
    return std::find(names.begin(), names.end(), name) != names.end();
  };
  if (!hasType("Spinner") || !hasType("PlayerMover")) {
    CALIGO_LOG_ERROR("script smoke did not find the expected gameplay scripts");
    host.Shutdown();
    return 4;
  }

  scene::Scene scene;
  const scene::EntityId spinner = scene.CreateEntity("Spinner");
  const scene::EntityId mover = scene.CreateEntity("Mover");
  scene::Transform moverTransform;
  moverTransform.position = math::Vec3{0.0f, 2.5f, 0.0f};
  if (spinner == scene::kNoEntity || mover == scene::kNoEntity ||
      !scene.SetTransform(mover, moverTransform) || !scene.AddScript(spinner, "Spinner") ||
      !scene.AddScript(mover, "PlayerMover")) {
    CALIGO_LOG_ERROR("script smoke could not prepare the scene");
    host.Shutdown();
    return 5;
  }

  const Result<void> started = host.BeginPlay(scene);
  if (!started) {
    CALIGO_LOG_ERROR("script smoke could not begin play: {}", started.GetError().message);
    host.Shutdown();
    return 6;
  }

  script::ScriptInput input;
  input.down[static_cast<size_t>(script::ScriptKey::W)] = true;
  host.SetInput(&input);

  constexpr float kFixedDelta = 1.0f / 60.0f;
  constexpr int kTicks = 60;
  for (int tick = 0; tick < kTicks; ++tick) {
    const Result<void> updated = host.Tick(kFixedDelta);
    if (!updated) {
      CALIGO_LOG_ERROR("script smoke failed at tick {}: {}", tick, updated.GetError().message);
      host.EndPlay();
      host.SetInput(nullptr);
      host.Shutdown();
      return 7;
    }
  }

  host.EndPlay();
  host.SetInput(nullptr);
  host.Shutdown();

  const scene::Entity* spinned = scene.Find(spinner);
  const scene::Entity* moved = scene.Find(mover);
  if (spinned == nullptr || moved == nullptr) {
    CALIGO_LOG_ERROR("script smoke lost its entities");
    return 8;
  }
  if (std::abs(spinned->transform.rotation.y) < 0.5f) {
    CALIGO_LOG_ERROR("script smoke did not observe the spinner rotation");
    return 9;
  }
  if (std::abs(moved->transform.position.z - 4.0f) > 0.05f) {
    CALIGO_LOG_ERROR("script smoke observed an unexpected player position: {}",
                     moved->transform.position.z);
    return 10;
  }
  if (std::abs(moved->transform.position.y - 2.5f) > 0.001f) {
    CALIGO_LOG_ERROR("script smoke changed the player height");
    return 11;
  }

  CALIGO_LOG_INFO("script smoke complete: {} types, {} ticks", names.size(), kTicks);
  return 0;
}

int RunHeadless(JobSystem& jobs, uint32_t frames) {
  constexpr uint32_t kWorkItems = 1024;
  constexpr uint32_t kMinRange = 64;
  constexpr uint64_t kExpectedPerFrame =
      static_cast<uint64_t>(kWorkItems - 1) * static_cast<uint64_t>(kWorkItems) / 2;

  FrameTimer timer;
  std::atomic<uint64_t> checksum{0};
  uint64_t ticks = 0;

  for (uint32_t frame = 0; frame < frames; ++frame) {
    const FrameStep step = timer.Advance(timer.FixedDelta());
    ticks += step.steps;
    checksum.store(0, std::memory_order_relaxed);
    jobs.ParallelFor(kWorkItems, kMinRange, [&checksum](uint32_t begin, uint32_t end) {
      uint64_t local = 0;
      for (uint32_t index = begin; index < end; ++index) {
        local += index;
      }
      checksum.fetch_add(local, std::memory_order_relaxed);
    });
    if (checksum.load(std::memory_order_relaxed) != kExpectedPerFrame) {
      CALIGO_LOG_ERROR("headless checksum failed at frame {}", frame);
      return 1;
    }
  }

  if (ticks != static_cast<uint64_t>(frames)) {
    CALIGO_LOG_ERROR("headless tick count mismatch: expected {}, got {}", frames, ticks);
    return 2;
  }

  CALIGO_LOG_INFO("headless run complete: {} frames, {} ticks", frames, ticks);
  return 0;
}

int RunWindowed(const Options& options) {
  platform::Window window;
  const float dpiScale = platform::SystemDpiScale();
  const Result<void> created =
      window.Create(platform::WindowDesc{.title = "Caligo Editor",
                                         .width = ScaledDimension(1440, dpiScale),
                                         .height = ScaledDimension(900, dpiScale)});
  if (!created) {
    CALIGO_LOG_ERROR("could not create window: {}", created.GetError().message);
    return 1;
  }

  editor::Editor editor;
  const Result<void> initialized = editor.Initialize(window, options.scenePath);
  if (!initialized) {
    CALIGO_LOG_ERROR("could not start editor: {}", initialized.GetError().message);
    window.Destroy();
    return 1;
  }

  uint32_t frameCount = 0;
  while (!window.ShouldClose() && editor.Frame()) {
    ++frameCount;
    if (options.frames > 0 && frameCount >= options.frames) {
      break;
    }
  }

  editor.Shutdown();
  window.Destroy();
  CALIGO_LOG_INFO("windowed run complete: {} frames", frameCount);
  return 0;
}

}

int RunApplication(int argc, char** argv) {
  Result<Options> parsed = ParseOptions(argc, argv);
  if (!parsed) {
    std::fprintf(stderr, "error: %s\n", parsed.GetError().message.c_str());
    PrintUsage();
    return 1;
  }

  const Options options = parsed.Value();
  if (options.help) {
    PrintUsage();
    return 0;
  }

  platform::ConfigureProcess();
  Logger& logger = GetLogger();
  logger.SetMinimumLevel(options.logLevel);
  logger.AddSink(std::make_unique<ConsoleSink>());

  const Result<std::filesystem::path> executableDirectory = platform::ExecutableDirectory();
  if (executableDirectory) {
    const std::filesystem::path logPath = executableDirectory.Value() / "logs" / "caligo.log";
    auto fileSink = std::make_unique<FileSink>(logPath);
    if (fileSink->IsOpen()) {
      logger.AddSink(std::move(fileSink));
    } else {
      CALIGO_LOG_WARN("could not open log file {}", PathText(logPath));
    }
  } else {
    CALIGO_LOG_WARN("could not resolve executable directory: {}",
                    executableDirectory.GetError().message);
  }

  CALIGO_LOG_INFO("Caligo {} ({})", CALIGO_VERSION, CALIGO_CONFIGURATION);

  JobSystem jobs;
  jobs.Initialize();

  int result = 0;
  if (options.scriptSmoke) {
    result = RunScriptSmoke();
  } else if (options.headless) {
    result = RunHeadless(jobs, options.frames == 0 ? 120 : options.frames);
  } else {
    result = RunWindowed(options);
  }

  jobs.Shutdown();
  logger.Flush();
  return result;
}

}
