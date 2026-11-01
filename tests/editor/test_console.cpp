#include <caligo/editor/ConsoleBuffer.h>
#include <doctest/doctest.h>
#include <memory>
#include <string>

TEST_CASE("console buffer keeps the newest entries") {
  auto buffer = std::make_shared<caligo::editor::ConsoleBuffer>();
  caligo::editor::ConsoleSink sink(buffer);

  sink.Write(caligo::LogLevel::Info, "primeira\n");
  sink.Write(caligo::LogLevel::Warn, "segunda\r\n");
  CHECK(buffer->Count() == 2);

  const std::vector<caligo::editor::ConsoleEntry> snapshot = buffer->Snapshot();
  REQUIRE(snapshot.size() == 2);
  CHECK(snapshot[0].text == "primeira");
  CHECK(snapshot[0].level == caligo::LogLevel::Info);
  CHECK(snapshot[1].text == "segunda");
  CHECK(snapshot[1].level == caligo::LogLevel::Warn);

  buffer->Clear();
  CHECK(buffer->Count() == 0);

  for (size_t index = 0; index < caligo::editor::ConsoleBuffer::kCapacity + 3; ++index) {
    buffer->Add(caligo::LogLevel::Debug, std::to_string(index));
  }
  CHECK(buffer->Count() == caligo::editor::ConsoleBuffer::kCapacity);
  const std::vector<caligo::editor::ConsoleEntry> trimmed = buffer->Snapshot();
  REQUIRE(!trimmed.empty());
  CHECK(trimmed.front().text == "3");
  CHECK(trimmed.back().text == std::to_string(caligo::editor::ConsoleBuffer::kCapacity + 2));
}
