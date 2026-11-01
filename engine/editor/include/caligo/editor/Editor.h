#pragma once

#include <caligo/core/Result.h>
#include <filesystem>
#include <memory>

namespace caligo::platform {
class Window;
}

namespace caligo::editor {

struct EditorState;

class Editor final {
public:
  Editor();
  ~Editor();

  Editor(const Editor&) = delete;
  Editor& operator=(const Editor&) = delete;
  Editor(Editor&&) = delete;
  Editor& operator=(Editor&&) = delete;

  [[nodiscard]] Result<void> Initialize(platform::Window& window,
                                        const std::filesystem::path& scenePath = {});
  [[nodiscard]] bool Frame();
  void Shutdown() noexcept;

private:
  std::unique_ptr<EditorState> m_state;
};

}
