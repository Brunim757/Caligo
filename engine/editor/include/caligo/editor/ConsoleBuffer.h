#pragma once

#include <caligo/core/Log.h>
#include <cstddef>
#include <deque>
#include <memory>
#include <mutex>
#include <string>
#include <string_view>
#include <vector>

namespace caligo::editor {

struct ConsoleEntry {
  LogLevel level = LogLevel::Info;
  std::string text;
};

class ConsoleBuffer final {
public:
  static constexpr size_t kCapacity = 4096;

  void Add(LogLevel level, std::string_view text);
  void Clear();
  [[nodiscard]] std::vector<ConsoleEntry> Snapshot() const;
  [[nodiscard]] size_t Count() const;

private:
  mutable std::mutex m_mutex;
  std::deque<ConsoleEntry> m_entries;
};

class ConsoleSink final : public LogSink {
public:
  explicit ConsoleSink(std::shared_ptr<ConsoleBuffer> buffer);

  void Write(LogLevel level, std::string_view line) override;
  void Flush() override;

private:
  std::shared_ptr<ConsoleBuffer> m_buffer;
};

}
