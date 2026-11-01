#include "EditorPanels.h"
#include <array>
#include <caligo/core/Log.h>
#include <caligo/platform/Input.h>
#include <string>
#include <utility>
#include <vector>

namespace caligo::editor {

namespace {

constexpr std::array<std::pair<platform::Key, script::ScriptKey>, 11> kScriptKeys = {
    std::pair{platform::Key::W, script::ScriptKey::W},
    std::pair{platform::Key::A, script::ScriptKey::A},
    std::pair{platform::Key::S, script::ScriptKey::S},
    std::pair{platform::Key::D, script::ScriptKey::D},
    std::pair{platform::Key::Space, script::ScriptKey::Space},
    std::pair{platform::Key::LeftShift, script::ScriptKey::LeftShift},
    std::pair{platform::Key::Up, script::ScriptKey::Up},
    std::pair{platform::Key::Down, script::ScriptKey::Down},
    std::pair{platform::Key::Left, script::ScriptKey::Left},
    std::pair{platform::Key::Right, script::ScriptKey::Right},
    std::pair{platform::Key::Escape, script::ScriptKey::Escape}};

void FillScriptInput(EditorState& state) {
  if (state.window == nullptr) {
    state.scriptInput = script::ScriptInput{};
    return;
  }
  const platform::InputState& input = state.window->GetInput().State();
  for (const std::pair<platform::Key, script::ScriptKey>& binding : kScriptKeys) {
    const size_t index = static_cast<size_t>(binding.second);
    state.scriptInput.down[index] = input.IsKeyDown(binding.first);
    state.scriptInput.pressed[index] =
        state.scriptInput.pressed[index] || input.WasKeyPressed(binding.first);
  }
}

void ClearPressedInput(EditorState& state) noexcept {
  state.scriptInput.pressed.fill(false);
}

void BeginTick(EditorState& state, float deltaSeconds) {
  const Result<void> ticked = state.scriptHost.Tick(deltaSeconds);
  if (!ticked) {
    ShowError(state, "A execução foi interrompida", ticked.GetError().message);
    StopPlay(state);
    return;
  }
  ++state.play.ticks;
  ClearPressedInput(state);
}

}

void RefreshScriptTypes(EditorState& state) {
  state.scriptTypes.clear();
  const Result<std::vector<std::string>> types = state.scriptHost.ScriptTypes();
  if (!types) {
    return;
  }
  state.scriptTypes = types.Value();
}

void StartPlay(EditorState& state) {
  if (state.play.active) {
    return;
  }
  if (!state.scriptHost.IsReady()) {
    ShowError(state, "Execução indisponível",
              "Os scripts do jogo não foram carregados. Verifique o console e recarregue os "
              "scripts.");
    return;
  }

  state.play.snapshot = state.scene;
  state.play.paused = false;
  state.play.stepRequested = false;
  state.play.ticks = 0;
  state.play.timer.Reset();
  state.play.clock = Clock{};
  state.scriptInput = script::ScriptInput{};
  state.scriptHost.SetInput(&state.scriptInput);

  const Result<void> started = state.scriptHost.BeginPlay(state.scene);
  if (!started) {
    state.scriptHost.SetInput(nullptr);
    state.scene = std::move(state.play.snapshot);
    state.play.snapshot = scene::Scene{};
    state.nameOwner = scene::kNoEntity;
    state.eulerOwner = scene::kNoEntity;
    state.scriptInput = script::ScriptInput{};
    ShowError(state, "Não foi possível executar a cena", started.GetError().message);
    return;
  }

  state.play.active = true;
  CALIGO_LOG_INFO("execução iniciada: {} entidades na cena", state.scene.Entities().size());
}

void StopPlay(EditorState& state) {
  if (!state.play.active) {
    return;
  }
  state.scriptHost.EndPlay();
  state.scriptHost.SetInput(nullptr);

  const uint64_t ticks = state.play.ticks;
  state.play.active = false;
  state.play.paused = false;
  state.play.stepRequested = false;
  state.play.ticks = 0;
  state.scene = std::move(state.play.snapshot);
  state.play.snapshot = scene::Scene{};
  state.scriptInput = script::ScriptInput{};
  state.nameOwner = scene::kNoEntity;
  state.eulerOwner = scene::kNoEntity;
  CALIGO_LOG_INFO("execução encerrada após {} quadros simulados", ticks);
}

void UpdatePlay(EditorState& state) {
  if (!state.play.active) {
    return;
  }

  FillScriptInput(state);
  const double frameDelta = state.play.clock.DeltaSeconds();
  if (state.play.paused) {
    if (!state.play.stepRequested) {
      return;
    }
    state.play.stepRequested = false;
    BeginTick(state, static_cast<float>(state.play.timer.FixedDelta()));
    return;
  }

  const FrameStep step = state.play.timer.Advance(frameDelta);
  for (uint32_t index = 0; index < step.steps && state.play.active; ++index) {
    BeginTick(state, static_cast<float>(step.fixedDelta));
  }
}

void TogglePlayPause(EditorState& state) {
  if (!state.play.active) {
    return;
  }
  state.play.paused = !state.play.paused;
  state.play.stepRequested = false;
  if (state.play.paused) {
    CALIGO_LOG_INFO("execução pausada");
  } else {
    CALIGO_LOG_INFO("execução retomada");
  }
}

void RequestPlayStep(EditorState& state) {
  if (!state.play.active || !state.play.paused) {
    return;
  }
  state.play.stepRequested = true;
}

void ReloadScripts(EditorState& state) {
  const bool wasPlaying = state.play.active;
  if (wasPlaying) {
    state.scriptHost.EndPlay();
  }

  const Result<void> reloaded = state.scriptHost.Reload();
  if (!reloaded) {
    if (wasPlaying) {
      StopPlay(state);
    }
    RefreshScriptTypes(state);
    ShowError(state, "Não foi possível recarregar os scripts", reloaded.GetError().message);
    return;
  }
  RefreshScriptTypes(state);

  if (!wasPlaying) {
    CALIGO_LOG_INFO("scripts recarregados: {} tipos disponíveis", state.scriptTypes.size());
    return;
  }

  state.scriptHost.SetInput(&state.scriptInput);
  const Result<void> started = state.scriptHost.BeginPlay(state.scene);
  if (!started) {
    StopPlay(state);
    ShowError(state, "Não foi possível reiniciar a execução", started.GetError().message);
    return;
  }
  CALIGO_LOG_INFO("scripts recarregados durante a execução: {} tipos disponíveis",
                  state.scriptTypes.size());
}

}