#include <windows.h>

#include <caligo/render/RenderGraph.h>
#include <caligo/render/RhiDevice.h>
#include <caligo/render/RhiTexture.h>
#include <doctest/doctest.h>
#include <vector>

namespace {

caligo::render::RenderGraphTextureDesc ColorDescription() {
  caligo::render::RenderGraphTextureDesc description;
  description.width = 8;
  description.height = 8;
  description.format = caligo::render::RhiFormat::Rgba8Unorm;
  return description;
}

}

TEST_CASE("o grafo de renderizacao executa os passes na ordem declarada") {
  auto device = caligo::render::RhiDevice::Create(true);
  REQUIRE(device.HasValue());

  caligo::render::RenderGraph graph;
  std::vector<int> order;

  graph.AddPass([&order](caligo::render::RenderGraphContext&) {
    order.push_back(1);
    return caligo::Result<void>{};
  });
  graph.AddPass([&order](caligo::render::RenderGraphContext&) {
    order.push_back(2);
    return caligo::Result<void>{};
  });

  REQUIRE(graph.Execute(*device.Value()).HasValue());
  REQUIRE(order.size() == 2);
  CHECK(order[0] == 1);
  CHECK(order[1] == 2);
}

TEST_CASE("o grafo de renderizacao resolve o mesmo recurso para quem escreve e quem le") {
  auto device = caligo::render::RhiDevice::Create(true);
  REQUIRE(device.HasValue());

  caligo::render::RenderGraph graph;
  const caligo::render::RenderGraph::ResourceId resource = graph.CreateTexture(ColorDescription());

  const caligo::render::RhiTexture* written = nullptr;
  const caligo::render::RhiTexture* read = nullptr;

  const caligo::render::RenderGraph::PassId writer =
      graph.AddPass([&written, resource](caligo::render::RenderGraphContext& context) {
        written = &context.Texture(resource);
        return caligo::Result<void>{};
      });
  graph.Write(writer, resource);

  const caligo::render::RenderGraph::PassId reader =
      graph.AddPass([&read, resource](caligo::render::RenderGraphContext& context) {
        read = &context.Texture(resource);
        return caligo::Result<void>{};
      });
  graph.Read(reader, resource);

  REQUIRE(graph.Execute(*device.Value()).HasValue());
  REQUIRE(written != nullptr);
  REQUIRE(read != nullptr);
  CHECK(written == read);
}

TEST_CASE("o grafo de renderizacao reutiliza a memoria de recursos com vidas que nao se cruzam") {
  auto device = caligo::render::RhiDevice::Create(true);
  REQUIRE(device.HasValue());

  caligo::render::RenderGraph graph;
  const caligo::render::RenderGraph::ResourceId first = graph.CreateTexture(ColorDescription());
  const caligo::render::RenderGraph::ResourceId second = graph.CreateTexture(ColorDescription());

  const caligo::render::RhiTexture* firstPointer = nullptr;
  const caligo::render::RhiTexture* secondPointer = nullptr;

  const caligo::render::RenderGraph::PassId writeFirst =
      graph.AddPass([&firstPointer, first](caligo::render::RenderGraphContext& context) {
        firstPointer = &context.Texture(first);
        return caligo::Result<void>{};
      });
  graph.Write(writeFirst, first);

  const caligo::render::RenderGraph::PassId readFirst =
      graph.AddPass([first](caligo::render::RenderGraphContext& context) {
        static_cast<void>(context.Texture(first));
        return caligo::Result<void>{};
      });
  graph.Read(readFirst, first);

  const caligo::render::RenderGraph::PassId writeSecond =
      graph.AddPass([&secondPointer, second](caligo::render::RenderGraphContext& context) {
        secondPointer = &context.Texture(second);
        return caligo::Result<void>{};
      });
  graph.Write(writeSecond, second);

  REQUIRE(graph.Execute(*device.Value()).HasValue());
  REQUIRE(firstPointer != nullptr);
  REQUIRE(secondPointer != nullptr);
  CHECK(firstPointer == secondPointer);
}

TEST_CASE("o grafo de renderizacao recusa leitura antes da escrita") {
  auto device = caligo::render::RhiDevice::Create(true);
  REQUIRE(device.HasValue());

  caligo::render::RenderGraph graph;
  const caligo::render::RenderGraph::ResourceId resource = graph.CreateTexture(ColorDescription());

  const caligo::render::RenderGraph::PassId reader =
      graph.AddPass([](caligo::render::RenderGraphContext&) { return caligo::Result<void>{}; });
  graph.Read(reader, resource);

  CHECK_FALSE(graph.Execute(*device.Value()).HasValue());
}
