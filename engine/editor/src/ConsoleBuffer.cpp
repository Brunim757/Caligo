#include <caligo/editor/ConsoleBuffer.h>
#include <utility>

namespace caligo::editor {

void ConsoleBuffer::Add(LogLevel level, std::string_view text) {
  std::lock_guard<std::mutex> lock(m_mutex);
  ConsoleEntry entry;
  entry.level = level;
  entry.text.assign(text);
  m_entries.push_back(std::move(entry));
  while (m_entries.size() > kCapacity) {
    m_entries.pop_front();
  }
}

void ConsoleBuffer::Clear() {
  std::lock_guard<std::mutex> lock(m_mutex);
  m_entries.clear();
}

std::vector<ConsoleEntry> ConsoleBuffer::Snapshot() const {
  std::lock_guard<std::mutex> lock(m_mutex);
  return std::vector<ConsoleEntry>(m_entries.begin(), m_entries.end());
}

size_t ConsoleBuffer::Count() const {
  std::lock_guard<std::mutex> lock(m_mutex);
  return m_entries.size();
}

ConsoleSink::ConsoleSink(std::shared_ptr<ConsoleBuffer> buffer) : m_buffer(std::move(buffer)) {}

void ConsoleSink::Write(LogLevel level, std::string_view line) {
  if (!m_buffer) {
    return;
  }
  size_t length = line.size();
  while (length > 0 && (line[length - 1] == '\n' || line[length - 1] == '\r')) {
    --length;
  }
  m_buffer->Add(level, line.substr(0, length));
}

void ConsoleSink::Flush() {}

}
